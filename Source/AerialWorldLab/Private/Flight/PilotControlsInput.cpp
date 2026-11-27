// Button chords, deliberate holds, action dispatch and pause/input lifecycle.
#include "Flight/PilotControlsComponent.h"
#include "Components/InputComponent.h"
#include "Flight/QuadrotorPawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

int32 UPilotControlsComponent::FindBinding(FKey Key, int32 Modifiers) const
{
    for (int Index = 0; Index < Buttons.Num(); ++Index)
    {
        if (Buttons[Index].Key == Key && Buttons[Index].Modifiers == Modifiers)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

void UPilotControlsComponent::Bind(UInputComponent *Input)
{
    FString Error;
    if (!Validate(Error))
    {
        RestoreDefaults();
        Status = TEXT("Invalid config restored to defaults: ") + Error;
    }
    auto &Pressed =
        Input->BindKey(EKeys::AnyKey, IE_Pressed, this, &UPilotControlsComponent::Press);
    Pressed.bExecuteWhenPaused = true;
    Pressed.bConsumeInput = false;
    auto &Released =
        Input->BindKey(EKeys::AnyKey, IE_Released, this, &UPilotControlsComponent::Release);
    Released.bExecuteWhenPaused = true;
    Released.bConsumeInput = false;
}

void UPilotControlsComponent::CancelHeld()
{
    HeldReset = FKey();
    HeldArm = FKey();
    ArmSeconds = 0;
    HeldKeys.Reset();
    bPrecision = false;
    AuxiliaryChannels[0] = 0;
    AuxiliaryChannels[1] = 0;
    bFlightModifier = false;
    bCameraModifier = false;
    if (Drone())
    {
        Drone()->EndResetHold();
    }
}

void UPilotControlsComponent::Press(FKey Key)
{
    if (HeldKeys.Contains(Key))
    {
        return; // OS repeats must not restart holds or retrigger actions.
    }
    HeldKeys.Add(Key);
    if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
    {
        // A newly introduced modifier cancels any dangerous unmodified hold.
        HeldReset = FKey();
        HeldArm = FKey();
        ArmSeconds = 0;
        if (Drone())
        {
            Drone()->EndResetHold();
        }
        if (Key == EKeys::Gamepad_LeftShoulder)
        {
            bFlightModifier = true;
        }
        else
        {
            bCameraModifier = true;
        }
        return;
    }
    if (bMenuOpen)
    {
        MenuKey(Key);
        return;
    }
    if (Key == EKeys::Escape)
    {
        ToggleMenu();
        return;
    }
    const int Index = FindBinding(Key, ModifierMask());
    if (Index == INDEX_NONE)
    {
        return;
    }
    const auto Action = Buttons[Index].Action;
    if (Action == EPilotAction::Reset)
    {
        HeldReset = Key;
        if (Drone())
        {
            Drone()->BeginResetHold();
        }
    }
    else if (Action == EPilotAction::Arm)
    {
        HeldArm = Key;
        ArmSeconds = 0;
    }
    else
    {
        Dispatch(Action);
    }
}

void UPilotControlsComponent::Release(FKey Key)
{
    HeldKeys.Remove(Key);
    if (Key == HeldReset)
    {
        HeldReset = FKey();
        if (Drone())
        {
            Drone()->EndResetHold();
        }
    }
    if (Key == HeldArm)
    {
        HeldArm = FKey();
        ArmSeconds = 0;
    }
    if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
    {
        // Removing either modifier invalidates a pending chord, including custom reset chords.
        HeldReset = FKey();
        HeldArm = FKey();
        ArmSeconds = 0;
        if (Drone())
        {
            Drone()->EndResetHold();
        }
        if (Key == EKeys::Gamepad_LeftShoulder)
        {
            bFlightModifier = false;
        }
        else
        {
            bCameraModifier = false;
        }
    }
}

void UPilotControlsComponent::StepHolds(float DeltaSeconds)
{
    if (HeldArm.IsValid() && !bMenuOpen)
    {
        ArmSeconds += FMath::Max(0.0F, DeltaSeconds);
        if (ArmSeconds >= HoldSeconds)
        {
            HeldArm = FKey();
            ArmSeconds = 0;
            if (Drone())
            {
                Drone()->ToggleArm();
            }
        }
    }
}

void UPilotControlsComponent::Dispatch(EPilotAction Action)
{
    AQuadrotorPawn *DronePawn = Drone();
    if (!DronePawn)
    {
        return;
    }
    switch (Action)
    {
    case EPilotAction::Confirm:
        Status = TEXT("Ready; open Menu/Options for settings");
        break;
    case EPilotAction::Cancel:
        DronePawn->bLanding = false;
        DronePawn->AltitudeTargetMeters = DronePawn->GetActorLocation().Z / 100.0F;
        DronePawn->RequireNeutralInput();
        Status = TEXT("Operation cancelled");
        break;
    case EPilotAction::Camera:
        DronePawn->ToggleViewMode();
        break;
    case EPilotAction::Settings:
        ToggleMenu();
        break;
    case EPilotAction::Telemetry:
        bTelemetry = !bTelemetry;
        break;
    case EPilotAction::Profile:
        DronePawn->CycleInputProfile();
        break;
    case EPilotAction::NextMode:
        DronePawn->RequestFlightMode(static_cast<EDroneFlightMode>(
            (static_cast<int>(DronePawn->FlightMode) + 1) % Aether::FlightModeCount));
        break;
    case EPilotAction::PreviousMode:
        DronePawn->RequestFlightMode(static_cast<EDroneFlightMode>(
            (static_cast<int>(DronePawn->FlightMode) + Aether::FlightModeCount - 1) %
            Aether::FlightModeCount));
        break;
    case EPilotAction::IncreaseLimit:
    case EPilotAction::DecreaseLimit:
    {
        if (DronePawn->FlightMode == EDroneFlightMode::LegacyAssisted)
        {
            Status = TEXT("Select Cinematic/Sport/Training/High Performance to adjust limits");
            return;
        }
        auto &ModeTuning = Modes[static_cast<int>(DronePawn->FlightMode)];
        float Sign = -1.0F;
        if (Action == EPilotAction::IncreaseLimit)
        {
            Sign = 1.0F;
        }
        if (DronePawn->IsManualMode())
        {
            ModeTuning.RateDegrees = Aether::ClampFlightTuningValue(
                &FPilotModeTuning::RateDegrees, ModeTuning.RateDegrees + Sign * 10);
        }
        else
        {
            ModeTuning.Speed =
                Aether::ClampFlightTuningValue(&FPilotModeTuning::Speed, ModeTuning.Speed + Sign);
        }
        break;
    }
    case EPilotAction::Brake:
        if (!DronePawn->IsManualMode())
        {
            DronePawn->RequireNeutralInput();
            Status = TEXT("Braking; hold captures when settled");
        }
        else
        {
            Status = TEXT("Brake/position hold unavailable in manual modes");
        }
        break;
    case EPilotAction::Recenter:
        DronePawn->FirstPersonCameraController->RecenterGimbal();
        break;
    case EPilotAction::GimbalUp:
    case EPilotAction::GimbalDown:
        if (DronePawn->ViewMode != EDroneViewMode::StabilizedFirstPerson)
        {
            Status = TEXT("Gimbal pitch is available only in stabilized FPV");
        }
        else
        {
            float PitchStep = -5.0F;
            if (Action == EPilotAction::GimbalUp)
            {
                PitchStep = 5.0F;
            }
            DronePawn->FirstPersonCameraController->AdjustGimbalPitch(PitchStep *
                                                                      CameraSensitivity);
        }
        break;
    case EPilotAction::ZoomIn:
    case EPilotAction::ZoomOut:
    {
        float ZoomStep = -1.0F;
        if (Action == EPilotAction::ZoomIn)
        {
            ZoomStep = 1.0F;
        }
        DronePawn->AdjustPilotCameraZoom(ZoomStep * CameraSensitivity);
        break;
    }
    case EPilotAction::Takeoff:
        DronePawn->RequestTakeoff();
        break;
    case EPilotAction::Land:
        DronePawn->RequestLanding();
        break;
    default:
        break;
    }
}

void UPilotControlsComponent::ToggleMenu()
{
    AQuadrotorPawn *DronePawn = Drone();
    if (!DronePawn)
    {
        return;
    }
    APlayerController *PlayerController = Cast<APlayerController>(DronePawn->GetController());
    if (!PlayerController)
    {
        return;
    }
    if (bMenuOpen)
    {
        PlayerController->SetPause(false);
        bMenuOpen = false;
        DronePawn->RequireNeutralInput();
    }
    else
    {
        bMenuOpen = PlayerController->SetPause(true);
        CancelHeld();
    }
    bCapturing = false;
}

void UPilotControlsComponent::RefreshPausedInput()
{
    // Actor flight ticks stop while paused; axis bindings continue so calibration stays live.
    if (bMenuOpen && Drone())
    {
        Drone()->UpdatePilotInput();
    }
}
