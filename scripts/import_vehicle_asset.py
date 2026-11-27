"""Import the prepared licensed vehicle without requiring a particular city map.

Use Interchange directly: UE 5.2's AssetTools import path broadcasts warnings to
an editor results tab even in a commandlet, which crashes without a Slate window.
The synchronous manager API avoids that UI path and still uses authored PBR data.
"""

import hashlib
import json
from pathlib import Path

import unreal

project = Path(unreal.Paths.project_dir())
root = "/Game/External/CarConcept"
asset = root + "/SurveyTrafficCar"
source_file = project / "Assets/External/CarConcept/SurveyTrafficCar.glb"
source_hash = hashlib.sha256(source_file.read_bytes()).hexdigest()
cache_path = project / "Saved/AssetImport/vehicle.json"
cache = {}
if cache_path.is_file():
    cache = json.loads(cache_path.read_text())

# A mesh alone is not evidence of a completed multi-asset glTF import: a previous
# crash may have saved the mesh before every texture/material reached disk.
complete = False
if cache.get("source_sha256") == source_hash:
    complete = isinstance(unreal.load_asset(asset), unreal.StaticMesh)
if not complete:
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    meshes = pipeline.get_editor_property("mesh_pipeline")
    meshes.set_editor_property("combine_static_meshes", True)
    meshes.set_editor_property("generate_lightmap_u_vs", False)
    pipeline.get_editor_property("material_pipeline").set_editor_property("import_materials", True)
    parameters = unreal.ImportAssetParameters()
    parameters.set_editor_property("is_automated", True)
    parameters.set_editor_property("override_pipelines", [pipeline])
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    source = unreal.InterchangeManager.create_source_data(str(source_file))
    if not manager.import_asset(root, source, parameters):
        raise RuntimeError("Interchange vehicle import failed; inspect the preceding import log")
    if not isinstance(unreal.load_asset(asset), unreal.StaticMesh):
        raise TypeError("Vehicle import did not produce its combined native mesh")
    if not unreal.EditorAssetLibrary.save_directory(root, only_if_is_dirty=False, recursive=True):
        raise RuntimeError("Vehicle mesh/material/texture packages could not all be saved")
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = cache_path.with_suffix(".tmp")
    temporary.write_text(json.dumps({"source_sha256": source_hash}, indent=2))
    temporary.replace(cache_path)
unreal.log_warning("Native vehicle asset is ready")
