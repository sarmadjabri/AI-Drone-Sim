"""Build spatial/material batches from the authored New York-style placements.

Dense foliage is reduced before repetition. Architectural planes retain their source geometry and UVs; dense ornamental
prototypes are reduced. Batches bound draw calls without introducing synthetic city
geometry. The original download and intermediate prototypes are untouched.
"""

import hashlib
import json
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import fast_simplification
import numpy as np
from city_asset_tools import (
    load_city_settings,
    select_leaf_faces,
    stable_tile_name,
    vegetation_kind,
)
from PIL import Image
from scipy.spatial import cKDTree
from scipy.spatial.transform import Rotation

settings = load_city_settings()
ROOT = Path("Assets/External/NvidiaBrownstone")
manifest = json.loads((ROOT / "prepared_manifest.json").read_text())
output = ROOT / "batched"
output.mkdir(exist_ok=True)
mesh_lookup = {mesh["name"]: mesh for mesh in manifest["meshes"]}
cache = {}
groups = defaultdict(list)


def load_geometry(description):
    """Read our own OBJ output, preserving per-corner texture coordinates."""
    name = description["name"]
    if name in cache:
        return cache[name]
    points = []
    coordinates = []
    triangles = []
    uv_triangles = []
    with Path(description["obj"]).open() as handle:
        for line in handle:
            if line.startswith("v "):
                points.append([float(value) for value in line.split()[1:4]])
            elif line.startswith("vt "):
                coordinates.append([float(value) for value in line.split()[1:3]])
            elif line.startswith("f "):
                corners = [
                    tuple(int(value) - 1 for value in entry.split("/")[:2])
                    for entry in line.split()[1:]
                ]
                for index in range(1, len(corners) - 1):
                    triangle = [corners[0], corners[index], corners[index + 1]]
                    triangles.append([corner[0] for corner in triangle])
                    uv_triangles.append([corner[1] for corner in triangle])
    points = np.asarray(points, dtype=np.float64)
    faces = np.asarray(triangles, dtype=np.int32)
    uv = np.asarray(coordinates, dtype=np.float32)
    uv_faces = np.asarray(uv_triangles, dtype=np.int32)
    used, inverse = np.unique(faces, return_inverse=True)
    points = points[used]
    faces = inverse.reshape(-1, 3).astype(np.int32)
    # Material subsets can reference a tiny part of a large UV array. Compact
    # those unused entries before repeating the geometry around the city.
    used_uv, uv_inverse = np.unique(uv_faces, return_inverse=True)
    uv = uv[used_uv]
    uv_faces = uv_inverse.reshape(-1, 3).astype(np.int32)
    category = description.get("category", "architecture")
    source_name = description.get("source_path", "").split("/")[-1].lower()
    material = manifest["materials"][description["material"]]
    kind = vegetation_kind(description, material)
    woody = category == "vegetation" and kind == "wood"
    leaf = category == "vegetation" and kind == "leaf"
    if leaf:
        budget = settings["leaf_triangle_budget_per_prototype"]
        if float(np.ptp(points, axis=0).max()) < settings["large_canopy_threshold_cm"]:
            budget = settings["small_leaf_triangle_budget_per_prototype"]
        if "/plantertree/leaves/" in description.get("source_path", "").lower():
            budget = min(budget, settings["branch_leaf_triangle_budget_per_prototype"])
        if "flower" in source_name:
            budget = settings["flower_triangle_budget_per_prototype"]
        selected = select_leaf_faces(points, faces, budget, uv[uv_faces])
        faces = faces[selected]
        uv_faces = uv_faces[selected]
        # Preserve exact leaf vertices and corner UVs. Never run QEM on leaves.
    else:
        budget = settings["architecture_triangle_budget_per_prototype"]
        if woody:
            budget = settings["wood_triangle_budget_per_prototype"]
            selected = select_leaf_faces(points, faces, budget, uv[uv_faces], preserve_largest=True)
            faces = faces[selected]
            uv_faces = uv_faces[selected]
        if not woody and len(faces) > budget:
            original = points
            original_faces = faces
            welded, remap = np.unique(np.round(points, 3), axis=0, return_inverse=True)
            welded_faces = remap[faces].astype(np.int32)
            center = welded.mean(axis=0)
            extent = max(float(np.ptp(welded, axis=0).max()), 1.0)
            points, faces = fast_simplification.simplify(
                (welded - center) / extent, welded_faces, target_count=budget
            )
            points = points * extent + center
            vertex_uv = np.zeros((len(original), 2), dtype=np.float32)
            vertex_uv[original_faces.ravel()] = uv[uv_faces.ravel()]
            nearest = cKDTree(original).query(points)[1]
            uv = vertex_uv[nearest]
            uv_faces = faces.copy()
    # Selecting patches leaves unused source vertices/UVs behind. Compact both
    # arrays before applying hundreds of placements; otherwise RAM scales with
    # the original canopy instead of the retained geometry.
    used, inverse = np.unique(faces, return_inverse=True)
    points = points[used]
    faces = inverse.reshape(-1, 3).astype(np.int32)
    used_uv, inverse_uv = np.unique(uv_faces, return_inverse=True)
    uv = uv[used_uv]
    uv_faces = inverse_uv.reshape(-1, 3).astype(np.int32)
    cache[name] = (points, faces, uv, uv_faces)
    return cache[name]


