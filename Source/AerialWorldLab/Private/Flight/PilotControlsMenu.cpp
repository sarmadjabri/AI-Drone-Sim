// Settings navigation and editing; menu presentation does not execute flight control.
#include "Flight/PilotControlsComponent.h"
#include "PilotMenuLayout.h"
#include "Components/InputComponent.h"
#include "Flight/QuadrotorPawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

void UPilotControlsComponent::MenuKey(FKey Key)
{
    if (bCapturing)
    {
        if (Key == EKeys::Escape)
        {
            bCapturing = false;
            return;
        }
        FPilotButton PreviousBinding = Buttons[ButtonIndex];
        Buttons[ButtonIndex].Key = Key;
        Buttons[ButtonIndex].Modifiers = ModifierMask();
        FString Error;
        if (!Validate(Error))
        {
            Buttons[ButtonIndex] = PreviousBinding;
            Status = Error;
        }
        else
        {
            Status = TEXT("Binding changed");
            bCapturing = false;
        }
        return;
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right ||
        Key == EKeys::Gamepad_Special_Right)
    {
        ToggleMenu();
        return;
    }
    if (Key == EKeys::Tab || Key == EKeys::Gamepad_FaceButton_Top)
    {
        Page = (Page + 1) % Aether::PilotMenu::PageCount;
        Row = 0;
        return;
    }
    const int RowCounts[] = {
        static_cast<int32>(Aether::PilotMenu::EGeneralRow::Count),
        static_cast<int32>(Aether::PilotMenu::EAxisRow::Count),
        Aether::GetFlightTuningFields().Num(),
        2, // Action selection and modifier selection.
        1  // Read-only controller map.
    };
    if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
    {
        Row = (Row + RowCounts[Page] - 1) % RowCounts[Page];
    }
    else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
    {
        Row = (Row + 1) % RowCounts[Page];
    }
    else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
    {
        Adjust(-1);
    }
    else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
    {
        Adjust(1);
    }
    else if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (Page == Aether::PilotMenu::ButtonsPage)
        {
            bCapturing = true;
            Status = TEXT("Hold LB/L1 or RB/R1, then press the new button; Esc cancels");
        }
        else if (Page == Aether::PilotMenu::GeneralPage &&
                 Row == static_cast<int32>(Aether::PilotMenu::EGeneralRow::SaveProfile))
        {
            SaveSlot(SlotIndex);
        }
        else if (Page == Aether::PilotMenu::GeneralPage &&
                 Row == static_cast<int32>(Aether::PilotMenu::EGeneralRow::LoadProfile))
        {
            LoadSlot(SlotIndex);
        }
        else if (Page == Aether::PilotMenu::GeneralPage &&
                 Row == static_cast<int32>(Aether::PilotMenu::EGeneralRow::RestoreDefaults))
        {
            if (Drone() && Drone()->IsManualMode() && Drone()->MotorState != EMotorState::Disarmed)
            {
                Status = TEXT("Disarm before restoring defaults");
            }
            else
            {
                RestoreDefaults();
                Drone()->ResetHoldSeconds = ResetHoldSeconds;
                Status = TEXT("Defaults restored");
            }
        }
        else
        {
            Adjust(1);
        }
    }
}

void UPilotControlsComponent::Adjust(int32 AdjustmentDirection)
{
    AQuadrotorPawn *DronePawn = Drone();
    if (!DronePawn)
    {
        return;
    }
    if ((Page == Aether::PilotMenu::AxesPage ||
         (Page == Aether::PilotMenu::GeneralPage && Row >= 2 && Row <= 4)) &&
        DronePawn->IsManualMode() && DronePawn->MotorState != EMotorState::Disarmed)
    {
        Status = TEXT("Disarm before changing manual axis/throttle interpretation");
        return;
    }
    switch (Page)
    {
    case Aether::PilotMenu::GeneralPage:
        AdjustGeneralSettings(AdjustmentDirection, DronePawn);
        break;
    case Aether::PilotMenu::AxesPage:
        AdjustAxisCalibration(AdjustmentDirection, DronePawn);
        break;
    case Aether::PilotMenu::ModeTuningPage:
        AdjustFlightModeSettings(AdjustmentDirection, DronePawn);
        break;
    case Aether::PilotMenu::ButtonsPage:
        AdjustButtonSettings(AdjustmentDirection, DronePawn);
        break;
    default:
        break;
    }
}

