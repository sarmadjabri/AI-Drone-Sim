"""Convert NVIDIA's authored brownstone neighborhood to native-importable meshes.

The source is an artistic New York-style environment, not a geospatial NYC scan.
OpenUSD supplies geometry, placements and material bindings. Identical geometry
is exported once and instanced in Unreal. MDL is translated to a documented
base-color/roughness subset; this is not an exact reproduction of NVIDIA RTX.
Source coordinates are centimeters, Z up. OBJ import reflects Y into Unreal.
"""

import hashlib
import json
import re
from pathlib import Path

import numpy as np
from city_asset_tools import include_vegetation_source, is_excluded_source, load_city_settings
from pxr import Usd, UsdGeom, UsdShade
from scipy.spatial.transform import Rotation

settings = load_city_settings()
ROOT = Path("Assets/External/NvidiaBrownstone")
OUTPUT = ROOT / "prepared"
OUTPUT.mkdir(parents=True, exist_ok=True)
STAGE_PATH = next(ROOT.rglob("World_BrownstoneDemopack_Lite*.usd"))
stage = Usd.Stage.Open(str(STAGE_PATH))
xforms = UsdGeom.XformCache()
materials = {}
meshes = {}
instances = []
material_cache = {}
reflection = np.diag([1.0, -1.0, 1.0])


def material_description(material):
    """Resolve authored MDL defaults and USD overrides into portable PBR values."""
    if not material:
        return {
            "color": [0.5, 0.5, 0.5],
            "roughness": 0.8,
            "project_uv": False,
            "uv_scale": [1.0, 1.0],
            "texture": "",
            "metallic": 0.0,
            "opacity": 1.0,
        }
    shader = None
    for child in material.GetPrim().GetChildren():
        if child.IsA(UsdShade.Shader):
            shader = UsdShade.Shader(child)
            break
    if not shader:
        return {
            "color": [0.5, 0.5, 0.5],
            "roughness": 0.8,
            "project_uv": False,
            "uv_scale": [1.0, 1.0],
            "texture": "",
            "metallic": 0.0,
            "opacity": 1.0,
        }
    values = {}
    source_attribute = shader.GetPrim().GetAttribute("info:mdl:sourceAsset")
    source = source_attribute.Get()
    mdl_path = None
    if source and source.resolvedPath:
        mdl_path = Path(source.resolvedPath)
    if mdl_path and mdl_path.exists():
        text = mdl_path.read_text(errors="replace")
        for name, path in re.findall(r'(\w+)\s*:\s*texture_2d\("([^"]+)"', text):
            values[name] = str((mdl_path.parent / path).resolve())
        for name, numbers in re.findall(r"(\w+)\s*:\s*(?:color|float2)\(([^)]*)\)", text):
            try:
                values[name] = [float(number.strip().rstrip("f")) for number in numbers.split(",")]
            except ValueError:
                pass
        for name, number in re.findall(r"(\w+)\s*:\s*(-?[\d.]+)f", text):
            values[name] = float(number)
        for name, value in re.findall(r"(\w+)\s*:\s*(true|false)", text):
            values[name] = value == "true"
    for value in shader.GetInputs():
        data = value.Get()
        if data is None:
            continue
        if hasattr(data, "resolvedPath"):
            if data.resolvedPath:
                data = data.resolvedPath
            else:
                continue
        elif not isinstance(data, (str, float, int, bool)):
            data = list(data)
        values[value.GetBaseName()] = data
    color = values.get("diffuse_color_constant", values.get("paint_color", [0.5, 0.5, 0.5]))
    texture = values.get("diffuse_texture", "")
    if texture and not Path(texture).is_file():
        texture = ""
    if texture:
        # Texture replaces diffuse constant in OmniPBR; tint remains multiplicative.
        color = values.get("diffuse_tint", [1.0, 1.0, 1.0])
        brightness = values.get("albedo_brightness", 1.0)
        color = [component * brightness for component in color]
    description = {
        "color": color,
        "texture": texture,
        "roughness": values.get(
            "reflection_roughness_constant", values.get("paint_roughness", 0.8)
        ),
        "metallic": values.get("metallic_constant", 0.0),
        "project_uv": values.get("project_uvw", False),
        "uv_scale": values.get("texture_scale", [1.0, 1.0]),
        "opacity": 1.0,
        "opacity_texture": values.get("opacity_texture", ""),
    }
    if values.get("enable_opacity", False):
        description["opacity"] = values.get("opacity_constant", 1.0)
    return description


def get_material(prim):
    material = UsdShade.MaterialBindingAPI(prim).ComputeBoundMaterial()[0]
    path = str(material.GetPath())
    if path in material_cache:
        return material_cache[path]
    description = material_description(material)
    serialized = json.dumps(description, sort_keys=True)
    key = "M_" + hashlib.sha1(serialized.encode()).hexdigest()[:12]
    materials[key] = description
    material_cache[path] = key
    return key


