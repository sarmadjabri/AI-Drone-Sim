"""Contracts for reusable city tiles and topology-preserving foliage reduction."""

import importlib.util
from pathlib import Path

import numpy as np

MODULE_PATH = Path(__file__).resolve().parents[1] / "scripts/city_asset_tools.py"
spec = importlib.util.spec_from_file_location("city_asset_tools", MODULE_PATH)
city = importlib.util.module_from_spec(spec)
spec.loader.exec_module(city)


def test_neighboring_tiles_do_not_rename_existing_tiles():
    original = city.stable_tile_name("brick", 2, -1)
    expanded = [city.stable_tile_name("brick", x, -1) for x in range(5)]
    assert expanded[2] == original
    assert len(set(expanded)) == 5
    assert city.stable_tile_name("glass", 2, -1) != original


def test_leaf_reduction_keeps_complete_patches_and_original_uvs():
    points = []
    faces = []
    for leaf in range(8):
        first = len(points)
        x = leaf * 3
        points.extend([[x, 0, 0], [x + 1, 0, 0], [x + 1, 1, 0], [x, 1, 0]])
        faces.extend([[first, first + 1, first + 2], [first, first + 2, first + 3]])
    points = np.asarray(points, dtype=float)
    faces = np.asarray(faces)
    uv = np.tile(np.array([[[0, 0], [1, 0], [1, 1]], [[0, 0], [1, 1], [0, 1]]]), (8, 1, 1))
    original_points = points.copy()
    original_uv = uv.copy()
    selected = city.select_leaf_faces(points, faces, 6, uv)
    assert len(selected) == 6
    for first in range(0, len(faces), 2):
        assert (first in selected) == (first + 1 in selected)
    np.testing.assert_array_equal(points, original_points)
    np.testing.assert_array_equal(uv, original_uv)


def test_material_subset_named_bark_can_still_contain_needles():
    description = {"source_path": "/PaintTool/Yew/asset/Pine_bark_Mat"}
    material = {"texture": "/textures/pine_needles.png"}
    assert city.vegetation_kind(description, material) == "leaf"


def test_connected_trunk_is_retained_when_over_soft_budget():
    points = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]], dtype=float)
    faces = np.array([[0, 1, 2], [0, 2, 3]])
    selected = city.select_leaf_faces(points, faces, 1, preserve_largest=True)
    np.testing.assert_array_equal(selected, [0, 1])


def test_material_identity_does_not_depend_on_checkout_location(tmp_path):
    first_root = tmp_path / "first"
    second_root = tmp_path / "second"
    first = {"color": [1, 1, 1], "texture": str(first_root / "textures/leaf.png")}
    second = {"color": [1, 1, 1], "texture": str(second_root / "textures/leaf.png")}
    assert city.canonical_material_key(first, first_root) == city.canonical_material_key(
        second, second_root
    )


def test_selected_tree_assemblies_keep_canopy_instancers_and_planter_fences():
    settings = city.load_city_settings(MODULE_PATH.parents[1])
    assert city.include_vegetation_source(
        "/World/BrownstoneSite/BrownstoneSite_Trees/PlanterFence_1/TreeGlobal/PlanterTree/top",
        settings,
    )
    assert city.include_vegetation_source(
        "/World/BrownstoneSite/BrownstoneSite_Trees/PlanterFence_1/TreeGlobal/PlanterTree/leaves",
        settings,
    )
    assert city.include_vegetation_source(
        "/World/BrownstoneSite/BrownstoneSite_Trees/PlanterFence_1/FenceGlobal/Frame", settings
    )
    assert city.include_vegetation_source(
        "/World/BrownstoneSite/BrownstoneSite_Trees/PlanterFence_1/TreeGlobal/PlanterTree/branches",
        settings,
    )
    assert not city.include_vegetation_source("/PaintTool/Douglas_Fir_dense/pointInstancer", settings)
