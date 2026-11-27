"""Repair level-scale metadata and render flags without reimporting the source scan."""

import json
from pathlib import Path

import unreal

manifest = json.loads(
    (
        Path(unreal.Paths.project_dir()) / "Assets/External/HelsinkiSurvey/prepared_manifest.json"
    ).read_text()
)
if not unreal.EditorLevelLibrary.load_level("/Game/Maps/HelsinkiSenateSquare"):
    raise RuntimeError("Missing imported survey level")
count = 0
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if "HelsinkiSurvey" in [str(tag) for tag in actor.tags]:
        actor.set_actor_scale3d(unreal.Vector(100, 100, 100))
        actor.static_mesh_component.set_cast_shadow(False)
        count += 1
    elif actor.get_actor_label() == "SenateSquare_DroneLaunch":
        actor.set_actor_location(unreal.Vector(*manifest["launch_position_cm"]), False, False)
        actor.set_actor_rotation(unreal.Rotator(0, -90, 0), False)
if count != len(manifest["tiles"]):
    raise ValueError("Survey actor count differs from the source manifest")
unreal.EditorLevelLibrary.save_current_level()
unreal.log_warning(f"Survey scale verified for {count} real-city actors")
