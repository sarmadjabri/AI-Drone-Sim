// Physical input mapping, pilot command shaping and independent trigger roles.
#include "Flight/QuadrotorPawn.h"

#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Flight/FlightControl.h"
#include "GameFramework/PlayerController.h"
#include "QuadrotorControlFrame.h"

void AQuadrotorPawn::SetupPlayerInputComponent(UInputComponent *PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    check(PlayerInputComponent);
    bUseMappedInput = true;
    MotorState = EMotorState::Disarmed;
    CollectiveCommandNewtons = 0;
    MotorThrustsNewtons.fill(0);
    RequireNeutralInput();
    PlayerInputComponent->BindAxis(TEXT("PadLeftX"), this, &AQuadrotorPawn::SetPadLeftX);
    PlayerInputComponent->BindAxis(TEXT("PadLeftY"), this, &AQuadrotorPawn::SetPadLeftY);
    PlayerInputComponent->BindAxis(TEXT("PadRightX"), this, &AQuadrotorPawn::SetPadRightX);
    PlayerInputComponent->BindAxis(TEXT("PadRightY"), this, &AQuadrotorPawn::SetPadRightY);
    PlayerInputComponent->BindAxis(TEXT("PadTriggers"), this, &AQuadrotorPawn::SetPadTriggers);
    PlayerInputComponent->BindAxis(TEXT("FirstPersonThrottle"), this,
                                   &AQuadrotorPawn::SetFirstPersonKeyboardThrottle);

    PlayerInputComponent->BindAxis(TEXT("DroneStrafe"), this, &AQuadrotorPawn::SetKeyboardLateral);
    PlayerInputComponent->BindAxis(TEXT("DroneForward"), this, &AQuadrotorPawn::SetKeyboardForward);
    PlayerInputComponent->BindAxis(TEXT("DroneYaw"), this, &AQuadrotorPawn::SetKeyboardYaw);
    PlayerInputComponent->BindAxis(TEXT("DroneThrottle"), this,
                                   &AQuadrotorPawn::SetKeyboardThrottle);
    PlayerInputComponent->BindAxis(TEXT("PadLeftTrigger"), this, &AQuadrotorPawn::SetLeftTrigger);
    PlayerInputComponent->BindAxis(TEXT("PadRightTrigger"), this, &AQuadrotorPawn::SetRightTrigger);
    PilotControls->Bind(PlayerInputComponent);
    for (auto &Binding : PlayerInputComponent->AxisBindings)
    {
        Binding.bExecuteWhenPaused = true;
    }
}

float AQuadrotorPawn::ApplyDeadzone(float Value) const
{
    const float Deadzone = FMath::Clamp(StickDeadzone, 0.0F, 0.5F);
    Value = FMath::Clamp(Value, -1.0F, 1.0F);
    const float Magnitude = FMath::Abs(Value);
    if (Magnitude <= Deadzone)
    {
        return 0.0F;
    }
    const float Rescaled = (Magnitude - Deadzone) / (1.0F - Deadzone);
    const float Expo = (1.0F - FMath::Clamp(StickExpo, 0.0F, 1.0F)) * Rescaled +
                       FMath::Clamp(StickExpo, 0.0F, 1.0F) * Rescaled * Rescaled * Rescaled;
    return FMath::Sign(Value) * Expo;
}

void AQuadrotorPawn::SetLateralInput(float Value)
{
    RawLateralInput = FMath::Clamp(Value, -1.0F, 1.0F);
    if (FlightMode == EDroneFlightMode::LegacyAssisted && !bUseMappedInput)
    {
        LateralInputTarget = ApplyDeadzone(RawLateralInput);
    }
    else
    {
        LateralInputTarget = PilotControls->Shape(1, RawLateralInput, FlightMode);
    }
    if (FMath::Abs(RawLateralInput) > FMath::Clamp(StickDeadzone, 0.0F, 0.5F))
    {
        bHoldingPosition = false;
        TranslationNeutralSeconds = 0.0F;
    }
}

