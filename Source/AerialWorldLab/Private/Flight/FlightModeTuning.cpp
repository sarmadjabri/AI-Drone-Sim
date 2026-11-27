#include "Flight/FlightModeTuning.h"

namespace
{
    const Aether::FlightTuningField ModeFields[] = {
        {TEXT("Speed m/s"), &FPilotModeTuning::Speed, 0.5F, 0.5F, 40},
        {TEXT("Tilt degrees"), &FPilotModeTuning::Tilt, 1, 5, 60},
        {TEXT("Brake tilt degrees"), &FPilotModeTuning::BrakeTilt, 1, 5, 60},
        {TEXT("Acceleration jerk m/s^3"), &FPilotModeTuning::Jerk, 2, 1, 300},
        {TEXT("Braking jerk m/s^3"), &FPilotModeTuning::BrakeJerk, 2, 1, 400},
        {TEXT("Velocity gain /s"), &FPilotModeTuning::VelocityGain, 0.1F, 0, 10},
        {TEXT("Braking gain /s"), &FPilotModeTuning::BrakeGain, 0.1F, 0, 12},
        {TEXT("Prediction seconds"), &FPilotModeTuning::Prediction, 0.01F, 0, 0.5F},
        {TEXT("Attitude gain /s"), &FPilotModeTuning::AttitudeGain, 0.5F, 1, 12},
        {TEXT("Roll/pitch rate deg/s"), &FPilotModeTuning::RateDegrees, 5, 30, 360},
        {TEXT("Yaw rate deg/s"), &FPilotModeTuning::YawDegrees, 5, 15, 180},
        {TEXT("Ascent m/s"), &FPilotModeTuning::Ascent, 0.1F, 0.1F, 10},
        {TEXT("Descent m/s"), &FPilotModeTuning::Descent, 0.1F, 0.1F, 6},
        {TEXT("Vertical acceleration m/s^2"), &FPilotModeTuning::VerticalAcceleration, 0.2F, 1, 12},
        {TEXT("Pitch/roll expo"), &FPilotModeTuning::Expo, 0.05F, 0, 1},
        {TEXT("Yaw expo"), &FPilotModeTuning::YawExpo, 0.05F, 0, 1},
        {TEXT("Input slew normalized/s (0=off)"), &FPilotModeTuning::Slew, 1, 0, 100},
        {TEXT("Acceleration cap m/s^2"), &FPilotModeTuning::Acceleration, 0.5F, 0.5F, 30},
        {TEXT("Braking cap m/s^2"), &FPilotModeTuning::Braking, 0.5F, 0.5F, 30},
        {TEXT("Position hold gain /s"), &FPilotModeTuning::HoldGain, 0.1F, 0, 2}};

}

TConstArrayView<Aether::FlightTuningField> Aether::GetFlightTuningFields()
{
    return MakeArrayView(ModeFields);
}

bool FPilotModeTuning::IsValid() const
{
    for (const Aether::FlightTuningField &Field : Aether::GetFlightTuningFields())
    {
        const float Value = this->*(Field.Value);
        if (!FMath::IsFinite(Value))
        {
            return false;
        }
        if (Value < Field.Minimum || Value > Field.Maximum)
        {
            return false;
        }
    }
    return true;
}

FPilotModeTuning Aether::MakeDefaultFlightModeTuning(EDroneFlightMode Mode)
{
    FPilotModeTuning Tuning;
    // These are simulated flight presets, not calibration of a physical F450.
    // Force allocation, drag, motor lag and collision remain active at all speeds.
    Tuning.Speed = 24.0F;
    Tuning.Ascent = 6.0F;
    Tuning.Descent = 4.0F;
    Tuning.VerticalAcceleration = 7.0F;
    switch (Mode)
    {
    case EDroneFlightMode::Cinematic:
        Tuning.Speed = 12.0F;
        Tuning.Tilt = 30.0F;
        Tuning.BrakeTilt = 40.0F;
        Tuning.Acceleration = 7.0F;
        Tuning.Braking = 9.0F;
        Tuning.Jerk = 40.0F;
        Tuning.BrakeJerk = 60.0F;
        Tuning.Ascent = 4.0F;
        Tuning.Descent = 3.0F;
        Tuning.Expo = 0.45F;
        Tuning.YawExpo = 0.4F;
        Tuning.YawDegrees = 90.0F;
        Tuning.Slew = 12.0F;
        break;
    case EDroneFlightMode::Sport:
        Tuning.Tilt = 48.0F;
        Tuning.BrakeTilt = 55.0F;
        Tuning.Expo = 0.25F;
        Tuning.YawExpo = 0.2F;
        Tuning.YawDegrees = 130.0F;
        break;
    case EDroneFlightMode::Training:
        Tuning.Speed = 6.0F;
        Tuning.Tilt = 20.0F;
        Tuning.BrakeTilt = 25.0F;
        Tuning.Acceleration = 3.0F;
        Tuning.Braking = 4.0F;
        Tuning.Jerk = 16.0F;
        Tuning.BrakeJerk = 24.0F;
        Tuning.Ascent = 2.5F;
        Tuning.Descent = 2.0F;
        Tuning.Expo = 0.4F;
        Tuning.YawExpo = 0.35F;
        Tuning.YawDegrees = 60.0F;
        Tuning.HoldGain = 1.0F;
        break;
    case EDroneFlightMode::HighPerformance:
        Tuning.Speed = 35.0F;
        Tuning.Tilt = 55.0F;
        Tuning.BrakeTilt = 60.0F;
        Tuning.Jerk = 180.0F;
        Tuning.BrakeJerk = 260.0F;
        Tuning.RateDegrees = 220.0F;
        Tuning.YawDegrees = 150.0F;
        Tuning.Ascent = 8.0F;
        Tuning.Descent = 5.0F;
        Tuning.VerticalAcceleration = 10.0F;
        Tuning.Expo = 0.15F;
        Tuning.YawExpo = 0.15F;
        Tuning.Slew = 40.0F;
        break;
    case EDroneFlightMode::Angle:
        Tuning.Tilt = 40.0F;
        Tuning.Expo = 0.25F;
        Tuning.YawExpo = 0.2F;
        Tuning.YawDegrees = 120.0F;
        break;
    case EDroneFlightMode::Acro:
        Tuning.RateDegrees = 240.0F;
        Tuning.YawDegrees = 150.0F;
        Tuning.Expo = 0.2F;
        Tuning.YawExpo = 0.15F;
        Tuning.Slew = 0.0F;
        break;
    case EDroneFlightMode::LegacyAssisted:
        Tuning.Speed = 20.0F;
        break;
    }
    return Tuning;
}

float Aether::ClampFlightTuningValue(float FPilotModeTuning::*Member, float Value)
{
    for (const FlightTuningField &Field : GetFlightTuningFields())
    {
        if (Field.Value == Member)
        {
            return FMath::Clamp(Value, Field.Minimum, Field.Maximum);
        }
    }
    return Value;
}
