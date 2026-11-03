"""Six-DOF rigid body with individual first-order rotor-speed dynamics."""
from dataclasses import dataclass, field

import numpy as np

from .config import DroneConfig
from .math3d import integrate, rotation


@dataclass
class State:
    position: np.ndarray = field(default_factory=lambda: np.array([0., 0., 1.5]))
    velocity: np.ndarray = field(default_factory=lambda: np.zeros(3))
    quaternion: np.ndarray = field(default_factory=lambda: np.array([1., 0., 0., 0.]))
    omega: np.ndarray = field(default_factory=lambda: np.zeros(3))
    motors: np.ndarray = field(default_factory=lambda: np.zeros(4))
    energy_j: float = 0.0

    def vector(self):
        return np.concatenate([
            self.position, self.velocity, self.quaternion, self.omega, self.motors
        ])

    @classmethod
    def from_vector(cls, x):
        x = np.asarray(x, dtype=float)
        if x.shape != (17,) or not np.all(np.isfinite(x)) or np.linalg.norm(x[6:10]) < 1e-9:
            raise ValueError("state requires 17 finite values and a nonzero quaternion")
        return cls(x[:3].copy(), x[3:6].copy(), x[6:10]/np.linalg.norm(x[6:10]),
                   x[10:13].copy(), x[13:17].copy())


class Quadrotor:
    def __init__(self, config=None, state=None):
        self.config = config or DroneConfig()
        self.state = state or State()
        self.last_acceleration = np.zeros(3)

    def step(self, command, dt, wind=(0., 0., 0.)):
        c, s = self.config, self.state
        command = np.asarray(command, dtype=float)
        if command.shape != (4,) or not np.all(np.isfinite(command)):
            raise ValueError("motor commands must be four finite normalized values")
        if not np.isfinite(dt) or not 0 < dt <= 0.02:
            raise ValueError("physics dt must be in (0, 0.02]")
        wind = np.asarray(wind, dtype=float)
        if wind.shape != (3,) or not np.all(np.isfinite(wind)):
            raise ValueError("wind must be a finite three-vector")
        desired = np.clip(command, 0, 1)*c.max_rpm_rad
        if s.energy_j >= c.battery_wh*3600:
            desired[:] = 0
        s.motors += (desired-s.motors)*(-np.expm1(-dt/c.motor_tau))
        thrust = c.thrust_coefficient*s.motors**2
        wrench = c.mixer @ thrust
        r = rotation(s.quaternion)
        acc = (r[:, 2]*wrench[0] - c.linear_drag*(s.velocity-wind))/c.mass
        acc[2] -= c.gravity
        inertia = np.asarray(c.inertia)
        angular_acc = (wrench[1:] - np.cross(s.omega, inertia*s.omega)
                       - c.angular_drag*s.omega)/inertia
        # Constant-acceleration translation and midpoint angular exponential.
        s.position += s.velocity*dt + 0.5*acc*dt**2
        s.velocity += acc*dt
        s.quaternion = integrate(s.quaternion, s.omega+0.5*angular_acc*dt, dt)
        s.omega += angular_acc*dt
        s.energy_j += float(8 + 5*np.sum(thrust**1.5))*dt
        self.last_acceleration = acc
        return s
