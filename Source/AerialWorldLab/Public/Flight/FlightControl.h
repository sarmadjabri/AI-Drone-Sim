#pragma once

#include "CoreMinimal.h"
#include "Math/RotationMatrix.h"

namespace Aether
{
    inline float SlewTowards(float Current, float Target, float RatePerSecond, float DeltaSeconds)
    {
        const float MaximumStep = FMath::Max(0.0F, RatePerSecond) *
            FMath::Max(0.0F, DeltaSeconds);
        return Current + FMath::Clamp(Target - Current, -MaximumStep, MaximumStep);
    }

    // Anticipate velocity accumulated while the aircraft levels out. Without this,
    // motor/attitude lag lets a high-gain speed loop keep braking past its target.
    inline FVector VelocityAcceleration(const FVector& TargetVelocity,
        const FVector& Velocity, const FVector& BodyUp, float Gravity,
        float Gain, float MaxAcceleration, float LevelingTime)
    {
        FVector CurrentAcceleration = BodyUp * (Gravity / FMath::Max(BodyUp.Z, 0.7));
        CurrentAcceleration.Z = 0.0;
        const FVector PredictedVelocity = Velocity + CurrentAcceleration * LevelingTime;
        return ((TargetVelocity - PredictedVelocity) * Gain).GetClampedToMaxSize(MaxAcceleration);
    }

    // Shortest body space rotation, in the same axes as Chaos angular velocit
    inline FVector AttitudeError(const FQuat& Current, const FVector& Acceleration,
        float Gravity, float HeadingDegrees)
    {
        const float Heading = FMath::DegreesToRadians(HeadingDegrees);
        const FVector Forward(FMath::Cos(Heading), FMath::Sin(Heading), 0.0);
        const FVector Up = (Acceleration + FVector(0.0, 0.0, Gravity)).GetSafeNormal();
        FQuat Error = Current.Inverse() * FRotationMatrix::MakeFromZX(Up, Forward).ToQuat();
        Error.Normalize();
        if (Error.W < 0.0)
        {
            Error = Error * -1.0;
        }
        FVector Axis;
        double Angle;
        Error.ToAxisAndAngle(Axis, Angle);
        return Axis * Angle;
    }
}
