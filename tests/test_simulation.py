import numpy as np
import pytest

from aether.config import DroneConfig, Scenario
from aether.environment import collides, segment_box
from aether.math3d import integrate, rotation
from aether.physics import Quadrotor, State
from aether.simulation import Simulation


def test_quaternion_rotation_and_integration():
    half_turn = np.array([np.cos(np.pi / 4), 0, 0, np.sin(np.pi / 4)])
    rotated = rotation(half_turn) @ np.array([1.0, 0.0, 0.0])
    np.testing.assert_allclose(rotated, [0, 1, 0], atol=1e-12)
    q = integrate(np.array([1.0, 0, 0, 0]), [0, 0, 2], 0.5)
    np.testing.assert_allclose(np.linalg.norm(q), 1.0, atol=1e-12)
    np.testing.assert_allclose(rotation(q) @ [1, 0, 0], [np.cos(1), np.sin(1), 0], atol=1e-12)


def test_zero_thrust_free_fall():
    drone = Quadrotor(state=State(position=np.array([0.0, 0.0, 10.0])))
    drone.step(np.zeros(4), 0.01)
    assert drone.state.velocity[2] == pytest.approx(-9.81 * 0.01)
    assert drone.state.position[2] == pytest.approx(10 - 0.5 * 9.81 * 0.01**2)


def test_hover_thrust_matches_weight_and_stays_stable():
    config = DroneConfig()
    hover_speed = np.sqrt(config.mass * config.gravity / 4 / config.thrust_coefficient)
    assert config.thrust_coefficient * 4 * hover_speed**2 == pytest.approx(
        config.mass * config.gravity
    )

    scenario = Scenario(start=[0, 0, 2], waypoints=[], wind=[0, 0, 0])
    sim = Simulation(scenario)
    for _ in range(round(4 / scenario.dt)):
        sim.step()
    assert abs(sim.drone.state.position[2] - 2) < 0.12
    assert np.linalg.norm(sim.drone.state.velocity) < 0.15


def test_wind_moves_drone_and_motors_are_independent():
    drone = Quadrotor()
    drone.step(np.array([0.6, 0.4, 0.4, 0.4]), 0.01)
    assert drone.state.motors[0] > drone.state.motors[1]
    assert drone.state.motors[1] == pytest.approx(drone.state.motors[2])

    wind_drone = Quadrotor(state=State(position=np.array([0.0, 0.0, 3.0])))
    wind_drone.step(np.full(4, 0.5), 0.01, wind=[1, 0, 0])
    assert wind_drone.state.velocity[0] > 0


def test_physics_rejects_invalid_motor_and_wind_inputs():
    drone = Quadrotor()
    with pytest.raises(ValueError, match="motor commands"):
        drone.step([0, 0, np.nan, 0], 0.01)
    with pytest.raises(ValueError, match="wind"):
        drone.step(np.zeros(4), 0.01, wind=[0, np.inf, 0])
    with pytest.raises(ValueError, match="dt"):
        drone.step(np.zeros(4), float("nan"))


def test_segment_box_and_floor_collision():
    obstacle = {"center": [0, 0, 1], "half_size": [0.5, 0.5, 0.5]}
    assert segment_box([-1, 0, 1], [1, 0, 1], obstacle["center"], obstacle["half_size"])
    assert not segment_box([-1, 2, 1], [1, 2, 1], obstacle["center"], obstacle["half_size"])
    assert collides([0, 0, 1.5], [0, 0, 1.5], [obstacle])
    assert collides([0, 0, 0.2], [0, 0, 0.2], [])


def test_waypoint_mission_completes_without_collision():
    sim = Simulation()
    result = sim.run(30)
    assert result["mission_complete"]
    assert not result["collision"]
    assert result["target_error_m"] < 0.1
    assert result["path_length_m"] > 0
    assert result["energy_j"] > 0


def test_simulation_is_deterministic_for_seed():
    scenario = Scenario(seed=23)
    first = Simulation(scenario).run(3)
    second = Simulation(Scenario(seed=23)).run(3)
    np.testing.assert_allclose(first["final_position"], second["final_position"])
    assert first["energy_j"] == pytest.approx(second["energy_j"])


@pytest.mark.parametrize("duration", [-1, float("nan"), float("inf")])
def test_run_rejects_invalid_duration(duration):
    with pytest.raises(ValueError, match="duration"):
        Simulation().run(duration)


def test_scenario_rejects_nonfinite_timestep():
    with pytest.raises(ValueError, match="dt"):
        Scenario(dt=float("nan"))