void UPilotControlsComponent::AdjustGeneralSettings(int32 AdjustmentDirection,
                                                    AQuadrotorPawn *DronePawn)
{
    switch (static_cast<Aether::PilotMenu::EGeneralRow>(Row))
    {
    case Aether::PilotMenu::EGeneralRow::InputProfile:
        DronePawn->CycleInputProfile();
        break;
    case Aether::PilotMenu::EGeneralRow::FlightMode:
        DronePawn->RequestFlightMode(
            static_cast<EDroneFlightMode>((static_cast<int>(DronePawn->FlightMode) +
                                           AdjustmentDirection + Aether::FlightModeCount) %
                                          Aether::FlightModeCount));
        break;
    case Aether::PilotMenu::EGeneralRow::CollectiveMode:
        if (CollectiveMode == ECollectiveMode::Incremental)
        {
            CollectiveMode = ECollectiveMode::Direct;
        }
        else
        {
            CollectiveMode = ECollectiveMode::Incremental;
        }
        Throttle = 0;
        break;
    case Aether::PilotMenu::EGeneralRow::ThrottleIncrease:
        ThrottleIncrease =
            FMath::Clamp(ThrottleIncrease + AdjustmentDirection * 0.05F, 0.05F, 1.0F);
        break;
    case Aether::PilotMenu::EGeneralRow::ThrottleDecrease:
        ThrottleDecrease =
            FMath::Clamp(ThrottleDecrease + AdjustmentDirection * 0.05F, 0.05F, 1.0F);
        break;
    case Aether::PilotMenu::EGeneralRow::PrecisionScale:
        PrecisionScale = FMath::Clamp(PrecisionScale + AdjustmentDirection * 0.05F, 0.1F, 1.0F);
        break;
    case Aether::PilotMenu::EGeneralRow::RightTriggerRole:
        TriggerRole = static_cast<ETriggerRole>(
            (static_cast<int>(TriggerRole) + AdjustmentDirection + 5) % 5);
        break;
    case Aether::PilotMenu::EGeneralRow::LeftTriggerRole:
        LeftTriggerRole = static_cast<ETriggerRole>(
            (static_cast<int>(LeftTriggerRole) + AdjustmentDirection + 5) % 5);
        break;
    case Aether::PilotMenu::EGeneralRow::CameraSensitivity:
        CameraSensitivity =
            FMath::Clamp(CameraSensitivity + AdjustmentDirection * 0.1F, 0.1F, 3.0F);
        break;
    case Aether::PilotMenu::EGeneralRow::ArmHoldDuration:
        HoldSeconds = FMath::Clamp(HoldSeconds + AdjustmentDirection * 0.25F, 0.5F, 5.0F);
        break;
    case Aether::PilotMenu::EGeneralRow::ResetHoldDuration:
        DronePawn->ResetHoldSeconds =
            FMath::Clamp(DronePawn->ResetHoldSeconds + AdjustmentDirection * 0.25F, 0.5F, 5.0F);
        break;
    case Aether::PilotMenu::EGeneralRow::ProfileSlot:
        if (SlotIndex == 1)
        {
            SlotIndex = 2;
        }
        else
        {
            SlotIndex = 1;
        }
        break;
    default:
        break;
    }
}

void UPilotControlsComponent::AdjustAxisCalibration(int32 AdjustmentDirection,
                                                    AQuadrotorPawn *DronePawn)
{
    if (Row == static_cast<int32>(Aether::PilotMenu::EAxisRow::SelectedAxis))
    {
        AxisIndex = (AxisIndex + AdjustmentDirection + 4) % 4;
        return;
    }
    auto &AxisCalibration = Axes[AxisIndex];
    const FPrecisionAxis PreviousCalibration = AxisCalibration;
    switch (static_cast<Aether::PilotMenu::EAxisRow>(Row))
    {
    case Aether::PilotMenu::EAxisRow::PhysicalSource:
    {
        const int Source =
            (AxisCalibration.Source + AdjustmentDirection + Aether::PhysicalAxis::Count) %
            Aether::PhysicalAxis::Count;
        for (int Index = 0; Index < 4; ++Index)
        {
            if (Index != AxisIndex && Axes[Index].Source == Source)
            {
                Axes[Index].Source = AxisCalibration.Source;
            }
        }
        AxisCalibration.Source = Source;
        DronePawn->InputProfile = EDroneInputProfile::Custom;
        break;
    }
    case Aether::PilotMenu::EAxisRow::Invert:
        AxisCalibration.bInvert = !AxisCalibration.bInvert;
        break;
    case Aether::PilotMenu::EAxisRow::InnerDeadZone:
        AxisCalibration.Inner =
            FMath::Clamp(AxisCalibration.Inner + AdjustmentDirection * 0.01F, 0.0F, 0.5F);
        break;
    case Aether::PilotMenu::EAxisRow::OuterDeadZone:
        AxisCalibration.Outer =
            FMath::Clamp(AxisCalibration.Outer + AdjustmentDirection * 0.01F, 0.0F, 0.2F);
        break;
    case Aether::PilotMenu::EAxisRow::Center:
        AxisCalibration.Center += AdjustmentDirection * 0.01F;
        break;
    case Aether::PilotMenu::EAxisRow::Minimum:
        AxisCalibration.Minimum =
            FMath::Clamp(AxisCalibration.Minimum + AdjustmentDirection * 0.01F, -1.0F, 0.0F);
        break;
    case Aether::PilotMenu::EAxisRow::Maximum:
        AxisCalibration.Maximum =
            FMath::Clamp(AxisCalibration.Maximum + AdjustmentDirection * 0.01F, 0.0F, 1.0F);
        break;
    case Aether::PilotMenu::EAxisRow::Sensitivity:
        AxisCalibration.Sensitivity =
            FMath::Clamp(AxisCalibration.Sensitivity + AdjustmentDirection * 0.05F, 0.1F, 2.0F);
        break;
    case Aether::PilotMenu::EAxisRow::ExpoOffset:
        AxisCalibration.ExpoOffset =
            FMath::Clamp(AxisCalibration.ExpoOffset + AdjustmentDirection * 0.05F, -1.0F, 1.0F);
        break;
    default:
        break;
    }
    if (!AxisCalibration.IsValid())
    {
        AxisCalibration = PreviousCalibration;
        Status = TEXT("Calibration range must span its center");
    }
}