void AQuadrotorPawn::SetForwardInput(float Value)
{
    RawForwardInput = FMath::Clamp(Value, -1.0F, 1.0F);
    if (FlightMode == EDroneFlightMode::LegacyAssisted && !bUseMappedInput)
    {
        ForwardInputTarget = ApplyDeadzone(RawForwardInput);
    }
    else
    {
        ForwardInputTarget = PilotControls->Shape(0, RawForwardInput, FlightMode);
    }
    if (FMath::Abs(RawForwardInput) > FMath::Clamp(StickDeadzone, 0.0F, 0.5F))
    {
        bHoldingPosition = false;
        TranslationNeutralSeconds = 0.0F;
    }
}

void AQuadrotorPawn::SetYawInput(float Value)
{
    if (FlightMode == EDroneFlightMode::LegacyAssisted && !bUseMappedInput)
    {
        YawInputTarget = ApplyDeadzone(Value);
    }
    else
    {
        YawInputTarget = PilotControls->Shape(2, Value, FlightMode);
    }
}

void AQuadrotorPawn::SetThrottleInput(float Value)
{
    ThrottleInputTarget = FMath::Clamp(Value, -1.0F, 1.0F);
}

void AQuadrotorPawn::RequireNeutralInput()
{
    bAwaitingNeutralInput = true;
    SetForwardInput(0.0F);
    SetLateralInput(0.0F);
    SetYawInput(0.0F);
    SetThrottleInput(0.0F);
    ForwardInput = 0.0F;
    LateralInput = 0.0F;
    YawInput = 0.0F;
    ThrottleInput = 0.0F;
}

void AQuadrotorPawn::UpdatePilotInput()
{
    if (bControllerDisconnected)
    {
        PadLeftX = 0;
        PadLeftY = 0;
        PadRightX = 0;
        PadRightY = 0;
        PadTriggers = 0;
        PadLeftTrigger = 0;
        PadRightTrigger = 0;
    }
    const float Physical[] = {PadLeftX,  PadLeftY,       PadRightX,
                              PadRightY, PadLeftTrigger, PadRightTrigger};
    for (int InputIndex = 0; InputIndex < 6; ++InputIndex)
    {
        PilotControls->Raw[InputIndex] = Physical[InputIndex];
    }
    bool LeftAvailable = UsesMode2Axes();
    bool RightAvailable = UsesMode2Axes();
    Aether::PilotAxes Axes = Aether::RoutePilotAxes(InputProfile, IsFirstPerson(), PadLeftX,
                                                    PadLeftY, PadRightX, PadRightY, PadTriggers);
    if (InputProfile == EDroneInputProfile::Custom)
    {
        float Values[4];
        for (int InputIndex = 0; InputIndex < 4; ++InputIndex)
        {
            const int Source = PilotControls->Axes[InputIndex].Source;
            Values[InputIndex] = Physical[Source];
            if (Source >= 4)
            {
                // Triggers report [0, 1]; flight axes use [-1, 1].
                Values[InputIndex] = Physical[Source] * 2.0F - 1.0F;
            }
        }
        Axes = {Values[0], Values[1], Values[2], Values[3]};
        for (const auto &Axis : PilotControls->Axes)
        {
            if (Axis.Source == Aether::PhysicalAxis::LeftTrigger)
            {
                LeftAvailable = false;
            }
            if (Axis.Source == Aether::PhysicalAxis::RightTrigger)
            {
                RightAvailable = false;
            }
        }
    }
    PilotControls->UpdateAuxiliary(LeftAvailable, RightAvailable);

    const bool bLegacy = FlightMode == EDroneFlightMode::LegacyAssisted && !bUseMappedInput;
    if (bInvertForward)
    {
        Axes.Forward = -Axes.Forward;
    }
    if (bInvertLateral)
    {
        Axes.Right = -Axes.Right;
    }
    if (bInvertYaw)
    {
        Axes.Yaw = -Axes.Yaw;
    }
    SetForwardInput(Axes.Forward);
    SetLateralInput(Axes.Right);
    SetYawInput(Axes.Yaw);
    // Calibration and inversion describe hardware axes, never keyboard directions.
    ForwardInputTarget = FMath::Clamp(ForwardInputTarget + KeyboardForward, -1.0F, 1.0F);
    LateralInputTarget = FMath::Clamp(LateralInputTarget + KeyboardLateral, -1.0F, 1.0F);
    YawInputTarget = FMath::Clamp(YawInputTarget + KeyboardYaw, -1.0F, 1.0F);
    float Vertical = Axes.Vertical;
    if (bLegacy)
    {
        if (UsesMode2Axes())
        {
            Vertical = ApplyDeadzone(Vertical);
        }
    }
    else
    {
        float Expo = PilotControls->Tuning(FlightMode).Expo;
        if (IsManualMode() && PilotControls->CollectiveMode == ECollectiveMode::Direct)
        {
            Expo = 0.0F;
        }
        Vertical = PilotControls->Axes[3].Process(Vertical, Expo);
    }
    if (bInvertVertical)
    {
        Vertical = -Vertical;
    }
    float KeyboardVertical = KeyboardThrottle;
    if (UsesMode2Axes())
    {
        KeyboardVertical = FirstPersonKeyboardThrottle;
    }
    Vertical = FMath::Clamp(KeyboardVertical + Vertical, -1.0F, 1.0F);
    SetThrottleInput(Vertical);
    PilotControls->Processed[Aether::CommandAxis::Forward] = ForwardInputTarget;
    PilotControls->Processed[Aether::CommandAxis::Lateral] = LateralInputTarget;
    PilotControls->Processed[Aether::CommandAxis::Yaw] = YawInputTarget;
    PilotControls->Processed[Aether::CommandAxis::Vertical] = Vertical;
    if (PilotControls->IsMenuOpen())
    {
        ForwardInputTarget = 0;
        LateralInputTarget = 0;
        YawInputTarget = 0;
        ThrottleInputTarget = 0;
        return;
    }
    if (bAwaitingNeutralInput)
    {
        bool bVerticalSafe = FMath::Abs(Vertical) < 0.001F;
        if (IsManualMode() && PilotControls->CollectiveMode == ECollectiveMode::Direct)
        {
            bVerticalSafe = FMath::Abs((Vertical + 1) * 0.5F - PilotControls->Throttle) < 0.03F;
        }
        const bool bNeutral =
            FMath::Abs(ForwardInputTarget) < 0.001F && FMath::Abs(LateralInputTarget) < 0.001F &&
            FMath::Abs(YawInputTarget) < 0.001F && bVerticalSafe &&
            (InputProfile == EDroneInputProfile::Custom ||
             (FMath::Abs(PadTriggers) < 0.08F && FMath::Abs(KeyboardThrottle) < 0.001F &&
              FMath::Abs(FirstPersonKeyboardThrottle) < 0.001F));
        if (!bNeutral)
        {
            ForwardInputTarget = 0;
            LateralInputTarget = 0;
            YawInputTarget = 0;
            ThrottleInputTarget = 0;
            return;
        }
        bAwaitingNeutralInput = false;
        if (IsManualMode() && PilotControls->CollectiveMode == ECollectiveMode::Direct)
        {
            ThrottleInput = ThrottleInputTarget;
        }
    }
}

