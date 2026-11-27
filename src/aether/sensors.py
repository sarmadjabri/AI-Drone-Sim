"""Explicit noisy navigation instrument; never an implicit truth fallback."""

from dataclasses import dataclass

import numpy as np

from .math3d import integrate
from .physics import State


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
        """Perturb a copied state vector; the true physical state must remain unchanged."""
        # Standard deviations below are m, m/s, radians, rad/s and rotor rad/s.
        measured_state_vector = state.vector()
        measured_state_vector[:3] += self.rng.normal(0, 0.01 * self.noise, 3)
        measured_state_vector[3:6] += self.rng.normal(0, 0.02 * self.noise, 3)
        measured_state_vector[6:10] = integrate(
            measured_state_vector[6:10], self.rng.normal(0, 0.003 * self.noise, 3), 1.0
        )
        measured_state_vector[10:13] += self.rng.normal(0, 0.01 * self.noise, 3)
        # Rotor tachometers are observed; motor states are not inferred from commands.
        measured_state_vector[13:17] += self.rng.normal(0, self.noise, 4)
        return NavigationObservation(timestamp, State.from_vector(measured_state_vector))
