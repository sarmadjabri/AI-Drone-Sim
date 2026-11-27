// Authored model/rotor layout shared by visual setup and physical force application.
#pragma once
#include "CoreMinimal.h"

namespace Aether
{
    namespace QuadrotorLayout
    {

        // Dimensionless plus-layout offsets: front, right, rear, left. Multiply by arm length.
        inline const FVector MotorArmUnitOffsets[4] = {
            FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(-1.0, 0.0, 0.0),
            FVector(0.0, -1.0, 0.0)};

        inline constexpr float RotorYawSigns[4] = {1.0F, -1.0F, 1.0F, -1.0F};
        inline const TCHAR *F450PartNames[] = {
            TEXT("Arm1"),    TEXT("Arm2"),   TEXT("Arm3"),   TEXT("Arm4"),
            TEXT("Battery"), TEXT("Body7"),  TEXT("Bottom"), TEXT("Leg1"),
            TEXT("Leg2"),    TEXT("Leg3"),   TEXT("Leg4"),   TEXT("Motor1"),
            TEXT("Motor2"),  TEXT("Motor3"), TEXT("Motor4"), TEXT("Top")};
        inline constexpr float F450ModelOriginOffsetCentimeters = -11.0F;
        // Indices into F450PartNames, in rotor order: Motor4, Motor1, Motor2, Motor3.
        inline constexpr int32 MotorAssetIndices[4] = {14, 11, 12, 13};
    } // namespace QuadrotorLayout
} // namespace Aether
