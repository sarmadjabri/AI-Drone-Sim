"""Reduce oversized ornamental/foliage batches before native Unreal import.

Quadric simplification keeps the source layout and approximate silhouettes. UVs
transfer from the nearest authored vertex. Small meshes and architectural planes
are unchanged. This trades tiny ornamental detail for an interactive GPU budget;
it is not an exact triangle-for-triangle copy of the offline scene.
"""

import hashlib
import json
from pathlib import Path

import fast_simplification
import numpy as np
from city_asset_tools import canonical_material_key, load_city_settings, stable_tile_name
from PIL import Image
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from scipy.spatial import cKDTree

settings = load_city_settings()
ROOT = Path("Assets/External/NvidiaBrownstone")
manifest_path = ROOT / "batched_manifest.json"
manifest = json.loads(manifest_path.read_text())

# Normalize semantic IDs after batching. Absolute download paths and the set of
# neighboring tiles must not change asset identities in the committed manifest.
material_keys = {}
normalized_materials = {}
for old_key, description in manifest["materials"].items():
    new_key = canonical_material_key(description, ROOT)
    material_keys[old_key] = new_key
    normalized_materials[new_key] = description
for batch in manifest["batches"]:
    batch["material"] = material_keys[batch["material"]]
    name = stable_tile_name(batch["material"] + batch["category"], batch["cell_x"], batch["cell_y"])
    original = Path(batch["obj"])
    destination = original.with_name(name + ".obj")
    if original != destination:
        original.replace(destination)
    batch["name"] = name
    batch["obj"] = str(destination)
manifest["materials"] = normalized_materials

for index, batch in enumerate(manifest["batches"]):
    # Leaves are already selected as complete source patches. A second geometry
    # reduction would undo that guarantee and recreate the disappearing canopy.
    if batch.get("category") in ["leaf", "wood"]:
        continue
    if batch["triangles"] <= 80000:
        continue
    path = Path(batch["obj"])
    # These OBJ files are emitted by our batcher with triangulated v/vt faces.
    # NumPy parses numeric blocks in C, avoiding millions of Python float objects.
    lines = path.read_text().splitlines()
    positions_text = "\n".join(line[2:] for line in lines if line.startswith("v "))
    coordinates_text = "\n".join(line[3:] for line in lines if line.startswith("vt "))
    faces_text = "\n".join(line[2:].replace("/", " ") for line in lines if line.startswith("f "))
    del lines
    points = np.fromstring(positions_text, sep=" ", dtype=np.float64).reshape(-1, 3)
    uv = np.fromstring(coordinates_text, sep=" ", dtype=np.float32).reshape(-1, 2)
    corner_indices = np.fromstring(faces_text, sep=" ", dtype=np.int32).reshape(-1, 6) - 1
    faces = corner_indices[:, [0, 2, 4]]
    uv_faces = corner_indices[:, [1, 3, 5]]
    del positions_text, coordinates_text, faces_text, corner_indices
    vertex_uv = np.zeros((len(points), 2), dtype=np.float32)
    vertex_uv[faces.ravel()] = uv[uv_faces.ravel()]
    # USD exports often duplicate vertices at every normal/UV seam. Weld
    # coincident positions at 0.01 mm before quadric reduction; otherwise
    # disconnected triangles cannot collapse and the target is never reached.
    welded, remap = np.unique(np.round(points, 3), axis=0, return_inverse=True)
    welded_faces = remap[faces].astype(np.int32)
    texture_path = manifest["materials"][batch["material"]].get("texture", "").lower()
    is_foliage = any(term in texture_path for term in ["vegetation", "trees", "shrub", "grass"])
    if len(faces) <= 80000:
        batch["triangles"] = len(faces)
        continue
    if is_foliage:
        # Connect faces across manifold edges, not just a common vertex. Leaves
        # can touch at a stem vertex while remaining independent visual patches.
        edges = np.concatenate(
            [welded_faces[:, [0, 1]], welded_faces[:, [1, 2]], welded_faces[:, [2, 0]]]
        )
        edges.sort(axis=1)
        face_ids = np.tile(np.arange(len(welded_faces)), 3)
        order = np.lexsort((edges[:, 1], edges[:, 0]))
        ordered = edges[order]
        _, first_edges, edge_counts = np.unique(
            ordered, axis=0, return_index=True, return_counts=True
        )
        starts = first_edges[edge_counts == 2]
        rows = face_ids[order[starts]]
        columns = face_ids[order[starts + 1]]
        graph = coo_matrix(
            (np.ones(len(rows), dtype=np.uint8), (rows, columns)),
            shape=(len(welded_faces), len(welded_faces)),
        ).tocsr()
        component_count, labels = connected_components(graph, directed=False)
        if component_count > 128:
            counts = np.bincount(labels, minlength=component_count)
            identifiers = np.arange(component_count)
            selection_order = np.argsort((identifiers.astype(np.uint64) * 2654435761) % (2**32))
            identifiers = identifiers[selection_order]
            cumulative = np.cumsum(counts[identifiers])
            retained = identifiers[cumulative <= 60000]
            if len(retained):
                welded_faces = welded_faces[np.isin(labels, retained)]
        del graph, edges, ordered, face_ids, order, rows, columns

    # QEM error thresholds are scale sensitive: normalize centimeter world
    # coordinates before reducing, then restore the source coordinate frame.
    center = welded.mean(axis=0)
    extent = max(float(np.ptp(welded, axis=0).max()), 1.0)
    reduced, triangles = fast_simplification.simplify(
        (welded - center) / extent, welded_faces, target_count=30000, agg=10.0
    )
    reduced = reduced * extent + center
    used_vertices = np.unique(welded_faces)
    used_positions = welded[used_vertices]
    nearest_original = cKDTree(points).query(used_positions)[1]
    nearest = cKDTree(used_positions).query(reduced)[1]
    reduced_uv = vertex_uv[nearest_original[nearest]]
    temporary = path.with_suffix(".optimized.obj")
    with temporary.open("w") as handle:
        handle.write("# NVIDIA source scene; bounded ornamental detail; centimeters.\n")
        for point in reduced:
            handle.write(f"v {point[0]:.6f} {point[1]:.6f} {point[2]:.6f}\n")
        for coordinate in reduced_uv:
            handle.write(f"vt {coordinate[0]:.6f} {coordinate[1]:.6f}\n")
        for face in triangles:
            entries = [f"{int(vertex) + 1}/{int(vertex) + 1}" for vertex in face]
            handle.write("f " + " ".join(entries) + "\n")
    temporary.replace(path)
    batch["original_triangles"] = batch["triangles"]
    batch["triangles"] = len(triangles)
    print(f"Reduced {index}: {batch['original_triangles']:,} -> {len(triangles):,}", flush=True)

