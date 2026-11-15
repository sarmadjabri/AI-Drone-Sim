#pragma once

#include "CoreMinimal.h"
#include "PilotInput.generated.h"

UENUM()
enum class EDroneInputProfile : uint8
{
    ProfessionalMode2, Legacy3D, LegacyFPV, LegacyViewDependent, Custom
};

UENUM()
enum class EDroneFlightMode : uint8
{
    LegacyAssisted, Cinematic, Sport, Training, HighPerformance, Angle, Acro
};

namespace Aether
{
    struct PilotAxes
    {
        float Forward, Right, Yaw, Vertical;
    };

    // Input values use the project's normalized axes: positive Y means stick forward.
    inline PilotAxes RoutePilotAxes(EDroneInputProfile Profile, bool FirstPerson,
        float LeftX, float LeftY, float RightX, float RightY, float Triggers)
    {
        const bool Mode2 = Profile == EDroneInputProfile::ProfessionalMode2 ||
            Profile == EDroneInputProfile::LegacyFPV ||
            (Profile == EDroneInputProfile::LegacyViewDependent && FirstPerson);
        return Mode2 ? PilotAxes{RightY, RightX, LeftX, LeftY}
                     : PilotAxes{LeftY, LeftX, RightX, Triggers};
    }

    struct AssistedTuning
    {
        float SpeedScale, TiltLimit, BrakingTiltLimit, AccelerationJerk, BrakingJerk;
    };

    inline AssistedTuning GetAssistedTuning(EDroneFlightMode Mode)
    {
        switch (Mode)
        {
        case EDroneFlightMode::Cinematic: return {0.45F, 20.0F, 25.0F, 12.0F, 20.0F};
        case EDroneFlightMode::Sport: return {1.0F, 38.0F, 44.0F, 100.0F, 160.0F};
        case EDroneFlightMode::Training: return {0.25F, 12.0F, 18.0F, 8.0F, 16.0F};
        default: return {1.0F, 90.0F, 90.0F, MAX_flt, MAX_flt};
        }
    }
}