bool AQuadrotorPawn::UsesMode2Axes() const
{
    return InputProfile == EDroneInputProfile::Custom ||
           InputProfile == EDroneInputProfile::ProfessionalMode2 ||
           InputProfile == EDroneInputProfile::LegacyFPV ||
           (InputProfile == EDroneInputProfile::LegacyViewDependent && IsFirstPerson());
}

FString AQuadrotorPawn::GetInputProfileName() const
{
    switch (InputProfile)
    {
    case EDroneInputProfile::Custom:
        return TEXT("Custom");
    case EDroneInputProfile::Legacy3D:
        return TEXT("Legacy 3D");
    case EDroneInputProfile::LegacyFPV:
        return TEXT("Legacy FPV");
    case EDroneInputProfile::LegacyViewDependent:
        return TEXT("Legacy view-dependent");
    default:
        return TEXT("Professional Mode 2");
    }
}

FString AQuadrotorPawn::GetFlightModeName() const
{
    switch (FlightMode)
    {
    case EDroneFlightMode::HighPerformance:
        return TEXT("High Performance (assisted)");
    case EDroneFlightMode::Angle:
        return TEXT("Angle / collective");
    case EDroneFlightMode::Acro:
        return TEXT("Acro / body rate / collective");
    case EDroneFlightMode::Cinematic:
        return TEXT("Cinematic (assisted)");
    case EDroneFlightMode::Sport:
        return TEXT("Sport (assisted)");
    case EDroneFlightMode::Training:
        return TEXT("Training (assisted)");
    default:
        return TEXT("Legacy assisted");
    }
}

