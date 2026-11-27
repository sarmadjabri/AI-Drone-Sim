"""Run with UnrealEditor-Cmd -run=pythonscript -script=<absolute path> -EnablePlugins=PythonScriptPlugin."""
import unreal
from pathlib import Path
import tempfile
import shutil

root = '/Game/External/KenneyCityKit'
texture = unreal.load_asset(root + '/T_CityPalette')
if not texture:
    temporary = tempfile.TemporaryDirectory()
    source = Path(temporary.name) / 'T_CityPalette.png'
    shutil.copyfile(Path(__file__).resolve().parents[1] / 'Assets/External/KenneyCityKit/Textures/colormap.png', source)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(source))
    task.set_editor_property('destination_path', root)
    task.set_editor_property('destination_name', 'T_CityPalette')
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(root + '/T_CityPalette')
if not isinstance(texture, unreal.Texture):
    raise RuntimeError('Missing Kenney palette texture')
material = unreal.load_asset(root + '/M_CityPalette')
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_CityPalette', root, unreal.Material, unreal.MaterialFactoryNew())
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -300, 0)
    sample.set_editor_property('texture', texture)
    unreal.MaterialEditingLibrary.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -300, 200)
    roughness.set_editor_property('r', 0.8)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
for suffix in 'abcd':
    mesh = unreal.load_asset(root + '/low-detail-building-' + suffix)
    if not mesh:
        raise RuntimeError('Missing building ' + suffix)
    for slot in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(slot, material)
    # Static concave architecture needs collision matching its visible surfaces
    mesh.get_editor_property('body_setup').set_editor_property(
        'collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    unreal.log('Repaired city building ' + suffix)