def export_mesh(prim, world):
    """Keep original face topology/UVs, split material subsets, deduplicate prototypes."""
    source_path = str(prim.GetPath()).lower()
    if (
        not settings["include_flower_geometry"]
        and is_vegetation_path(source_path)
        and "flower" in source_path
    ):
        return
    mesh = UsdGeom.Mesh(prim)
    counts_value = mesh.GetFaceVertexCountsAttr().Get()
    if counts_value is None:
        return
    points = np.asarray(mesh.GetPointsAttr().Get(), dtype=np.float32)
    counts = np.asarray(counts_value, dtype=np.int64)
    indices = np.asarray(mesh.GetFaceVertexIndicesAttr().Get(), dtype=np.int64)
    if not len(points) or not len(indices):
        return
    offsets = np.concatenate(([0], np.cumsum(counts)))
    uv_primvar = UsdGeom.PrimvarsAPI(prim).GetPrimvar("st")
    uv = None
    interpolation = ""
    if uv_primvar and uv_primvar.HasValue():
        uv = np.asarray(uv_primvar.ComputeFlattened(), dtype=np.float32)
        interpolation = uv_primvar.GetInterpolation()
    subsets = UsdGeom.Subset.GetGeomSubsets(mesh, UsdGeom.Tokens.face)
    groups = []
    assigned = set()
    for subset in subsets:
        face_ids = list(subset.GetIndicesAttr().Get() or [])
        if not face_ids:
            continue
        groups.append((get_material(subset.GetPrim()), face_ids))
        assigned.update(face_ids)
    remainder = [index for index in range(len(counts)) if index not in assigned]
    if remainder:
        groups.append((get_material(prim), remainder))
    for material_key, face_ids in groups:
        digest = hashlib.sha1()
        digest.update(points.tobytes())
        digest.update(indices.tobytes())
        digest.update(np.asarray(face_ids, dtype=np.int32).tobytes())
        digest.update(material_key.encode())
        if uv is not None:
            digest.update(uv.tobytes())
        name = "NY_" + digest.hexdigest()[:14]
        if name not in meshes:
            description = materials[material_key]
            output = OUTPUT / (name + ".obj")
            triangle_count = sum(max(0, int(counts[index]) - 2) for index in face_ids)
            with output.open("w") as handle:
                handle.write("# NVIDIA authored geometry; centimeters, Z up.\n")
                for point in points:
                    handle.write(f"v {point[0]:.7g} {point[1]:.7g} {point[2]:.7g}\n")
                has_uv = uv is not None and not description["project_uv"]
                if has_uv:
                    for coordinate in uv:
                        handle.write(f"vt {coordinate[0]:.7g} {coordinate[1]:.7g}\n")
                else:
                    # MDL world projection is approximated by the dominant planar
                    # surface of this authored mesh; centimeter texture scale is retained.
                    spans = np.ptp(points, axis=0)
                    axes = np.argsort(spans)[-2:]
                    scale = description["uv_scale"]
                    if len(scale) == 1:
                        scale = scale * 2
                    for point in points:
                        handle.write(
                            f"vt {point[axes[0]] * scale[0]:.7g} {point[axes[1]] * scale[1]:.7g}\n"
                        )
                reverse = mesh.GetOrientationAttr().Get() == UsdGeom.Tokens.leftHanded
                for face in face_ids:
                    corners = list(range(offsets[face], offsets[face + 1]))
                    if reverse:
                        corners.reverse()
                    # Let the importer triangulate authored polygons and generate normals.
                    entries = []
                    for corner in corners:
                        vertex = int(indices[corner])
                        uv_index = vertex
                        if has_uv:
                            if interpolation == "faceVarying":
                                uv_index = corner
                            elif interpolation == "uniform":
                                uv_index = face
                            elif interpolation == "constant":
                                uv_index = 0
                        entries.append(f"{vertex + 1}/{uv_index + 1}")
                    handle.write("f " + " ".join(entries) + "\n")
            category = "architecture"
            source_path = str(prim.GetPath()).lower()
            if is_vegetation_path(source_path):
                category = "vegetation"
            meshes[name] = {
                "name": name,
                "obj": str(output),
                "material": material_key,
                "triangles": triangle_count,
                "category": category,
                "source_path": str(prim.GetPath()),
            }
        # Reflect both world and local frames. Native OBJ importer performs the
        # local Y reflection; actor transform accounts for the world reflection.
        rotation_scale = reflection @ world[:3, :3] @ reflection
        scale = np.linalg.norm(rotation_scale, axis=0)
        rotation = rotation_scale / np.maximum(scale, 1e-12)
        if np.linalg.det(rotation) < 0:
            scale[0] *= -1
            rotation[:, 0] *= -1
        instances.append(
            {
                "mesh": name,
                "position_cm": (reflection @ world[:3, 3]).tolist(),
                "rotation_xyzw": Rotation.from_matrix(rotation).as_quat().tolist(),
                "scale": scale.tolist(),
            }
        )


