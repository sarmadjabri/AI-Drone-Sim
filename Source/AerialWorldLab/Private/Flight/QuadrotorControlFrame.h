// Per-tick inputs and resolved limits. This is transient control data, not saved configuration.
#pragma once
#include "CoreMinimal.h"
#include "Flight/QuadrotorDynamics.h"

struct FQuadrotorControlFrame
{
    bool bUsesLegacyTuning = false;
    float PrecisionScale = 1.0F;
    float HorizontalSpeedMetersPerSecond = 0.0F;
    float PositionHoldGainPerSecond = 0.0F;
    float VelocityPredictionSeconds = 0.0F;
    float MaximumYawRateRadiansPerSecond = 0.0F;
    float RollAttitudeGainPerSecond = 0.0F;
    float PitchAttitudeGainPerSecond = 0.0F;
    float MaximumRollRateRadiansPerSecond = 0.0F;
    float MaximumPitchRateRadiansPerSecond = 0.0F;
    float MaximumVerticalAccelerationMetersPerSecondSquared = 0.0F;
    float MinimumVerticalThrustFraction = 0.0F;
    FRotator CurrentRotation = FRotator::ZeroRotator;
    FQuat BodyRotation = FQuat::Identity;
    FVector AngularVelocityBodyRadiansPerSecond = FVector::ZeroVector;
    FVector DragForceWorldNewtons = FVector::ZeroVector;
    Aether::DroneParameters DynamicsParameters{};
};
