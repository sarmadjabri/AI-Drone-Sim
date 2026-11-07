# AerialWorldLab

AerialWorldLab is an Unreal Engine 5 C++ quadrotor simulation with a Python-only
AI roadmap. Unreal/Chaos is intended to own authoritative simulation state;
Python is reserved for future learned models, training, datasets, and evaluation.

## Current implementation status

The repository now contains a UE C++ project scaffold, a Chaos-driven four-motor
quadrotor pawn, stabilized manual input, a runtime-generated obstacle course, a
third-person camera, basic telemetry, and an engine-independent motor allocation
library with standalone C++ tests.

The project has been compiled successfully with the Linux Unreal Engine 5.2
distribution on Linux Mint 22.3. A standalone game run loaded the built-in starter
map and Aether game mode without pawn-spawn, asset-load, or fatal errors. The
standalone smoke test has completed. The PC detects a DualSense connected over USB;
stick and trigger directions and flight behavior still need hands-on verification.

The previous `src/aether` Python simulator remains as a testable prototype/reference
for existing behavior. It is not the canonical simulation, is not connected to
Unreal, and does not contain the learned world model.

## Requirements

- Unreal Engine 5.2.0 was used for the verified Linux build
- C++ toolchain supported by Unreal (the provided Linux toolchain was used here)
- Python 3.11+ only for the retained prototype and future AI/data tooling
- C++17 compiler for standalone motor-model tests
- GNU Make for the short project commands below

## Open in Unreal

This project was built using UE 5.2.0. Run `make editor` to open the project,
then use Play-In-Editor. The configured game default map is Unreal's built-in
`Template_Default`; the game mode generates the road grid, procedural buildings,
and obstacles at runtime.

Run it as a standalone game with `make play`. These commands use the default
Unreal install path `/home/sarmy/UnrealEngine/UE_5.2`. For another install, pass
its directory as `UE_ROOT`, for example `make editor UE_ROOT=/opt/UnrealEngine`.

Run `make` to show all available project commands.

## Manual control

The pawn uses attitude-stabilized manual control, not direct motor/acro mode:

| Input | Action |
|---|---|
| DualSense left stick X | Roll target |
| DualSense left stick Y | Pitch target |
| DualSense right stick X | Yaw-rate target |
| DualSense L2 / R2 | Decrease / increase collective thrust |
| DualSense triangle | Cycle Cinematic / Normal / Sport |
| DualSense square | Reset to launch point |
| D-pad up/down | Camera zoom |
| Keyboard A/D, W/S | Roll and pitch fallback |
| Keyboard Q/E | Yaw fallback |
| Keyboard F/R | Throttle decrease / increase |
| Keyboard Tab / Backspace | Cycle flight mode / reset |
| PageUp / PageDown | Camera zoom |

Collective throttle starts at the configured hover thrust and ramps while a trigger
or throttle key is held. Stick axes have a small deadzone. Input recognition,
polarity, camera behavior, and gains still need hands-on flight testing. DualSense
Bluetooth behavior depends on the operating system/driver; USB detection has been
confirmed on this machine.

The view is third-person: the camera follows the drone while staying level through
pitch and roll, with spring-arm position/rotation lag substepping. D-pad/PageUp/
PageDown adjust its distance. Stick targets use a gentle expo curve and are
interpolated before the controller; Cinematic mode is gentler while Sport allows
larger tilt, quicker response, and faster throttle changes. Triangle/Tab cycles
the modes. Square/Backspace returns the drone to its launch point and zeros its
velocity. These are control profiles, not validated real-drone flight modes.

The test scene is generated at runtime from Unreal's built-in meshes: a small
street grid, curb/road markings, eight windowed buildings, and block obstacles.
The drone is assembled from colored primitive parts (frame, arms, motors,
propellers, landing legs, and a front camera housing). These are procedural
blockout assets rather than imported photorealistic models or scanned city assets.
The city facade windows are instanced, physics substeps are enabled, and Unreal's
smooth frame-rate range is configured for 30–60 FPS to avoid an uncapped render
loop. This is a target range, not a measured performance guarantee.

## Build and tests

Run `make build` to compile the Unreal Editor target, `make test` to run both test
suites, or `make python-test` / `make cpp-test` to run one suite. The C++ tests
cover the motor allocator and actuator lag without requiring Unreal. Python tests
cover only the retained reference simulator.

## Architecture and limitations

See [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md),
[Docs/PHYSICS.md](Docs/PHYSICS.md), [Docs/FLIGHT_CONTROLS.md](Docs/FLIGHT_CONTROLS.md),
[Docs/TESTING.md](Docs/TESTING.md), [Docs/ROADMAP.md](Docs/ROADMAP.md), and
[Docs/DECISIONS.md](Docs/DECISIONS.md).

Current work is Phase 0 and the beginning of Phase 1. The Unreal Editor target
builds and the standalone game starts on the configured map. PIE flight behavior,
controller response/tuning, and repeatable physics verification remain. Sensors,
Python communication, autonomy baselines, learned world modeling, training, and
predictive planning have not been implemented. Do not interpret initial controller
gains or physical parameters as validated aircraft data.
