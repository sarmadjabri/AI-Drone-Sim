// Editable mode limits and shared validation metadata. Serialized field names
// stay stable so existing Saved/Controls profiles remain readable.
#pragma once

#include "CoreMinimal.h"
#include "Flight/PilotInput.h"
#include "FlightModeTuning.generated.h"

USTRUCT()

struct FPilotModeTuning
{
    GENERATED_BODY()
    // Horizontal command limit, meters/second.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Speed = 10.0F;
    // Maximum commanded tilt, degrees.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Tilt = 38.0F;
    // Maximum braking tilt, degrees.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float BrakeTilt = 44.0F;
    // Horizontal acceleration cap, meters/second squared.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Acceleration = 30.0F;
    // Horizontal braking cap, meters/second squared.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Braking = 30.0F;
    // Acceleration slew limit, meters/second cubed.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Jerk = 100.0F;
    // Braking slew limit, meters/second cubed.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float BrakeJerk = 160.0F;
    // Velocity error to acceleration gain, inverse seconds.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float VelocityGain = 5.0F;
    // Velocity gain while braking, inverse seconds.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float BrakeGain = 8.0F;
    // Look-ahead duration used by the velocity controller, seconds.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Prediction = 0.2F;
    // Attitude error to angular-rate gain, inverse seconds.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float AttitudeGain = 9.0F;
    // Roll/pitch angular-rate limit, degrees/second.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float RateDegrees = 172.0F;
    // Yaw angular-rate limit, degrees/second.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float YawDegrees = 86.0F;
    // Assisted climb-speed command limit, meters/second.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Ascent = 1.5F;
    // Assisted descent-speed command limit, meters/second.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Descent = 1.5F;
    // Vertical acceleration cap, meters/second squared.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float VerticalAcceleration = 4.0F;
    // Roll/pitch response curve blend, normalized 0..1.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Expo = 0.35F;
    // Yaw response curve blend, normalized 0..1.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float YawExpo = 0.35F;
    // Normalized stick units per second; zero disables shaping slew.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float Slew = 24.0F;
    // Position error to target velocity gain, inverse seconds.
    UPROPERTY(EditAnywhere, Category = "Flight Tuning")
    float HoldGain = 0.8F;
    bool IsValid() const;
};

namespace Aether
{
    // One descriptor supplies both runtime validation and controller-menu limits.
    struct FlightTuningField
    {
        const TCHAR *Label;
        float FPilotModeTuning::*Value;
        float Step;
        float Minimum;
        float Maximum;
    };

    TConstArrayView<FlightTuningField> GetFlightTuningFields();
    float ClampFlightTuningValue(float FPilotModeTuning::*Member, float Value);
    FPilotModeTuning MakeDefaultFlightModeTuning(EDroneFlightMode Mode);
} // namespace Aether