void UPilotControlsComponent::AdjustFlightModeSettings(int32 AdjustmentDirection,
                                                       AQuadrotorPawn *DronePawn)
{
    if (DronePawn->FlightMode == EDroneFlightMode::LegacyAssisted)
    {
        Status = TEXT("Original legacy tuning is read-only; select a configurable mode");
        return;
    }
    auto &ModeTuning = Modes[static_cast<int>(DronePawn->FlightMode)];
    const auto &FieldDefinition = Aether::GetFlightTuningFields()[Row];
    float &CurrentValue = ModeTuning.*(FieldDefinition.Value);
    const float RequestedValue = CurrentValue + AdjustmentDirection * FieldDefinition.Step;
    CurrentValue = FMath::Clamp(RequestedValue, FieldDefinition.Minimum, FieldDefinition.Maximum);
}

void UPilotControlsComponent::AdjustButtonSettings(int32 AdjustmentDirection,
                                                   AQuadrotorPawn *DronePawn)
{
    if (Row == 0)
    {
        ButtonIndex = (ButtonIndex + AdjustmentDirection + Buttons.Num()) % Buttons.Num();
    }
    else
    {
        Buttons[ButtonIndex].Modifiers =
            (Buttons[ButtonIndex].Modifiers + AdjustmentDirection + 3) % 3;
        FString Error;
        if (!Validate(Error))
        {
            Buttons[ButtonIndex].Modifiers =
                (Buttons[ButtonIndex].Modifiers - AdjustmentDirection + 3) % 3;
            Status = Error;
        }
    }
}

