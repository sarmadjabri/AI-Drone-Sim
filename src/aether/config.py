"""SI-unit configuration; world and body frames are right-handed, Z up."""
import json
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np


@dataclass(frozen=True)
class DroneConfig:
    mass: float = 1.0
    inertia: tuple[float, float, float] = (0.02, 0.02, 0.04)
    arm: float = 0.23
    gravity: float = 9.81
    thrust_coefficient: float = 1.8e-5
    torque_ratio: float = 0.018
    max_rpm_rad: float = 900.0
    motor_tau: float = 0.035
    linear_drag: float = 0.18
    angular_drag: float = 0.015
    radius: float = 0.3
    battery_wh: float = 30.0

    def __post_init__(self):
        for name in ("mass", "arm", "gravity", "thrust_coefficient", "torque_ratio",
                     "max_rpm_rad", "motor_tau", "radius", "battery_wh"):
            if not np.isfinite(getattr(self, name)) or getattr(self, name) <= 0:
                raise ValueError(f"{name} must be finite and positive")
        inertia = np.asarray(self.inertia, dtype=float)
        if inertia.shape != (3,) or not np.all(np.isfinite(inertia)) or np.any(inertia <= 0):
            raise ValueError("inertia requires three positive principal moments")
        for name in ("linear_drag", "angular_drag"):
            value = getattr(self, name)
            if not np.isfinite(value) or value < 0:
                raise ValueError(f"{name} must be finite and nonnegative")

    @property
    def max_thrust(self):
        return self.thrust_coefficient * self.max_rpm_rad**2

    @property
    def mixer(self):
        # Plus layout: +X, +Y, -X, -Y. Alternating reaction torques.
        a, k = self.arm, self.torque_ratio
        return np.array([[1, 1, 1, 1], [0, a, 0, -a], [-a, 0, a, 0], [k, -k, k, -k]])


@dataclass
class Scenario:
    start: list[float] = field(default_factory=lambda: [0., 0., 1.5])
    waypoints: list[list[float]] = field(default_factory=lambda: [[3., 0., 2.], [3., 3., 2.]])
    wind: list[float] = field(default_factory=lambda: [0.3, 0., 0.])
    obstacles: list[dict] = field(default_factory=lambda: [
        {"center": [1.5, 2., 1.], "half_size": [0.45, 0.45, 1.]},
        {"center": [-2., -2., 1.5], "half_size": [0.6, 0.6, 1.5]}])
    dt: float = 0.005
    control_dt: float = 0.02
    seed: int = 7

    def __post_init__(self):
        if (not np.isfinite(self.dt) or not np.isfinite(self.control_dt)
                or not (0 < self.dt <= 0.02) or self.control_dt < self.dt):
            raise ValueError("require 0 < dt <= 0.02 and control_dt >= dt")
        if not np.isclose(self.control_dt / self.dt, round(self.control_dt / self.dt)):
            raise ValueError("control_dt must be a multiple of dt")
        for value in [self.start, self.wind, *self.waypoints]:
            if np.shape(value) != (3,) or not np.all(np.isfinite(value)):
                raise ValueError("positions and wind must be finite three-vectors")
        for obstacle in self.obstacles:
            if "center" not in obstacle or "half_size" not in obstacle:
                raise ValueError("obstacles require center and half_size")
            if np.shape(obstacle["center"]) != (3,) or np.shape(obstacle["half_size"]) != (3,):
                raise ValueError("obstacles require center and half_size three-vectors")
            if (not np.all(np.isfinite(obstacle["center"]))
                    or not np.all(np.isfinite(obstacle["half_size"]))
                    or np.any(np.asarray(obstacle["half_size"]) <= 0)):
                raise ValueError("obstacle half sizes must be positive")

    @classmethod
    def load(cls, path: str | Path):
        return cls(**json.loads(Path(path).read_text()))

    def save(self, path: str | Path):
        from dataclasses import asdict
        Path(path).write_text(json.dumps(asdict(self), indent=2) + "\n")
