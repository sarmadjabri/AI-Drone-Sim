#include "Flight/QuadrotorDynamics.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace
{
    bool NearlyEqual(float left, float right, float tolerance = 1.0e-4F)
    {
        return std::abs(left - right) <= tolerance;
    }

    void TestHoverAllocation()
    {
        const Aether::DroneParameters parameters;
        const Aether::DesiredWrench hover{parameters.massKg * 9.81F, 0.0F, 0.0F, 0.0F};
        Aether::MotorThrusts motors{};
        assert(Aether::QuadrotorDynamics::AllocateMotorThrusts(hover, parameters, motors));
        const float perMotor = hover.collectiveThrustN / 4.0F;
        for (const float thrust : motors)
        {
            assert(NearlyEqual(thrust, perMotor));
        }
    }

    void TestIndependentTorqueAllocation()
    {
        const Aether::DroneParameters parameters;
        const Aether::DesiredWrench wrench{20.0F, 0.23F, 0.46F, 0.018F};
        Aether::MotorThrusts motors{};
        assert(Aether::QuadrotorDynamics::AllocateMotorThrusts(wrench, parameters, motors));

        float total = 0.0F;
        float roll = 0.0F;
        float pitch = 0.0F;
        float yaw = 0.0F;
        const float arm = parameters.armLengthM;
        for (std::size_t motor = 0; motor < motors.size(); ++motor)
        {
            total += motors[motor];
            // Reconstruct physical torques from rotor positions, independent of the allocator.
            const float rollArms[] = {0.0F, arm, 0.0F, -arm};
            const float pitchArms[] = {-arm, 0.0F, arm, 0.0F};
            const float spinDirections[] = {1.0F, -1.0F, 1.0F, -1.0F};
            roll += rollArms[motor] * motors[motor];
            pitch += pitchArms[motor] * motors[motor];
            yaw += spinDirections[motor] * parameters.yawMomentPerNewtonNm * motors[motor];
        }

        assert(NearlyEqual(total, wrench.collectiveThrustN));
        assert(NearlyEqual(roll, wrench.rollTorqueNm));
        assert(NearlyEqual(pitch, wrench.pitchTorqueNm));
        assert(NearlyEqual(yaw, wrench.yawTorqueNm));
    }

    void TestMotorLag()
    {
        const Aether::DroneParameters parameters;
        const Aether::MotorThrusts stopped{0.0F, 0.0F, 0.0F, 0.0F};
        const Aether::MotorThrusts target{6.0F, 3.0F, 0.0F, 12.0F};
        Aether::MotorThrusts updated{};
        assert(Aether::QuadrotorDynamics::UpdateMotorThrusts(
            stopped, target, parameters.motorTimeConstantS, parameters, updated));
        const float expectedFraction = 1.0F - std::exp(-1.0F);
        for (std::size_t motor = 0; motor < updated.size(); ++motor)
        {
            assert(NearlyEqual(updated[motor], target[motor] * expectedFraction));
        }
    }

    void TestAerodynamicDrag()
    {
        std::array<float, 3> force{};
        assert(Aether::QuadrotorDynamics::CalculateBodyDrag({10.0F, -10.0F, 0.0F},
                                                            {0.04F, 0.08F, 0.04F}, 1.225F, force));
        assert(NearlyEqual(force[0], -2.45F));
        assert(NearlyEqual(force[1], 4.9F));
        assert(NearlyEqual(force[2], 0.0F));
        // Drag must dissipate energy relative to the surrounding air.
        assert(force[0] * 10.0F + force[1] * -10.0F < 0.0F);
        const auto previous = force;
        assert(!Aether::QuadrotorDynamics::CalculateBodyDrag({0.0F, 0.0F, 0.0F},
                                                             {-1.0F, 0.0F, 0.0F}, 1.225F, force));
        assert(force == previous);
        assert(!Aether::QuadrotorDynamics::CalculateBodyDrag(
            {std::numeric_limits<float>::infinity(), 0.0F, 0.0F}, {}, 1.225F, force));
        assert(Aether::QuadrotorDynamics::CalculateBodyDrag({}, {}, 0.0F, force));
        assert((force == std::array<float, 3>{}));
    }

    void TestInvalidMotorUpdateDoesNotPartiallyWrite()
    {
        Aether::MotorThrusts output{1.0F, 2.0F, 3.0F, 4.0F};
        const auto previous = output;
        Aether::MotorThrusts target{5.0F, 5.0F, 5.0F, std::numeric_limits<float>::quiet_NaN()};
        assert(!Aether::QuadrotorDynamics::UpdateMotorThrusts(output, target, 0.01F, {}, output));
        assert(output == previous);
    }

    void TestInvalidInputsAreRejected()
    {
        Aether::DroneParameters invalid;
        invalid.massKg = 0.0F;
        Aether::MotorThrusts motors{};
        assert(!Aether::QuadrotorDynamics::AllocateMotorThrusts({}, invalid, motors));
        assert(!Aether::QuadrotorDynamics::UpdateMotorThrusts({}, {}, 0.0F, {}, motors));
    }
} // namespace

int main()
{
    TestAerodynamicDrag();
    TestInvalidMotorUpdateDoesNotPartiallyWrite();
    TestHoverAllocation();
    TestIndependentTorqueAllocation();
    TestMotorLag();
    TestInvalidInputsAreRejected();
}