FString UPilotControlsComponent::DescribeMenu() const
{
    const AQuadrotorPawn *DronePawn = Drone();
    if (!DronePawn)
    {
        return TEXT("");
    }
    FString Description =
        TEXT("CONTROLS — PAUSED: inputs shown here do not move the aircraft\nTab/Y/Triangle: page "
             "| arrows/D-pad: select/change | Enter/A/Cross: confirm | Esc/B/Circle: back\n");
    const auto AppendMenuRow = [&](int Index, const FString &Text)
    {
        if (Row == Index)
        {
            Description += TEXT("> ");
        }
        else
        {
            Description += TEXT("  ");
        }
        Description += Text + TEXT("\n");
    };
    if (Page == Aether::PilotMenu::GeneralPage)
    {
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::InputProfile),
                      TEXT("Profile: ") + DronePawn->GetInputProfileName());
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::FlightMode),
                      TEXT("Mode: ") + DronePawn->GetFlightModeName());
        FString ThrottleDescription = TEXT("Direct (center = 50%)");
        if (CollectiveMode == ECollectiveMode::Incremental)
        {
            ThrottleDescription = TEXT("Incremental (center holds)");
        }
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::CollectiveMode),
                      FString(TEXT("Manual throttle: ")) + ThrottleDescription);
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::ThrottleIncrease),
                      FString::Printf(TEXT("Throttle increase /s: %.2f"), ThrottleIncrease));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::ThrottleDecrease),
                      FString::Printf(TEXT("Throttle decrease /s: %.2f"), ThrottleDecrease));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::PrecisionScale),
                      FString::Printf(TEXT("Precision scale: %.2f"), PrecisionScale));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::RightTriggerRole),
                      TEXT("RT/R2 role: ") + StaticEnum<ETriggerRole>()->GetNameStringByValue(
                                                 static_cast<int64>(TriggerRole)));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::CameraSensitivity),
                      FString::Printf(TEXT("Camera sensitivity: %.2f"), CameraSensitivity));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::ArmHoldDuration),
                      FString::Printf(TEXT("Arm hold seconds: %.2f"), HoldSeconds));
        AppendMenuRow(
            static_cast<int32>(Aether::PilotMenu::EGeneralRow::ResetHoldDuration),
            FString::Printf(TEXT("Reset hold seconds: %.2f"), DronePawn->ResetHoldSeconds));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::ProfileSlot),
                      FString::Printf(TEXT("Custom slot: %d"), SlotIndex));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::SaveProfile),
                      TEXT("Save profile"));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::LoadProfile),
                      TEXT("Load profile"));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::RestoreDefaults),
                      TEXT("Restore defaults"));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EGeneralRow::LeftTriggerRole),
                      TEXT("LT/L2 role: ") + StaticEnum<ETriggerRole>()->GetNameStringByValue(
                                                 static_cast<int64>(LeftTriggerRole)));
    }
    else if (Page == Aether::PilotMenu::AxesPage)
    {
        const auto &AxisCalibration = Axes[AxisIndex];
        const TCHAR *Names[] = {TEXT("Pitch/forward"), TEXT("Roll/lateral"), TEXT("Yaw"),
                                TEXT("Vertical")};
        const TCHAR *Sources[] = {TEXT("LX"), TEXT("LY"),          TEXT("RX"),
                                  TEXT("RY"), TEXT("LT (signed)"), TEXT("RT (signed)")};
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::SelectedAxis),
                      FString(TEXT("Axis: ")) + Names[AxisIndex]);
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::PhysicalSource),
                      FString(TEXT("Custom source: ")) + Sources[AxisCalibration.Source]);
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::Invert),
                      FString::Printf(TEXT("Invert: %d"), AxisCalibration.bInvert));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::InnerDeadZone),
                      FString::Printf(TEXT("Inner dead zone: %.2f"), AxisCalibration.Inner));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::OuterDeadZone),
                      FString::Printf(TEXT("Outer dead zone: %.2f"), AxisCalibration.Outer));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::Center),
                      FString::Printf(TEXT("Center: %.2f"), AxisCalibration.Center));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::Minimum),
                      FString::Printf(TEXT("Minimum: %.2f"), AxisCalibration.Minimum));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::Maximum),
                      FString::Printf(TEXT("Maximum: %.2f"), AxisCalibration.Maximum));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::Sensitivity),
                      FString::Printf(TEXT("Sensitivity: %.2f"), AxisCalibration.Sensitivity));
        AppendMenuRow(static_cast<int32>(Aether::PilotMenu::EAxisRow::ExpoOffset),
                      FString::Printf(TEXT("Expo offset: %.2f"), AxisCalibration.ExpoOffset));
    }
    else if (Page == Aether::PilotMenu::ModeTuningPage)
    {
        Description += DronePawn->GetFlightModeName() + TEXT("\n");
        const auto &ModeTuning = Tuning(DronePawn->FlightMode);
        for (int Index = 0; Index < Aether::GetFlightTuningFields().Num(); ++Index)
        {
            AppendMenuRow(Index, FString::Printf(
                                     TEXT("%s: %.2f"), Aether::GetFlightTuningFields()[Index].Label,
                                     ModeTuning.*(Aether::GetFlightTuningFields()[Index].Value)));
        }
    }
    else if (Page == Aether::PilotMenu::ButtonsPage)
    {
        const auto &SelectedBinding = Buttons[ButtonIndex];
        AppendMenuRow(0, GetActionName(SelectedBinding.Action) + TEXT(" : ") +
                             ButtonLabel(SelectedBinding.Key));
        AppendMenuRow(1, FString::Printf(TEXT("Modifier: %d (0 none, 1 LB/L1, 2 RB/R1)"),
                                         SelectedBinding.Modifiers));
        if (bCapturing)
        {
            Description += TEXT("Listening for button...\n");
        }
        else
        {
            Description += TEXT("A/Cross/Enter: select and press a new button\n");
        }
    }
    if (Page == Aether::PilotMenu::ControllerMapPage)
    {
        return Description +
               TEXT("Controller map: hold LB/L1 or RB/R1 to inspect modified actions. Active "
                    "controls glow.\n");
    }
    return Description + TEXT("\n") + Diagnostics();
}

bool UPilotControlsComponent::IsMapPage() const
{
    return Page == Aether::PilotMenu::ControllerMapPage;
}
