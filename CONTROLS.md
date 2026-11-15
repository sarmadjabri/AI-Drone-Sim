# Project Aether flight controls — DualSense and Xbox

## Start flying

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

## Controller support and layouts

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

## Default buttons

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

## Precision and tuning

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
| Legacy Assisted | 10 | 38 / 44 | .35 | .35 | Existing pawn limits | 24 | 1.5 / 1.5 |
| Cinematic | 4.5 | 20 / 25 | .45 | .40 | 172 / 60 | 8 | 1.5 / 1.5 |
| Sport | 10 | 38 / 44 | .25 | .20 | 172 / 110 | 24 | 1.5 / 1.5 |
| Training | 2.5 | 12 / 18 | .40 | .35 | 172 / 45 | 24 | 1.5 / 1.5 |
| High Performance | 20 | 50 / 55 | .15 | .15 | 220 / 150 | 40 | 4 / 3 |
| Angle | Not velocity controlled | 40 / unused | .25 | .20 | 172 / 120 | 24 | Manual collective |
| Acro | Not velocity controlled | No leveling limit | .20 | .15 | 240 / 150 | 0 (off) | Manual collective |

| Mode | Acceleration / braking caps m/s² | Acceleration / braking jerk m/s³ |
| --- | --- | --- |
| Cinematic | 3 / 4 | 12 / 20 |
| Training | 2 / 3 | 8 / 16 |
| Sport | 30 / 30, additionally limited by tilt | 100 / 160 |
| High Performance | 30 / 30, additionally limited by tilt | 180 / 260 |

Horizontal acceleration is also bounded by `g*tan(tiltLimit)`. The motor allocator
and thrust ramp retain their physical limits. Velocity/braking gains, prediction,
attitude gain, position-hold gain, vertical acceleration, and all table parameters
are configurable per non-legacy mode. Legacy Assisted retains the original pawn
flight tuning; its mode-table row is read-only. Slew limits normalized command
change, separate from physical actuator response; releases/reversals use the
existing faster release multiplier. Acro's default has no input slew.

## Manual throttle and mode transitions

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

## Settings, calibration, and custom profiles

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

## Historical camera reference

Reference: **516063db90b7a1311d969ecdb80ff5a140900418** (`main`, “added full screen toggle”).
Git log, branches, reflog, and historical file comparisons were inspected. The camera
code also matches `61127b3` and the later checkpoint `4e2c3af`; the working tree's
body-locked calculation, mount defaults, setup, and timing already match this
historical implementation. No destructive revert or invented restoration was needed.

Body-locked position is `bodyTransform * (12,0,8) cm`; orientation is normalized
`bodyQuaternion * mountQuaternion * uptiltQuaternion`, with 10° default uptilt and
110° FOV. It updates post-physics, without interpolation or horizon leveling.
Stabilized FPV retains its bounded gimbal dynamics and 95° FOV. Chase camera lag,
FPV owner-mesh exclusion, physics, imported F450 assets, and scenery are preserved.
Explicit zoom can change FOV; gimbal pitch affects only stabilized FPV.

## Validation and remaining limits

Final checks passed: editor build, all 12 Python tests, standalone C++ dynamics
tests, and all 14 Unreal flight tests. See `IMPLEMENTATION_REPORT.md` for the file
list, coverage, measured results, and hardware limitations. Run `make build`, `make test`, and `make unreal-test`.
Unreal automation uses NullRHI and tests 30/60/120 Hz; it does not establish rendered
frame rate, controller latency, or Bluetooth reliability. A DualSense is detected
and live raw input has been observed in the rendered settings screen. Actual flight
button/direction confirmation is still pending; Xbox hardware was not available.

The existing F450/Poly Haven assets are unchanged. Earlier notes about evaluating
City Sample/Megascans, verifying their licenses and UE 5.2 compatibility, and
measuring frame-time/memory before importing assets remain follow-up work outside
this controls change. No engine upgrade or scenery replacement is included.
