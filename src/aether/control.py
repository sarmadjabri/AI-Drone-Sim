"""Position PI-D → desired attitude → body rates → rotor allocation."""

import numpy as np

from .config import DroneConfig
from .math3d import rotation


class PositionController:
    def __init__(self, config=None):
        """Cache the inverse plus-layout mixer and start with zero accumulated position error."""
        if config is None:
            self.config = DroneConfig()
        else:
            self.config = config
        self.integral = np.zeros(3)
        self.inverse_mixer = np.linalg.inv(self.config.mixer)

    def command(self, state, target, dt, velocity=None):
        """Convert world position/velocity targets into four bounded rotor-speed commands."""
        parameters = self.config

        error = np.clip(np.asarray(target) - state.position, -3, 3)

        if velocity is None:
            target_velocity = np.zeros(3)
        else:
            target_velocity = np.asarray(velocity)

        # Position PI-D supplies a bounded world-frame acceleration request.
        acceleration = 2.5 * error + 2.8 * (target_velocity - state.velocity) + 0.35 * self.integral

        acceleration[:2] = np.clip(acceleration[:2], -3.0, 3.0)

        acceleration[2] = np.clip(acceleration[2], -4, 4)

        force = parameters.mass * (acceleration + np.array([0, 0, parameters.gravity]))

        # Align body up with the desired force while keeping a world-X heading.
        desired_z = force / np.linalg.norm(force)

        desired_y = np.cross(desired_z, [1.0, 0.0, 0.0])

        desired_y /= np.linalg.norm(desired_y)

        desired_rotation = np.column_stack([np.cross(desired_y, desired_z), desired_y, desired_z])

        body_rotation = rotation(state.quaternion)

        # The skew part of the relative rotation gives the small body attitude error.
        skew = (desired_rotation.T @ body_rotation - body_rotation.T @ desired_rotation) / 2

        attitude_error = np.array([skew[2, 1], skew[0, 2], skew[1, 0]])

        desired_rate = np.clip(-6 * attitude_error, -3, 3)

        torque = np.asarray(parameters.inertia) * 12 * (desired_rate - state.omega)

        # Cancel the gyroscopic cross product used by the rigid-body dynamics.
        torque += np.cross(state.omega, np.asarray(parameters.inertia) * state.omega)

        thrust = max(0.0, float(force @ body_rotation[:, 2]))

        # Allocate body torques to individual rotor thrusts, then invert the squared-speed law.
        allocation = self.inverse_mixer @ np.r_[thrust, torque]

        # Conditional integration: no integrator growth while actuators saturate.

        if np.all((allocation >= 0) & (allocation <= parameters.max_thrust)):
            self.integral = np.clip(self.integral + error * dt, -2, 2)
        return (
            np.sqrt(np.clip(allocation, 0, parameters.max_thrust) / parameters.thrust_coefficient)
            / parameters.max_rpm_rad
        )
