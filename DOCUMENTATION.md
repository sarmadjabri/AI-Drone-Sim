# AerialWorldLab project documentation

This is the maintained reference for building, operating, extending and evaluating
AerialWorldLab (Project Aether). The Unreal simulator is implemented; the physical
drone and custom neural world model are research objectives. Plans below are not
claims of implemented functionality. Documentation reflects the Development
working tree verified on 2026-11-27.

## Contents

- [Project status](#project-status)
- [Installation and commands](#installation-and-commands)
- [Repository architecture](#repository-architecture)
- [Physics and control implementation](#physics-and-control-implementation)
- [Pilot controls and profiles](#pilot-controls-and-profiles)
- [Camera and display behavior](#camera-and-display-behavior)
- [Tuning reference](#tuning-reference)
- [City environment and asset pipeline](#city-environment-and-asset-pipeline)
- [Asset provenance and source control](#asset-provenance-and-source-control)
- [Validation and performance](#validation-and-performance)
- [Troubleshooting](#troubleshooting)
- [Development agreements](#development-agreements)
- [Research objectives and roadmap](#research-objectives-and-roadmap)

## Project status

| Area | Current implementation and limits |
| --- | --- |
| Interactive simulator | Unreal Engine 5.2, C++, Linux Development build; Chaos owns rigid-body integration and collision. |
| Flight | Rotor forces, reaction torque, motor lag, drag, assisted flight, Angle and Acro, arming and failsafes. Parameters are assumptions pending physical calibration. |
| Input | Keyboard and normalized DualSense/Xbox bindings, calibration, remapping and saved profiles. Hardware behavior depends on OS/Unreal recognition. |
| Cameras | Chase, stabilized FPV and rigid body-locked FPV; standalone F11 borderless fullscreen. |
| Environment | Externally authored NVIDIA New York-style brownstones and landscaping, daylight and collision. Artistic neighborhood, not georeferenced Manhattan. |
| Traffic | Four detailed static-mesh cars on an authored closed street circuit; swept collision and bounded acceleration. Kinematic obstacles, without tire simulation or animated wheels. |
| Python backend | Retained NumPy/SciPy simulator and optional Panda3D viewer. Different assumptions from the Unreal simulator. |
| AI, synchronized research datasets, physical drone | Future work; a PyTorch dependency option is not an implemented neural network or an Unreal bridge. |
| Deployment | Linux editor/standalone workflows verified locally; see the clean-setup record. Windows/macOS native execution, packaged distribution and physical flight require separate validation. |

Current scope is simulator physics, city assets, performance and maintainability.
Retain existing controller mappings and body-locked FPV while evolving the city.

## Installation and commands

### Supported setup paths and hardware

The command runner supports Windows, Linux and macOS desktop layouts. Full native
execution is verified locally on Linux; Windows/macOS command construction is
covered by portable tests, but their native builds and rendering still need a
machine running those systems. This is not a claim that every computer can run
Unreal or convert this city's offline-render assets.

| Host | Engine/toolchain baseline | Project validation |
| --- | --- | --- |
| Windows x64 | UE 5.2, Visual Studio 2022 17.4+ with Game development with C++, MSVC C++ tools and a compatible Windows SDK; use Developer PowerShell for VS 2022 for portable `cl.exe` tests. | Runner/argument tests; native build and rendered play unverified here. |
| Linux x86-64 | UE 5.2 Linux editor and its bundled compiler/SDK; Epic's UE 5.2 baseline includes Ubuntu 22.04. Install a host GCC/Clang compiler for portable C++ tests. | Local native build, controls/city regressions and rendering verified. See the clean-setup record below. |
| macOS Intel or Apple Silicon | UE 5.2 universal editor, macOS 12.5+ and a UE-5.2-compatible Xcode installation; Epic specifies Xcode 14.1 minimum. | Runner/argument tests; native build and rendered play unverified here. |

Use the toolchain matrix for **UE 5.2**, rather than assuming the latest Xcode or
MSVC is compatible with this older engine. Engine installation and SDK setup are
external prerequisites; this project does not silently install/upgrade Unreal.
Sources: [Epic's UE 5.2 hardware/software requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/hardware-and-software-specifications-for-unreal-engine?application_version=5.2),
[Visual Studio compatibility](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.2),
and [UE 5.2 on Apple Silicon](https://www.unrealengine.com/tech-blog/unreal-engine-5-2-brings-native-support-for-apple-silicon-and-other-developments-for-macos).

Use **Python 3.12, 64-bit**, as the documented setup baseline. The package declares
Python 3.11+, but this setup's fresh-environment verification uses 3.12. On Apple
Silicon, use native arm64 Python and native packages consistently. Avoid mixing
Rosetta x86-64 packages into an arm64 environment. The asset preparation and Unreal
Python commandlets use separate interpreters: install asset extras into `.venv`,
not into Unreal's embedded Python. Obtain Python from [python.org](https://www.python.org/downloads/)
or a trusted OS package source; install [Git](https://git-scm.com/downloads) too.

For **full city conversion**, plan for at least 32 GiB RAM and 40 GiB free project
storage, with 64 GiB RAM recommended for headroom. These are conservative project
planning budgets, not formally established minimum hardware specifications. The
engine/toolchain need their own additional storage, particularly for source builds.
A modern graphics driver and Unreal-compatible GPU are needed for rendering; the
measured integrated-GPU result is not an FPS guarantee for other hardware.
The approximately 2 GB city archive expands into source, prepared geometry, native
assets and caches. Bootstrap/import can take many minutes. Keep the machine awake
and do not mistake a long mesh/texture build for an immediate failure.

Python-only tests can run without Unreal/GPU, and the portable C++ test needs only
a host compiler. Machines unable to run Unreal can still work on those parts.
Phones, tablets, browser-only machines and Windows-on-ARM/Linux-ARM native Unreal
builds are outside the documented desktop setup. Headless Unreal is still the
engine/physics world; it is not an accelerated training backend.

### Get the correct project revision

These instructions describe this working-tree revision. Commit/push it to
`Development` before expecting another computer's clone to contain the new runner,
constraints and documentation. This setup work does not commit or push anything.
From a terminal with Git installed:

```sh
git clone --branch Development https://github.com/sarmadjabri/AI-Drone-Sim.git
cd AI-Drone-Sim
```

If the checkout already exists, use the intended `Development` revision and preserve
local work. Do not delete your current checkout to follow fresh-install examples.
Curated drone art is tracked in Git; generated New York city/car binaries are
intentionally absent until bootstrap. No city-service API key is required.

### Windows setup (PowerShell)

Get Epic Games Launcher from [Epic's download page](https://www.unrealengine.com/download),
sign in, and select a **5.2.x** engine installation in its Unreal Engine library, following
[Epic's version-selection instructions](https://dev.epicgames.com/documentation/en-us/unreal-engine/installing-unreal-engine?application_version=5.2)
(rather than installing a newer engine for this project). An existing compatible
source build can also be selected. Install the C++ workload/SDK above and Python 3.12 with the Python launcher.
Open **Developer PowerShell for VS 2022**, enter the checkout, then run:

```powershell
py -3.12 -m venv .venv
.venv\Scripts\python.exe -m pip install --upgrade pip
.venv\Scripts\python.exe -m pip install -c requirements-setup.txt -e '.[dev,assets]'
$env:UE_ROOT = 'C:\Program Files\Epic Games\UE_5.2'
.venv\Scripts\python.exe scripts/project.py doctor --assets
.venv\Scripts\python.exe scripts/project.py bootstrap-city
.venv\Scripts\python.exe scripts/project.py test
.venv\Scripts\python.exe scripts/project.py unreal-test
.venv\Scripts\python.exe scripts/project.py play-windowed
```

Change `UE_ROOT` if the engine lives elsewhere. Virtual-environment activation is
not required; invoking its Python directly avoids PowerShell execution-policy
changes. This path does not require Make, Bash, WSL or a Linux `/tmp` directory.
If `cl.exe` is missing, reopen the Visual Studio developer shell after checking
its C++ workload. UBT's native compiler/SDK diagnostics remain authoritative.

### Linux setup (Bash)

Get the UE 5.2 Linux editor/toolchain from [Epic's Linux downloads](https://www.unrealengine.com/en-US/linux)
(an Epic account is required), or use an existing compatible engine source build.
Extract the downloaded editor into a directory you own and use that directory as
`UE_ROOT`. If a source build is used, complete the engine's dependency/SDK setup
and editor build before running this project's commands. Install Git, a host C++17 compiler and
Python 3.12 with `venv` support through a trusted method for your distribution.
A distribution's default `python3` may be too old; check the version explicitly.
Then enter the checkout and run:

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install -c requirements-setup.txt -e '.[dev,assets]'
export UE_ROOT='/path/to/UE_5.2'
.venv/bin/python scripts/project.py doctor --assets
.venv/bin/python scripts/project.py bootstrap-city
.venv/bin/python scripts/project.py test
.venv/bin/python scripts/project.py unreal-test
.venv/bin/python scripts/project.py play-windowed
```

Use the engine root containing `Engine/Build/Build.version`, not the `Engine`
subdirectory or the editor executable itself. Rendered play needs an active
graphical session and working Vulkan driver; `headless`/`unreal-test` use NullRHI.

### macOS setup (Terminal)

Get [Epic Games Launcher](https://www.unrealengine.com/download), sign in and
select a **5.2.x** engine installation in its Unreal Engine library. Install a
compatible full Xcode installation too.
Select the intended Xcode in its Settings/Locations panel or with
`sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer`, and complete
Xcode's first-launch setup/license prompts. Install Git and 64-bit Python 3.12.
Keep Python and its binary wheels native to the selected Mac architecture.
Then enter the checkout and run:

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install -c requirements-setup.txt -e '.[dev,assets]'
export UE_ROOT='/Users/Shared/Epic Games/UE_5.2'
.venv/bin/python scripts/project.py doctor --assets
.venv/bin/python scripts/project.py bootstrap-city
.venv/bin/python scripts/project.py test
.venv/bin/python scripts/project.py unreal-test
.venv/bin/python scripts/project.py play-windowed
```

Adjust the engine/Xcode paths to your actual installations. The runner uses
`Mac/Build.sh`, target `Mac` and the executable inside `UnrealEditor.app`; Linux
binary paths are not used. macOS uses Metal rather than the Linux Vulkan path.
No claim of macOS rendering/controller validation is made by the Linux tests.

### Engine discovery, dependencies and command reference

Use the virtual environment's Python for every `scripts/project.py` command.
Examples below use `python` to mean that interpreter; substitute
`.venv/bin/python` on Linux/macOS or `.venv\Scripts\python.exe` on Windows.
The runner always executes from the repository root, including when launched
from a different directory, and passes paths with spaces as complete arguments.

Engine selection order is `--ue-root`, then the `UE_ROOT` environment variable,
then conventional installs: `Program Files/Epic Games/UE_5.2` on Windows,
`/Users/Shared/Epic Games/UE_5.2` on macOS, and `~/UnrealEngine/UE_5.2`,
`~/UnrealEngine` or `~/UnrealEngine-5.2` on Linux. An invalid explicit path fails
rather than silently selecting another engine. Major/minor versions other than
5.2 are rejected. No developer username or mandatory absolute engine path is
stored in the runner or Makefile.

`requirements-setup.txt` constrains known-working direct setup dependencies.
It is a tested baseline, not a complete transitive/hash-locked environment.
Optional Panda3D/PyTorch extras remain separate. If pip cannot find a matching
binary wheel, check Python version, architecture and OS support before attempting
an unrelated source build or dropping version constraints.

```sh
python scripts/project.py --help
python scripts/project.py doctor --assets
python scripts/project.py doctor --python-only
python scripts/project.py build --ue-root '/path/to/UE_5.2'
python scripts/project.py bootstrap-city --dry-run
```

`doctor` performs no installs/downloads. It checks interpreter, free disk, engine
version/tool paths and optional asset modules, prints resource guidance, and reports
whether the generated map/car are present. A missing generated map on first setup
is expected; `bootstrap-city` restores it. The actual build verifies compiler/SDK
compatibility. `--dry-run` prints commands without executing them; selecting a native
command still requires an identifiable engine installation/version.

| Command after `python scripts/project.py` | Purpose |
| --- | --- |
| `doctor` | Diagnose setup; add `--assets` for asset-module checks or `--python-only` to omit engine discovery. |
| `build` | Build the editor target for the host platform. |
| `editor` | Open Unreal Editor. |
| `play` | Run standalone fullscreen. |
| `play-windowed` | Run standalone at 1280×720. |
| `play-low` | Run a window with reduced graphics quality. |
| `headless` | Run Unreal with NullRHI and no sound. |
| `profile` | Start a 360-frame launch CSV capture; warm caches before interpreting timing. |
| `test` | No-ternary guard, Python tests and portable C++ dynamics test. |
| `python-test` / `cpp-test` / `readability-check` | Run one portable check. |
| `unreal-test` | Build, then run Aether.Flight automation with NullRHI. |
| `prepare-new-york` | Download, base/detailed USD export, spatial batching and optimization. |
| `import-new-york` | Build and import prepared geometry into the native map. |
| `bootstrap-city` | Build first, restore city inputs, prepare/import vehicle, then import city. |

`unreal-test --test-filter Aether.Flight.CityEnvironment` selects one test.
Use `--test-arg=-ExportCityRoadGrid` for an additional Unreal switch. GNU Make is
an optional Linux/macOS shortcut around the same runner: `make build`, `make test`,
`make unreal-test`, etc. `make build UE_ROOT=/path/to/UE_5.2` still works. Override
`PYTHON` if the virtual environment is elsewhere. Native Windows users should use
the Python runner directly rather than installing Make solely for this project.

### Retained Python experiment

With the environment's Python:

```sh
python -m aether.cli --seconds 30 --seed 7
python -m aether.cli --scenario /path/to/scenario.json --seconds 30
python -m pip install -c requirements-setup.txt -e '.[viewer]'
python -m aether.cli --viewer --seconds 30
```

The headless experiment prints JSON results. `Scenario.load/save` defines its
scenario format in `src/aether/config.py`. Its seed controls Python sensor noise.
Python results do not establish equivalence to Chaos or a real aircraft. The
optional `learning` extra installs PyTorch for future research, not an Unreal bridge.

### Clean-setup verification record

A Linux trial used a separate source snapshot, a new Python 3.12 environment and
a project path containing spaces. No generated native city/car assets were copied
from the working project. Dependency installation, native editor compilation,
city download/checksum verification, source conversion and the corrected headless
vehicle import passed. Conversion produced 584 batches and 25,869,605 triangles.
The trial used the existing Unreal installation and shared engine caches.

The final city import was observed progressing, but its completion and final exit
status were not verified. Therefore **full clean-source bootstrap is not yet
confirmed end to end**. The existing working project's 16 Unreal tests and rendered
inspection remain separate evidence, not validation of that fresh restoration.

Windows and macOS have documented setup commands and portable command-construction
tests; native builds, imports, rendering and controller behavior still require
verification on those operating systems. The current working-tree portable checks passed 39 Python
tests, the C++ dynamics test and the no-ternary guard. These limits should be
updated with actual native results rather than inferred from command tests.

## Repository architecture

| Location | Responsibility |
| --- | --- |
| `AerialWorldLab.uproject` | Runtime module and plugin declarations. |
| `Source/AerialWorldLab.Target.cs`, `AerialWorldLabEditor.Target.cs` | Unreal game/editor targets. |
| `Source/AerialWorldLab/AerialWorldLab.Build.cs` | Module dependencies. |
| `Source/AerialWorldLab/Private/Flight/QuadrotorDynamics.cpp` and matching public header | Portable SI force, torque, drag and motor math. |
| `Public/Flight/FlightControl.h` | Stateless controller math. |
| `Private/Flight/QuadrotorPawn.cpp` and public header | Aircraft construction/lifecycle and the ordered Tick pipeline. |
| `Private/Flight/QuadrotorPawnControl.cpp` | Resolved limits, drag, assisted/manual laws and motor force application. |
| `Private/Flight/QuadrotorPawnInput.cpp` | Raw input mapping, trigger roles and command smoothing. |
| `Private/Flight/QuadrotorPawnSafety.cpp` | Ground checks, arming, takeoff/landing and mode-transition gates. |
| `Private/Flight/QuadrotorPawnCamera.cpp`, `QuadrotorPawnTelemetry.cpp` | Camera controls and human-readable aircraft status. |
| `Private/Flight/QuadrotorControlFrame.h`, `QuadrotorLayout.h` | Transient per-tick data and authored model/rotor layout. |
| `Public/Flight/SimulationUnits.h` | Shared SI-to-Unreal conversion factors, including force, torque and inertia. |
| `Private/Flight/PilotControlsComponent.cpp` and public header | Calibration, validation and default controls. |
| `Private/Flight/PilotControlsInput.cpp` | Button chords, held actions, dispatch and pause/input lifecycle. |
| `Private/Flight/PilotControlsProfiles.cpp` | Validated profile save/load transactions. |
| `Private/Flight/PilotControlsMenu.cpp`, `PilotMenuLayout.h` | Named menu pages/rows, navigation, separate page editors and labels. |
| `Private/Flight/PilotControlsDiagnostics.cpp` | Controller labels and diagnostic formatting. |
| `Public/Flight/PilotInput.h` | Input/layout/mode types. |
| `Private/Flight/FlightModeTuning.cpp`, `Public/Flight/FlightModeTuning.h` | Named defaults and shared field descriptors/validation. |
| `Private/Flight/DroneCameraControllerComponent.cpp` and public header | Stabilized and body-locked camera mounts. |
| `Private/Flight/AetherGameMode.cpp` and public header | World startup, daylight, aircraft and traffic setup. |
| `Private/Flight/AetherHUD.cpp` and public header | Flight status, input diagnostics, menus and attribution. |
| `Public/Flight/CitySimulationSettings.h` | Shared configuration for daylight, exposure, street probes and launch clearance. |
| `Private/Flight/CityVehicle.cpp`, `Public/Flight/CityVehicle.h` | Route-following kinematic cars and collision stopping. |
| `Private/Flight/CityTileActor.cpp`, `Public/Flight/CityTileActor.h` | Generated static city geometry without unnecessary ticking/navigation export. |
| `Private/Display/`, `Public/Display/` | Saved graphics policy and standalone F11 handling. |
| `Source/AerialWorldLab/Private/Tests/` | Unreal flight, controls, display, collision and traffic automation. |
| `Config/DefaultEngine.ini` | Default map, rendering and engine configuration. |
| `Config/DefaultGame.ini` | Legacy flight, camera, daylight, traffic and packaging settings. |
| `Config/DefaultInput.ini` | Normalized engine input bindings. |
| `Config/CityImport.json` | Conversion budgets, source selection, tile grid and launch metadata. |
| `Content/CityData/` | Small scene/traffic manifests and attribution, staged for packaged builds. |
| `Assets/External/` | Curated source art, provenance, licenses and ignored downloads/intermediates. |
| `scripts/` | Portable command runner, asset acquisition/conversion/import, material repairs, readability checks and CSV summaries. |
| `requirements-setup.txt` | Known-working direct dependency constraints for the Python 3.12 setup baseline. |
| `src/aether/` | Retained Python experiment: configuration, environment, math, physics, control, sensors, simulation, viewer and CLI. |
| `tests/`, `tests/cpp/` | Python and portable C++ regressions. |
| `docs/*.json` | Small machine-readable historical/current benchmark summaries. |
| `Saved/` | Local logs, profiles, import cache, screenshots, captures and diagnostic exports; ignored. |

Source paths in the architecture table are relative to `Source/AerialWorldLab/`
when they start with `Private/` or `Public/`. Generated Unreal headers remain last
in their corresponding include lists.

## Physics and control implementation

The native simulator applies independent plus-layout rotor thrusts and reaction
torques to a Chaos rigid body. Portable dynamics code supplies allocation, motor
lag and body-axis quadratic aerodynamic drag. Mass/inertia, force limits, drag,
world wind and air density are editable assumptions; matching an F450 visual model
does not physically calibrate those values.

Internal dynamics calculations use SI units. At the Unreal boundary, positions
are centimeters, Newton forces multiply by 100 and Newton-meter torques multiply
by 10000. Keep the existing frame conversions explicit; do not mix body-axis
quantities and world-space vectors. Chaos integrates motion and resolves collision;
flight speed presets must not become direct position/velocity teleports.

Assisted flight turns heading-relative velocity requests into bounded acceleration,
tilt and thrust. Releasing translation triggers braking and then position capture;
vertical commands integrate an altitude target. Attitude and rate controllers
feed bounded rotor allocation. Angle mode commands tilt with manual collective;
Acro commands body rates with manual collective and no automatic leveling.

Chaos physics uses substeps, but the controller and actuator updates still run on
the render tick with hitch clamping. A fully fixed-rate actuator/controller path
remains outstanding. Tests at 30/60/120 Hz exercise response and settling; they
are not measurements of a real airframe, sensor suite or flight controller.

Faster presets continue to obey drag, tilt, motor response, allocation and collision.
The high-performance stopping envelope scales with `v²/(2a)` plus a transient
allowance rather than retaining a fixed low-speed stopping-distance limit.

## Pilot controls and profiles

### Start flying

The aircraft starts **disarmed**. Seeing stick values in calibration does not mean
motors are enabled. Settings **pause flight**, while input readings remain live.

1. Close settings with Options / Xbox Menu / Esc. Let the aircraft settle on the ground.
2. Release the sticks. Hold **L1 + R3 / Xbox LB + RS click / keyboard M** for 1.5 seconds.
3. In assisted modes, press **L1 + Cross / Xbox LB + A / keyboard T** to take off to
   1.5 m above the ground. Arming alone does not start an assisted takeoff.
4. In Professional Mode 2, left-stick up climbs, right-stick up moves forward,
   right-stick right moves right, and left-stick right yaws right.
5. Use **L1 + Circle / LB + B / L** to land in assisted modes. Landing disarms on
   ground detection. Reset is a deliberate 1.5-second hold and also disarms.

The HUD always shows why flight is inhibited and the last action result, even
when detailed diagnostics are hidden. The help text reflects custom button bindings.
For manual flight, arm with low direct throttle or centered incremental throttle,
then raise collective yourself; assisted takeoff/landing are unavailable.

### Controller support and layouts

DualSense and Xbox share Unreal's normalized positional gamepad inputs. No brand
switch is required. Paired labels identify both controllers. The operating system
and Unreal backend must recognize the device; the Linux build uses SDL.
Unreal's positive-up stick convention is preserved by the input configuration.

Camera mode, flight mode, and mapping profile are independent. The sole exception
is the explicitly selected Legacy View-Dependent profile.

| Profile | Left X | Left Y | Right X | Right Y | Triggers |
| --- | --- | --- | --- | --- | --- |
| Professional Mode 2 | Yaw | Vertical | Lateral/roll | Forward/pitch | Independent auxiliary roles |
| Legacy FPV | Yaw | Vertical | Lateral/roll | Forward/pitch | Independent auxiliary roles |
| Legacy 3D | Lateral/roll | Forward/pitch | Yaw | Unassigned | RT/R2 minus LT/L2 is vertical |
| Legacy View-Dependent | Legacy 3D in chase; Legacy FPV in either FPV view | | | | |
| Custom | Any of six physical axes can drive each command | | | | |

Custom mapping reserves any trigger assigned to flight: that trigger cannot also
operate precision, zoom, gimbal, or an auxiliary channel. Legacy 3D reserves both
triggers. Duplicate flight-axis sources are rejected. In custom mappings a trigger's
0–1 travel is converted to -1–1 before calibration; use **Direct** collective when
mapping a single trigger as an absolute throttle. Incremental throttle on a trigger
requires a calibrated center and is generally less useful than a centered stick.

| Flight mode | Forward/pitch and lateral/roll | Yaw | Vertical |
| --- | --- | --- | --- |
| Legacy Assisted, Cinematic, Sport, High Performance, Training | Heading-relative horizontal velocity, m/s | Heading command integrated from yaw rate | Climb/descent request, altitude target integration |
| Angle | Desired pitch/roll tilt; neutral levels the body | Body yaw rate | Manual collective |
| Acro | Body pitch/roll rates; neutral stops rotation without leveling | Body yaw rate | Manual collective |

Assisted modes brake when translation is released and capture position after speed
and tilt settle. They hold altitude with a bounded thrust controller. Angle has no
altitude/position hold or automatic translational braking. Acro runs only body-rate
feedback and collective allocation: no leveling, altitude hold, or position hold.

Keyboard WASD controls horizontal commands; Q/E yaws. Space/LeftCtrl supplies
vertical commands for Mode 2/Legacy FPV/Custom; R/F does so in Legacy 3D.
Controller calibration and inversion never change keyboard direction conventions.
Legacy pawn `bInvertForward/Lateral/Yaw/Vertical` Game.ini flags are still honored;
leave them false when using the per-axis inversion controls in the runtime menu.
F6 cycles layouts, F7 cycles flight modes, V cycles cameras, F8 toggles diagnostics,
Esc opens settings, M holds arm/disarm, T requests takeoff, L requests landing,
Backspace holds reset, PageUp/PageDown adjusts camera zoom.

### Default buttons

| DualSense | Xbox | Action |
| --- | --- | --- |
| Cross | A | Confirm/context |
| Circle | B | Cancel operation / back in settings |
| Triangle | Y | Cycle chase → stabilized FPV → body-locked FPV; next settings page |
| Square (hold) | X (hold) | Reset aircraft; release cancels |
| Options | Menu/Start | Pause/settings |
| Create | View/Back | Show/hide detailed input diagnostics |
| L1 | LB | Flight modifier |
| R1 | RB | Camera modifier |
| L3 | LS click | Assisted braking; position captures after settling |
| R3 | RS click | Recenter stabilized gimbal; body-locked remains rigid |
| L1 + D-pad left/right | LB + D-pad left/right | Previous/next mode |
| L1 + D-pad up/down | LB + D-pad up/down | Increase/decrease speed, or manual rate limit |
| L1 + Cross | LB + A | Assisted takeoff |
| L1 + Circle | LB + B | Assisted landing |
| L1 + R3 (hold) | LB + RS click (hold) | Arm/disarm; disarm requires ground contact |
| R1 + D-pad up/down | RB + D-pad up/down | Stabilized FPV gimbal pitch |
| R1 + D-pad left/right | RB + D-pad left/right | Zoom out/in |
| R1 + R3 | RB + RS click | Recenter gimbal |
| D-pad up/down | D-pad up/down | Zoom in/out (chase distance or FPV FOV) |
| L2 | LT | Unused by default; independently configurable |
| R2 | RT | Precision while held beyond 50% by default |

Hold the modifier **before** pressing its action. Exact modifier matching prevents
unmodified actions from also firing; both shoulders together suppress actions.
Adding or releasing a modifier cancels pending dangerous holds. Button release,
disconnect, and opening settings also cancel holds. OS repeat events do not restart
holds. Reset and arm execute once per completed hold; re-press to repeat.

Each trigger can be Unused, Precision, Zoom, Gimbal, or Auxiliary. Left-trigger
camera adjustments run negative, right-trigger positive. Precision scales speed,
vertical request, and manual angular commands by 0.25 without changing the mode.
Auxiliary outputs are two normalized channels shown in diagnostics, available to
future aircraft systems; they do not control an invented actuator.
Touchpad, Xbox Guide/Share, double press, haptics, and adaptive triggers are not used.
The suggested R1+Triangle optional camera-profile action is unassigned; the three
camera modes remain available through Triangle. L1+R3 is deliberately arm/disarm,
rather than an undocumented stabilization toggle.

### Precision and tuning

The hardware pipeline is: normalized raw axis → center/range calibration → inner
and outer dead-zone rescaling → inversion → expo and sensitivity → mode units →
optional command slew → flight controller → bounded motor allocation → Chaos forces.
The engine's gamepad axis dead zones are zero to avoid duplicate dead-zone filtering.

For calibrated magnitude `u`, inner dead zone `d`, and outer dead zone `o`:

```
x = clamp((abs(u) - d) / (1 - d - o), 0, 1)
e = clamp(modeExpo + axisExpoOffset, 0, 1)
y = sign(u) * clamp(sensitivity * ((1-e)*x + e*x^3), 0, 1)
```

Default axis center is 0, range -1…1, inner dead zone 0.05, outer dead zone 0.02,
sensitivity 1, expo offset 0, and inversion off. Each command axis has independent
settings. Camera sensitivity is independently 1. Sensitivity is dimensionless;
mode speed is m/s, angular rates deg/s, jerk m/s³, and acceleration m/s².
These are starting values, not hardware-specific calibration guarantees.

| Mode | Speed m/s | Tilt / brake tilt ° | Pitch/roll expo | Yaw expo | Rate / yaw °/s | Input slew /s | Ascent / descent m/s |
| --- | ---: | --- | ---: | ---: | --- | ---: | --- |
| Legacy Assisted | 20 | 45 / 44 | .35 | .35 | Existing pawn limits | 24 | 4.5 / 4.5 |
| Cinematic | 12 | 30 / 40 | .45 | .40 | 172 / 90 | 12 | 4 / 3 |
| Sport | 24 | 48 / 55 | .25 | .20 | 172 / 130 | 24 | 6 / 4 |
| Training | 6 | 20 / 25 | .40 | .35 | 172 / 60 | 24 | 2.5 / 2 |
| High Performance | 35 | 55 / 60 | .15 | .15 | 220 / 150 | 40 | 8 / 5 |
| Angle | Not velocity controlled | 40 / unused | .25 | .20 | 172 / 120 | 24 | Manual collective |
| Acro | Not velocity controlled | No leveling limit | .20 | .15 | 240 / 150 | 0 (off) | Manual collective |

| Mode | Acceleration / braking caps m/s² | Acceleration / braking jerk m/s³ |
| --- | --- | --- |
| Cinematic | 7 / 9 | 40 / 60 |
| Training | 3 / 4 | 16 / 24 |
| Sport | 30 / 30, additionally limited by tilt | 100 / 160 |
| High Performance | 30 / 30, additionally limited by tilt | 180 / 260 |

Horizontal acceleration is also bounded by `g*tan(tiltLimit)`. The motor allocator
and thrust ramp retain their physical limits. Velocity/braking gains, prediction,
attitude gain, position-hold gain, vertical acceleration, and all table parameters
are configurable per non-legacy mode. Legacy Assisted uses the separate pawn
flight tuning in `Config/DefaultGame.ini`; its mode-table row is read-only. Slew limits normalized command
change, separate from physical actuator response; releases/reversals use the
existing faster release multiplier. Acro's default has no input slew.

### Manual throttle and mode transitions

**Incremental** (default for Angle/Acro): stick-up increases collective by up to
0.35 of full scale per second, down decreases it by up to 0.50 per second, center
holds the latched value. This is a spring-centered gamepad adaptation, not a
conventional non-centering RC transmitter. Collective is shown as a percentage.

**Direct**: calibrated signed input maps through `(input+1)/2` to 0–100% collective.
A centered stick therefore means 50%. Arming requires low throttle; switching into
manual flight requires matching current collective to within 3%. Matching seeds
absolute throttle without slewing from the midpoint. Physical thrust/motor ramping
still applies. Direct vertical processing uses zero base expo; per-axis calibration,
sensitivity, inversion, and any explicit expo offset still apply.

Assisted-to-assisted mode changes preserve velocity, motor state, acceleration
state, and held inputs. Manual entry requires neutral rotation commands plus
centered incremental throttle or matching direct throttle. Entry initializes
collective from current commanded thrust. Manual-to-assisted switching requires
body-up within 60° of upright, captures current altitude/heading, initializes
acceleration from current tilt, and requests neutral inputs. No view changes occur.

Changing the manual layout, throttle interpretation, or calibration requires
disarming. Loading an entire saved profile requires landing and disarming.
Disconnection clears analog inputs, modifiers, auxiliary outputs, and holds.
Assisted flight brakes/holds with no pilot command; manual flight enters a motors-off
failsafe, never a hidden autopilot. Reconnection never arms the aircraft.

### Settings, calibration, and custom profiles

Options/Menu/Esc opens the paused menu. Tab/Triangle/Y changes page; arrows/D-pad
select and adjust; Enter/Cross/A confirms; Esc/Circle/B closes the menu. Five pages:

1. General: layout, mode, collective interpretation/rates, precision, trigger roles,
   camera sensitivity, hold durations, custom slot, save/load, restore defaults.
2. Axes: select pitch/roll/yaw/vertical, physical source, inversion, inner/outer dead
   zones, center/min/max calibration, sensitivity, and expo offset.
3. Mode tuning: 20 independently editable mode parameters with displayed units.
4. Buttons: select an action binding, choose modifier 0/1/2, then confirm and press
   a new button. Hold an optional modifier before capture. Esc cancels capture;
   Circle/B can itself be captured. Duplicate chords and reserved keyboard flight
   axes are rejected. Shoulders remain dedicated modifiers.
5. Controller map: current button functions for the held modifier, highlighted
   pressed controls, and moving raw-stick crosshairs. Binding edits use page 4.

Calibration shows live LX/LY/RX/RY and triggers, processed axes, dead-zone/neutral
state, modifiers, pressed buttons, and auxiliary channels even while physics is
paused. Rest sticks to inspect drift, adjust center and endpoints, then tune dead
zones. Verify both signs of each axis in Training mode. The map uses a schematic
layout and paired Xbox/DualSense names rather than branded controller artwork.

Save/load uses two custom slots, `Saved/Controls/Custom1.ini` and `Custom2.ini`.
They persist calibration, mode tunings, buttons/chords, triggers, throttle settings,
hold times, selected layout, and selected mode. Copy these files to export/import;
invalid values, duplicate axes, or duplicate chords are rejected before application.
Changes are live; explicitly save to retain a custom configuration. Restore defaults
resets component tuning/bindings; it does not silently change the active flight mode.
Keyboard profile cycling also retains the existing pawn Game.ini persistence.


## Camera and display behavior

The body-locked FPV mount position is `bodyTransform * (12,0,8) cm`. Orientation
is normalized `bodyQuaternion * mountQuaternion * uptiltQuaternion`, with 10°
default uptilt and 110° default FOV. It updates post-physics without interpolation
or horizon leveling. Stabilized FPV uses bounded gimbal dynamics and 95° default
FOV. Explicit zoom changes FOV; gimbal pitch affects only stabilized FPV.
Chase camera lag and FPV owner-mesh exclusion remain separate from flight mode.

F11 toggles standalone borderless fullscreen, restores the previous window size,
ignores repeat events and works while the settings menu is paused. Unreal Editor
keeps its normal F11 viewport behavior. On Linux, the handler updates Slate and
SDL maximum-size hints to avoid a work-area/taskbar gap. A live toggle to
1920×1080 was checked; headless automation alone cannot establish window sizing.

Initial graphics migration applies a balanced preset once and retains later saved
preferences. TAA replaces TSR, motion blur is disabled and flight HUD diagnostic
text is formatted at 10 Hz while drawing every frame. Input/controller-map readings
remain live. Lower rendering cost does not change flight speed or simulation time.

## Tuning reference

The current presets target Cinematic 12 m/s, Sport 24 m/s, Training 6 m/s and
High Performance 35 m/s. Legacy targets 20 m/s. These are velocity commands;
forces, drag, motor response, tilt limits and collision still determine motion.
Saved custom profiles retain their own values. Restore defaults in the controls
menu to replace an old custom preset with the new defaults.

- `Source/AerialWorldLab/Private/Flight/FlightModeTuning.cpp`: named mode defaults
  and the single field-range table shared by validation, menu sliders and shortcuts.
- `Source/AerialWorldLab/Public/Flight/FlightModeTuning.h`: serialized tuning fields
  with units. Keep their property names stable for existing profile files.
- `Config/DefaultGame.ini`: legacy flight settings, shared daylight/street sampling,
  and vehicle dimensions, speed, acceleration, look-ahead and timing budgets.
- `QuadrotorDynamics`: portable SI force allocation, motor lag and drag math.
- `FlightControl`: stateless control math; `QuadrotorPawn` applies it to Chaos.
- `PilotControlsComponent`: input calibration, action routing, menu and profiles.
- `DroneCameraControllerComponent`: camera mounts and modes, including body-locked FPV.
- `CitySimulationSettings`: shared world policy; the native importer reads the same
  daylight values used at runtime.
- `CityVehicle`: kinematic obstacle motion with bounded acceleration/swept collision.
- `CityTileActor`: generated static geometry with navigation export disabled.

Mode settings and legacy settings are separate because Legacy preserves its
existing pawn-based configuration. Manual Angle/Acro modes use collective/rates;
the assisted horizontal velocity limits do not act as a manual-mode speed clamp.
No rigid-body velocity teleport or simulation-time multiplier was added.

The old 20 m/s high-performance stopping-distance bound was updated to scale with
v²/(2a), plus an attitude/motor transient allowance. Faster flight needs more road
space. Tests still check settling, no opposite-direction rebound and altitude
retention at 30, 60 and 120 Hz. They do not calibrate a physical aircraft.

`make readability-check` rejects ternaries in first-party C++/Python, rejects
chained C++ assignments and combined scalar declarations, and checks every
first-party C++ header/source (including native tests) against `.clang-format`.
The formatter requires explicit braces and expanded blocks, functions and lambdas.
The pinned clang-format dev dependency is discovered beside the environment's
Python even without activation. Naming, units and algorithms still need human review.

## City environment and asset pipeline

The default native map is `/Game/Maps/NewYorkDaytime`. It uses NVIDIA's externally
authored Architectural Brownstone Lite neighborhood, with streets, detailed
brownstone buildings, a park, trees and street furniture. It is an artistic
New York-style neighborhood, not a surveyed or georeferenced Manhattan replica.
The previous Helsinki scan is no longer the default.

Source and terms are recorded in
`Assets/External/NvidiaBrownstone/SOURCE.txt` and `PACKAGE-LICENSES/`.
The detailed moving car is Khronos CarConcept by Darmstadt Graphics Group /
Eric Chadwick, CC BY 4.0; see `Assets/External/CarConcept/SOURCE.txt`.
Cars follow an authored street circuit with bounded acceleration and collision
stopping. They are kinematic traffic obstacles; this is not a full tire or
traffic-behavior model. Wheels are part of the static authored car mesh.

The environment uses a high sun, neutral daylight, skylight and fixed daytime
camera exposure. Buildings retain directional shadows. Conversion retains the
source exterior layout and base-color textures, but approximates MDL materials
with Unreal PBR base color, matching normal maps and scalar roughness/metallic. It does not reproduce
all NVIDIA RTX materials, translucent interiors or the original offline renderer.

The conversion pipeline batches geometry by material and 64-meter spatial cell,
reduces oversized architectural batches and caps textures at 1024px.
Street trees retain their authored trunks, branches, leaf instances and top
canopies. Species selection for separate park scatter is configurable. Whole leaf patches retain their original
geometry and UVs rather than being collapsed by mesh simplification. Dense conifer and flower scatter and the pink Kalmar furniture set are
omitted. Buildings and trunks have collision; leaves do not block the drone.
The current conversion contains 584 batches and approximately 25.9 million triangles.
Conversion budgets, species selection and spatial cell size are in
`Config/CityImport.json`. Drone dynamics, controller inputs,
body-locked FPV and the standalone F11 fullscreen toggle are preserved.

### Reproduce the native assets

Use the host-specific virtual environment from [installation](#installation-and-commands).
The source pack is approximately 2 GB compressed. Raw inputs, intermediate
conversions and generated native city assets are ignored by Git; do not redistribute
the source pack standalone. Commands use that environment's Python:

```sh
python scripts/project.py prepare-new-york
python scripts/project.py import-new-york
```

`bootstrap-city` restores both city and vehicle on a fresh checkout. Preparation
runs the source steps in order; import builds the editor target and runs the native
commandlet with the required plugin. Select an engine using `UE_ROOT` or
`--ue-root`. Normal builds and gameplay require no downloads once assets exist.
The individual source steps remain available for diagnosis:
`download_new_york.py`, `prepare_new_york.py`, `prepare_detailed_new_york.py`,
`batch_new_york.py`, then `optimize_new_york.py`.

Unreal imports are resumable: unchanged source hashes reuse existing meshes,
changed geometry is reimported, and materials and collision are reassigned. Only
actors tagged `NewYorkBrownstone` are replaced in the map. Source units are centimeters; OBJ import reflects Y into
Unreal, and map actors have unit scale. Physics ground traces place the drone
220 cm above the authored street at launch. Traffic routes are stored in
`Content/CityData/new_york_traffic.json`.

Run `make test` and `make unreal-test`. NullRHI validates geometry loading,
collision and traffic logic; inspect a rendered standalone run to check appearance.
See [validation and performance](#validation-and-performance) for the measured launch-view performance and its limits.

The authored launch metadata places the aircraft on the street; camera mode and
its chase offset determine the initial view. `scripts/finalize_new_york.py` reapplies the launch view and foliage settings
from the small native scene manifest without needing the original USD download.
Run it through the same Unreal Python commandlet after changing those settings.

The city integration regression checks scale, street collision and four cars
traveling through a full circuit over 120 simulated seconds. To export collision
heights when adapting the route to another source layout, run:

```sh
python scripts/project.py unreal-test --test-filter Aether.Flight.CityEnvironment --test-arg=-ExportCityRoadGrid
```

This writes `Saved/CityRoadHeights.csv` from the actual game-world collision scene.
An editor commandlet's world may lack an initialized physics scene and cannot
be used as equivalent evidence of drivable road clearance.


### Pipeline responsibilities and expansion

| Script | Responsibility |
| --- | --- |
| `download_new_york.py` | Download the pinned NVIDIA archive and verify checksum/archive integrity. |
| `prepare_new_york.py` | Open USD, resolve materials/transforms and expand selected authored instances. |
| `prepare_detailed_new_york.py` | Substitute detailed brownstone rows for Lite massing; run after the base export. |
| `city_asset_tools.py` | Shared settings, source filtering, material identity, stable tile names and whole-patch leaf selection. |
| `batch_new_york.py` | Group by spatial cell/material/category and preserve exact source leaf UVs. |
| `optimize_new_york.py` | Reduce oversized architecture, prepare bounded textures and write the small public scene manifest. |
| `import_new_york.py` | Native mesh/material import, source-hash cache, collision, tagged actors, daylight and level save. |
| `finalize_new_york.py` | Reapply launch/foliage settings from the small manifest without the USD download. |
| `prepare_vehicle_asset.py`, `import_vehicle_asset.py` | Download/flatten and natively import the detailed vehicle independently of the city. |
| `download_helsinki_survey.py`, `prepare_survey_assets.py`, `prepare_helsinki_traffic.py`, `import_survey_city.py`, `finalize_survey_level.py` | Optional earlier survey-city workflow; not the default map. |
| `import_osm_city.py`, `import_vehicle_assets.py` | Optional older OSM/procedural/vehicle experiments. |
| `repair_city_assets.py`, `repair_facade_materials.py` | Asset repair tools for earlier curated/procedural art. |
| `project.py` | Cross-platform build, run, bootstrap and validation commands; engine discovery and setup diagnostics. |

Do not run only the detailed-row preparation repeatedly against an already augmented
manifest; run the documented chain from the base export. Geometry selection and
texture limits live in `Config/CityImport.json`. Native import uses the UE 5.2
Interchange pipeline with automatic lightmap UV generation disabled. Vehicle import calls the
Interchange manager directly: UE 5.2's AssetTools glTF warning path can attempt to
open an editor results tab and crash in a headless commandlet. Its local completion
marker is written only after all vehicle packages are saved. It explicitly
assigns surface materials, including a fallback slot if OBJ import supplies none.
Per-batch garbage collection bounds transient import buffers for future larger maps.

Tile names derive from material, category and cell coordinates. Keep cell size stable
while adding neighboring geometry; changing it intentionally rebuilds the grid.
Tree-leaf filtering must retain both direct leaf meshes and the branch/leaf point
instancers: some authored trees split their crown across these layouts. Dense offline
source geometry cannot be removed indiscriminately without changing canopy coverage.

Leaves are two-sided where required, retain original UVs and have no blocking
collision or dynamic leaf shadows. Buildings/trunks retain collision. Foliage uses
a 150 m draw-distance limit. These are rendering choices, not a vegetation growth
or wind simulation. The converted geometry uses centimeter world coordinates at
unit actor scale. Spatial batching does not implement World Partition, streaming,
floating origins or an unlimited world. Those need separate work for major expansion.

The street circuit is a list of world-centimeter route points in
`Content/CityData/new_york_traffic.json`. Adapt it against actual street collision,
not roof heights or visual estimates alone. Street probes are bounded around
expected grade to ignore roofs/crowns. The map currently spans approximately
380×300 m; generation scripts and the scene manifest describe bounds rather than
requiring a fixed tile count in the city test.


## Asset provenance and source control

Keep simulator source, Config/CityImport.json, Config/DefaultGame.ini, dataset
notices/licenses, download checksums, import scripts and small scene/traffic
manifests in Git. Generated native city meshes, imported city levels, downloaded
archives, extracted source packs, prepared OBJ/GLB files and build caches are
ignored. Their binaries are reproducible outputs, not useful code-review diffs.

Existing tracked curated assets (including the F450 drone) remain versioned.
Git ignore rules do not untrack files already committed. Use Git LFS for curated
binary art that cannot be rebuilt, rather than assuming an ignore rule removes
its history. This change does not rewrite repository history or delete local art.

Tile names are derived from material, category and spatial cell coordinates.
Adding another block does not renumber existing tiles. Native import cache hashes
ensure a changed source mesh is reimported rather than silently reusing an old
asset at the same name. Cache data is under Saved/ and is not committed.

The generated native map refers to its active meshes directly. Packaging cooks
those references; obsolete cached city tiles do not need an AlwaysCook directory.
Drone/vehicle assets loaded by code still have explicit cook rules.

Rebuild ignored city assets before launching a fresh checkout. See [city environment and asset pipeline](#city-environment-and-asset-pipeline)
for the preparation/import commands. Keep manually authored, non-reproducible levels outside the two exact
ignored generated level paths; do not ignore all .uasset or .umap files globally.

Use `python scripts/project.py bootstrap-city` with the virtual environment's
Python on a fresh checkout after installing the constrained asset tools. It downloads the pinned city source, prepares geometry and the licensed
vehicle, imports the vehicle, and builds the generated native level. It requires
the existing Unreal installation and approximately 2 GB of city download data.
No source archive or generated city binary needs to be uploaded with the code.

### Default environment attribution

The New York-style brownstone neighborhood is authored by NVIDIA and converted
from the Architectural Brownstone OpenUSD sample pack, AECDemo_NVD@10012.
This is an artistic scene, not a surveyed replica of Manhattan.

Catalog: https://docs.omniverse.nvidia.com/usd/latest/usd_content_samples/downloadable_packs.html
Terms: https://docs.omniverse.nvidia.com/platform/latest/common/NVIDIA_Omniverse_License_Agreement.html
Original package notice is retained at Assets/External/NvidiaBrownstone/PACKAGE-LICENSES/.
NVIDIA content is incorporated into the simulator, not offered as a standalone asset pack.

The detailed CarConcept vehicle is by Darmstadt Graphics Group / Eric Chadwick
(2024), CC BY 4.0, derived from Unity Fan's CC0 concept car. The model's original
Khronos logos retain their applicable trademark terms.
Source: https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/CarConcept
License: https://creativecommons.org/licenses/by/4.0/
Adaptation: scene-node transforms baked into a combined native static mesh;
visual size/orientation fitted to a kinematic collision envelope.

Drone and other retained optional asset notices remain beside their sources in
Assets/External/. Optional Helsinki/OSM/Kenney environments are not the default city.


## Validation and performance

### Verified local checks

The current native build and `make unreal-test` passed with 16 Aether.Flight tests:
AssistedModes, AttitudeDirection, ChaosStability, CityEnvironment, ControlSafety,
DisplaySettings, HighPerformance, ImportedAssets, ManualFlight, PilotProfiles,
ProfessionalControls, ResponseSmoothing, StepAndRelease, VelocityNoRebound,
ViewModes and XboxControls. `make test` passed 39 Python tests plus the portable
C++ dynamics test and no-ternary guard. These are local results, not CI promises.

Coverage includes controller shaping/transitions/profiles, manual/assisted response,
30/60/120 Hz settling, high-performance acceleration/stopping/altitude retention,
map scale, collision and four cars traversing a full circuit over 120 simulated
seconds. Whole-patch leaf/UV retention, source selection and stable material/tile
identity have portable regressions. NullRHI does not validate visual appearance,
controller hardware, Bluetooth latency, photorealism or physical accuracy.

The rebuilt map was also rendered and its filled-out leaf crowns inspected. The
excluded pink decorative set was removed. A rendered view is evidence of that
specific view, not a claim that every material and camera path is perfect. A
DualSense has been detected with live raw readings; Xbox hardware validation is
not established by the automation test named XboxControls.

For lint, the current maintained-source check is:

```sh
python -m ruff check src tests scripts --exclude scripts/repair_city_assets.py
```

The old repair script has pre-existing lint issues and an unrelated local comment
edit; that exclusion is deliberate, not a claim that every historical script is clean.
`git diff --check` checks whitespace. Real hardware, clean-clone restoration,
packaged-game operation and physical calibration remain separate checks.

### Current rendered performance

A warmed capture on 2026-11-27 used UE 5.2 Development, Vulkan SM5, RADV RENOIR
integrated graphics, balanced quality, 1920×1080 borderless fullscreen and the
stationary native third-person launch view. The regression-test process had exited
before capture. The first 120 of 360 frames were discarded, leaving 240 samples.

| Metric | Complete street-tree crowns |
| --- | ---: |
| Median frame time | 26.39 ms (approximately 37.9 FPS) |
| 95th percentile frame time | 33.39 ms |
| Median GPU time | 25.59 ms |
| Median game-thread work | 2.90 ms |
| Median render-thread work | 2.73 ms |

The current full neighborhood contains 584 batches and approximately 25.9 million
triangles; the total-map count is not the number drawn in one frame. This is one
capture, not sustained whole-city performance or a controlled comparison with a
different scene/camera. First launch builds textures, meshes and shaders; wait for
that work to finish before assessing appearance or recording a benchmark.

Current numerical summary: [docs/new-york-foliage-performance-summary.json](docs/new-york-foliage-performance-summary.json).
The local raw capture is `Saved/Profiling/CSV/Profile(20261127_125627).csv`.
Historical summaries are [docs/new-york-performance-summary.json](docs/new-york-performance-summary.json)
and [docs/performance-capture-summary.json](docs/performance-capture-summary.json).
The older New York scene used incomplete crowns, 663 batches/14.2 million triangles
and a different view; its 27.38 ms median does not benchmark this revision. The
retired synthetic scene's 159.56→20.68 ms comparison at 1280×720 included quality
reductions and is not a result for the current map. Old facade batching/culling
settings do not apply to the imported city.

### Capture procedure

1. Launch the native map, wait for texture/shader/mesh caches to finish, and choose
   a fixed camera, resolution and graphics preset.
2. With the Unreal console, enter `CSVProfile FRAMES=360`. GPU statistics can be
   enabled with the launch argument `-csvGpuStats`.
3. Summarize the CSV, retaining the same warmup policy for comparisons:

```sh
python scripts/summarize_frame_capture.py 'Saved/Profiling/CSV/Profile(...).csv'
```

`make profile` starts capture at launch, so an uncached first run is not a warmed
benchmark. Repeat comparisons only with matching scene, view, settings and resolution.

## Troubleshooting

| Symptom | Check or correction |
| --- | --- |
| Native city is missing after checkout | Install the constrained asset extras and run `python scripts/project.py bootstrap-city` with the environment's Python; generated levels/meshes are intentionally ignored. |
| Engine executable not found | Run `doctor`; set `UE_ROOT` or `--ue-root` to the directory containing `Engine/Build/Build.version`. |
| Pip reports no matching wheel / native module import fails | Use 64-bit Python 3.12 and matching architecture. Recreate `.venv` for that architecture; do not install project asset extras into Unreal's embedded Python. |
| Windows cannot find `cl.exe` | Use Developer PowerShell for VS 2022 with its C++ workload/SDK installed. |
| macOS rejects SDK/Xcode | Select the UE-5.2-compatible full Xcode installation and complete first-launch setup; a newer incompatible SDK is not fixed by changing project paths. |
| Large import is slow or runs out of memory/storage | Check the resource budget, logs and free disk; per-batch import garbage collection reduces transient mesh buffers. Keep completed imports/cache for retries. |
| Gray checker/fallback materials at first launch | Check pending texture/shader work and logs before changing material bindings. Persistent issues require checking native slots and texture references. |
| Bare/sparse tree crowns after pipeline edits | Retain direct leaves and branch/leaf instances; preserve whole leaf patches and UVs. Do not apply architecture QEM/nearest-UV transfer to thin foliage. |
| Drone does not respond to flight sticks | Close paused settings, allow it to settle, arm deliberately and request assisted takeoff; inspect the HUD inhibition reason. |
| Faster defaults are absent | Existing custom profiles preserve saved values. Restore defaults in controls, then explicitly save if wanted. |
| Controller axes or directions are wrong | Confirm OS/SDL recognition, input profile, calibration and per-axis inversion. Avoid doubling per-axis inversion with legacy pawn flags. |
| Cars stop unexpectedly after route edits | Export `Saved/CityRoadHeights.csv` with the city test; inspect curbs, barriers, grade and swept collision clearance. |
| Changed source tile is not visible | Regenerate the complete source/batch/optimization chain and import; the cache compares source hashes. |
| Low FPS | Compare warmed timings at the same view/resolution, try `make play-low`, and inspect GPU time before changing physics. |
| Fullscreen dimensions are wrong | Test standalone F11 and saved display settings; Editor F11 is a separate viewport behavior. |
| Optional import fails | Run its commandlet with the required plugin enabled, inspect its log and confirm assets exist before altering the default map. |

## Development agreements

The C++ aircraft tick is an ordered pipeline: update raw/held input, resolve limits,
smooth commands, sample body state, apply drag, evaluate assisted or manual control,
then allocate and apply rotor forces. Helpers retain the original calculation order,
SI units, mode defaults and serialized property names. `FQuadrotorControlFrame`
contains transient values for one tick; it is not another configuration store.

Use `SimulationUnits.h` for engine-boundary conversions and `PilotInput.h` for
named physical/logical axis indices. Menu edits and labels share named indices
from `PilotMenuLayout.h`. Actual tunable values remain in the existing reflected
properties, `Config/DefaultGame.ini`, mode factory/field table and city-import JSON.
The refactoring does not introduce new flight modes or physical calibration.


Use `Development`, preserve unrelated edits and keep changes within the current
authorized scope. Simulator development currently covers physics, city environments
and maintainability; AI remains future work. Preserve controller mappings, flight
modes and body-locked FPV.

Use explicit if/else control flow without ternaries or compressed multi-statement
lines. Prefer descriptive names and comments explaining units, frames, assumptions
and algorithms. Do not restyle generated/third-party code. Use `.clang-format` and
keep Unreal generated headers last in include lists. The readability guard checks
syntax; it cannot replace human review of naming and maintainability.

Chaos owns native integration/collision. Use SI internally and the documented
force/torque conversions only at the engine boundary. The Python backend has
different assumptions and is not proof of Unreal accuracy. Run `make test` for
portable code and `make build`/`make unreal-test` for native changes. Inspect a real
rendered run for graphics or fullscreen work. Do not claim physical calibration
without measured data or photorealism without visual inspection. Preserve asset
provenance/license notices and distinguish research plans from working features.

Before a development commit, review the selected source/config/manifests/notices,
keep ignored downloads/generated tiles/caches out, and inspect curated binary-art
changes separately. Ignore rules do not remove already tracked art. No commit,
staging update, history rewrite or push is performed by documentation consolidation.

## Research objectives and roadmap

The following retained project context describes intended research and priorities,
not an instruction to implement every phase now. In addition to that context, the
current architectural preference is an editable PyTorch system combining world
models, YOLO-style semantic perception and vision-transformer representations.
Weights, biases, layer/neuron sizes, attention, losses, backpropagation and optimizer
settings should remain inspectable and tunable in code.

Approximate Bayesian inference should primarily target the action-conditioned,
short-horizon latent-state predictor rather than making the whole network Bayesian.
Predict distributions over possible future states from observation history, sensor
updates and proposed actions. Distinguish aleatoric uncertainty (sensor noise and
inherent motion variability) from epistemic uncertainty (limited data/unfamiliar
environments), and evaluate calibration and runtime cost experimentally. The eventual
planner should use those predictions for uncertainty-aware feasible trajectories.
None of this neural architecture, inference or planning is implemented by the city work.

Low-level physical flight control remains independent of experimental AI. The
original full project objectives follow so consolidation does not discard design
intent or experimental expectations.

### 1. Project Overview

I am developing AerialWorldLab, also referred to as Project Aether, an independent artificial intelligence, autonomous robotics, and flight simulation research project.

The project consists of three primary interconnected components:

1. A visually realistic drone simulator developed in Unreal Engine 5.2 using C++ and custom quadrotor flight dynamics.
2. A physical quadrotor drone that I will construct and use for real-world experimentation, data collection, and simulation validation.
3. A custom neural network architecture based on predictive world models, designed to investigate autonomous aerial navigation, environmental understanding, future-state prediction, and simulation-to-reality transfer.

These are not three independent projects. They are components of one research and engineering system.

The simulator provides a controlled environment for training and experimentation. The physical drone provides real-world measurements and deployment opportunities. The neural network serves as the experimental intelligence system connecting the two environments.

Do not interpret this project as merely a drone video game, a conventional flight simulator, or a basic object detection application.

However, do not exaggerate its capabilities. Many of the following objectives are planned research and engineering milestones, not completed functionality.

### 2. Visually Realistic Simulation

The simulator must maintain a reasonable degree of graphical realism. Graphical quality is an explicit project requirement, both for the manual flight experience and for generating visually useful training environments for computer vision.

I do not want a simulator consisting entirely of primitive cubes, empty landscapes, placeholder buildings, or environments that look nothing like the real world.

The simulator should eventually include:

- Realistically proportioned urban buildings with varied architectural styles.
- Streets, intersections, sidewalks, parking lots, and other urban infrastructure.
- Realistic vehicles with physically plausible movement.
- Trees, vegetation, parks, and natural terrain.
- Industrial, suburban, residential, and commercial environments.
- Complex obstacles and confined spaces.
- Realistic building materials, textures, reflections, and shadows.
- Different times of day and lighting conditions.
- Environmental variation, including fog, rain, wind, and changing visibility where technically practical.
- Realistic drone models with appropriately animated propellers.
- Correct environmental scale and collision geometry.

Use Unreal Engine's existing rendering capabilities, including physically based materials, suitable lighting systems, realistic assets, and appropriate environmental design.

The target is a convincing, recognizable representation of the real world, not necessarily cinematic photorealism or AAA-level production quality.

Do not sacrifice simulation performance, sensor accuracy, or training throughput for unnecessary graphical effects.

Provide configurable graphical settings so that I can use:

- High-quality rendering for manual flight, demonstrations, and realistic visual data collection.
- Optimized rendering for routine simulation and autonomous testing.
- Reduced-rendering or headless configurations for computationally intensive experiments where appropriate.

Graphical realism alone does not guarantee successful simulation-to-reality transfer. Lighting variation, camera calibration, sensor noise, environmental diversity, and differences between synthetic and real data must also be considered.

### 3. Drone Flight Simulator

The simulator is being developed using Unreal Engine 5.2 and C++.

The existing project has a quadrotor simulation foundation using an F450-style drone configuration, Unreal Chaos rigid-body physics, custom motor force allocation, flight control systems, controller inputs, and multiple camera modes.

The exact implementation status must be verified from the current repository rather than assumed.

The intended flight dynamics system should include:

- Six-degree-of-freedom rigid-body dynamics.
- Translational and rotational motion.
- Independent rotor thrust and reaction torque.
- Motor response characteristics and actuator lag.
- Physically meaningful mass and inertia.
- Gravity and aerodynamic drag.
- Angular damping.
- Wind disturbances.
- Motor saturation.
- Ground effect, where useful and experimentally justified.
- Configurable motor and propeller characteristics.
- Appropriate collision behavior.
- Battery and power consumption modeling where sufficient data are available.
- Fixed-step or appropriately controlled physics integration.

I intend to develop custom quadrotor dynamics rather than relying on generic Unreal Engine vehicle movement behavior.

This does not necessarily mean replacing Unreal Chaos with an entirely independent general-purpose physics engine. Custom flight forces, aerodynamic models, actuator models, and control algorithms can operate alongside Unreal's existing rigid-body and collision systems.

If developing a separate numerical flight dynamics implementation becomes useful for headless training or scientific validation, it should share physical parameters and behavioral assumptions with the Unreal simulation.

Avoid maintaining multiple inconsistent flight physics implementations.

### 4. Manual Flight Experience

The simulator should remain usable as a realistic manual drone simulator, even when the AI systems are disabled.

Existing controls and camera functionality should not be unnecessarily removed or rewritten.

The intended manual flight experience includes:

- PS5 DualSense controller support.
- Professional Mode 2 controls.
- Precise and configurable input sensitivity.
- Appropriate dead zones and response curves.
- Manual and assisted flight configurations.
- Stabilized flight modes.
- Body-locked FPV.
- Other existing FPV and third-person camera modes.
- Real-time flight telemetry.
- Responsive and predictable flight behavior.

The body-locked FPV behavior previously present in the main repository is important and should be preserved or restored if necessary.

The simulator should eventually support manual flight, AI-assisted flight, and autonomous flight without requiring entirely separate applications.

Manual flights should also be recordable for analysis, demonstration data, and potential imitation learning experiments.

### 5. Physical Drone Development

I intend to construct an actual physical quadrotor drone to complement the simulator.

The physical drone is intended to serve as a research instrument rather than simply a recreational flying vehicle.

Its eventual hardware configuration may include:

- A quadrotor frame.
- Motors, propellers, ESCs, and a suitable battery.
- An established flight controller.
- An onboard companion computer capable of neural network inference.
- RGB cameras.
- An inertial measurement unit.
- GPS or other localization systems.
- Depth sensing or LiDAR if justified by the experimental requirements and available budget.
- Telemetry and data recording systems.
- Communication interfaces between onboard systems.

Specific hardware selections have not yet been finalized. Do not assume unlimited budget, power, computational capacity, or payload capacity.

The physical drone and its simulated counterpart should share equivalent configurations wherever practical, including:

- Frame dimensions.
- Mass distribution.
- Motor characteristics.
- Propeller characteristics.
- Sensor positions and orientations.
- Flight controller behavior.
- Maximum acceleration and angular velocity.
- Camera intrinsics and field of view.

The simulator must be calibrated using measurements from the physical drone.

For flight safety, established low-level flight control systems should remain independent of experimental neural network components. The AI should initially generate bounded high-level navigation or trajectory commands rather than directly controlling individual motors.

Physical experiments should occur in controlled, authorized environments with appropriate emergency intervention, operating boundaries, and independent safety mechanisms. Unrestricted autonomous operation in populated cities is a long-term research aspiration, not an initial deployment requirement.

### 6. Custom Neural World Model Architecture

The main AI research objective is to design, implement, and evaluate a custom neural network architecture based on learned world models.

I am not satisfied with simply installing an existing object detection model and calling the project autonomous artificial intelligence.

Existing architectures may be used as components, references, or experimental baselines. However, I want to investigate a meaningful architectural modification or an independently designed predictive system.

The network should eventually be capable of learning an internal representation of the drone and its surroundings from sequential observations.

A possible high-level architecture consists of the following modules. These are proposed directions, not finalized architectural decisions.

#### A. Multimodal Observation Encoder

Process relevant observations from:

- RGB camera frames.
- Depth measurements.
- IMU data.
- Drone velocity and orientation.
- Position estimates.
- Previous control inputs.
- Other available sensor measurements.

Investigate convolutional neural networks, vision transformers, or other suitable encoders based on their computational requirements and experimental performance.

The architecture must be compatible with the limitations of onboard computing hardware.

#### B. Semantic Perception

Develop environmental perception capabilities that can identify relevant objects and structures, including:

- Buildings.
- Cars and other vehicles.
- Roads.
- Trees and vegetation.
- Pedestrians.
- Static obstacles.
- Moving obstacles.
- Other relevant environmental structures.

Potential methods include object detection, semantic segmentation, instance segmentation, depth estimation, and object tracking.

Do not confuse object recognition with environmental understanding. Identifying a building in an image does not automatically provide its geometry, distance, occupancy, or relationship to surrounding objects.

The perception system should eventually combine semantic information with spatial and temporal information.

#### C. Spatial and Temporal Representation

The network should maintain an internal representation of relevant environmental information across time.

Potential approaches include:

- Recurrent neural networks.
- Transformers.
- Recurrent state-space models.
- Latent spatial representations.
- Occupancy representations.
- Object-centric representations.
- Learned three-dimensional representations.

Investigate these approaches before selecting an architecture.

The model should account for partially observable environments, temporary occlusion, sensor noise, and the fact that a drone cannot observe its entire surroundings simultaneously.

#### D. Predictive World Model

The predictive world model is intended to estimate future environmental and drone states given current observations, previous observations, and possible actions.

The model should investigate the prediction of:

- Future drone position.
- Future drone orientation and velocity.
- Nearby object movement.
- Dynamic obstacle trajectories.
- Future spatial occupancy.
- Potential collisions.
- Consequences of proposed drone actions.
- Relevant environmental changes.

The world model should be action-conditioned where appropriate, allowing it to evaluate the potential consequences of different control decisions.

A world model is not an omniscient representation of reality. It cannot accurately predict everything in a city or know the future behavior of every object.

The research objective is to predict relevant observable or inferable states over a limited time horizon, with measurable prediction accuracy and uncertainty.

Initially, prioritize compact latent-state or occupancy prediction over computationally expensive photorealistic future-video generation, unless video prediction becomes central to a specific research question.

#### E. Uncertainty Estimation

Investigate methods for quantifying uncertainty in model predictions.

Potential approaches include:

- Probabilistic neural networks.
- Bayesian approximations.
- Deep ensembles.
- Probabilistic latent representations.
- Predictive distributions.

Uncertainty estimation may help distinguish reliable predictions from uncertain or unfamiliar environmental conditions.

This is a potential research direction, not a mandatory requirement that every Bayesian technique be implemented.

#### F. Planning and Decision-Making

The drone should eventually use its learned environmental representation to evaluate potential navigation trajectories.

Relevant objectives include:

- Reaching specified destinations.
- Avoiding obstacles.
- Minimizing travel time.
- Minimizing unnecessary energy consumption.
- Maintaining physically feasible trajectories.
- Reducing collision risk.
- Accounting for environmental uncertainty.

Investigate model predictive control, model-based reinforcement learning, and established planning algorithms.

The planner should operate over a finite prediction horizon and update its decisions when new observations become available.

Do not claim globally optimal navigation unless mathematical guarantees or experimental evidence support that claim. The realistic objective is to find effective, feasible, and comparatively efficient trajectories under defined constraints.

### 7. Real-Time Environmental Understanding

The eventual system should allow the drone to navigate complex environments while processing observations in real time.

For example, in a controlled urban-like environment, the drone should eventually be capable of identifying nearby buildings and vehicles, estimating their spatial relationships, tracking relevant movement, predicting potential future conflicts, and updating its intended trajectory.

This requires multiple interconnected capabilities:

- Localization.
- Sensor fusion.
- Object detection.
- Object tracking.
- Spatial mapping.
- Temporal prediction.
- Trajectory planning.
- Flight control.

These capabilities must work together rather than existing as disconnected demonstration features.

The AI should not receive privileged access to Unreal Engine actor locations, exact obstacle coordinates, or other information that would be unavailable to the physical drone during deployment.

Simulation ground truth may be used for training labels, experimental evaluation, and explicitly designated oracle baselines. It should not silently become an input to the deployed perception system.

### 8. Simulation-to-Reality Transfer

A major research objective is to investigate how effectively neural networks trained using simulated environments can transfer to real-world drone operation.

The intended workflow is:

1. Develop and calibrate the simulation.
2. Generate simulated flight and sensor datasets.
3. Train initial neural network models.
4. Evaluate the models in previously unseen simulated environments.
5. Collect measurements and observations using the physical drone.
6. Compare simulated and physical behavior.
7. Identify discrepancies in dynamics, visual observations, and sensor measurements.
8. Improve simulator parameters using measured physical data.
9. Fine-tune or adapt the neural network using real-world observations.
10. Evaluate the resulting models on held-out physical experiments.

Investigate methods such as:

- Domain randomization.
- Domain adaptation.
- Transfer learning.
- Self-supervised learning.
- System identification.
- Continual learning.
- Model-based reinforcement learning.

Initially, real-world data should be collected and used for controlled offline training and model refinement.

Unsupervised modification of flight-critical policies during physical flight should not be treated as an initial requirement. Online adaptation may become a later research direction once its stability and safety can be evaluated.

The simulator should be able to generate synchronized datasets containing relevant observations, control inputs, timestamps, drone states, and available ground-truth labels.

Data formats should be consistent between simulated and physical experiments wherever possible.

### 9. Scientific Research Objectives

The project should produce measurable experimental findings rather than relying solely on demonstrations or impressive-looking visualizations.

Three primary research investigations are currently envisioned.

#### Experiment A: Flight Dynamics Validation

Research question:

How accurately does the custom quadrotor simulation reproduce the behavior of a corresponding physical drone?

Potential measurements:

- Position and velocity errors.
- Attitude response.
- Acceleration.
- Angular velocity.
- Trajectory tracking error.
- Motor response.
- Hover stability.
- Response to controlled disturbances.
- Energy consumption, if accurately measurable.

Use controlled, repeatable experiments and appropriate measurement equipment.

#### Experiment B: Predictive World Model Evaluation

Research question:

Does the proposed neural world model improve environmental prediction or autonomous navigation compared with simpler established approaches?

Potential baselines include:

- Reactive obstacle avoidance.
- Classical trajectory planning.
- Constant-velocity object prediction.
- Conventional recurrent predictive networks.
- Existing learned dynamics models.

Potential evaluation metrics include:

- Future-state prediction error.
- Object trajectory prediction error.
- Occupancy prediction accuracy.
- Collision frequency.
- Navigation success rate.
- Trajectory efficiency.
- Inference latency.
- Computational resource consumption.
- Uncertainty calibration.

Use ablation studies to determine whether individual architectural components actually contribute to performance.

#### Experiment C: Sim-to-Real Generalization

Research question:

How does simulation-based training affect model performance in previously unseen physical environments?

Potential experimental groups:

- Models trained only on simulated data.
- Models trained only on real-world data.
- Models pretrained in simulation and fine-tuned using real-world data.
- Models using additional adaptation techniques.

Control for dataset size, training resources, environmental complexity, and other relevant variables when comparing approaches.

Avoid training-test contamination. Separate data by environments, flight sessions, or other appropriate independent experimental units rather than randomly splitting highly correlated consecutive frames.

Report negative findings and limitations as well as successful results.

### 10. Software Architecture and Engineering Standards

The project should use a modular architecture that separates:

- Unreal Engine environment rendering.
- Drone physics.
- Flight control.
- Sensor simulation.
- Input and camera systems.
- Data recording.
- Neural network training.
- Neural network inference.
- State estimation.
- Autonomous planning.
- Physical drone communication.
- Experimental evaluation.

Unreal Engine and C++ should remain the main foundation for the interactive simulation.

Python and PyTorch are suitable candidates for neural network research, training, and experimental analysis.

Investigate an appropriate communication interface between simulation, training software, and physical hardware. Do not introduce unnecessary middleware or distributed systems without a clear reason.

Code must be:

- Maintainable.
- Modular.
- Readable.
- Efficient.
- Documented.
- Testable.
- Reproducible.

Do not create excessive abstractions or massive architectural rewrites merely to make the repository appear sophisticated.

Use clear naming conventions and understandable mathematical implementations.

Preserve existing functionality unless there is a demonstrated reason to change it.

### 11. Realistic Development Roadmap

Do not attempt to implement the entire project simultaneously.

#### Phase 1: Establish the Simulation Foundation

- Inspect and document the existing repository.
- Correct flight dynamics and control issues.
- Preserve functional controller and camera systems.
- Improve the visual environment.
- Establish realistic physical scale.
- Implement synchronized telemetry and repeatable flight tests.

#### Phase 2: Physical Drone Foundation

- Select compatible hardware within realistic budget constraints.
- Construct and validate the physical drone.
- Establish reliable manual and stabilized flight.
- Collect physical flight measurements.
- Calibrate simulation parameters.

#### Phase 3: Sensor and Data Infrastructure

- Implement simulated cameras and necessary virtual sensors.
- Establish corresponding physical sensors.
- Create consistent data formats.
- Record synchronized observations, actions, and state information.
- Establish experimental datasets and evaluation procedures.

#### Phase 4: Computer Vision and Navigation Baselines

- Implement environmental perception.
- Evaluate object detection and tracking.
- Establish localization and obstacle representations.
- Implement a conventional navigation baseline.
- Validate performance in controlled simulated environments.

#### Phase 5: Initial Neural World Model

- Define a narrow predictive research question.
- Select an initial architecture.
- Train the model using simulated data.
- Evaluate short-horizon prediction accuracy.
- Compare against simpler baselines.
- Conduct architectural ablation experiments.

#### Phase 6: Predictive Autonomous Navigation

- Connect the world model to a constrained planning system.
- Evaluate navigation performance in simulation.
- Test unseen environments.
- Measure computational requirements and inference latency.
- Investigate uncertainty-aware planning where justified.

#### Phase 7: Physical Deployment and Sim-to-Real Evaluation

- Deploy experimentally validated components on physical hardware.
- Collect real-world observations.
- Evaluate transfer performance in controlled environments.
- Fine-tune models using real-world data.
- Compare simulated and physical results.
- Document limitations and failure cases.

#### Phase 8: Expanded Research

Only after the preceding foundations are functional should the project expand into more complex environments, longer prediction horizons, improved multimodal architectures, continual adaptation, or additional vehicle types.

### 12. Graphical Quality and Research Performance

Do not treat graphics and research functionality as mutually exclusive.

I want the simulator to look convincingly realistic while still being technically useful for AI research.

The appropriate solution is configurable visual fidelity, efficient scene construction, realistic asset selection, optimized rendering, and separation between interactive simulation and accelerated training workflows where practical.

The simulator should not resemble an unfinished gray-box prototype in its intended demonstration mode.

However, do not spend disproportionate amounts of development time constructing enormous cities, cinematic effects, or decorative environments before establishing the underlying physics and research infrastructure.

A smaller, visually convincing, experimentally useful urban environment is preferable to a massive but poorly constructed open world.

### 13. Expectations for the LLM Assisting Me

When assisting with this project:

- Be technically critical and realistic.
- Do not flatter me or exaggerate the significance of the project.
- Do not claim the project is revolutionary, groundbreaking, or scientifically novel without evidence.
- Distinguish existing implementations from planned functionality.
- Verify repository contents before making statements about implementation status.
- Identify incorrect mathematical and engineering assumptions.
- Explain important architectural trade-offs.
- Recommend simpler approaches when they provide equivalent research value.
- Consider hardware limitations, computational costs, development time, and feasibility.
- Do not invent experimental results, benchmarks, or scientific findings.
- Do not treat existing AI-generated documentation as proof that functionality works.
- Avoid unnecessary changes to functional code.
- Embed technical recommendations directly into implementation plans.
- Prioritize measurable results and functioning systems over impressive terminology.
- Clearly communicate which capabilities are realistic in the short term and which remain long-term research objectives.

I am an independent student developer, not a fully funded industrial robotics laboratory. The project must be scoped accordingly.

A smaller working system with a defensible research contribution is preferable to an enormous system consisting of incomplete components.

### 14. Final Project Definition

AerialWorldLab is an independent sim-to-real autonomous aerial robotics research platform combining a visually realistic Unreal Engine drone simulator, custom quadrotor flight dynamics, a corresponding physical drone, and an experimental predictive neural world model.

Its central objective is to investigate how learned environmental representations and action-conditioned future-state predictions can support autonomous aerial navigation in complex environments, while experimentally evaluating differences between simulated and physical flight.

The intended outcome is a functioning experimental platform, a validated simulation foundation, a physical hardware demonstration, a documented neural network architecture, and reproducible research findings.

The project's scientific contribution must ultimately be determined by experimental evidence, comparisons with established methods, and an examination of existing research, rather than by the ambition of its proposed features alone.

Treat this research reference as the overarching project context for future technical discussions and implementation decisions. Do not interpret it as an instruction to implement every proposed capability immediately.
