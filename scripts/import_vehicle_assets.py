"""Run with Unreal's PythonScriptPlugin to import the licensed local vehicle assets.

Import sources and palette stay in Assets/External so the process is reproducible.
Convex collision is required for movable actors; complex-as-simple is static only.
"""

from pathlib import Path

import unreal

source_directory = Path(unreal.Paths.project_dir()) / "Assets/External/KenneyCarKit"
destination = "/Game/External/KenneyCarKit"
tasks = []
for filename in ["colormap.png", "sedan.fbx", "wheel-default.fbx", "cone.fbx"]:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_directory / filename))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    if filename.endswith(".png"):
        # Use the synchronous texture factory rather than asynchronous Interchange import.
        task.set_editor_property("factory", unreal.TextureFactory())
        task.set_editor_property("destination_name", "T_VehiclePalette")
    if filename.endswith(".fbx"):
        options = unreal.FbxImportUI()
        options.set_editor_property("import_as_skeletal", False)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        options.static_mesh_import_data.set_editor_property("combine_meshes", True)
        options.static_mesh_import_data.set_editor_property("auto_generate_collision", True)
        task.set_editor_property("options", options)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
texture = unreal.load_asset(destination + "/T_VehiclePalette")
if not texture:
    raise RuntimeError("Vehicle palette import failed")
material = unreal.load_asset(destination + "/M_CarPalette")
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_CarPalette", destination, unreal.Material, unreal.MaterialFactoryNew()
    )
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -300, 0
    )
    sample.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -300, 200
    )
    roughness.set_editor_property("r", 0.65)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
mesh_editor = unreal.EditorStaticMeshLibrary
for name in ["sedan", "wheel-default", "cone"]:
    mesh = unreal.load_asset(destination + "/" + name)
    if not mesh:
        raise RuntimeError("Missing imported vehicle mesh: " + name)
    # Do not assume the FBX importer's collision option actually produced a hull.
    if mesh_editor.get_convex_collision_count(mesh) == 0:
        collision_created = mesh_editor.set_convex_decomposition_collisions(mesh, 4, 16, 100000)
        if not collision_created:
            raise RuntimeError("Could not build movable collision for " + name)
    mesh.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX
    )
    unreal.log(
        name + " convex collision hulls: " + str(mesh_editor.get_convex_collision_count(mesh))
    )
    for slot in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(slot, material)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
unreal.EditorAssetLibrary.save_loaded_asset(material)
