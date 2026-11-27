// Calibration, shared validation and named default pilot settings.
#include "Flight/PilotControlsComponent.h"
#include "Components/InputComponent.h"
#include "Flight/QuadrotorPawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

bool FPrecisionAxis::IsValid() const
{
    const float CalibrationValues[] = {Center, Minimum,     Maximum,   Inner,
                                       Outer,  Sensitivity, ExpoOffset};
    for (const float Value : CalibrationValues)
    {
        if (!FMath::IsFinite(Value))
        {
            return false;
        }
    }
    // Each calibrated half-axis must have usable travel around its center.
    if (Minimum < -1.0F || Maximum > 1.0F || Center - Minimum < 0.1F || Maximum - Center < 0.1F)
    {
        return false;
    }
    if (Inner < 0.0F || Inner > 0.5F || Outer < 0.0F || Outer > 0.2F)
    {
        return false;
    }
    if (Sensitivity < 0.1F || Sensitivity > 2.0F || ExpoOffset < -1.0F || ExpoOffset > 1.0F)
    {
        return false;
    }
    return Source >= 0 && Source < 6;
}

float FPrecisionAxis::Process(float RawValue, float Expo) const
{
    if (!IsValid() || !FMath::IsFinite(RawValue) || !FMath::IsFinite(Expo))
    {
        return 0.0F;
    }
    const float Centered = RawValue - Center;
    // Calibration can be asymmetric about the measured stick center.
    float AxisRange = Center - Minimum;
    if (Centered >= 0)
    {
        AxisRange = Maximum - Center;
    }
    const float Normalized = Centered / AxisRange;
    const float Magnitude =
        FMath::Clamp((FMath::Abs(Normalized) - Inner) / (1.0F - Inner - Outer), 0.0F, 1.0F);
    const float ResponseExpo = FMath::Clamp(Expo + ExpoOffset, 0.0F, 1.0F);
    float Direction = FMath::Sign(Normalized);
    if (bInvert)
    {
        Direction = -Direction;
    }
    const float LinearResponse = (1.0F - ResponseExpo) * Magnitude;
    const float CubicResponse = ResponseExpo * Magnitude * Magnitude * Magnitude;
    const float ShapedMagnitude =
        FMath::Clamp((LinearResponse + CubicResponse) * Sensitivity, 0.0F, 1.0F);
    return ShapedMagnitude * Direction;
}

UPilotControlsComponent::UPilotControlsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    RestoreDefaults();
}

