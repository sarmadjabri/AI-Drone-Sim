#pragma once

#include <array>

namespace Aether
{
    struct DroneParameters
    {
        float massKg = 1.0F;
        float armLengthM = 0.23F;
        float maxMotorThrustN = 12.0F;
        float motorTimeConstantS = 0.035F;
        float yawMomentPerNewtonNm = 0.018F;

        bool IsValid() const;
    };

    struct DesiredWrench
    {
        float collectiveThrustN = 0.0F;
        float rollTorqueNm = 0.0F;
        float pitchTorqueNm = 0.0F;
        float yawTorqueNm = 0.0F;
    };

    using MotorThrusts = std::array<float, 4>;

    class QuadrotorDynamics final
    {
    public:
        // Motor order: front, right, rear, left (plus configuration).
        static bool AllocateMotorThrusts(
            const DesiredWrench& wrench,
            const DroneParameters& parameters,
            MotorThrusts& output);

        static bool UpdateMotorThrusts(
            const MotorThrusts& current,
            const MotorThrusts& target,
            float deltaTimeS,
            const DroneParameters& parameters,
            MotorThrusts& output);
    };
}
