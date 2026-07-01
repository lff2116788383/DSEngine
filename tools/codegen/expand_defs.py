#!/usr/bin/env python3
"""
Expand binding_defs.json with:
1. Reflect metadata (range/tooltip/color) on existing 13 components
2. 35 new components from component_reflection.cpp
3. Enum definitions

This generates the expanded binding_defs.json that drives ALL codegen:
- C ABI / Lua / C# scripting (fields with script != false)
- Reflection registration (all fields)
- C# high-level wrappers
"""

import json
from pathlib import Path

# New components to add (from component_reflection.cpp)
# Each component has: name, prefix, include, namespace, fields[], enums[], conditional
NEW_COMPONENTS = [
    {
        "name": "DecalComponent",
        "prefix": "decal",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "color", "type": "vec4", "color": True},
            {"name": "angle_fade", "type": "float", "range": [0.0, 1.0]},
        ]
    },
    {
        "name": "SkyboxComponent",
        "prefix": "skybox",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "cubemap_path", "type": "string", "buffer_size": 512},
        ]
    },
    {
        "name": "FreeCameraControllerComponent",
        "prefix": "free_camera",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "move_speed", "type": "float", "range": [0.0, 100.0]},
            {"name": "mouse_sensitivity", "type": "float", "range": [0.0, 4.0]},
            {"name": "pitch", "type": "float"},
            {"name": "yaw", "type": "float"},
        ]
    },
    {
        "name": "SubSceneComponent",
        "prefix": "sub_scene",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "scene_path", "type": "string", "buffer_size": 512},
        ]
    },
    {
        "name": "BoundingBoxComponent",
        "prefix": "bounding_box",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "min_extents", "type": "vec3"},
            {"name": "max_extents", "type": "vec3"},
        ]
    },
    {
        "name": "WaterComponent",
        "prefix": "water",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "water_level", "type": "float"},
            {"name": "deep_color", "type": "vec4", "color": True},
            {"name": "shallow_color", "type": "vec4", "color": True},
            {"name": "max_depth", "type": "float", "range": [0.0, 1000.0]},
            {"name": "transparency", "type": "float", "range": [0.0, 1.0]},
            {"name": "wave_amplitude", "type": "float", "range": [0.0, 10.0]},
            {"name": "wave_frequency", "type": "float", "range": [0.0, 16.0]},
            {"name": "wave_speed", "type": "float", "range": [0.0, 16.0]},
            {"name": "wave_direction", "type": "vec3"},
            {"name": "refraction_strength", "type": "float", "range": [0.0, 1.0]},
            {"name": "reflection_strength", "type": "float", "range": [0.0, 1.0]},
            {"name": "specular_power", "type": "float", "range": [1.0, 512.0]},
            {"name": "caustic_intensity", "type": "float", "range": [0.0, 4.0]},
            {"name": "caustic_scale", "type": "float", "range": [0.0, 64.0]},
            {"name": "foam_intensity", "type": "float", "range": [0.0, 4.0]},
            {"name": "foam_depth_threshold", "type": "float", "range": [0.0, 100.0]},
            {"name": "underwater_fog_density", "type": "float", "range": [0.0, 1.0]},
            {"name": "underwater_fog_color", "type": "vec4", "color": True},
        ]
    },
    {
        "name": "LightProbeComponent",
        "prefix": "light_probe",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "influence_radius", "type": "float", "range": [0.0, 1000.0]},
            {"name": "show_debug", "type": "bool"},
        ]
    },
    {
        "name": "ReflectionProbeComponent",
        "prefix": "reflection_probe",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "influence_radius", "type": "float", "range": [0.0, 1000.0]},
            {"name": "box_size_x", "type": "float", "range": [0.0, 1000.0]},
            {"name": "box_size_y", "type": "float", "range": [0.0, 1000.0]},
            {"name": "box_size_z", "type": "float", "range": [0.0, 1000.0]},
            {"name": "use_box_projection", "type": "bool"},
            {"name": "resolution", "type": "int", "range": [16.0, 2048.0]},
            {"name": "show_debug", "type": "bool"},
        ]
    },
    {
        "name": "GIProbeVolumeComponent",
        "prefix": "gi_probe",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "origin", "type": "vec3"},
            {"name": "extent", "type": "vec3"},
            {"name": "resolution_x", "type": "int", "range": [1.0, 256.0]},
            {"name": "resolution_y", "type": "int", "range": [1.0, 256.0]},
            {"name": "resolution_z", "type": "int", "range": [1.0, 256.0]},
            {"name": "irradiance_texels", "type": "int", "range": [1.0, 64.0]},
            {"name": "visibility_texels", "type": "int", "range": [1.0, 64.0]},
            {"name": "rays_per_probe", "type": "int", "range": [1.0, 4096.0]},
            {"name": "hysteresis", "type": "float", "range": [0.0, 1.0]},
            {"name": "gi_intensity", "type": "float", "range": [0.0, 16.0]},
            {"name": "normal_bias", "type": "float", "range": [0.0, 4.0]},
            {"name": "view_bias", "type": "float", "range": [0.0, 4.0]},
            {"name": "show_debug_probes", "type": "bool"},
        ]
    },
    {
        "name": "FoliageComponent",
        "prefix": "foliage",
        "include": "engine/ecs/components_3d_foliage.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "wind_strength", "type": "float", "range": [0.0, 10.0]},
            {"name": "stiffness", "type": "float", "range": [0.0, 1.0]},
            {"name": "phase_offset", "type": "float"},
            {"name": "push_response", "type": "float", "range": [0.0, 4.0]},
        ]
    },
    {
        "name": "RigidBody3DComponent",
        "prefix": "rigidbody3d",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "enums": [
            {"name": "RigidBody3DType", "qualified": "dse::RigidBody3DType", "entries": [
                {"name": "Static", "value": "Static"},
                {"name": "Kinematic", "value": "Kinematic"},
                {"name": "Dynamic", "value": "Dynamic"},
            ]}
        ],
        "fields": [
            {"name": "type", "type": "enum", "enum_name": "RigidBody3DType"},
            {"name": "mass", "type": "float", "range": [0.0, 10000.0]},
            {"name": "drag", "type": "float", "range": [0.0, 100.0]},
            {"name": "angular_drag", "type": "float", "range": [0.0, 100.0]},
            {"name": "use_gravity", "type": "bool", "default": "true"},
            {"name": "gravity_scale", "type": "float", "range": [-10.0, 10.0]},
            {"name": "is_kinematic", "type": "bool"},
            {"name": "collision_layer", "type": "int"},
            {"name": "collision_mask", "type": "int"},
        ]
    },
    {
        "name": "BoxCollider3DComponent",
        "prefix": "box_collider3d",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "fields": [
            {"name": "size", "type": "vec3"},
            {"name": "center", "type": "vec3"},
            {"name": "is_trigger", "type": "bool"},
            {"name": "bounciness", "type": "float", "range": [0.0, 1.0]},
            {"name": "friction", "type": "float", "range": [0.0, 2.0]},
        ]
    },
    {
        "name": "SphereCollider3DComponent",
        "prefix": "sphere_collider3d",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "fields": [
            {"name": "radius", "type": "float", "range": [0.01, 1000.0]},
            {"name": "center", "type": "vec3"},
            {"name": "is_trigger", "type": "bool"},
            {"name": "bounciness", "type": "float", "range": [0.0, 1.0]},
            {"name": "friction", "type": "float", "range": [0.0, 2.0]},
        ]
    },
    {
        "name": "CapsuleCollider3DComponent",
        "prefix": "capsule_collider3d",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "fields": [
            {"name": "radius", "type": "float", "range": [0.01, 100.0]},
            {"name": "height", "type": "float", "range": [0.01, 100.0]},
            {"name": "center", "type": "vec3"},
            {"name": "direction", "type": "int", "range": [0.0, 2.0], "tooltip": "0=X, 1=Y, 2=Z"},
            {"name": "is_trigger", "type": "bool"},
            {"name": "bounciness", "type": "float", "range": [0.0, 1.0]},
            {"name": "friction", "type": "float", "range": [0.0, 2.0]},
        ]
    },
    {
        "name": "MeshCollider3DComponent",
        "prefix": "mesh_collider3d",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "fields": [
            {"name": "convex", "type": "bool"},
            {"name": "is_trigger", "type": "bool"},
            {"name": "bounciness", "type": "float", "range": [0.0, 1.0]},
            {"name": "friction", "type": "float", "range": [0.0, 2.0]},
        ]
    },
    {
        "name": "CharacterController3DComponent",
        "prefix": "character_ctrl3d",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "fields": [
            {"name": "radius", "type": "float", "range": [0.01, 10.0]},
            {"name": "height", "type": "float", "range": [0.01, 10.0]},
            {"name": "slope_limit", "type": "float", "range": [0.0, 90.0]},
            {"name": "step_offset", "type": "float", "range": [0.0, 5.0]},
            {"name": "skin_width", "type": "float", "range": [0.0, 1.0]},
            {"name": "min_move_distance", "type": "float", "range": [0.0, 1.0]},
        ]
    },
    {
        "name": "Joint3DComponent",
        "prefix": "joint3d",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "enums": [
            {"name": "Joint3DType", "qualified": "dse::Joint3DType", "entries": [
                {"name": "Fixed", "value": "Fixed"},
                {"name": "Hinge", "value": "Hinge"},
                {"name": "Spring", "value": "Spring"},
                {"name": "Distance", "value": "Distance"},
            ]}
        ],
        "fields": [
            {"name": "type", "type": "enum", "enum_name": "Joint3DType"},
            {"name": "connected_entity_id", "type": "int"},
            {"name": "anchor", "type": "vec3"},
            {"name": "connected_anchor", "type": "vec3"},
            {"name": "axis", "type": "vec3"},
            {"name": "use_limits", "type": "bool"},
            {"name": "lower_limit", "type": "float", "range": [-180.0, 0.0]},
            {"name": "upper_limit", "type": "float", "range": [0.0, 180.0]},
            {"name": "min_distance", "type": "float", "range": [0.0, 1000.0]},
            {"name": "max_distance", "type": "float", "range": [0.0, 1000.0]},
            {"name": "spring_stiffness", "type": "float", "range": [0.0, 100000.0]},
            {"name": "spring_damping", "type": "float", "range": [0.0, 10000.0]},
            {"name": "break_force", "type": "float", "range": [0.0, 3.4e+38]},
            {"name": "break_torque", "type": "float", "range": [0.0, 3.4e+38]},
        ]
    },
    {
        "name": "RagdollComponent",
        "prefix": "ragdoll",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "conditional": "DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT",
        "fields": [
            {"name": "active", "type": "bool"},
            {"name": "auto_setup", "type": "bool"},
            {"name": "total_mass", "type": "float", "range": [0.1, 1000.0]},
            {"name": "joint_stiffness", "type": "float", "range": [0.0, 10000.0]},
            {"name": "joint_damping", "type": "float", "range": [0.0, 10000.0]},
            {"name": "collision_layer", "type": "int"},
            {"name": "collision_mask", "type": "int"},
        ]
    },
    {
        "name": "SoftBodyComponent",
        "prefix": "soft_body",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "stiffness", "type": "float", "range": [0.0, 1.0]},
            {"name": "solver_iterations", "type": "int", "range": [1.0, 32.0]},
            {"name": "damping", "type": "float", "range": [0.0, 1.0]},
            {"name": "use_gravity", "type": "bool", "default": "true"},
            {"name": "gravity_scale", "type": "float", "range": [-10.0, 10.0]},
            {"name": "volume_stiffness", "type": "float", "range": [0.0, 1.0]},
        ]
    },
    {
        "name": "VehicleComponent",
        "prefix": "vehicle",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "conditional": "DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "max_engine_force", "type": "float", "range": [0.0, 100000.0]},
            {"name": "max_brake_force", "type": "float", "range": [0.0, 100000.0]},
            {"name": "max_steer_angle", "type": "float", "range": [0.0, 90.0]},
        ]
    },
    {
        "name": "RopeComponent",
        "prefix": "rope",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "segment_count", "type": "int", "range": [2.0, 200.0]},
            {"name": "segment_length", "type": "float", "range": [0.01, 10.0]},
            {"name": "radius", "type": "float", "range": [0.001, 1.0]},
            {"name": "damping", "type": "float", "range": [0.0, 1.0]},
            {"name": "solver_iterations", "type": "int", "range": [1.0, 32.0]},
            {"name": "use_gravity", "type": "bool", "default": "true"},
            {"name": "gravity_scale", "type": "float", "range": [-10.0, 10.0]},
            {"name": "anchor_entity_a", "type": "int"},
            {"name": "anchor_entity_b", "type": "int"},
            {"name": "anchor_offset_a", "type": "vec3"},
            {"name": "anchor_offset_b", "type": "vec3"},
            {"name": "start_position", "type": "vec3"},
        ]
    },
    {
        "name": "BuoyancyComponent",
        "prefix": "buoyancy",
        "include": "engine/ecs/components_3d_physics.h",
        "namespace": "dse",
        "conditional": "DSE_ENABLE_PHYSX || DSE_ENABLE_JOLT",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "water_level", "type": "float", "range": [-1000.0, 1000.0]},
            {"name": "use_fluid_system", "type": "bool"},
            {"name": "buoyancy_force", "type": "float", "range": [0.0, 1000.0]},
            {"name": "water_drag", "type": "float", "range": [0.0, 100.0]},
            {"name": "water_angular_drag", "type": "float", "range": [0.0, 100.0]},
            {"name": "submerge_depth", "type": "float", "range": [0.01, 100.0]},
        ]
    },
    {
        "name": "AtmosphereComponent",
        "prefix": "atmosphere",
        "include": "engine/ecs/components_3d_sky.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "planet_radius", "type": "float", "range": [100000.0, 100000000.0]},
            {"name": "atmosphere_height", "type": "float", "range": [1000.0, 1000000.0]},
            {"name": "rayleigh_coeff", "type": "vec3"},
            {"name": "rayleigh_scale_height", "type": "float", "range": [100.0, 100000.0]},
            {"name": "mie_coeff", "type": "float", "range": [0.0, 0.001]},
            {"name": "mie_scale_height", "type": "float", "range": [100.0, 10000.0]},
            {"name": "mie_g", "type": "float", "range": [-1.0, 1.0]},
            {"name": "mie_albedo", "type": "vec3"},
            {"name": "ozone_coeff", "type": "vec3"},
            {"name": "ozone_center_h", "type": "float", "range": [0.0, 100000.0]},
            {"name": "ozone_width", "type": "float", "range": [0.0, 100000.0]},
            {"name": "sun_intensity", "type": "float"},
            {"name": "sun_disk_angle", "type": "float", "range": [0.0, 5.0]},
            {"name": "aerial_perspective_enabled", "type": "bool"},
        ]
    },
    {
        "name": "VolumetricCloudComponent",
        "prefix": "volumetric_cloud",
        "include": "engine/ecs/components_3d_sky.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "cloud_bottom", "type": "float", "range": [0.0, 20000.0]},
            {"name": "cloud_top", "type": "float", "range": [0.0, 20000.0]},
            {"name": "coverage", "type": "float", "range": [0.0, 1.0]},
            {"name": "density", "type": "float", "range": [0.0, 1.0]},
            {"name": "shape_scale", "type": "float", "range": [0.0, 0.01]},
            {"name": "detail_scale", "type": "float", "range": [0.0, 0.01]},
            {"name": "detail_strength", "type": "float", "range": [0.0, 1.0]},
            {"name": "erosion", "type": "float", "range": [0.0, 1.0]},
            {"name": "wind_direction", "type": "vec3"},
            {"name": "wind_speed", "type": "float", "range": [0.0, 200.0]},
            {"name": "silver_intensity", "type": "float", "range": [0.0, 2.0]},
            {"name": "silver_spread", "type": "float", "range": [0.0, 1.0]},
            {"name": "powder_strength", "type": "float", "range": [0.0, 10.0]},
            {"name": "ambient_strength", "type": "float", "range": [0.0, 2.0]},
            {"name": "half_resolution", "type": "bool"},
            {"name": "temporal_reprojection", "type": "bool"},
            {"name": "cloud_shadow_enabled", "type": "bool"},
        ]
    },
    {
        "name": "DayNightCycleComponent",
        "prefix": "day_night",
        "include": "engine/ecs/components_3d_sky.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "time_of_day", "type": "float", "range": [0.0, 24.0]},
            {"name": "time_speed", "type": "float", "range": [0.0, 100.0]},
            {"name": "auto_advance", "type": "bool"},
            {"name": "latitude", "type": "float", "range": [-90.0, 90.0]},
            {"name": "longitude", "type": "float", "range": [-180.0, 180.0]},
            {"name": "day_of_year", "type": "int", "range": [1.0, 365.0]},
        ]
    },
    {
        "name": "HairComponent",
        "prefix": "hair",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "hair_asset_path", "type": "string", "buffer_size": 512},
            {"name": "damping", "type": "float", "range": [0.0, 1.0]},
            {"name": "stiffness_local", "type": "float", "range": [0.0, 1.0]},
            {"name": "stiffness_global", "type": "float", "range": [0.0, 1.0]},
            {"name": "gravity", "type": "float", "range": [0.0, 100.0]},
            {"name": "wind", "type": "vec3"},
            {"name": "wind_turbulence", "type": "float", "range": [0.0, 2.0]},
            {"name": "root_color", "type": "vec4", "color": True},
            {"name": "tip_color", "type": "vec4", "color": True},
            {"name": "fiber_radius", "type": "float", "range": [0.001, 0.5]},
            {"name": "opacity", "type": "float", "range": [0.0, 1.0]},
            {"name": "cast_shadow", "type": "bool"},
            {"name": "receive_shadow", "type": "bool"},
        ]
    },
    {
        "name": "ImpostorComponent",
        "prefix": "impostor",
        "include": "engine/ecs/components_3d_impostor.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "atlas_path", "type": "string", "buffer_size": 512, "tooltip": "Baked .dimpostor atlas file"},
            {"name": "frames_x", "type": "int", "range": [1, 32], "tooltip": "Atlas horizontal frames"},
            {"name": "frames_y", "type": "int", "range": [1, 16], "tooltip": "Atlas vertical frames"},
            {"name": "transition_distance", "type": "float", "range": [1.0, 10000.0]},
            {"name": "fade_range", "type": "float", "range": [0.1, 200.0]},
            {"name": "cull_distance", "type": "float", "range": [10.0, 50000.0]},
            {"name": "impostor_size", "type": "float", "range": [0.1, 10.0]},
            {"name": "pivot_offset", "type": "vec3"},
            {"name": "cast_shadow", "type": "bool"},
            {"name": "use_frame_interpolation", "type": "bool"},
            {"name": "normal_strength", "type": "float", "range": [0.0, 2.0]},
            {"name": "auto_from_lod_group", "type": "bool"},
        ]
    },
    {
        "name": "StreamingOriginComponent",
        "prefix": "streaming_origin",
        "include": "engine/scene/world_partition.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "load_radius", "type": "float", "range": [32.0, 2048.0], "tooltip": "Load radius"},
            {"name": "unload_radius", "type": "float", "range": [64.0, 4096.0], "tooltip": "Unload radius (should > load)"},
        ]
    },
    {
        "name": "WorldPartitionConfigComponent",
        "prefix": "world_partition",
        "include": "engine/scene/world_partition.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "cell_size", "type": "float", "range": [32.0, 1024.0], "tooltip": "Cell edge length"},
            {"name": "cells_directory", "type": "string", "buffer_size": 512},
            {"name": "grid_min_x", "type": "int"},
            {"name": "grid_max_x", "type": "int"},
            {"name": "grid_min_y", "type": "int"},
            {"name": "grid_max_y", "type": "int"},
            {"name": "max_loads_per_frame", "type": "int", "range": [1, 16]},
        ]
    },
    {
        "name": "HLODConfigComponent",
        "prefix": "hlod_config",
        "include": "engine/render/hlod/hlod_system.h",
        "namespace": "dse::render",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "hlod_data_path", "type": "string", "buffer_size": 512, "tooltip": ".dhlod data file"},
            {"name": "distance_scale", "type": "float", "range": [0.1, 10.0]},
            {"name": "hysteresis", "type": "float", "range": [0.0, 0.5]},
        ]
    },
    {
        "name": "VirtualTextureComponent",
        "prefix": "virtual_texture",
        "include": "engine/render/virtual_texture/virtual_texture.h",
        "namespace": "dse::vt",
        "fields": [
            {"name": "enabled", "type": "bool", "default": "true"},
            {"name": "vt_id", "type": "int"},
            {"name": "tile_data_path", "type": "string", "buffer_size": 512, "tooltip": "VT tile data directory"},
            {"name": "virtual_width", "type": "int"},
            {"name": "virtual_height", "type": "int"},
            {"name": "mip_bias", "type": "float", "range": [-4.0, 4.0]},
        ]
    },
    {
        "name": "LightmapComponent",
        "prefix": "lightmap",
        "include": "engine/render/gi/lightmap_baker.h",
        "namespace": "dse::render",
        "fields": [
            {"name": "lightmap_path", "type": "string", "buffer_size": 512, "tooltip": ".dlightmap file path"},
            {"name": "intensity", "type": "float", "range": [0.0, 5.0]},
            {"name": "st_offset", "type": "vec4", "tooltip": "UV scale(xy) + offset(zw)"},
            {"name": "use_ao", "type": "bool", "tooltip": "Apply AO channel"},
        ]
    },
]