void UPilotControlsComponent::RestoreDefaults()
{
    Axes.SetNum(Aether::CommandAxis::Count);
    const int Sources[] = {Aether::PhysicalAxis::RightY, Aether::PhysicalAxis::RightX,
                           Aether::PhysicalAxis::LeftX, Aether::PhysicalAxis::LeftY};
    for (int Index = 0; Index < Aether::CommandAxis::Count; ++Index)
    {
        Axes[Index] = FPrecisionAxis();
        Axes[Index].Source = Sources[Index];
    }
    // Enum order is serialized in profiles. Build every mode through the named
    // factory rather than scattering numeric mode indices through this class.
    Modes.SetNum(Aether::FlightModeCount);
    for (int32 Index = 0; Index < Modes.Num(); ++Index)
    {
        const EDroneFlightMode Mode = static_cast<EDroneFlightMode>(Index);
        Modes[Index] = Aether::MakeDefaultFlightModeTuning(Mode);
    }
    CollectiveMode = ECollectiveMode::Incremental;
    ThrottleIncrease = 0.35F;
    ThrottleDecrease = 0.5F;
    ResetHoldSeconds = 1.5F;
    LeftTriggerRole = ETriggerRole::Unused;
    PrecisionScale = 0.25F;
    CameraSensitivity = 1;
    HoldSeconds = 1.5F;
    TriggerRole = ETriggerRole::Precision;
    Buttons = {{EKeys::Gamepad_FaceButton_Bottom, 0, EPilotAction::Confirm},
               {EKeys::Gamepad_FaceButton_Right, 0, EPilotAction::Cancel},
               {EKeys::Gamepad_FaceButton_Top, 0, EPilotAction::Camera},
               {EKeys::V, 0, EPilotAction::Camera},
               {EKeys::Gamepad_FaceButton_Left, 0, EPilotAction::Reset},
               {EKeys::BackSpace, 0, EPilotAction::Reset},
               {EKeys::Gamepad_Special_Right, 0, EPilotAction::Settings},
               {EKeys::Escape, 0, EPilotAction::Settings},
               {EKeys::Gamepad_Special_Left, 0, EPilotAction::Telemetry},
               {EKeys::F8, 0, EPilotAction::Telemetry},
               {EKeys::F6, 0, EPilotAction::Profile},
               {EKeys::F7, 0, EPilotAction::NextMode},
               {EKeys::Gamepad_DPad_Right, 1, EPilotAction::NextMode},
               {EKeys::Gamepad_DPad_Left, 1, EPilotAction::PreviousMode},
               {EKeys::Gamepad_DPad_Up, 1, EPilotAction::IncreaseLimit},
               {EKeys::Gamepad_DPad_Down, 1, EPilotAction::DecreaseLimit},
               {EKeys::Gamepad_LeftThumbstick, 0, EPilotAction::Brake},
               {EKeys::Gamepad_RightThumbstick, 0, EPilotAction::Recenter},
               {EKeys::Gamepad_RightThumbstick, 2, EPilotAction::Recenter},
               {EKeys::Gamepad_DPad_Up, 2, EPilotAction::GimbalUp},
               {EKeys::Gamepad_DPad_Down, 2, EPilotAction::GimbalDown},
               {EKeys::Gamepad_DPad_Left, 2, EPilotAction::ZoomOut},
               {EKeys::Gamepad_DPad_Right, 2, EPilotAction::ZoomIn},
               {EKeys::Gamepad_DPad_Up, 0, EPilotAction::ZoomIn},
               {EKeys::Gamepad_DPad_Down, 0, EPilotAction::ZoomOut},
               {EKeys::PageUp, 0, EPilotAction::ZoomIn},
               {EKeys::PageDown, 0, EPilotAction::ZoomOut},
               {EKeys::Gamepad_FaceButton_Bottom, 1, EPilotAction::Takeoff},
               {EKeys::Gamepad_FaceButton_Right, 1, EPilotAction::Land},
               {EKeys::Gamepad_RightThumbstick, 1, EPilotAction::Arm},
               {EKeys::M, 0, EPilotAction::Arm},
               {EKeys::T, 0, EPilotAction::Takeoff},
               {EKeys::L, 0, EPilotAction::Land}};
}

AQuadrotorPawn *UPilotControlsComponent::Drone() const
{
    return Cast<AQuadrotorPawn>(GetOwner());
}

const FPilotModeTuning &UPilotControlsComponent::Tuning(EDroneFlightMode Mode) const
{
    const int Index = static_cast<int>(Mode);
    if (Modes.IsValidIndex(Index))
    {
        return Modes[Index];
    }
    return Modes[0];
}

float UPilotControlsComponent::Shape(int32 Axis, float Value, EDroneFlightMode Mode) const
{
    if (!Axes.IsValidIndex(Axis))
    {
        return 0.0F;
    }
    float Expo = Tuning(Mode).Expo;
    if (Axis == 2)
    {
        Expo = Tuning(Mode).YawExpo;
    }
    return Axes[Axis].Process(Value, Expo);
}

