"""Optional Panda3D viewer for the physical quadrotor simulation."""

import math
import warnings

import numpy as np

try:
    import pygame
except ImportError:  # pragma: no cover - optional runtime dependency
    pygame = None

from .simulation import Simulation


def run_viewer(simulation, duration):
    """Run the simulation in a real-time 3D window."""
    try:
        from direct.gui.OnscreenText import OnscreenText
        from direct.showbase.ShowBase import ShowBase
        from direct.showbase.ShowBaseGlobal import globalClock
        from panda3d.core import (
            AmbientLight,
            DirectionalLight,
            LineSegs,
            Quat,
            Vec3,
            WindowProperties,
        )
    except ImportError as exc:
        raise RuntimeError(
            "The 3D viewer requires Panda3D; install it with `pip install '.[viewer]'`."
        ) from exc

    class AetherViewer(ShowBase):
        def __init__(self):
            super().__init__()
            self.disableMouse()
            self.setBackgroundColor(0.055, 0.075, 0.11, 1)
            self.setFrameRateMeter(True)
            properties = WindowProperties()
            properties.setTitle("Project AETHER — Quadrotor Simulator")
            if hasattr(self.win, "requestProperties"):
                self.win.requestProperties(properties)

            ambient = AmbientLight("ambient")
            ambient.setColor((0.55, 0.58, 0.66, 1))
            self.render.setLight(self.render.attachNewNode(ambient))
            sun = DirectionalLight("sun")
            sun.setColor((0.9, 0.88, 0.78, 1))
            sun_node = self.render.attachNewNode(sun)
            sun_node.setHpr(-35, -55, 0)
            self.render.setLight(sun_node)

            self.drone_model = self._make_drone()
            self.obstacle_nodes = [
                self._make_obstacle(item) for item in simulation.scenario.obstacles
            ]
            self.target_model = self._make_sphere((0.15, 0.95, 0.55, 1), 0.22)
            self.target_model.setPos(*simulation.target)
            self.path_node = None
            self.last_path_length = 0
            self.paused = False
            self.manual = False
            self.elapsed = 0.0
            self.max_duration = duration
            self.time_scale = 1.0
            self.camera_heading = -35.0
            self.camera_pitch = 24.0
            self.camera_distance = 11.0
            self.manual_target = simulation.drone.state.position.copy()
            self.keys = {key: False for key in ("w", "a", "s", "d", "r", "f")}
            self.controller = self._init_controller()
            for key in self.keys:
                self.accept(key, self._set_key, [key, True])
                self.accept(f"{key}-up", self._set_key, [key, False])
            self.accept("escape", self.userExit)
            self.accept("f11", self._toggle_fullscreen)
            self.accept("alt-enter", self._toggle_fullscreen)
            self.accept("space", self._toggle_pause)
            self.accept("m", self._toggle_manual)
            self.accept("backspace", self._reset)
            self.accept("arrow_left", self._camera, [-8, 0])
            self.accept("arrow_right", self._camera, [8, 0])
            self.accept("arrow_up", self._camera, [0, -5])
            self.accept("arrow_down", self._camera, [0, 5])
            self.accept("1", self._set_time_scale, [0.25])
            self.accept("2", self._set_time_scale, [0.5])
            self.accept("3", self._set_time_scale, [1.0])
            self.accept("4", self._set_time_scale, [2.0])
            self.accept("5", self._set_time_scale, [4.0])
            self._make_ground()
            self.telemetry = OnscreenText(
                text="",
                pos=(-1.31, 0.93),
                scale=0.045,
                align=0,
                mayChange=True,
                fg=(0.9, 0.95, 1, 1),
                shadow=(0, 0, 0, 0.8),
                parent=self.aspect2d,
            )
            OnscreenText(
                text="WASD + R/F: manual target  |  M: auto/manual  |  Space: pause"
                "  |  Arrows: orbit  |  1-5: time scale  |  F11: fullscreen"
                "  |  Backspace: reset",
                pos=(0, -0.94),
                scale=0.038,
                align=0,
                mayChange=False,
                fg=(0.78, 0.84, 0.93, 1),
                shadow=(0, 0, 0, 0.8),
                parent=self.aspect2d,
            )
            self.taskMgr.add(self._tick, "simulation")

        def _make_box(self, center, size, color):
            model = self.loader.loadModel("models/misc/rgbCube")
            model.reparentTo(self.render)
            model.setPos(*center)
            model.setScale(*size)
            model.setColor(*color)
            return model

        def _make_sphere(self, color, scale):
            model = self.loader.loadModel("models/misc/sphere")
            model.reparentTo(self.render)
            model.setScale(scale)
            model.setColor(*color)
            return model

        def _make_drone(self):
            root = self.render.attachNewNode("quadrotor")
            body = self._make_box((0, 0, 0), (0.32, 0.24, 0.12), (0.12, 0.74, 0.92, 1))
            body.reparentTo(root)
            for sign in (-1, 1):
                arm_x = self._make_box(
                    (sign * 0.32, 0, 0), (0.46, 0.07, 0.045), (0.77, 0.85, 0.93, 1)
                )
                arm_x.reparentTo(root)
                arm_y = self._make_box(
                    (0, sign * 0.32, 0), (0.07, 0.46, 0.045), (0.77, 0.85, 0.93, 1)
                )
                arm_y.reparentTo(root)
            for x, y in ((0.32, 0), (0, 0.32), (-0.32, 0), (0, -0.32)):
                motor = self._make_sphere((0.94, 0.58, 0.2, 1), 0.12)
                motor.reparentTo(root)
                motor.setPos(x, y, 0.04)
            return root

        def _make_obstacle(self, obstacle):
            size = np.asarray(obstacle["half_size"], dtype=float) * 2
            node = self._make_box(obstacle["center"], size, (0.8, 0.28, 0.23, 1))
            node.setTransparency(1)
            node.setColorScale(1, 1, 1, 0.72)
            return node

        def _make_ground(self):
            ground = self._make_box((0, 0, -0.08), (80, 80, 0.15), (0.14, 0.19, 0.24, 1))
            ground.setColorScale(1, 1, 1, 1)
            for x in range(-20, 21, 2):
                self._draw_line((x, -20, 0), (x, 20, 0), (0.24, 0.31, 0.37, 1), 0.5)
            for y in range(-20, 21, 2):
                self._draw_line((-20, y, 0), (20, y, 0), (0.24, 0.31, 0.37, 1), 0.5)

        def _draw_line(self, start, end, color, thickness):
            lines = LineSegs()
            lines.setThickness(thickness)
            lines.setColor(*color)
            lines.moveTo(*start)
            lines.drawTo(*end)
            self.render.attachNewNode(lines.create())

        def _init_controller(self):
            if pygame is None:
                return None
            try:
                pygame.init()
                pygame.joystick.init()
                if pygame.joystick.get_count() == 0:
                    return None
                joystick = pygame.joystick.Joystick(0)
                joystick.init()
                return joystick
            except pygame.error as exc:
                warnings.warn(
                    f"Could not initialize the game controller; using keyboard input: {exc}",
                    RuntimeWarning,
                    stacklevel=2,
                )
                return None

        def _controller_delta(self):
            if self.controller is None:
                return np.zeros(3)
            try:
                pygame.event.get()
                axes = [self.controller.get_axis(i) for i in range(self.controller.get_numaxes())]
            except pygame.error as exc:
                warnings.warn(
                    f"Game controller disconnected; using keyboard input: {exc}",
                    RuntimeWarning,
                    stacklevel=2,
                )
                self.controller = None
                return np.zeros(3)
            # DualSense mapping for the current position-target controller:
            # left stick x/y -> X/Y motion, right stick y -> Z motion
            if len(axes) > 0:
                horizontal_axis = axes[0]
            else:
                horizontal_axis = 0.0
            x = float(np.clip(horizontal_axis, -1.0, 1.0))
            if len(axes) > 1:
                forward_axis = axes[1]
            else:
                forward_axis = 0.0
            y = float(np.clip(forward_axis, -1.0, 1.0))
            if len(axes) > 3:
                vertical_axis = -axes[3]
            else:
                vertical_axis = 0.0
            z = float(np.clip(vertical_axis, -1.0, 1.0))
            return np.array([x, -y, z], dtype=float) * 2.0

        def _set_key(self, key, value):
            self.keys[key] = value

        def _toggle_pause(self):
            self.paused = not self.paused

        def _toggle_manual(self):
            self.manual = not self.manual
            simulation.manual = self.manual
            if self.manual:
                self.manual_target = simulation.drone.state.position.copy()
                simulation.target = self.manual_target.copy()
                simulation.completed = False
            elif simulation.scenario.waypoints:
                if simulation.waypoint_index < len(simulation.scenario.waypoints):
                    simulation.target = np.asarray(
                        simulation.scenario.waypoints[simulation.waypoint_index], dtype=float
                    )
                else:
                    simulation.completed = True

        def _reset(self):
            nonlocal simulation
            simulation = Simulation(simulation.scenario, simulation.config)
            self.manual_target = simulation.drone.state.position.copy()
            self.target_model.setPos(*simulation.target)
            self.manual = False
            self.paused = False
            self.elapsed = 0.0
            self.last_path_length = 0
            if self.path_node:
                self.path_node.removeNode()
                self.path_node = None

        def _camera(self, heading_delta, pitch_delta):
            self.camera_heading += heading_delta
            self.camera_pitch = float(np.clip(self.camera_pitch + pitch_delta, 5, 80))

        def _set_time_scale(self, scale):
            self.time_scale = scale

        def _toggle_fullscreen(self):
            properties = WindowProperties()
            properties.setFullscreen(not self.win.getProperties().getFullscreen())
            self.win.requestProperties(properties)

        def _update_path(self):
            path = simulation.path
            if len(path) == self.last_path_length or len(path) < 2:
                return
            self.last_path_length = len(path)
            if self.path_node:
                self.path_node.removeNode()
            lines = LineSegs()
            lines.setThickness(3)
            lines.setColor(0.1, 0.85, 1, 0.9)
            lines.moveTo(*path[0])
            for point in path[1:]:
                lines.drawTo(*point)
            self.path_node = self.render.attachNewNode(lines.create())

        def _tick(self, task):
            wall_dt = min(globalClock.getDt(), 0.05)
            if not self.paused and self.elapsed < self.max_duration:
                simulation_dt = wall_dt * self.time_scale
                if self.manual:
                    speed = 2.0 * simulation_dt
                    keyboard_delta = (
                        np.array(
                            [
                                float(self.keys["d"]) - float(self.keys["a"]),
                                float(self.keys["w"]) - float(self.keys["s"]),
                                float(self.keys["r"]) - float(self.keys["f"]),
                            ]
                        )
                        * speed
                    )
                    controller_delta = self._controller_delta() * simulation_dt
                    delta = keyboard_delta + controller_delta
                    if np.any(np.abs(controller_delta) > 0):
                        delta = controller_delta
                    self.manual_target = self.manual_target + delta
                    simulation.target = self.manual_target.copy()
                for _ in range(max(1, round(simulation_dt / simulation.scenario.dt))):
                    if self.elapsed >= self.max_duration or simulation.collision:
                        break
                    simulation.step()
                    self.elapsed = simulation.time

            state = simulation.drone.state
            self.drone_model.setPos(*state.position)
            self.drone_model.setQuat(Quat(*state.quaternion))
            self.target_model.setPos(*simulation.target)
            self._update_path()

            heading = math.radians(self.camera_heading)
            pitch = math.radians(self.camera_pitch)
            focus = state.position
            distance = self.camera_distance
            self.camera.setPos(
                focus[0] + distance * math.cos(pitch) * math.cos(heading),
                focus[1] + distance * math.cos(pitch) * math.sin(heading),
                focus[2] + distance * math.sin(pitch),
            )
            self.camera.lookAt(Vec3(*focus))
            if simulation.collision:
                status = "COLLISION"
            elif simulation.completed:
                status = "MISSION COMPLETE"
            else:
                status = "FLYING"
            if self.manual:
                mode = "MANUAL"
            else:
                mode = "AUTONOMOUS"
            if self.controller is not None:
                controller_status = "controller"
            else:
                controller_status = "keyboard"
            self.telemetry.setText(
                f"{status}  |  {mode}  |  {self.elapsed:5.1f}/{self.max_duration:.1f} s"
                f"  |  {self.time_scale:g}x  |  {controller_status}\n"
                f"position  {state.position[0]:6.2f}, {state.position[1]:6.2f}, "
                f"{state.position[2]:5.2f} m\n"
                f"velocity  {np.linalg.norm(state.velocity):5.2f} m/s"
                f"  |  energy  {state.energy_j:6.0f} J"
            )
            return task.cont

    viewer = AetherViewer()
    viewer.run()
