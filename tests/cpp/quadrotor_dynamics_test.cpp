#include "Flight/QuadrotorDynamics.h"

#include <cassert>
#include <cmath>

namespace
{
    bool NearlyEqual(float left, float right, float tolerance = 1.0e-4F)
    {
        return std::abs(left - right) <= tolerance;
    }

    void TestHoverAllocation()
    {
        const Aether::DroneParameters parameters;
        const Aether::DesiredWrench hover{ parameters.massKg * 9.81F, 0.0F, 0.0F, 0.0F };
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
        const Aether::DesiredWrench wrench{ 20.0F, 0.23F, 0.46F, 0.018F };
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
            roll += (motor == 1 ? arm : motor == 3 ? -arm : 0.0F) * motors[motor];
            pitch += (motor == 0 ? -arm : motor == 2 ? arm : 0.0F) * motors[motor];
            yaw += (motor == 0 || motor == 2 ? 1.0F : -1.0F) *
                parameters.yawMomentPerNewtonNm * motors[motor];
        }

        assert(NearlyEqual(total, wrench.collectiveThrustN));
        assert(NearlyEqual(roll, wrench.rollTorqueNm));
        assert(NearlyEqual(pitch, wrench.pitchTorqueNm));
        assert(NearlyEqual(yaw, wrench.yawTorqueNm));
    }

    void TestMotorLag()
    {
        const Aether::DroneParameters parameters;
        const Aether::MotorThrusts stopped{ 0.0F, 0.0F, 0.0F, 0.0F };
        const Aether::MotorThrusts target{ 6.0F, 3.0F, 0.0F, 12.0F };
        Aether::MotorThrusts updated{};
        assert(Aether::QuadrotorDynamics::UpdateMotorThrusts(
            stopped, target, parameters.motorTimeConstantS, parameters, updated));
        const float expectedFraction = 1.0F - std::exp(-1.0F);
        for (std::size_t motor = 0; motor < updated.size(); ++motor)
        {
            assert(NearlyEqual(updated[motor], target[motor] * expectedFraction));
        }
    }

    void TestInvalidInputsAreRejected()
    {
        Aether::DroneParameters invalid;
        invalid.massKg = 0.0F;
        Aether::MotorThrusts motors{};
        assert(!Aether::QuadrotorDynamics::AllocateMotorThrusts({}, invalid, motors));
        assert(!Aether::QuadrotorDynamics::UpdateMotorThrusts({}, {}, 0.0F, {}, motors));
    }
}

int main()
{
    TestHoverAllocation();
    TestIndependentTorqueAllocation();
    TestMotorLag();
    TestInvalidInputsAreRejected();
}
