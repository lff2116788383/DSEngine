--------------------------------------------------------------------------------
-- KF_Framework — 场景搭建 (Phase 1)
-- 从 demo.stage 二进制精确提取的位置 + 旋转
--------------------------------------------------------------------------------
local Config = require("script.config")
local ASSET = Config.ASSET

local ecs = dse.ecs

local Scene = {}

function Scene.setup()
    -- 1. Directional light (boosted to match KF's DX9 fixed-pipeline brightness)
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 0, 0)
    local ld = math.sqrt(1+16+1)
    ecs.add_directional_light_3d(sun,
        -1.0/ld, -4.0/ld, -1.0/ld,
         0.92, 0.88, 0.84, 0.85, 0.30, 0.35)
    ecs.set_directional_light_shadow(sun, true, 1.0, 800, 3000, 15000)

    -- 2. Sky light
    local sky_light = ecs.create_entity()
    ecs.add_transform(sky_light, 0, 0, 0)
    ecs.add_sky_light(sky_light, 0.38, 0.45, 0.55, 0.12, 0.11, 0.10, 1.1)

    -- 3. Skybox (全景图) — rotate 180° to compensate for camera Z-flip
    local skybox_ent = ecs.create_entity()
    ecs.add_transform(skybox_ent, 0, 0, 0)
    ecs.set_transform_rotation(skybox_ent, 0, 180, 0)
    ecs.add_skybox(skybox_ent, ASSET.skybox_pano)

    -- 4. Ground (demoField.mesh from KF, converted to .dmesh)
    -- Z-flip: dmesh retains KF's original Z; scale_z=-100 flips to DSE coordinates
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 100, 100, -100)
    ecs.add_mesh_renderer(ground, 1.0, 1.0, 1.0, 1.0)
    ecs.set_mesh_path(ground, "cooked/demoField.dmesh")
    ecs.set_mesh_shader_variant(ground, "MESH_HALFLAMBERT_STATIC")
    ecs.set_mesh_material(ground, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    ecs.set_mesh_texture(ground, "albedo", ASSET.ground_tex)

    -- 5. 场景装饰物 — 从 DSE 原生 JSON 场景加载 (demo.stage 精确位置)
    local ok, count = ecs.load_sub_scene("scenes/kf_demo_stage.json")
    if ok then
        print("[Scene] Loaded kf_demo_stage.json: " .. tostring(count) .. " entities")
    else
        print("[Scene] WARNING: Failed to load kf_demo_stage.json: " .. tostring(count))
    end

    -- 为加载的 mesh 实体设置纹理 (texture handles 是运行时概念, JSON 无法存储路径)
    local tex_map = {
        ["cooked/Baker_house_0.dmesh"] = "assets/textures/stage/Baker_house.jpg",
        ["cooked/Decor_0.dmesh"]       = "assets/textures/stage/Fancy_Tavern_Decor.png",
        ["cooked/Ext_0.dmesh"]         = "assets/textures/stage/WindmillAtlas.tga",
        ["cooked/Fan_0.dmesh"]         = "assets/textures/stage/WindmillAtlas.tga",
        ["cooked/Fancy_tavern_0.dmesh"]= "assets/textures/stage/Fancy_Tavern.jpg",
        ["cooked/Fence_0.dmesh"]       = "assets/textures/stage/fence.jpg",
        ["cooked/Lamp_0.dmesh"]        = "assets/textures/stage/WindmillAtlas.tga",
        ["cooked/Med_house_0.dmesh"]   = "assets/textures/stage/Medieval_house_1_House_D.tga",
        ["cooked/Mesh1_0.dmesh"]       = "assets/textures/stage/Wall.jpg",
        ["cooked/Mesh1_1.dmesh"]       = "assets/textures/stage/Medieval house_wood.jpg",
        ["cooked/Mesh1_2.dmesh"]       = "assets/textures/stage/Medieval house_door.jpg",
        ["cooked/Mesh1_3.dmesh"]       = "assets/textures/stage/Medieval house_end.jpg",
        ["cooked/Mesh1_4.dmesh"]       = "assets/textures/stage/Medieval house_window.jpg",
        ["cooked/Mesh1_5.dmesh"]       = "assets/textures/stage/Medieval house_concrete.jpg",
        ["cooked/Mesh1_6.dmesh"]       = "assets/textures/stage/Roof.jpg",
        ["cooked/Model_0.dmesh"]       = "assets/textures/stage/Bridge.jpg",
        ["cooked/Model_1.dmesh"]       = "assets/textures/stage/Bridge_Main.jpg",
        ["cooked/Model_2.dmesh"]       = "assets/textures/stage/road_stone.jpg",
        ["cooked/Model_3.dmesh"]       = "assets/textures/stage/wall.jpg",
        ["cooked/Ornament_0.dmesh"]    = "assets/textures/stage/WindmillAtlas.tga",
        ["cooked/Rock-1_0.dmesh"]      = "assets/textures/stage/RockCliff.jpg",
        ["cooked/Rock-2_0.dmesh"]      = "assets/textures/stage/RockCliff.jpg",
        ["cooked/Rock-3_0.dmesh"]      = "assets/textures/stage/RockCliff.jpg",
        ["cooked/WindMill_0.dmesh"]    = "assets/textures/stage/WindmillAtlas.tga",
        ["cooked/house_0.dmesh"]       = "assets/textures/stage/house.jpg",
        ["cooked/tree_1_canopy_0.dmesh"] = "assets/textures/stage/branch.tga",
        ["cooked/tree_1_trunk_0.dmesh"]  = "assets/textures/stage/trunk.jpg",
        ["cooked/w_bin_0.dmesh"]       = "assets/textures/stage/well.jpg",
        ["cooked/w_bin_1.dmesh"]       = "assets/textures/stage/well.jpg",
        ["cooked/w_bn_0.dmesh"]        = "assets/textures/stage/well.jpg",
        ["cooked/water_0.dmesh"]       = "assets/textures/stage/well.jpg",
    }
    for mesh_path, tex_path in pairs(tex_map) do
        local found = ecs.find_entities_by_mesh_path(mesh_path)
        for _, e in ipairs(found) do
            ecs.set_mesh_texture(e, "albedo", tex_path)
        end
    end

    -- 6. Post-processing
    local pp = ecs.create_entity()
    ecs.add_post_process(pp, false, 1.0, 0.8, 1.0)
end

return Scene
