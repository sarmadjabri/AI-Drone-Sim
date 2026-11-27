"""Import the externally authored New York-style neighborhood into a native level.

Run with Unreal PythonScriptPlugin enabled. Runtime needs neither USD nor Python.
The offline source is batched by material and location; no replacement buildings
or streets are generated. Complex static collision follows the original surfaces.
"""

import hashlib
import json
from pathlib import Path

import unreal

project = Path(unreal.Paths.project_dir())
manifest = json.loads(
    (project / "Assets/External/NvidiaBrownstone/batched_manifest.json").read_text()
)
settings = json.loads((project / "Config/CityImport.json").read_text())
root = settings["asset_root"]
cache_path = project / "Saved/AssetImport/new_york.json"
cache_path.parent.mkdir(parents=True, exist_ok=True)
import_cache = {}
if cache_path.exists():
    import_cache = json.loads(cache_path.read_text())


def save_import_cache():
    temporary = cache_path.with_suffix(".tmp")
    temporary.write_text(json.dumps(import_cache, indent=2) + "\n")
    temporary.replace(cache_path)


def source_fingerprint(source):
    return hashlib.sha256(Path(source).read_bytes()).hexdigest()


tools = unreal.AssetToolsHelpers.get_asset_tools()
city_policy_class = unreal.load_class(None, "/Script/AerialWorldLab.CitySimulationSettings")
city_policy = unreal.get_default_object(city_policy_class)


def create_import_task(source, name, folder, mesh=False):
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.replace_existing_settings = True
    if mesh:
        # OBJ is handled by Interchange in UE 5.2. Its explicit pipeline
        # override controls mesh builds; FbxImportUI is ignored by that path.
        pipeline = unreal.InterchangeGenericAssetsPipeline()
        mesh_pipeline = pipeline.get_editor_property("mesh_pipeline")
        mesh_pipeline.set_editor_property("combine_static_meshes", True)
        mesh_pipeline.set_editor_property("generate_lightmap_u_vs", False)
        material_pipeline = pipeline.get_editor_property("material_pipeline")
        material_pipeline.set_editor_property("import_materials", False)
        overrides = unreal.InterchangePipelineStackOverride()
        overrides.set_editor_property("override_pipelines", [pipeline])
        task.options = overrides
    else:
        task.factory = unreal.TextureFactory()
    return task


def import_file(source, name, folder, mesh=False):
    destination = folder + "/" + name
    fingerprint = source_fingerprint(source)
    if (
        unreal.EditorAssetLibrary.does_asset_exist(destination)
        and import_cache.get(destination) == fingerprint
    ):
        return unreal.load_asset(destination)
    task = create_import_task(source, name, folder, mesh)
    tools.import_asset_tasks([task])
    asset = unreal.load_asset(destination)
    if not asset:
        raise RuntimeError("Import did not produce " + destination)
    import_cache[destination] = fingerprint
    save_import_cache()
    return asset


