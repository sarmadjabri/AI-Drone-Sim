#include "Flight/QuadrotorDynamics.h"

#include <algorithm>
#include <cmath>

namespace
{
    bool IsFinitePositive(float value)
    {
        return std::isfinite(value) && value > 0.0F;
    }
}

namespace Aether
{
    bool DroneParameters::IsValid() const
    {
        return IsFinitePositive(massKg) && IsFinitePositive(armLengthM) &&
            IsFinitePositive(maxMotorThrustN) && IsFinitePositive(motorTimeConstantS) &&
            IsFinitePositive(yawMomentPerNewtonNm);
    }

    bool QuadrotorDynamics::AllocateMotorThrusts(
        const DesiredWrench& wrench,
        const DroneParameters& parameters,
        MotorThrusts& output)
    {
        if (!parameters.IsValid() ||
            !std::isfinite(wrench.collectiveThrustN) ||
            !std::isfinite(wrench.rollTorqueNm) ||
            !std::isfinite(wrench.pitchTorqueNm) ||
            !std::isfinite(wrench.yawTorqueNm))
        {
            return false;
        }

        const float roll = wrench.rollTorqueNm / (2.0F * parameters.armLengthM);
        const float pitch = wrench.pitchTorqueNm / (2.0F * parameters.armLengthM);
        const float yaw = wrench.yawTorqueNm / (4.0F * parameters.yawMomentPerNewtonNm);
        const float collective = wrench.collectiveThrustN * 0.25F;
        const float maximum = parameters.maxMotorThrustN;

        output = {
            std::clamp(collective - pitch + yaw, 0.0F, maximum),
            std::clamp(collective + roll - yaw, 0.0F, maximum),
            std::clamp(collective + pitch + yaw, 0.0F, maximum),
            std::clamp(collective - roll - yaw, 0.0F, maximum)
        };
        return true;
    }

    bool QuadrotorDynamics::UpdateMotorThrusts(
        const MotorThrusts& current,
        const MotorThrusts& target,
        float deltaTimeS,
        const DroneParameters& parameters,
        MotorThrusts& output)
    {
        if (!parameters.IsValid() || !IsFinitePositive(deltaTimeS))
        {
            return false;
        }

        const float alpha = -std::expm1(-deltaTimeS / parameters.motorTimeConstantS);
        for (std::size_t motor = 0; motor < output.size(); ++motor)
        {
            if (!std::isfinite(current[motor]) || !std::isfinite(target[motor]))
            {
                return false;
            }
            const float boundedTarget = std::clamp(target[motor], 0.0F, parameters.maxMotorThrustN);
            const float boundedCurrent = std::clamp(current[motor], 0.0F, parameters.maxMotorThrustN);
            output[motor] = boundedCurrent + (boundedTarget - boundedCurrent) * alpha;
        }
        return true;
    }
}