# Metadata additions for existing 13 components
EXISTING_METADATA = {
    "Camera3DComponent": {
        "extra_fields": [
            {"name": "aspect_ratio", "type": "float", "range": [0.1, 10.0], "script": False},
        ],
        "field_metadata": {
            "fov": {"range": [1.0, 179.0]},
            "near_clip": {"range": [0.001, 100.0]},
            "far_clip": {"range": [1.0, 100000.0]},
        }
    },
    "MeshRendererComponent": {
        "extra_fields": [
            {"name": "ao", "type": "float", "range": [0.0, 1.0], "script": False},
            {"name": "normal_strength", "type": "float", "range": [0.0, 4.0], "script": False},
            {"name": "material_alpha_cutoff", "type": "float", "range": [0.0, 1.0], "script": False},
            {"name": "material_alpha_test", "type": "bool", "script": False},
            {"name": "material_double_sided", "type": "bool", "script": False},
            {"name": "sss_strength", "type": "float", "range": [0.0, 1.0], "script": False},
            {"name": "sss_tint", "type": "vec4", "color": True, "script": False},
            {"name": "clear_coat", "type": "float", "range": [0.0, 1.0], "script": False},
            {"name": "clear_coat_roughness", "type": "float", "range": [0.0, 1.0], "script": False},
            {"name": "anisotropy", "type": "float", "range": [-1.0, 1.0], "script": False},
            {"name": "pom_height_scale", "type": "float", "range": [0.0, 1.0], "script": False},
            {"name": "depth_test_enabled", "type": "bool", "script": False},
            {"name": "depth_write_enabled", "type": "bool", "script": False},
            {"name": "is_static", "type": "bool", "script": False},
            {"name": "sorting_layer", "type": "int", "script": False},
            {"name": "order_in_layer", "type": "int", "script": False},
        ],
        "field_metadata": {
            "color": {"color": True},
            "emissive": {"color": True},
            "metallic": {"range": [0.0, 1.0]},
            "roughness": {"range": [0.0, 1.0]},
        }
    },
    "DirectionalLight3DComponent": {
        "extra_fields": [
            {"name": "cascade_split_lambda", "type": "float", "range": [0.0, 1.0], "script": False},
        ],
        "field_metadata": {
            "color": {"color": True},
            "intensity": {"range": [0.0, 100.0]},
            "ambient_intensity": {"range": [0.0, 4.0]},
            "shadow_strength": {"range": [0.0, 1.0]},
        }
    },
    "PointLightComponent": {
        "extra_fields": [
            {"name": "falloff", "type": "float", "range": [0.0, 8.0], "script": False},
        ],
        "field_metadata": {
            "color": {"color": True},
            "intensity": {"range": [0.0, 100.0]},
            "radius": {"range": [0.0, 1000.0]},
        }
    },
    "SpotLightComponent": {
        "extra_fields": [
            {"name": "falloff", "type": "float", "range": [0.0, 8.0], "script": False},
        ],
        "field_metadata": {
            "color": {"color": True},
            "intensity": {"range": [0.0, 100.0]},
            "radius": {"range": [0.0, 1000.0]},
            "inner_cone_angle": {"range": [0.0, 90.0]},
            "outer_cone_angle": {"range": [0.0, 90.0]},
        }
    },
    "SkyLightComponent": {
        "field_metadata": {
            "up_color": {"color": True},
            "down_color": {"color": True},
            "intensity": {"range": [0.0, 16.0]},
        }
    },
    "TreeComponent": {
        "field_metadata": {
            "density": {"range": [0.0, 1.0], "tooltip": "Trees per square meter"},
            "spawn_radius": {"range": [1.0, 1000.0]},
            "chunk_size": {"range": [1.0, 256.0]},
        }
    },
    "TerrainTileManagerComponent": {
        "field_metadata": {
            "tile_world_size": {"range": [1.0, 1024.0]},
            "tile_resolution": {"range": [8.0, 512.0]},
            "max_height": {"range": [0.0, 10000.0]},
            "max_lod_levels": {"range": [1.0, 8.0]},
            "lod_distance_factor": {"range": [1.0, 1000.0]},
            "load_radius": {"range": [1.0, 10000.0]},
            "unload_radius": {"range": [1.0, 10000.0]},
        }
    },
    "DynamicObstacleComponent": {
        "field_metadata": {
            "cylinder_radius": {"range": [0.0, 100.0]},
            "cylinder_height": {"range": [0.0, 100.0]},
        }
    },
    "NavMeshAutoRebakeComponent": {
        "field_metadata": {
            "tile_size": {"range": [1.0, 256.0]},
            "rebake_cooldown": {"range": [0.0, 60.0]},
            "agent_height": {"range": [0.1, 10.0]},
            "agent_radius": {"range": [0.1, 10.0]},
            "agent_max_climb": {"range": [0.0, 10.0]},
            "agent_max_slope": {"range": [0.0, 90.0]},
            "cell_size": {"range": [0.01, 10.0]},
            "cell_height": {"range": [0.01, 10.0]},
        }
    },
}


