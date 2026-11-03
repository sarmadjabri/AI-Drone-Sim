"""Position PI-D → desired attitude → body rates → rotor allocation."""
import numpy as np
from .config import DroneConfig
from .math3d import rotation


class PositionController:
    def __init__(self, config=None):
        self.config = config or DroneConfig()
        self.integral = np.zeros(3)
        self.inverse_mixer = np.linalg.inv(self.config.mixer)

    def command(self, state, target, dt, velocity=None):
        c = self.config
        error = np.clip(np.asarray(target)-state.position, -3, 3)
        target_velocity = np.zeros(3) if velocity is None else np.asarray(velocity)
        acceleration = 2.5*error + 2.8*(target_velocity-state.velocity) + 0.35*self.integral
        acceleration[:2] = np.clip(acceleration[:2], -3.0, 3.0)
        acceleration[2] = np.clip(acceleration[2], -4, 4)
        force = c.mass*(acceleration + np.array([0, 0, c.gravity]))
        desired_z = force/np.linalg.norm(force)
        desired_y = np.cross(desired_z, [1., 0., 0.])
        desired_y /= np.linalg.norm(desired_y)
        desired_r = np.column_stack([np.cross(desired_y, desired_z), desired_y, desired_z])
        r = rotation(state.quaternion)
        skew = (desired_r.T@r-r.T@desired_r)/2
        attitude_error = np.array([skew[2, 1], skew[0, 2], skew[1, 0]])
        desired_rate = np.clip(-6*attitude_error, -3, 3)
        torque = np.asarray(c.inertia)*12*(desired_rate-state.omega)
        torque += np.cross(state.omega, np.asarray(c.inertia)*state.omega)
        thrust = max(0., float(force @ r[:, 2]))
        allocation = self.inverse_mixer @ np.r_[thrust, torque]
        # Conditional integration: no integrator growth while actuators saturate.
        if np.all((allocation >= 0) & (allocation <= c.max_thrust)):
            self.integral = np.clip(self.integral+error*dt, -2, 2)
        return np.sqrt(np.clip(allocation, 0, c.max_thrust)/c.thrust_coefficient)/c.max_rpm_rad
