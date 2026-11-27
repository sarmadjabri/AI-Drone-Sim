// Portable SI rotor allocation, actuator response and aerodynamic force calculations.
#pragma once

#include <array>

namespace Aether
{
    // SI parameters for a plus-layout quadrotor; no Unreal types are required.
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
        static bool AllocateMotorThrusts(const DesiredWrench &wrench,
                                         const DroneParameters &parameters, MotorThrusts &output);

        // Per-axis body drag: F = -0.5 * density * CdA * v * abs(v).
        // Velocity is relative to the air, not relative to the ground.
        static bool CalculateBodyDrag(const std::array<float, 3> &airVelocityBodyMps,
                                      const std::array<float, 3> &dragAreaSquareMeters,
                                      float airDensityKgPerCubicMeter,
                                      std::array<float, 3> &forceBodyNewtons);

        static bool UpdateMotorThrusts(const MotorThrusts &current, const MotorThrusts &target,
                                       float deltaTimeS, const DroneParameters &parameters,
                                       MotorThrusts &output);
    };
} // namespace Aether