# Also support batches produced by an older intermediate exporter.
Image.new("RGBA", (4, 4), (255, 255, 255, 255)).save(ROOT / "batched/White.png")
for name, description in manifest["materials"].items():
    if not description.get("texture"):
        continue
    with Image.open(description["texture"]) as source:
        image = source.convert("RGBA")
        description["alpha_mask"] = image.getchannel("A").getextrema()[0] < 250
        image.thumbnail(
            (settings["maximum_texture_size"], settings["maximum_texture_size"]),
            Image.Resampling.LANCZOS,
        )
        destination = ROOT / "batched" / (name + ".png")
        image.save(destination)
        description["prepared_texture"] = str(destination)
# Preserve source normal detail where the pack supplies a matching PBR map.
# NVIDIA's textures use both *_N and *_normal naming conventions.
Image.new("RGB", (4, 4), (128, 128, 255)).save(ROOT / "batched/FlatNormal.png")
for name, description in manifest["materials"].items():
    source_path = Path(description.get("texture", ""))
    if not source_path.is_file():
        continue
    stem = source_path.stem
    candidates = []
    for token in ["_BaseColor", "_basecolor", "_Base_Color"]:
        if token in stem:
            for replacement in ["_N", "_normal", "_Normal"]:
                candidates.append(
                    source_path.with_name(stem.replace(token, replacement) + source_path.suffix)
                )
    for candidate in candidates:
        if not candidate.is_file():
            continue
        with Image.open(candidate) as source:
            image = source.convert("RGB")
            image.thumbnail(
                (settings["maximum_texture_size"], settings["maximum_texture_size"]),
                Image.Resampling.LANCZOS,
            )
            destination = ROOT / "batched" / (name + "_Normal.png")
            image.save(destination)
            description["prepared_normal"] = str(destination)
        break

# Rendering/collision semantics are authored per tile, never guessed from a
# texture filename (a wood texture can occur on both a tree and a building).
for batch in manifest["batches"]:
    batch["source_sha256"] = hashlib.sha256(Path(batch["obj"]).read_bytes()).hexdigest()

manifest["launch_position_cm"] = settings["launch_position_cm"]
manifest["launch_yaw_degrees"] = settings["launch_yaw_degrees"]
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
# Runtime/test metadata contains no paths to a developer's original download.
scene = {
    "schema_version": 2,
    "city": manifest["city"],
    "asset_root": settings["asset_root"],
    "map_path": settings["map_path"],
    "publisher": manifest["publisher"],
    "bounds_min_source_cm": manifest["bounds_min_source_cm"],
    "bounds_max_source_cm": manifest["bounds_max_source_cm"],
    "launch_position_cm": manifest["launch_position_cm"],
    "launch_yaw_degrees": manifest["launch_yaw_degrees"],
    "batches": [
        {
            key: batch[key]
            for key in [
                "name",
                "triangles",
                "cast_shadow",
                "foliage",
                "collision",
                "category",
                "cell_x",
                "cell_y",
            ]
        }
        for batch in manifest["batches"]
    ],
}
Path("Content/CityData/new_york_scene.json").write_text(json.dumps(scene, indent=2) + "\n")
print(
    f"Native scene: {len(scene['batches'])} batches, {sum(batch['triangles'] for batch in scene['batches']):,} triangles",
    flush=True,
)