def create_master(name, masked):
    existing = unreal.load_asset(root + "/" + name)
    if existing:
        return existing
    master = tools.create_asset(name, root, unreal.Material, unreal.MaterialFactoryNew())
    master.set_editor_property("two_sided", True)
    if masked:
        master.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        master.set_editor_property("opacity_mask_clip_value", 0.35)
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        master, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0
    )
    sample.set_editor_property("parameter_name", "BaseTexture")
    sample.set_editor_property("texture", white)
    color = unreal.MaterialEditingLibrary.create_material_expression(
        master, unreal.MaterialExpressionVectorParameter, -600, 200
    )
    color.set_editor_property("parameter_name", "BaseColor")
    color.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    multiply = unreal.MaterialEditingLibrary.create_material_expression(
        master, unreal.MaterialExpressionMultiply, -200, 0
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(sample, "RGB", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "", multiply, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        multiply, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    if masked:
        unreal.MaterialEditingLibrary.connect_material_property(
            sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK
        )
    normal = unreal.MaterialEditingLibrary.create_material_expression(
        master, unreal.MaterialExpressionTextureSampleParameter2D, -600, 600
    )
    normal.set_editor_property("parameter_name", "NormalTexture")
    normal.set_editor_property("texture", flat_normal)
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    unreal.MaterialEditingLibrary.connect_material_property(
        normal, "RGB", unreal.MaterialProperty.MP_NORMAL
    )
    for parameter, property_name, default in [
        ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS, 0.8),
        ("Metallic", unreal.MaterialProperty.MP_METALLIC, 0.0),
    ]:
        expression = unreal.MaterialEditingLibrary.create_material_expression(
            master, unreal.MaterialExpressionScalarParameter, -400, 400
        )
        expression.set_editor_property("parameter_name", parameter)
        expression.set_editor_property("default_value", default)
        unreal.MaterialEditingLibrary.connect_material_property(expression, "", property_name)
    unreal.MaterialEditingLibrary.recompile_material(master)
    unreal.EditorAssetLibrary.save_loaded_asset(master)
    return master


white = import_file(
    project / "Assets/External/NvidiaBrownstone/batched/White.png", "T_NYC_White", root
)
flat_normal = import_file(
    project / "Assets/External/NvidiaBrownstone/batched/FlatNormal.png", "T_NYC_FlatNormal", root
)
flat_normal.set_editor_property("srgb", False)
flat_normal.set_editor_property(
    "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP
)
unreal.EditorAssetLibrary.save_loaded_asset(flat_normal)
opaque_master = create_master("M_CityOpaque", False)
masked_master = create_master("M_CityFoliage", True)
material_assets = {}
for name, description in manifest["materials"].items():
    master = opaque_master
    if description.get("alpha_mask", False):
        master = masked_master
    material = unreal.load_asset(root + "/" + name)
    if not material:
        material = tools.create_asset(
            name, root, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew()
        )
    unreal.MaterialEditingLibrary.set_material_instance_parent(material, master)
    texture = white
    if description.get("prepared_texture"):
        texture = import_file(project / description["prepared_texture"], "T_" + name, root)
        texture.set_editor_property("max_texture_size", 1024)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        material, "BaseTexture", texture
    )
    normal_texture = flat_normal
    if description.get("prepared_normal"):
        normal_texture = import_file(
            project / description["prepared_normal"], "T_" + name + "_Normal", root
        )
        normal_texture.set_editor_property("srgb", False)
        normal_texture.set_editor_property(
            "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP
        )
        # MDL uses OpenGL tangent normals; Unreal's texture convention flips Y.
        normal_texture.set_editor_property("flip_green_channel", True)
        unreal.EditorAssetLibrary.save_loaded_asset(normal_texture)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        material, "NormalTexture", normal_texture
    )
    color = list(description["color"])
    if len(color) == 1:
        color = color * 3
    if len(color) != 3:
        raise ValueError("Expected an RGB material color for " + name)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        material, "BaseColor", unreal.LinearColor(*color, 1)
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        material, "Roughness", description["roughness"]
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        material, "Metallic", description.get("metallic", 0)
    )
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    material_assets[name] = material

# Small resumable imports bound editor memory and avoid one enormous import transaction.
for first in range(0, len(manifest["batches"]), 8):
    batches = manifest["batches"][first : first + 8]
    tasks = []
    for batch in batches:
        destination = root + "/" + batch["name"]
        if (
            not unreal.EditorAssetLibrary.does_asset_exist(destination)
            or import_cache.get(destination) != batch["source_sha256"]
        ):
            tasks.append(create_import_task(project / batch["obj"], batch["name"], root, mesh=True))
    if tasks:
        tools.import_asset_tasks(tasks)
    for batch in batches:
        mesh = unreal.load_asset(root + "/" + batch["name"])
        if not mesh:
            raise RuntimeError("Missing city mesh " + batch["name"])
        material = material_assets[batch["material"]]
        slots = mesh.get_editor_property("static_materials")
        if not slots:
            # OBJ without MTL may produce no material slots. Explicitly create
            # one; iterating an empty list silently leaves the gray fallback.
            slot = unreal.StaticMaterial()
            slot.set_editor_property("material_interface", material)
            slot.set_editor_property("material_slot_name", "CitySurface")
            mesh.set_editor_property("static_materials", [slot])
        else:
            for index in range(len(slots)):
                mesh.set_material(index, material)
        setup = mesh.get_editor_property("body_setup")
        setup.set_editor_property(
            "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
        )
        setup.set_editor_property("double_sided_geometry", True)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        import_cache[root + "/" + batch["name"]] = batch["source_sha256"]
    save_import_cache()
    # Imported packages can retain large CPU mesh buffers until the editor runs
    # garbage collection. Bound that transient memory as the city expands.
    tasks.clear()
    unreal.SystemLibrary.collect_garbage()
    unreal.log_warning(
        f"NYC imported {min(first + 8, len(manifest['batches']))}/{len(manifest['batches'])} batches"
    )

