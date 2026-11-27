"""Repair imported facade color space and instancing support in Unreal Engine 5.2.

The imported roughness map is linear data, while base color uses sRGB. A matching
sampler type prevents silent fallback to Unreal's untextured default material.
Only plaster material slots change; window/glass and other authored slots remain.
"""

import unreal

root = "/Game/External/PolyHaven/ModularUrbanApartments"
diffuse = unreal.load_asset(root + "/modular_urban_apartments_facade_plaster_diff_1k")
roughness = unreal.load_asset(root + "/modular_urban_apartments_facade_plaster_rough_1k")
if not diffuse or not roughness:
    raise RuntimeError("Missing existing Poly Haven facade textures")
diffuse.set_editor_property("srgb", True)
roughness.set_editor_property("srgb", False)
unreal.EditorAssetLibrary.save_loaded_asset(diffuse)
unreal.EditorAssetLibrary.save_loaded_asset(roughness)
material = unreal.load_asset(root + "/M_ApartmentFacade")
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_ApartmentFacade", root, unreal.Material, unreal.MaterialFactoryNew()
    )
    color_sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -300, 0
    )
    color_sample.set_editor_property("texture", diffuse)
    unreal.MaterialEditingLibrary.connect_material_property(
        color_sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    roughness_sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -300, 200
    )
    roughness_sample.set_editor_property("texture", roughness)
    roughness_sample.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness_sample, "R", unreal.MaterialProperty.MP_ROUGHNESS
    )
unreal.MaterialEditingLibrary.set_material_usage(
    material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES
)
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
for asset_path in unreal.EditorAssetLibrary.list_assets(root):
    asset = unreal.load_asset(asset_path)
    if not isinstance(asset, unreal.StaticMesh):
        continue
    changed = False
    for index, slot in enumerate(asset.get_editor_property("static_materials")):
        previous = slot.get_editor_property("material_interface")
        if previous and "plaster" in previous.get_name():
            asset.set_material(index, material)
            changed = True
    if changed:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
