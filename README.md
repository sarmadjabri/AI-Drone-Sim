# AerialWorldLab

Hello

Welcome to my shameful attempt at making a drone sim (thats linked to a predictive world model and a short horizon type neural net fusion + more)

This is a platform that integrates a visually realistic Unreal Engine 5.2 drone simulator, custom quadrotor flight dynamics, a corresponding physical drone, and a custom neural network architecture 
based on predictive world models. The system enables autonomous drones to perceive and navigate complex urban and natural environments, identify and track buildings, vehicles, and other objects
in real time, construct internal spatial and temporal representations of their surroundings, predict future environmental states, and optimize flight trajectories based on anticipated outcomes. 
By combining simulated and real world flight data (from the physical drone we build), the platform supports neural net training, environmental adaptation, simulation to reality transfer, validation of
the actual results.

The purpose you may ask? none
I'll let the agent generate the rest of the documentation, have fun

Unreal Engine 5.2 quadrotor simulator and an independent sim-to-real aerial robotics
research project. Current work covers manual flight, custom dynamics and an
externally authored New York-style daytime neighborhood. The physical drone,
custom PyTorch neural world model and autonomous navigation remain future work.

[DOCUMENTATION.md](DOCUMENTATION.md) contains the complete setup, architecture,
controls, tuning, asset pipeline, validation and research roadmap. Contributors
should read its [development agreements](DOCUMENTATION.md#development-agreements).

## Setup

Install Git, **64-bit Python 3.12**, **Unreal Engine 5.2** and your platform's Unreal
C++ toolchain. Use the [Windows, Linux or macOS setup instructions](DOCUMENTATION.md#installation-and-commands)
for compiler/SDK requirements and engine installation paths. Native execution is
verified on Linux; Windows/macOS command handling is tested, with native validation
still required on those systems.

Clone the `Development` revision containing this setup work:

```sh
git clone --branch Development https://github.com/sarmadjabri/AI-Drone-Sim.git
cd AI-Drone-Sim
```

Linux/macOS:

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install -c requirements-setup.txt -e '.[dev,assets]'
export UE_ROOT='/path/to/UE_5.2'
.venv/bin/python scripts/project.py doctor --assets
.venv/bin/python scripts/project.py bootstrap-city
.venv/bin/python scripts/project.py play-windowed
```

Windows, in Developer PowerShell for Visual Studio 2022:

```powershell
py -3.12 -m venv .venv
.venv\Scripts\python.exe -m pip install -c requirements-setup.txt -e '.[dev,assets]'
$env:UE_ROOT = 'C:\Program Files\Epic Games\UE_5.2'
.venv\Scripts\python.exe scripts/project.py doctor --assets
.venv\Scripts\python.exe scripts/project.py bootstrap-city
.venv\Scripts\python.exe scripts/project.py play-windowed
```

No virtual-environment activation or Make installation is required. Replace the
engine path with your own; the runner also recognizes conventional installation
locations. Bootstrap restores ignored native city/car assets and downloads about
2 GB. Plan for 32 GiB RAM, preferably 64 GiB, and 40 GiB free project storage in
addition to Unreal/toolchain storage. Ordinary builds/runs stay offline after setup.
See the [clean-setup verification record](DOCUMENTATION.md#clean-setup-verification-record)
for what has actually been exercised.

## Fly and verify

Close settings, release the sticks, and hold **M** (or **L1 + R3 / LB + RS click**)
for 1.5 seconds to arm. Press **T** for assisted takeoff. WASD moves horizontally,
Q/E yaws, and Space/LeftCtrl commands climb/descent in the default layout.
**L** lands, **V** changes camera, **Esc** opens paused settings, and **F11** toggles
standalone borderless fullscreen. See the
[complete controls reference](DOCUMENTATION.md#pilot-controls-and-profiles).

Run these commands using the virtual environment's Python:

```sh
python scripts/project.py build
python scripts/project.py test
python scripts/project.py unreal-test
```

On Linux/macOS, `make build`, `make test` and `make unreal-test` remain optional
shortcuts. Python-only tests can run without Unreal; portable C++ tests require
a host compiler. NullRHI verifies engine logic, not graphics/controller hardware.

Chaos integrates the native aircraft and resolves collision. Speed commands include
Cinematic 12 m/s, Sport 24 m/s and High Performance 35 m/s. Existing custom profiles
retain their values; Restore defaults applies new presets. Physical calibration
remains future work. The retained Python simulator is a separate experiment.

The city uses NVIDIA Brownstone assets and DGG/Eric Chadwick's Khronos CarConcept.
It is artistic New York-style scenery, not surveyed Manhattan. Cars are kinematic
obstacles with static wheels. Generated city meshes/maps, downloads and caches are
ignored; curated tracked art and plain-text asset notices remain versioned. See
[provenance](DOCUMENTATION.md#asset-provenance-and-source-control) and
[performance/validation](DOCUMENTATION.md#validation-and-performance) for details.
