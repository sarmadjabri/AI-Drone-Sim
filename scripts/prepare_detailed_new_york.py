"""Replace NVIDIA's Lite massing placeholders with its authored detailed facades.

The main Lite scene supplies landscaping/streets. This source option supplies
complete brownstone rows at their original placements without full interiors.
No buildings are modeled or positioned procedurally by this project.
"""

import json

import numpy as np
import prepare_new_york as converter
from pxr import Usd, UsdGeom

manifest_path = converter.ROOT / "prepared_manifest.json"
manifest = json.loads(manifest_path.read_text())
# Identify the Lite massing exports using the same deterministic geometry keys.
for prim in converter.stage.Traverse():
    if prim.IsA(UsdGeom.Mesh) and "BuildingsMassing" in str(prim.GetPath()):
        converter.export_mesh(prim, np.array(converter.xforms.GetLocalToWorldTransform(prim)).T)
excluded = set(converter.meshes)
converter.meshes.clear()
converter.instances.clear()
converter.materials.clear()
converter.material_cache.clear()
source = converter.STAGE_PATH.parent / "Options/Options_SiteBuildingsDetailed.usd"
converter.stage = Usd.Stage.Open(str(source))
converter.xforms = UsdGeom.XformCache()
for index, prim in enumerate(converter.stage.Traverse(Usd.TraverseInstanceProxies())):
    if not prim.IsA(UsdGeom.Mesh):
        continue
    if UsdGeom.Imageable(prim).ComputeVisibility() == UsdGeom.Tokens.invisible:
        continue
    converter.export_mesh(prim, np.array(converter.xforms.GetLocalToWorldTransform(prim)).T)
    if index % 3000 == 0:
        print(f"Detailed facades: {len(converter.instances)} placements", flush=True)
manifest["meshes"] = [mesh for mesh in manifest["meshes"] if mesh["name"] not in excluded]
manifest["instances"] = [
    instance for instance in manifest["instances"] if instance["mesh"] not in excluded
]
existing = {mesh["name"]: mesh for mesh in manifest["meshes"]}
existing.update(converter.meshes)
manifest["meshes"] = list(existing.values())
manifest["instances"].extend(converter.instances)
manifest["materials"].update(converter.materials)
manifest["detailed_facade_source"] = str(source)
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
print(
    f"Authored detailed scene: {len(manifest['instances'])} placements, {len(existing)} prototypes",
    flush=True,
)
