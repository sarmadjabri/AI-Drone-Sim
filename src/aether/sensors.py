"""Explicit noisy navigation instrument; never an implicit truth fallback."""
from dataclasses import dataclass
import numpy as np
from .physics import State
from .math3d import integrate


@dataclass
class NavigationObservation:
    timestamp: float
    state: State


class NavigationSensor:
    """50 Hz fused-navigation surrogate; not a claimed IMU/GPS fusion implementation."""
    def __init__(self, seed=7, noise=1.0):
        self.rng = np.random.default_rng(seed)
        self.noise = noise

    def observe(self, state, timestamp):
        x = state.vector()
        x[:3] += self.rng.normal(0, 0.01*self.noise, 3)
        x[3:6] += self.rng.normal(0, 0.02*self.noise, 3)
        x[6:10] = integrate(x[6:10], self.rng.normal(0, 0.003*self.noise, 3), 1.)
        x[10:13] += self.rng.normal(0, 0.01*self.noise, 3)
        # Rotor tachometers are observed; motor states are not inferred from commands.
        x[13:17] += self.rng.normal(0, self.noise, 4)
        return NavigationObservation(timestamp, State.from_vector(x))
