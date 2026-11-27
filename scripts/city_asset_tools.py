"""Small, testable import operations shared by the external-city pipeline.

Generated meshes belong to the local asset cache. This module and the JSON
settings are the reproducible source of import behavior kept in Git.
"""

import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components


def load_city_settings(project=Path(".")):
    """Read dimensional budgets once; reject malformed import settings early."""
    settings = json.loads((project / "Config/CityImport.json").read_text())
    for field in [
        "spatial_cell_size_cm",
        "maximum_texture_size",
        "foliage_draw_distance_cm",
        "leaf_triangle_budget_per_prototype",
        "wood_triangle_budget_per_prototype",
        "architecture_triangle_budget_per_prototype",
    ]:
        value = settings[field]
        if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
            raise ValueError(field + " must be a positive integer")
    return settings


def stable_tile_name(material, cell_x, cell_y):
    """Adding a neighboring tile must not renumber existing native asset names."""
    identity = f"{material}:{cell_x}:{cell_y}"
    digest = hashlib.sha256(identity.encode()).hexdigest()[:16]
    return "NYC_Tile_" + digest


def is_excluded_source(path, settings):
    for prefix in settings["excluded_source_prefixes"]:
        if path.startswith(prefix):
            return True
    return False


def select_leaf_faces(points, faces, triangle_budget, corner_uv=None, preserve_largest=False):
    """Select whole leaf patches without moving vertices or changing UVs.

    Position welding is used ONLY to find neighboring faces. The returned indices
    refer to original faces; callers retain their original corner UVs. QEM reduction
    and nearest-vertex UV transfer are inappropriate for thin textured leaves.
    A budget is soft: a single connected canopy is retained rather than torn apart.
    """
    if len(faces) <= triangle_budget:
        return np.arange(len(faces))
    if corner_uv is None:
        _, remap = np.unique(np.round(points, 3), axis=0, return_inverse=True)
        connectivity = remap[faces]
    else:
        # UV seams separate neighboring leaf surfaces that share a stem vertex.
        # Welding across those seams would merge an entire tree into one patch.
        corners = np.concatenate(
            [np.round(points[faces].reshape(-1, 3), 3), np.round(corner_uv.reshape(-1, 2), 6)],
            axis=1,
        )
        _, remap = np.unique(corners, axis=0, return_inverse=True)
        connectivity = remap.reshape(-1, 3)
    edges = np.concatenate(
        [connectivity[:, [0, 1]], connectivity[:, [1, 2]], connectivity[:, [2, 0]]]
    )
    edges.sort(axis=1)
    face_ids = np.tile(np.arange(len(faces)), 3)
    order = np.lexsort((edges[:, 1], edges[:, 0]))
    _, first, counts = np.unique(edges[order], axis=0, return_index=True, return_counts=True)
    manifold = first[counts == 2]
    rows = face_ids[order[manifold]]
    columns = face_ids[order[manifold + 1]]
    graph = coo_matrix(
        (np.ones(len(rows), dtype=np.uint8), (rows, columns)), shape=(len(faces), len(faces))
    ).tocsr()
    component_count, labels = connected_components(graph, directed=False)
    if component_count == 1:
        return np.arange(len(faces))
    sizes = np.bincount(labels, minlength=component_count)
    identifiers = np.arange(component_count)
    # A fixed permutation spreads retained patches through the authored canopy.
    order = np.argsort((identifiers.astype(np.uint64) * 2654435761) % (2**32))
    remaining = triangle_budget
    retained = []
    largest = None
    if preserve_largest:
        largest = int(np.argmax(sizes))
        retained.append(largest)
        remaining = max(0, triangle_budget - int(sizes[largest]))
    for component in identifiers[order]:
        size = int(sizes[component])
        # Identifiers are unique. Only the pre-inserted trunk can repeat;
        # scanning the retained list here would make large canopies quadratic.
        if largest is not None and int(component) == largest:
            continue
        if size <= remaining:
            retained.append(component)
            remaining -= size
    if not retained:
        return np.arange(len(faces))
    return np.flatnonzero(np.isin(labels, retained))


def vegetation_kind(description, material):
    """Material subsets can be needles inside a mesh named bark; prefer the material."""
    texture_name = Path(material.get("texture", "")).name.lower()
    if any(word in texture_name for word in ["needle", "leaf", "leaves", "honeylocust"]):
        return "leaf"
    if any(word in texture_name for word in ["bark", "trunk", "wood"]):
        return "wood"
    source_name = description.get("source_path", "").split("/")[-1].lower()
    if any(word in source_name for word in ["trunk", "bark", "branch", "stem", "stalk", "twigs"]):
        return "wood"
    return "leaf"


def canonical_material_key(description, source_root):
    """Material/tile IDs must survive moving the checkout to another machine."""
    identity = {}
    for key, value in description.items():
        if key.startswith("prepared_") or key == "alpha_mask":
            continue
        if key in ["texture", "opacity_texture"] and value:
            path = Path(value).resolve()
            try:
                value = path.relative_to(source_root.resolve()).as_posix()
            except ValueError:
                value = path.name
        identity[key] = value
    encoded = json.dumps(identity, sort_keys=True).encode()
    return "M_" + hashlib.sha256(encoded).hexdigest()[:16]


def include_vegetation_source(path, settings):
    """Retain all named parts of the selected authored street-tree assemblies.

    Branch and leaf point instancers form the crown; they are not optional
    microdetail. Species filtering only applies to the separate park scatter.
    """
    lowered = path.lower()
    if "/brownstonesite_trees/" in lowered and (
        "/treeglobal/" in lowered or "/plantertree/" in lowered
    ):
        name = lowered.split("/")[-1]
        return name in settings["street_tree_meshes"]
    if path.startswith("/PaintTool/"):
        species = path.split("/")[2]
        for enabled in settings["enabled_paint_tree_species"]:
            if species.startswith(enabled):
                return True
        return False
    return True
