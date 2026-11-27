// Stateless control math. Inputs use SI units; quaternion/body-rate axes follow Chaos.
#pragma once

#include "CoreMinimal.h"
#include "Math/RotationMatrix.h"

namespace Aether
{
    inline float SlewTowards(float Current, float Target, float RatePerSecond, float DeltaSeconds)
    {
        const float NonnegativeRate = FMath::Max(0.0F, RatePerSecond);
        const float NonnegativeDeltaSeconds = FMath::Max(0.0F, DeltaSeconds);
        const float MaximumStep = NonnegativeRate * NonnegativeDeltaSeconds;
        const float RequestedStep = Target - Current;
        const float BoundedStep = FMath::Clamp(RequestedStep, -MaximumStep, MaximumStep);
        return Current + BoundedStep;
    }

    // Anticipate velocity accumulated while leveling. Include modeled drag so a
    // steady tilt balancing air load is not mistaken for continuing acceleration.
    inline FVector VelocityAcceleration(const FVector &TargetVelocity, const FVector &Velocity,
                                        const FVector &BodyUp, float Gravity, float Gain,
                                        float MaxAcceleration, float LevelingTime,
                                        const FVector &ExternalAcceleration = FVector::ZeroVector)
    {
        constexpr double MinimumPredictionUprightProjection = 0.7;
        const double UprightProjection = FMath::Max(BodyUp.Z, MinimumPredictionUprightProjection);
        const double TiltCompensatedGravity = Gravity / UprightProjection;
        FVector CurrentAcceleration = BodyUp * TiltCompensatedGravity;
        CurrentAcceleration += ExternalAcceleration;
        CurrentAcceleration.Z = 0.0;

        const FVector PredictedVelocityChange = CurrentAcceleration * LevelingTime;
        const FVector PredictedVelocity = Velocity + PredictedVelocityChange;
        const FVector VelocityError = TargetVelocity - PredictedVelocity;
        const FVector RequestedAcceleration = VelocityError * Gain;
        return RequestedAcceleration.GetClampedToMaxSize(MaxAcceleration);
    }

    // Shortest body-space rotation, in the same axes as Chaos angular velocity.
    // Return an axis-angle rotation vector in radians, not an Euler-angle error.
    inline FVector AttitudeError(const FQuat &Current, const FVector &Acceleration, float Gravity,
                                 float HeadingDegrees)
    {
        const float HeadingRadians = FMath::DegreesToRadians(HeadingDegrees);
        const FVector HeadingForward(FMath::Cos(HeadingRadians), FMath::Sin(HeadingRadians), 0.0);
        const FVector DesiredThrustDirection = Acceleration + FVector(0.0, 0.0, Gravity);
        const FVector DesiredUp = DesiredThrustDirection.GetSafeNormal();
        const FQuat DesiredOrientation =
            FRotationMatrix::MakeFromZX(DesiredUp, HeadingForward).ToQuat();
        const FQuat WorldToBody = Current.Inverse();
        FQuat OrientationError = WorldToBody * DesiredOrientation;
        OrientationError.Normalize();
        if (OrientationError.W < 0.0)
        {
            // q and -q represent the same rotation. Choose the shortest arc.
            OrientationError = OrientationError * -1.0;
        }
        FVector RotationAxis;
        double RotationAngleRadians;
        OrientationError.ToAxisAndAngle(RotationAxis, RotationAngleRadians);
        return RotationAxis * RotationAngleRadians;
    }
} // namespace Aether
