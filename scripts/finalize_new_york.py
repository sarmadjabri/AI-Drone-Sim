"""Apply native city presentation settings without reimporting its geometry.

This uses the small scene manifest kept with the project, not the original USD
source pack. It is useful after adjusting the launch view or foliage budgets.
"""

import json
from pathlib import Path

import unreal

project = Path(unreal.Paths.project_dir())
scene = json.loads((project / "Content/CityData/new_york_scene.json").read_text())
batches = {batch["name"]: batch for batch in scene["batches"]}
if not unreal.EditorLevelLibrary.load_level(scene["map_path"]):
    raise RuntimeError("New York daytime map is missing")
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_actor_label() == "NewYork_DroneLaunch":
        actor.set_actor_location(unreal.Vector(*scene["launch_position_cm"]), False, False)
        actor.set_actor_rotation(unreal.Rotator(0, scene["launch_yaw_degrees"], 0), False)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component or not component.static_mesh:
        continue
    description = batches.get(component.static_mesh.get_name())
    if not description:
        continue
    component.set_cast_shadow(description.get("cast_shadow", True))
    if not description.get("collision", True):
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    if description.get("foliage", False):
        component.set_editor_property("ld_max_draw_distance", 15000.0)
unreal.EditorLevelLibrary.save_current_level()
unreal.log_warning("New York launch view and foliage settings saved")
