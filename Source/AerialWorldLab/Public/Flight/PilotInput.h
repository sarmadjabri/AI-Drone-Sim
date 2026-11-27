// Defines controller profiles and axis routing independently of the chosen camera.
#pragma once

#include "CoreMinimal.h"
#include "PilotInput.generated.h"

UENUM()
enum class EDroneInputProfile : uint8
{
    ProfessionalMode2,
    Legacy3D,
    LegacyFPV,
    LegacyViewDependent,
    Custom
};

UENUM()
enum class EDroneFlightMode : uint8
{
    LegacyAssisted,
    Cinematic,
    Sport,
    Training,
    HighPerformance,
    Angle,
    Acro
};

namespace Aether
{
    constexpr int32 FlightModeCount = static_cast<int32>(EDroneFlightMode::Acro) + 1;

    // Raw physical-axis indices are saved in calibration profiles. Keep this
    // order stable even if labels, menu presentation or flight mappings change.
    namespace PhysicalAxis
    {
        inline constexpr int32 LeftX = 0;
        inline constexpr int32 LeftY = 1;
        inline constexpr int32 RightX = 2;
        inline constexpr int32 RightY = 3;
        inline constexpr int32 LeftTrigger = 4;
        inline constexpr int32 RightTrigger = 5;
        inline constexpr int32 Count = 6;
    } // namespace PhysicalAxis

    namespace CommandAxis
    {
        inline constexpr int32 Forward = 0;
        inline constexpr int32 Lateral = 1;
        inline constexpr int32 Yaw = 2;
        inline constexpr int32 Vertical = 3;
        inline constexpr int32 Count = 4;
    } // namespace CommandAxis

    struct PilotAxes
    {
        float Forward = 0.0F;
        float Right = 0.0F;
        float Yaw = 0.0F;
        float Vertical = 0.0F;
    };

    // Input values use the project's normalized axes: positive Y means stick forward.
    inline PilotAxes RoutePilotAxes(EDroneInputProfile Profile, bool FirstPerson, float LeftX,
                                    float LeftY, float RightX, float RightY, float Triggers)
    {
        const bool Mode2 = Profile == EDroneInputProfile::ProfessionalMode2 ||
                           Profile == EDroneInputProfile::LegacyFPV ||
                           (Profile == EDroneInputProfile::LegacyViewDependent && FirstPerson);
        if (Mode2)
        {
            PilotAxes Commands;
            Commands.Forward = RightY;
            Commands.Right = RightX;
            Commands.Yaw = LeftX;
            Commands.Vertical = LeftY;
            return Commands;
        }
        else
        {
            PilotAxes Commands;
            Commands.Forward = LeftY;
            Commands.Right = LeftX;
            Commands.Yaw = RightX;
            Commands.Vertical = Triggers;
            return Commands;
        }
    }

} // namespace Aether