def write_tile(arguments):
    key, pieces = arguments
    minimum = np.full(3, np.inf)
    maximum = np.full(3, -np.inf)
    name = stable_tile_name(key[0] + key[3], key[1], key[2])
    path = output / (name + ".obj")
    vertex_offset = 0
    uv_offset = 0
    triangles = 0
    with path.open("w") as handle:
        handle.write("# NVIDIA authored scene; spatial/material batch; source centimeters.\n")
        for points, faces, uv, uv_faces in pieces:
            minimum = np.minimum(minimum, points.min(axis=0))
            maximum = np.maximum(maximum, points.max(axis=0))
            for point in points:
                handle.write(f"v {point[0]:.6f} {point[1]:.6f} {point[2]:.6f}\n")
            for coordinate in uv:
                handle.write(f"vt {coordinate[0]:.6f} {coordinate[1]:.6f}\n")
            for face, texture in zip(faces, uv_faces, strict=True):
                entries = [
                    f"{int(vertex) + vertex_offset + 1}/{int(texcoord) + uv_offset + 1}"
                    for vertex, texcoord in zip(face, texture, strict=True)
                ]
                handle.write("f " + " ".join(entries) + "\n")
            vertex_offset += len(points)
            uv_offset += len(uv)
            triangles += len(faces)
    record = {
        "name": name,
        "obj": str(path),
        "material": key[0],
        "triangles": triangles,
        "cast_shadow": key[3] != "leaf",
        "category": key[3],
        "foliage": key[3] != "architecture",
        "collision": key[3] != "leaf",
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "cell_x": key[1],
        "cell_y": key[2],
        "bounds_min_source_cm": minimum.tolist(),
        "bounds_max_source_cm": maximum.tolist(),
    }
    return record, minimum.tolist(), maximum.tolist()


def main():
    for index, instance in enumerate(manifest["instances"]):
        description = mesh_lookup[instance["mesh"]]
        points, faces, uv, uv_faces = load_geometry(description)
        rotation = Rotation.from_quat(instance["rotation_xyzw"]).as_matrix()
        # Source OBJ frame is local X, local Y, Z/up; bake in that frame.
        reflect = np.diag([1.0, -1.0, 1.0])
        linear = reflect @ rotation @ np.diag(instance["scale"]) @ reflect
        translation = reflect @ np.array(instance["position_cm"])
        world = points @ linear.T + translation
        center = world.mean(axis=0)
        category = description.get("category", "architecture")
        if category == "vegetation":
            category = vegetation_kind(description, manifest["materials"][description["material"]])
        material_key = description["material"]
        material = manifest["materials"][material_key]
        if (
            category == "leaf"
            and not material.get("texture")
            and material["color"] == [0.5, 0.5, 0.5]
        ):
            # Unsupported/unbound foliage shaders need an explicit leaf fallback,
            # separate from gray stone/plastic using the same missing shader.
            material_key = "M_LeafFallback_" + material_key
            material = dict(material)
            material["color"] = settings["unbound_leaf_color_linear"]
            manifest["materials"][material_key] = material
        key = (
            material_key,
            int(np.floor(center[0] / settings["spatial_cell_size_cm"])),
            int(np.floor(center[1] / settings["spatial_cell_size_cm"])),
            category,
        )
        if np.linalg.det(linear) < 0:
            faces = faces[:, ::-1]
            uv_faces = uv_faces[:, ::-1]
        groups[key].append((world.astype(np.float32), faces, uv, uv_faces))
        if index % 2000 == 0:
            print(f"Batched {index}/{len(manifest['instances'])} placements", flush=True)

    # Texture resolution is bounded for the interactive simulator; originals remain intact.
    Image.new("RGBA", (4, 4), (255, 255, 255, 255)).save(output / "White.png")
    for material_name, description in manifest["materials"].items():
        if not description.get("texture"):
            continue
        with Image.open(description["texture"]) as source_image:
            image = source_image.convert("RGBA")
            description["alpha_mask"] = image.getchannel("A").getextrema()[0] < 250
            image.thumbnail(
                (settings["maximum_texture_size"], settings["maximum_texture_size"]),
                Image.Resampling.LANCZOS,
            )
            texture_path = output / (material_name + ".png")
            image.save(texture_path)
            description["prepared_texture"] = str(texture_path)

    batches = []
    minimum = np.full(3, np.inf)
    maximum = np.full(3, -np.inf)
    with ProcessPoolExecutor(max_workers=4) as executor:
        for record, tile_minimum, tile_maximum in executor.map(write_tile, sorted(groups.items())):
            batches.append(record)
            minimum = np.minimum(minimum, tile_minimum)
            maximum = np.maximum(maximum, tile_maximum)
    manifest["source_meshes"] = manifest.pop("meshes")
    manifest["source_placements_count"] = len(manifest.pop("instances"))
    manifest["batches"] = batches
    manifest["bounds_min_source_cm"] = minimum.tolist()
    manifest["bounds_max_source_cm"] = maximum.tolist()
    (ROOT / "batched_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(
        f"Finished {len(batches)} batches, {sum(batch['triangles'] for batch in batches):,} triangles",
        flush=True,
    )


if __name__ == "__main__":
    main()
