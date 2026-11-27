// Portable SI rotor allocation, actuator response and aerodynamic force calculations.
#include "Flight/QuadrotorDynamics.h"

#include <algorithm>
#include <cmath>

namespace
{
    bool IsFinitePositive(float value)
    {
        return std::isfinite(value) && value > 0.0F;
    }
} // namespace

namespace Aether
{
    bool DroneParameters::IsValid() const
    {
        return IsFinitePositive(massKg) && IsFinitePositive(armLengthM) &&
               IsFinitePositive(maxMotorThrustN) && IsFinitePositive(motorTimeConstantS) &&
               IsFinitePositive(yawMomentPerNewtonNm);
    }

    bool QuadrotorDynamics::AllocateMotorThrusts(const DesiredWrench &wrench,
                                                 const DroneParameters &parameters,
                                                 MotorThrusts &output)
    {
        if (!parameters.IsValid() || !std::isfinite(wrench.collectiveThrustN) ||
            !std::isfinite(wrench.rollTorqueNm) || !std::isfinite(wrench.pitchTorqueNm) ||
            !std::isfinite(wrench.yawTorqueNm))
        {
            return false;
        }

        // Opposing rotors share roll/pitch torque; alternating spin supplies yaw.
        const float RollThrustDifferenceNewtons =
            wrench.rollTorqueNm / (2.0F * parameters.armLengthM);
        const float PitchThrustDifferenceNewtons =
            wrench.pitchTorqueNm / (2.0F * parameters.armLengthM);
        const float YawThrustDifferenceNewtons =
            wrench.yawTorqueNm / (4.0F * parameters.yawMomentPerNewtonNm);
        const float CollectiveThrustPerMotorNewtons = wrench.collectiveThrustN * 0.25F;
        const float MaximumMotorThrustNewtons = parameters.maxMotorThrustN;

        const float FrontRequestedNewtons = CollectiveThrustPerMotorNewtons -
                                            PitchThrustDifferenceNewtons +
                                            YawThrustDifferenceNewtons;
        const float RightRequestedNewtons = CollectiveThrustPerMotorNewtons +
                                            RollThrustDifferenceNewtons -
                                            YawThrustDifferenceNewtons;
        const float RearRequestedNewtons = CollectiveThrustPerMotorNewtons +
                                           PitchThrustDifferenceNewtons +
                                           YawThrustDifferenceNewtons;
        const float LeftRequestedNewtons = CollectiveThrustPerMotorNewtons -
                                           RollThrustDifferenceNewtons - YawThrustDifferenceNewtons;
        output[0] = std::clamp(FrontRequestedNewtons, 0.0F, MaximumMotorThrustNewtons);
        output[1] = std::clamp(RightRequestedNewtons, 0.0F, MaximumMotorThrustNewtons);
        output[2] = std::clamp(RearRequestedNewtons, 0.0F, MaximumMotorThrustNewtons);
        output[3] = std::clamp(LeftRequestedNewtons, 0.0F, MaximumMotorThrustNewtons);
        return true;
    }

    bool QuadrotorDynamics::CalculateBodyDrag(const std::array<float, 3> &airVelocityBodyMps,
                                              const std::array<float, 3> &dragAreaSquareMeters,
                                              float airDensityKgPerCubicMeter,
                                              std::array<float, 3> &forceBodyNewtons)
    {
        if (!std::isfinite(airDensityKgPerCubicMeter) || airDensityKgPerCubicMeter < 0.0F)
        {
            return false;
        }
        std::array<float, 3> result{};
        for (std::size_t axis = 0; axis < result.size(); ++axis)
        {
            const float velocity = airVelocityBodyMps[axis];
            const float dragArea = dragAreaSquareMeters[axis];
            if (!std::isfinite(velocity) || !std::isfinite(dragArea) || dragArea < 0.0F)
            {
                return false;
            }
            result[axis] =
                -0.5F * airDensityKgPerCubicMeter * dragArea * velocity * std::abs(velocity);
            if (!std::isfinite(result[axis]))
            {
                return false;
            }
        }
        forceBodyNewtons = result;
        return true;
    }

    bool QuadrotorDynamics::UpdateMotorThrusts(const MotorThrusts &current,
                                               const MotorThrusts &target, float deltaTimeS,
                                               const DroneParameters &parameters,
                                               MotorThrusts &output)
    {
        if (!parameters.IsValid() || !IsFinitePositive(deltaTimeS))
        {
            return false;
        }

        // Exact first-order thrust response for a constant target over this step.
        // Validate the complete input before writing, including when output aliases current.
        for (std::size_t motor = 0; motor < output.size(); ++motor)
        {
            if (!std::isfinite(current[motor]) || !std::isfinite(target[motor]))
            {
                return false;
            }
        }
        const float ResponseFraction = -std::expm1(-deltaTimeS / parameters.motorTimeConstantS);
        for (std::size_t motor = 0; motor < output.size(); ++motor)
        {
            const float boundedTarget = std::clamp(target[motor], 0.0F, parameters.maxMotorThrustN);
            const float boundedCurrent =
                std::clamp(current[motor], 0.0F, parameters.maxMotorThrustN);
            output[motor] = boundedCurrent + (boundedTarget - boundedCurrent) * ResponseFraction;
        }
        return true;
    }
} // namespace Aether
