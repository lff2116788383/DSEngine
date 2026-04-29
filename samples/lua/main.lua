local config_module = require("config")
local Config = config_module
if type(Config) ~= "table" then
    Config = _G.Config or {}
end

local RuntimeEntry = nil
local runtime_config = nil
local game_entry = type(Config.game_entry) == "string" and Config.game_entry or "phase1_2d_physics_showcase"
if game_entry == "3d_triangle" then
    RuntimeEntry = require("3d.triangle")
    runtime_config = Config.basic_3d or {}
elseif game_entry == "3d_square" then
    RuntimeEntry = require("3d.square")
    runtime_config = Config.basic_3d or {}
elseif game_entry == "3d_cube" then
    RuntimeEntry = require("3d.cube")
    runtime_config = Config.basic_3d or {}
elseif game_entry == "3d_static_model" then
    RuntimeEntry = require("3d.3d_static_model")
    runtime_config = Config.demo_3d_static_model or Config.basic_3d or {}
elseif game_entry == "3d_material_showcase" then
    RuntimeEntry = require("3d.3d_material_showcase")
    runtime_config = Config.demo_3d_material_showcase or Config.basic_3d or {}
elseif game_entry == "3d_lighting_showcase" then
    RuntimeEntry = require("3d.3d_lighting_showcase")
    runtime_config = Config.demo_3d_lighting_showcase or Config.basic_3d or {}
elseif game_entry == "3d_camera_showcase" then
    RuntimeEntry = require("3d.3d_camera_showcase")
    runtime_config = Config.demo_3d_camera_showcase or Config.basic_3d or {}
elseif game_entry == "3d_textured_cube" then
    RuntimeEntry = require("3d.3d_textured_cube")
    runtime_config = Config.demo_3d_textured_cube or Config.basic_3d or {}
elseif game_entry == "3d_scene_showcase" then
    RuntimeEntry = require("3d.3d_scene_showcase")
    runtime_config = Config.demo_3d_scene_showcase or Config.basic_3d or {}
elseif game_entry == "3d_skybox_environment" then
    RuntimeEntry = require("3d.3d_skybox_environment")
    runtime_config = Config.demo_3d_skybox_environment or Config.basic_3d or {}
elseif game_entry == "3d_postprocess_showcase" then
    RuntimeEntry = require("3d.3d_postprocess_showcase")
    runtime_config = Config.demo_3d_postprocess_showcase or Config.basic_3d or {}
elseif game_entry == "3d_particles_showcase" then
    RuntimeEntry = require("3d.3d_particles_showcase")
    runtime_config = Config.demo_3d_particles_showcase or Config.basic_3d or {}
elseif game_entry == "3d_physics_stack" then
    RuntimeEntry = require("3d.3d_physics_stack")
    runtime_config = Config.demo_3d_physics_stack or Config.basic_3d or {}
elseif game_entry == "3d_terrain_heightmap" then
    RuntimeEntry = require("3d.3d_terrain_heightmap")
    runtime_config = Config.demo_3d_terrain_heightmap or Config.basic_3d or {}
elseif game_entry == "3d_shadow_showcase" then
    RuntimeEntry = require("3d.3d_shadow_showcase")
    runtime_config = Config.demo_3d_shadow_showcase or Config.basic_3d or {}
elseif game_entry == "3d_animation_basic" then
    RuntimeEntry = require("3d.3d_animation_basic")
    runtime_config = Config.demo_3d_animation_basic or Config.basic_3d or {}
elseif game_entry == "3d_character_third_person" then
    RuntimeEntry = require("3d.3d_character_third_person")
    runtime_config = Config.demo_3d_character_third_person or Config.basic_3d or {}
elseif game_entry == "3d_audio_spatial" then
    RuntimeEntry = require("3d.3d_audio_spatial")
    runtime_config = Config.demo_3d_audio_spatial or Config.basic_3d or {}
elseif game_entry == "3d_physics_raycast_pick" then
    RuntimeEntry = require("3d.3d_physics_raycast_pick")
    runtime_config = Config.demo_3d_physics_raycast_pick or Config.basic_3d or {}
elseif game_entry == "3d_texture_material_slots" then
    RuntimeEntry = require("3d.3d_texture_material_slots")
    runtime_config = Config.demo_3d_texture_material_slots or Config.basic_3d or {}
elseif game_entry == "3d_terrain_lod_zones" then
    RuntimeEntry = require("3d.3d_terrain_lod_zones")
    runtime_config = Config.demo_3d_terrain_lod_zones or Config.basic_3d or {}
elseif game_entry == "phase1_2d_physics_showcase" then
    RuntimeEntry = require("phase1_2d_physics_showcase")
    runtime_config = Config.phase1_2d_physics_showcase or {}
elseif game_entry == "frog_jump" then
    RuntimeEntry = require("frog_jump")
    runtime_config = Config.frog_jump or {}
elseif game_entry == "phase1_2d_mvp" then
    RuntimeEntry = require("phase1_2d_mvp")
    runtime_config = Config.phase1_2d or {}
elseif game_entry == "phase1_2d_showcase" then
    RuntimeEntry = require("phase1_2d_showcase")
    runtime_config = Config.phase1_2d_showcase or {}
elseif game_entry == "vse_demo_15_7" then
    RuntimeEntry = require("vse_demo.demo15_7")
    runtime_config = Config.vse_demo_15_7 or {}
elseif game_entry == "vse_demo_15_8" then
    RuntimeEntry = require("vse_demo.demo15_8")
    runtime_config = Config.vse_demo_15_8 or {}
elseif game_entry == "vse_demo_15_9" then
    RuntimeEntry = require("vse_demo.demo15_9")
    runtime_config = Config.vse_demo_15_9 or {}
else
    RuntimeEntry = require("phase2_3d_mvp")
    runtime_config = Config.phase2_3d or {}
end

function Awake()
    if type(Config.title) == "string" and Config.title ~= "" then
        dse.app.set_window_title(Config.title)
    end
    if type(Config.data_path) == "string" and Config.data_path ~= "" then
        dse.app.set_data_root(Config.data_path)
    end
    RuntimeEntry.Setup(runtime_config or {})
end

function Update(delta_time)
    RuntimeEntry.Update(delta_time)
end

function exit()
end

function main()
    Awake()
end
