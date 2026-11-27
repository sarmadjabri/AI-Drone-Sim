"""Six-DOF rigid body with individual first-order rotor-speed dynamics."""

from dataclasses import dataclass, field

import numpy as np

from .config import DroneConfig
from .math3d import integrate, rotation


@dataclass
class State:
    position: np.ndarray = field(default_factory=lambda: np.array([0.0, 0.0, 1.5]))
    velocity: np.ndarray = field(default_factory=lambda: np.zeros(3))
    quaternion: np.ndarray = field(default_factory=lambda: np.array([1.0, 0.0, 0.0, 0.0]))
    omega: np.ndarray = field(default_factory=lambda: np.zeros(3))
    motors: np.ndarray = field(default_factory=lambda: np.zeros(4))
    energy_j: float = 0.0

    def vector(self):
        """Pack position, velocity, scalar-first quaternion, body rate and rotor speeds."""
        return np.concatenate(
            [self.position, self.velocity, self.quaternion, self.omega, self.motors]
        )

    @classmethod
    def from_vector(cls, x):
        """Restore the 17-state vector; energy is not part of this dynamics snapshot."""
        x = np.asarray(x, dtype=float)
        if x.shape != (17,) or not np.all(np.isfinite(x)) or np.linalg.norm(x[6:10]) < 1e-9:
            raise ValueError("state requires 17 finite values and a nonzero quaternion")
        return cls(
            x[:3].copy(),
            x[3:6].copy(),
            x[6:10] / np.linalg.norm(x[6:10]),
            x[10:13].copy(),
            x[13:17].copy(),
        )


class Quadrotor:
    def __init__(self, config=None, state=None):
        """Initialize an SI rigid-body experiment; this legacy backend is not Chaos validation."""
        if config is None:
            self.config = DroneConfig()
        else:
            self.config = config
        if state is None:
            self.state = State()
        else:
            self.state = state
        self.last_acceleration = np.zeros(3)

    def step(self, command, dt, wind=(0.0, 0.0, 0.0)):
        """Advance seconds using normalized motor commands and world-frame wind in m/s."""
        parameters = self.config
        state = self.state
        command = np.asarray(command, dtype=float)
        if command.shape != (4,) or not np.all(np.isfinite(command)):
            raise ValueError("motor commands must be four finite normalized values")
        if not np.isfinite(dt) or not 0 < dt <= 0.02:
            raise ValueError("physics dt must be in (0, 0.02]")
        wind = np.asarray(wind, dtype=float)
        if wind.shape != (3,) or not np.all(np.isfinite(wind)):
            raise ValueError("wind must be a finite three-vector")
        desired = np.clip(command, 0, 1) * parameters.max_rpm_rad
        if state.energy_j >= parameters.battery_wh * 3600:
            desired[:] = 0
        state.motors += (desired - state.motors) * (-np.expm1(-dt / parameters.motor_tau))
        thrust = parameters.thrust_coefficient * state.motors**2
        wrench = parameters.mixer @ thrust
        body_rotation = rotation(state.quaternion)
        acceleration = (
            body_rotation[:, 2] * wrench[0] - parameters.linear_drag * (state.velocity - wind)
        ) / parameters.mass
        acceleration[2] -= parameters.gravity
        inertia = np.asarray(parameters.inertia)
        angular_acceleration = (
            wrench[1:]
            - np.cross(state.omega, inertia * state.omega)
            - parameters.angular_drag * state.omega
        ) / inertia
        # Constant-acceleration translation and midpoint angular exponential.
        state.position += state.velocity * dt + 0.5 * acceleration * dt**2
        state.velocity += acceleration * dt
        state.quaternion = integrate(
            state.quaternion, state.omega + 0.5 * angular_acceleration * dt, dt
        )
        state.omega += angular_acceleration * dt
        state.energy_j += float(8 + 5 * np.sum(thrust**1.5)) * dt
        self.last_acceleration = acceleration
        return state