map_path = settings["map_path"]
if unreal.EditorAssetLibrary.does_asset_exist(map_path):
    level = unreal.EditorLevelLibrary.load_level(map_path)
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if "NewYorkBrownstone" in [str(tag) for tag in actor.tags]:
            unreal.EditorLevelLibrary.destroy_actor(actor)
else:
    level = unreal.EditorLevelLibrary.new_level(map_path)
if not level:
    raise RuntimeError("Cannot create New York daytime map")
for batch in manifest["batches"]:
    mesh = unreal.load_asset(root + "/" + batch["name"])
    tile_class = unreal.load_class(None, "/Script/AerialWorldLab.CityTileActor")
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(tile_class, unreal.Vector(0, 0, 0))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_label(batch["name"])
    actor.tags = ["NewYorkBrownstone"]
    component = actor.static_mesh_component
    component.set_collision_profile_name("BlockAll")
    if not batch.get("collision", True):
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    # Dense foliage shadows are expensive on integrated GPUs. Buildings still
    # cast directional shadows; small leaf clusters receive daylight and skylight.
    component.set_cast_shadow(batch.get("cast_shadow", True))
    if batch.get("foliage", False):
        component.set_editor_property(
            "ld_max_draw_distance", float(settings["foliage_draw_distance_cm"])
        )
# Store daylight in the native level as well, so the editor opens to a lit
# neighborhood. Runtime GameMode reapplies this same daylight policy on play.
sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.DirectionalLight,
    unreal.Vector(0, 0, 1000),
    city_policy.get_editor_property("SunRotationDegrees"),
)
sun.tags = ["NewYorkBrownstone"]
sun.set_actor_label("NewYork_DaytimeSun")
sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
sun_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun_component.set_intensity(city_policy.get_editor_property("SunIlluminanceLux"))
sun_component.set_light_color(city_policy.get_editor_property("SunColor"))
sun_component.set_editor_property("atmosphere_sun_light", True)
atmosphere = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)
)
atmosphere.tags = ["NewYorkBrownstone"]
sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0))
sky.tags = ["NewYorkBrownstone"]
sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky_component.set_intensity(city_policy.get_editor_property("SkylightIntensity"))

exposure = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.PostProcessVolume, unreal.Vector(0, 0, 0)
)
exposure.tags = ["NewYorkBrownstone"]
exposure.set_actor_label("NewYork_DaytimeExposure")
exposure.set_editor_property("unbound", True)
settings = exposure.get_editor_property("settings")
for property_name, value in [
    ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
    ("auto_exposure_bias", 0.0),
    ("auto_exposure_apply_physical_camera_exposure", True),
    ("camera_iso", city_policy.get_editor_property("CameraISO")),
    (
        "camera_shutter_speed",
        city_policy.get_editor_property("CameraShutterSpeedReciprocalSeconds"),
    ),
    ("depth_of_field_fstop", city_policy.get_editor_property("CameraFStop")),
]:
    settings.set_editor_property("override_" + property_name, True)
    settings.set_editor_property(property_name, value)
exposure.set_editor_property("settings", settings)

launch = manifest["launch_position_cm"]
start = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.PlayerStart, unreal.Vector(*launch), unreal.Rotator(0, manifest["launch_yaw_degrees"], 0)
)
start.set_actor_label("NewYork_DroneLaunch")
start.tags = ["NewYorkBrownstone"]
unreal.EditorLevelLibrary.save_current_level()
unreal.log_warning("New York daytime native level saved")
