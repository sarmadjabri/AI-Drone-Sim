"""Import real survey geometry and textures, then build a native Unreal city level.

Run inside Unreal 5.2 with PythonScriptPlugin and GLTFImporter enabled. The map
contains surveyed surfaces, not procedural road/building substitutes. Complex
static collision follows the imported scan; it is never used for movable bodies.
"""

import json
from pathlib import Path

import unreal

project = Path(unreal.Paths.project_dir())
manifest = json.loads(
    (project / "Assets/External/HelsinkiSurvey/prepared_manifest.json").read_text()
)
root = "/Game/External/HelsinkiSurvey"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# One shader shared by all photographic tiles; texture instances avoid hundreds of shader variants.
master = unreal.load_asset(root + "/M_SurveySurface")
if not master:
    master = asset_tools.create_asset(
        "M_SurveySurface", root, unreal.Material, unreal.MaterialFactoryNew()
    )
    master.set_editor_property("two_sided", True)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        master, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0
    )
    sample.set_editor_property("parameter_name", "SurveyColor")
    sample.set_editor_property(
        "texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture")
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        master, unreal.MaterialExpressionConstant, -400, 200
    )
    roughness.set_editor_property("r", 0.9)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    # Photographic shadows are already baked into the source imagery. A small fill prevents double-black shadows.
    fill = unreal.MaterialEditingLibrary.create_material_expression(
        master, unreal.MaterialExpressionMultiply, -100, 300
    )
    fill.set_editor_property("const_b", 0.15)
    unreal.MaterialEditingLibrary.connect_material_expressions(sample, "RGB", fill, "A")
    unreal.MaterialEditingLibrary.connect_material_property(
        fill, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.recompile_material(master)
    unreal.EditorAssetLibrary.save_loaded_asset(master)

# A texture parameter requires a valid default even when every instance overrides it.
sample = unreal.find_object(master, "MaterialExpressionTextureSampleParameter2D_0")
if not sample:
    raise RuntimeError("Survey material is missing its texture parameter")
sample.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
unreal.MaterialEditingLibrary.recompile_material(master)
unreal.EditorAssetLibrary.save_loaded_asset(master)

options = unreal.FbxImportUI()
options.set_editor_property("import_as_skeletal", False)
options.set_editor_property("import_materials", False)
options.set_editor_property("import_textures", False)
options.static_mesh_import_data.set_editor_property("combine_meshes", True)
options.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
options.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", False)
options.static_mesh_import_data.set_editor_property("convert_scene", False)
options.static_mesh_import_data.set_editor_property("convert_scene_unit", False)
options.static_mesh_import_data.set_editor_property("import_uniform_scale", 100.0)

# Import in small batches to bound editor memory and make interrupted imports resumable.
for start in range(0, len(manifest["tiles"]), 12):
    batch = manifest["tiles"][start : start + 12]
    tasks = []
    for tile in batch:
        for kind in ["mesh", "texture"]:
            name = tile["name"]
            source = project / tile["obj"]
            if kind == "texture":
                name = "T_" + name
                source = project / tile["texture"]
            if unreal.EditorAssetLibrary.does_asset_exist(root + "/" + name):
                continue
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(source))
            task.set_editor_property("destination_path", root)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            if kind == "mesh":
                task.set_editor_property("options", options)
            else:
                task.set_editor_property("factory", unreal.TextureFactory())
            tasks.append(task)
    if tasks:
        asset_tools.import_asset_tasks(tasks)
    for tile in batch:
        mesh = unreal.load_asset(root + "/" + tile["name"])
        texture = unreal.load_asset(root + "/T_" + tile["name"])
        if not mesh or not texture:
            raise RuntimeError("Survey import failed: " + tile["name"])
        instance_name = "MI_" + tile["name"]
        instance = unreal.load_asset(root + "/" + instance_name)
        if not instance:
            instance = asset_tools.create_asset(
                instance_name,
                root,
                unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew(),
            )
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, master)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            instance, "SurveyColor", texture
        )
        for slot in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(slot, instance)
        setup = mesh.get_editor_property("body_setup")
        setup.set_editor_property(
            "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
        )
        setup.set_editor_property("double_sided_geometry", True)
        unreal.EditorAssetLibrary.save_loaded_asset(instance)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    unreal.log_warning(
        f"Survey imported: {min(start + 12, len(manifest['tiles']))}/{len(manifest['tiles'])}"
    )

# A single authored vehicle mesh retains the original asset's materials and geometry.
car_task = unreal.AssetImportTask()
car_task.set_editor_property(
    "filename", str(project / "Assets/External/CarConcept/SurveyTrafficCar.glb")
)
car_task.set_editor_property("destination_path", "/Game/External/CarConcept")
car_task.set_editor_property("automated", True)
car_task.set_editor_property("save", True)
car_options = unreal.GLTFImportOptions()
car_options.set_editor_property("generate_lightmap_u_vs", False)
car_task.set_editor_property("options", car_options)
if not unreal.EditorAssetLibrary.does_asset_exist("/Game/External/CarConcept/SurveyTrafficCar"):
    asset_tools.import_asset_tasks([car_task])
car = unreal.load_asset("/Game/External/CarConcept/SurveyTrafficCar")
if not isinstance(car, unreal.StaticMesh):
    raise TypeError("Missing combined vehicle static mesh")
# A primitive collision envelope is fitted in the runtime vehicle; scans remain static.
# Convex decomposition invokes editor UI and is unsuitable for this commandlet.

map_path = "/Game/Maps/HelsinkiSenateSquare"
if unreal.EditorAssetLibrary.does_asset_exist(map_path):
    level = unreal.EditorLevelLibrary.load_level(map_path)
    for existing in unreal.EditorLevelLibrary.get_all_level_actors():
        if (
            "HelsinkiSurvey" in [str(tag) for tag in existing.tags]
            or existing.get_actor_label() == "SenateSquare_DroneLaunch"
        ):
            unreal.EditorLevelLibrary.destroy_actor(existing)
else:
    level = unreal.EditorLevelLibrary.new_level(map_path)
if not level:
    raise RuntimeError("Could not create the surveyed city map")
for tile in manifest["tiles"]:
    mesh = unreal.load_asset(root + "/" + tile["name"])
    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, unreal.Vector(0, 0, 0))
    actor.set_actor_label(tile["name"])
    # OBJ has no unit metadata; the Interchange importer retains the survey's meter values.
    # Apply the explicit meter-to-centimeter boundary once at the level actor.
    actor.set_actor_scale3d(unreal.Vector(100, 100, 100))
    component = actor.static_mesh_component
    component.set_collision_profile_name("BlockAll")
    # Static survey shadows are baked into its photographs; only moving actors cast new shadows.
    component.set_cast_shadow(False)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    actor.tags = ["HelsinkiSurvey"]
# The launch point is in the surveyed Senate Square, not a cleared hole cut out of the city.
launch = manifest.get("launch_position_cm", [0, 0, 100])
start = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.PlayerStart, unreal.Vector(*launch), unreal.Rotator(0, -90, 0)
)
start.set_actor_label("SenateSquare_DroneLaunch")
unreal.EditorLevelLibrary.save_current_level()
unreal.log_warning("Survey city level saved successfully")