void AQuadrotorPawn::CycleInputProfile()
{
    if (IsManualMode() && MotorState != EMotorState::Disarmed)
    {
        PilotControls->Status = TEXT("Disarm before changing the manual control layout");
        return;
    }
    InputProfile = static_cast<EDroneInputProfile>((static_cast<uint8>(InputProfile) + 1) % 5);
    RequireNeutralInput();
    SaveConfig();
}

void AQuadrotorPawn::CycleFlightMode()
{
    RequestFlightMode(static_cast<EDroneFlightMode>((static_cast<int>(FlightMode) + 1) % 7));
}

void AQuadrotorPawn::UpdatePilotCommands(float DeltaSeconds)
{
    PilotControls->StepHolds(DeltaSeconds);
    UpdateResetHold(DeltaSeconds);
    if (bUseMappedInput)
    {
        UpdatePilotInput();
    }
    if (UsesMode2Axes())
    {
        float AvailableLeftTrigger = PadLeftTrigger;
        float AvailableRightTrigger = PadRightTrigger;
        if (InputProfile == EDroneInputProfile::Custom)
        {
            for (const auto &Axis : PilotControls->Axes)
            {
                if (Axis.Source == Aether::PhysicalAxis::LeftTrigger)
                {
                    AvailableLeftTrigger = 0;
                }
                if (Axis.Source == Aether::PhysicalAxis::RightTrigger)
                {
                    AvailableRightTrigger = 0;
                }
            }
        }
        const float Values[] = {AvailableLeftTrigger, AvailableRightTrigger};
        const ETriggerRole Roles[] = {PilotControls->LeftTriggerRole, PilotControls->TriggerRole};
        for (int InputIndex = 0; InputIndex < 2; ++InputIndex)
        {
            float Direction = 1.0F;
            if (InputIndex == 0)
            {
                Direction = -1.0F;
            }
            const float Auxiliary = Values[InputIndex] * Direction *
                                    PilotControls->CameraSensitivity *
                                    FMath::Min(DeltaSeconds, 0.05F);
            if (Roles[InputIndex] == ETriggerRole::Zoom)
            {
                AdjustPilotCameraZoom(Auxiliary * 6);
            }
            else if (Roles[InputIndex] == ETriggerRole::Gimbal)
            {
                FirstPersonCameraController->AdjustGimbalPitch(Auxiliary * 45);
            }
        }
    }
}

float AQuadrotorPawn::SmoothPilotAxis(float Current, float Target, float DeltaSeconds,
                                      const FQuadrotorControlFrame &Frame,
                                      const FPilotModeTuning &ModeTuning) const
{
    if (!Frame.bUsesLegacyTuning && ModeTuning.Slew == 0)
    {
        return Target;
    }
    const bool bReleasing = Current * Target < 0.0F || FMath::Abs(Target) < FMath::Abs(Current);
    float ResponseSpeed = ModeTuning.Slew;
    if (Frame.bUsesLegacyTuning)
    {
        ResponseSpeed = StickResponseSpeed;
    }
    if (bReleasing)
    {
        ResponseSpeed *= StickReleaseMultiplier;
    }
    return Aether::SlewTowards(Current, Target, ResponseSpeed, DeltaSeconds);
}

void AQuadrotorPawn::UpdateSmoothedPilotCommands(float DeltaSeconds,
                                                 const FQuadrotorControlFrame &Frame,
                                                 const FPilotModeTuning &ModeTuning)
{
    LateralInput =
        SmoothPilotAxis(LateralInput, LateralInputTarget, DeltaSeconds, Frame, ModeTuning);
    ForwardInput =
        SmoothPilotAxis(ForwardInput, ForwardInputTarget, DeltaSeconds, Frame, ModeTuning);
    YawInput = SmoothPilotAxis(YawInput, YawInputTarget, DeltaSeconds, Frame, ModeTuning);
    // Direct collective is absolute: slewing from neutral after matching would command 50% thrust.
    if (IsManualMode() && PilotControls->CollectiveMode == ECollectiveMode::Direct)
    {
        ThrottleInput = ThrottleInputTarget;
    }
    else
    {
        ThrottleInput =
            SmoothPilotAxis(ThrottleInput, ThrottleInputTarget, DeltaSeconds, Frame, ModeTuning);
    }
}