# Components that only need reflection registration (no scripting bindings)
REFLECT_ONLY_COMPONENTS = [
    {
        "name": "GrassComponent",
        "include": "engine/ecs/components_3d_foliage.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool"},
            {"name": "density", "type": "float", "range": [0.0, 16.0], "tooltip": "Grass blades per unit area"},
            {"name": "spawn_radius", "type": "float", "range": [1.0, 500.0]},
            {"name": "seed", "type": "int"},
            {"name": "chunk_size", "type": "float", "range": [1.0, 64.0]},
            {"name": "blade_width", "type": "float", "range": [0.0, 2.0]},
            {"name": "blade_height", "type": "float", "range": [0.0, 8.0]},
            {"name": "blade_height_variation", "type": "float", "range": [0.0, 1.0]},
            {"name": "base_color", "type": "vec4", "color": True},
            {"name": "tip_color", "type": "vec4", "color": True},
            {"name": "wind_direction", "type": "vec3"},
            {"name": "wind_speed", "type": "float", "range": [0.0, 10.0]},
            {"name": "wind_strength", "type": "float", "range": [0.0, 4.0]},
            {"name": "wind_turbulence", "type": "float", "range": [0.0, 4.0]},
            {"name": "lod_near", "type": "float", "range": [0.0, 500.0]},
            {"name": "lod_far", "type": "float", "range": [0.0, 500.0]},
            {"name": "fade_range", "type": "float", "range": [0.0, 50.0]},
            {"name": "cast_shadow", "type": "bool"},
            {"name": "shadow_distance", "type": "float", "range": [0.0, 200.0]},
        ]
    },
    {
        "name": "LODGroupComponent",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "nested_types": [
            {
                "name": "LODLevelConfig",
                "qualified": "dse::LODLevelConfig",
                "fields": [
                    {"name": "mesh_path", "type": "string"},
                    {"name": "screen_size_threshold", "type": "float", "range": [0.0, 1.0]},
                ]
            }
        ],
        "fields": [
            {"name": "enabled", "type": "bool"},
            {"name": "levels", "type": "custom"},
            {"name": "global_scale", "type": "float", "range": [0.0, 100.0]},
            {"name": "hysteresis", "type": "float", "range": [0.0, 1.0]},
            {"name": "min_screen_size", "type": "float", "range": [0.0, 1.0]},
            {"name": "original_mesh_path", "type": "string"},
        ]
    },
    {
        "name": "MorphTargetComponent",
        "include": "engine/ecs/components_3d_render.h",
        "namespace": "dse",
        "fields": [
            {"name": "enabled", "type": "bool"},
        ]
    },
    {
        "name": "GpuParticleComponent",
        "include": "engine/render/particles/gpu_particle_system.h",
        "namespace": "dse",
        "fields": []
    },
    {
        "name": "HLODMemberComponent",
        "include": "engine/render/hlod/hlod_system.h",
        "namespace": "dse::render",
        "fields": [
            {"name": "cluster_index", "type": "int"},
            {"name": "hidden_by_hlod", "type": "bool"},
        ]
    },
]


