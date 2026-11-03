# Project AETHER

A motor-driven quadrotor simulator with fixed-step 6-DoF physics, noisy navigation
observations, waypoint control, obstacle collision checks, and an optional live
Panda3D viewer. The simulator uses SI units and a right-handed, Z-up world frame.

## Install

Python 3.11 or newer is required. Install the project and its optional viewer and
development dependencies in a virtual environment:

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -e '.[viewer,dev]'
```

## Run a simulation

Run the default two-waypoint mission headlessly and print a JSON metrics summary:

```bash
aether --seconds 30
```

Use the interactive 3D viewer:

```bash
aether --viewer --seconds 60
```

The viewer follows the drone and displays obstacles, the destination, the traveled
path, and live telemetry. Press **M** to switch between autonomous waypoint flight
and manual position-target control. In manual mode use **W/A/S/D** to move
horizontally and **R/F** to change altitude. **Space** pauses, **Backspace**
restarts, **arrow keys** orbit the camera, **1–5** select simulation speed, and
**Escape** exits.

Pass a scenario JSON file with `--scenario path/to/scenario.json`; create one from
Python with `Scenario().save("scenario.json")`. Use `--seed` to override the
scenario's sensor-noise seed.

## Run tests

```bash
pytest
```

## Current scope

Implemented: motor-level quadrotor dynamics, attitude/position stabilization,
noisy navigation-state observations, waypoint missions, wind, static box obstacles,
swept collision checks, headless metrics, and optional interactive visualization.
The sensor is a noisy navigation-state surrogate, not a full IMU/GPS fusion stack.
Neural world modeling, learned prediction, and predictive planning are not yet
implemented.