def is_vegetation_path(path):
    """Use path components: a substring search for tree also matches street."""
    components = path.lower().split("/")
    for component in components:
        if component in ["painttool", "vegetation", "treeglobal", "plantertree"]:
            return True
        if component.startswith(("trees", "shrubs", "foliage")):
            return True
        if "_foliage" in component:
            return True
    return False


def has_instancer_ancestor(prim):
    parent = prim.GetParent()
    while parent and not parent.IsPseudoRoot():
        if parent.IsA(UsdGeom.PointInstancer):
            return True
        parent = parent.GetParent()
    return False


def expand_instancer(prim, effective_world, depth=0):
    """Expand nested USD instances once, preserving authored prototype transforms."""
    if is_excluded_source(str(prim.GetPath()), settings):
        return
    if not include_vegetation_source(str(prim.GetPath()), settings):
        return
    if "grass" in str(prim.GetPath()).lower():
        return
    if not settings["include_flower_geometry"] and "flower" in str(prim.GetPath()).lower():
        return
    instancer = UsdGeom.PointInstancer(prim)
    prototypes = instancer.GetPrototypesRel().GetTargets()
    prototype_indices = instancer.GetProtoIndicesAttr().Get()
    transforms = instancer.ComputeInstanceTransformsAtTime(
        Usd.TimeCode.Default(), Usd.TimeCode.Default()
    )
    mask = instancer.ComputeMaskAtTime(Usd.TimeCode.Default())
    selected_indices = []
    for prototype_index in range(len(prototypes)):
        candidates = []
        for index, candidate_prototype in enumerate(prototype_indices):
            if candidate_prototype != prototype_index:
                continue
            if mask and not mask[index]:
                continue
            candidates.append(index)
        # Keep outer tree placements. Bound the repeated twigs/branches inside
        # each plant, preserving whole authored pieces and their transforms.
        if depth > 0 and len(candidates) > settings["maximum_nested_instances_per_prototype"]:
            offsets = np.linspace(
                0,
                len(candidates) - 1,
                settings["maximum_nested_instances_per_prototype"],
                dtype=int,
            )
            candidates = [candidates[int(offset)] for offset in offsets]
        selected_indices.extend(candidates)
    for index in selected_indices:
        prototype_index = prototype_indices[index]
        prototype = stage.GetPrimAtPath(prototypes[prototype_index])
        inverse_prototype = np.linalg.inv(np.array(xforms.GetLocalToWorldTransform(prototype)).T)
        instance_world = effective_world @ np.array(transforms[index]).T
        iterator = iter(Usd.PrimRange(prototype, Usd.TraverseInstanceProxies()))
        for child in iterator:
            if is_excluded_source(str(child.GetPath()), settings):
                iterator.PruneChildren()
                continue
            relative = inverse_prototype @ np.array(xforms.GetLocalToWorldTransform(child)).T
            if child.IsA(UsdGeom.PointInstancer):
                expand_instancer(child, instance_world @ relative, depth + 1)
                iterator.PruneChildren()
            elif child.IsA(UsdGeom.Mesh):
                export_mesh(child, instance_world @ relative)


def prepare_scene():
    for prim in stage.Traverse(Usd.TraverseInstanceProxies()):
        imageable = UsdGeom.Imageable(prim)
        if not imageable:
            continue
        if imageable.ComputeVisibility() == UsdGeom.Tokens.invisible:
            continue
        if imageable.ComputePurpose() not in ["default", "render"]:
            continue
        if is_excluded_source(str(prim.GetPath()), settings) or has_instancer_ancestor(prim):
            continue
        world = np.array(xforms.GetLocalToWorldTransform(prim)).T
        if prim.IsA(UsdGeom.PointInstancer):
            expand_instancer(prim, world)
        elif prim.IsA(UsdGeom.Mesh):
            export_mesh(prim, world)
    manifest = {
        "schema_version": 1,
        "city": "New York-style Brownstone Neighborhood",
        "publisher": "NVIDIA",
        "source_stage": str(STAGE_PATH),
        "source_url": "https://d4i3qtqj3r0z5.cloudfront.net/AECDemo_NVD%4010012.zip",
        "coordinate_units": "centimeters",
        "meshes": list(meshes.values()),
        "materials": materials,
        "instances": instances,
        "launch_position_cm": [-5900, 202, 203],
        "launch_yaw_degrees": -90,
    }
    (OUTPUT.parent / "prepared_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(
        f"Finished: {len(meshes)} meshes, {len(instances)} placements, {len(materials)} materials",
        flush=True,
    )


if __name__ == "__main__":
    prepare_scene()