def main():
    script_dir = Path(__file__).resolve().parent
    defs_path = script_dir / "binding_defs.json"

    with open(defs_path, encoding="utf-8") as f:
        defs = json.load(f)

    # 1. Add metadata to existing components
    for comp in defs["components"]:
        name = comp["name"]
        if name in EXISTING_METADATA:
            meta = EXISTING_METADATA[name]
            # Add range/tooltip/color to existing fields
            if "field_metadata" in meta:
                for field in comp["fields"]:
                    if field["name"] in meta["field_metadata"]:
                        field.update(meta["field_metadata"][field["name"]])
            # Add extra reflection-only fields
            if "extra_fields" in meta:
                comp.setdefault("reflect_extra_fields", []).extend(meta["extra_fields"])

    # 2. Add new components
    for new_comp in NEW_COMPONENTS:
        new_comp.setdefault("lua_table", "ecs")
        # Auto-generate lua_getter/lua_setter for ALL fields
        for f in new_comp["fields"]:
            if "lua_getter" not in f:
                f["lua_getter"] = f"get_{new_comp['prefix']}_{f['name']}"
                f["lua_setter"] = f"set_{new_comp['prefix']}_{f['name']}"
            if f["type"] == "string":
                f.setdefault("buffer_size", 256)
            # Enum fields map to int for scripting
            if f["type"] == "enum":
                f["type"] = "int"
        defs["components"].append(new_comp)

    # 3. Add reflect-only components (no scripting bindings, only reflection)
    defs["reflect_only_components"] = REFLECT_ONLY_COMPONENTS

    # Write expanded file
    out_path = script_dir / "binding_defs.json"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(defs, f, indent=4, ensure_ascii=False)

    print(f"[expand_defs] Done: {len(defs['components'])} components total")
    print(f"  - Original: 13")
    print(f"  - New: {len(NEW_COMPONENTS)}")
    print(f"  - Reflect-only: {len(REFLECT_ONLY_COMPONENTS)}")


if __name__ == "__main__":
    main()
