"""Fixed-step simulation and waypoint mission execution."""

import time

import numpy as np

from .config import DroneConfig, Scenario
from .control import PositionController
from .environment import collides
from .physics import Quadrotor, State
from .sensors import NavigationSensor


class Simulation:
    def __init__(self, scenario=None, config=None, planner=None):
        self.scenario = scenario or Scenario()
        self.config = config or DroneConfig()
        start = np.array(self.scenario.start, dtype=float)
        self.drone = Quadrotor(self.config, State(position=start))
        self.drone.state.motors[:] = np.sqrt(
            self.config.mass * self.config.gravity / 4 / self.config.thrust_coefficient
        )
        self.controller = PositionController(self.config)
        self.sensor = NavigationSensor(self.scenario.seed)
        self.planner = planner
        self.observation = self.sensor.observe(self.drone.state, 0.0)
        self.time = 0.0
        self.steps = 0
        self.waypoint_index = 0
        self.completed = not self.scenario.waypoints
        self.collision = False
        self.target = np.array(
            self.scenario.waypoints[0] if self.scenario.waypoints else self.scenario.start,
            dtype=float,
        )
        self.command = np.zeros(4)
        self.path = [self.drone.state.position.copy()]
        self.predicted = np.empty((0, 3))
        self.planning_ms = []
        self.manual = False
        self.dwell = 0.0

    def step(self):
        if self.collision:
            return
        sc = self.scenario
        ratio = round(sc.control_dt / sc.dt)
        if self.steps % ratio == 0:
            self.observation = self.sensor.observe(self.drone.state, self.time)
            estimate = self.observation.state
            if not self.manual and not self.completed:
                reached = np.linalg.norm(estimate.position - self.target) < 0.2
                settled = np.linalg.norm(estimate.velocity) < 0.3
                self.dwell = self.dwell + sc.control_dt if reached and settled else 0
                if self.dwell >= 0.5:
                    self.waypoint_index += 1
                    self.dwell = 0
                    if self.waypoint_index == len(sc.waypoints):
                        self.completed = True
                    else:
                        self.target = np.array(sc.waypoints[self.waypoint_index], dtype=float)
            control_target = self.target
            if self.planner is not None and self.steps % (ratio * 10) == 0:
                started = time.perf_counter()
                # The planner receives only observations and the explicitly supplied known map.
                control_target, self.predicted = self.planner.plan(estimate, self.target)
                self.planning_ms.append((time.perf_counter() - started) * 1000)
                self.planner_target = control_target
            elif self.planner is not None:
                control_target = self.planner_target
            self.command = self.controller.command(estimate, control_target, sc.control_dt)
            self.path.append(self.drone.state.position.copy())
        previous = self.drone.state.position.copy()
        self.drone.step(self.command, sc.dt, np.asarray(sc.wind))
        self.collision = collides(
            previous, self.drone.state.position, sc.obstacles, self.config.radius
        )
        self.steps += 1
        self.time = self.steps * sc.dt

    def run(self, seconds):
        if not np.isfinite(seconds) or seconds < 0:
            raise ValueError("simulation duration must be finite and nonnegative")
        started = time.perf_counter()
        for _ in range(round(seconds / self.scenario.dt)):
            self.step()
            if self.collision:
                break
        wall = time.perf_counter() - started
        return {
            "simulated_seconds": self.time,
            "wall_seconds": wall,
            "physics_steps_per_second": self.steps / max(wall, 1e-9),
            "mission_complete": self.completed,
            "collision": self.collision,
            "final_position": self.drone.state.position.tolist(),
            "target_error_m": float(np.linalg.norm(self.drone.state.position - self.target)),
            "energy_j": self.drone.state.energy_j,
            "path_length_m": float(np.linalg.norm(np.diff(self.path, axis=0), axis=1).sum()),
            "planning_mean_ms": float(np.mean(self.planning_ms)) if self.planning_ms else None,
        }