bool UPilotControlsComponent::Validate(FString &Error) const
{
    if (Axes.Num() != Aether::CommandAxis::Count || Modes.Num() != Aether::FlightModeCount ||
        Buttons.Num() < 1 || Buttons.Num() > 64)
    {
        Error = TEXT("Invalid array size");
        return false;
    }
    for (const auto &AxisCalibration : Axes)
    {
        if (!AxisCalibration.IsValid())
        {
            Error = TEXT("Invalid axis calibration");
            return false;
        }
    }
    for (int Index = 0; Index < Aether::CommandAxis::Count; ++Index)
    {
        for (int J = Index + 1; J < 4; ++J)
        {
            if (Axes[Index].Source == Axes[J].Source)
            {
                Error = TEXT("Duplicate custom axis source");
                return false;
            }
        }
    }
    for (const auto &ModeTuning : Modes)
    {
        if (!ModeTuning.IsValid())
        {
            Error = TEXT("Mode tuning out of bounds");
            return false;
        }
    }
    if (!FMath::IsFinite(ThrottleIncrease) || ThrottleIncrease < 0.05F || ThrottleIncrease > 1 ||
        !FMath::IsFinite(ThrottleDecrease) || ThrottleDecrease < 0.05F || ThrottleDecrease > 1 ||
        !FMath::IsFinite(PrecisionScale) || PrecisionScale < 0.1F || PrecisionScale > 1 ||
        !FMath::IsFinite(CameraSensitivity) || CameraSensitivity < 0.1F || CameraSensitivity > 3 ||
        !FMath::IsFinite(HoldSeconds) || HoldSeconds < 0.5F || HoldSeconds > 5 ||
        !FMath::IsFinite(ResetHoldSeconds) || ResetHoldSeconds < 0.5F || ResetHoldSeconds > 5 ||
        static_cast<int>(TriggerRole) > 4 || static_cast<int>(LeftTriggerRole) > 4 ||
        static_cast<int>(CollectiveMode) > 1 || static_cast<int>(SavedProfile) > 4 ||
        static_cast<int>(SavedMode) > 6)
    {
        Error = TEXT("Invalid throttle/profile/safety setting");
        return false;
    }
    for (int Index = 0; Index < Buttons.Num(); ++Index)
    {
        const auto &B = Buttons[Index];
        const bool FlightAxisKey = B.Key == EKeys::W || B.Key == EKeys::A || B.Key == EKeys::S ||
                                   B.Key == EKeys::D || B.Key == EKeys::Q || B.Key == EKeys::E ||
                                   B.Key == EKeys::R || B.Key == EKeys::F ||
                                   B.Key == EKeys::SpaceBar || B.Key == EKeys::LeftControl;
        if (FlightAxisKey || (B.Key == EKeys::Escape && B.Action != EPilotAction::Settings) ||
            !B.Key.IsValid() || B.Key.IsAnalog() || B.Key == EKeys::AnyKey ||
            B.Key == EKeys::Gamepad_LeftShoulder || B.Key == EKeys::Gamepad_RightShoulder ||
            B.Modifiers < 0 || B.Modifiers > 2 || B.Action >= EPilotAction::Count)
        {
            Error = TEXT("Invalid or reserved button");
            return false;
        }
        for (int J = 0; J < Index; ++J)
        {
            if (B.Key == Buttons[J].Key && B.Modifiers == Buttons[J].Modifiers)
            {
                Error = TEXT("Duplicate button chord");
                return false;
            }
        }
    }
    return true;
}

void UPilotControlsComponent::UpdateAuxiliary(bool LeftAvailable, bool RightAvailable)
{
    bPrecision = false;
    const bool Available[] = {LeftAvailable, RightAvailable};
    const ETriggerRole Roles[] = {LeftTriggerRole, TriggerRole};
    for (int Index = 0; Index < 2; ++Index)
    {
        float Value = 0.0F;
        if (Available[Index])
        {
            Value = FMath::Clamp(Raw[4 + Index], 0.0F, 1.0F);
        }
        AuxiliaryChannels[Index] = 0.0F;
        if (Roles[Index] == ETriggerRole::Auxiliary)
        {
            AuxiliaryChannels[Index] = Value;
        }
        bPrecision |= Roles[Index] == ETriggerRole::Precision && Value > 0.5F;
    }
}
