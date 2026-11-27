// Conversion factors at the SI/Unreal boundary. Flight calculations remain in SI.
#pragma once

namespace Aether
{
    namespace Units
    {
        inline constexpr float CentimetersPerMeter = 100.0F;
        inline constexpr float ChaosForceUnitsPerNewton = 100.0F;
        inline constexpr float ChaosTorqueUnitsPerNewtonMeter = 10000.0F;
        inline constexpr float ChaosInertiaUnitsPerKilogramMeterSquared = 10000.0F;
    } // namespace Units
} // namespace Aether
