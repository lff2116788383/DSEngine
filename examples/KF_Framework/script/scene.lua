--------------------------------------------------------------------------------
-- KF_Framework — 场景搭建 (Phase 1)
-- 从 demo.stage 二进制精确提取的位置 + 旋转
--------------------------------------------------------------------------------
local Config = require("script.config")
local ASSET = Config.ASSET

local ecs = dse.ecs

local Scene = {}

function Scene.setup()
    -- 1. Directional light — exact KF parameters:
    --    light_diffuse = (0.8, 0.8, 0.8), ambient = Color::kGray=(0.5,0.5,0.5)
    --    direction = (-1, -4, +1).Normalized() → DSE Z-flip → (-1, -4, -1)
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 0, 0)
    local ld = math.sqrt(1+16+1)
    ecs.add_directional_light_3d(sun,
        -1.0/ld, -4.0/ld, -1.0/ld,
         0.8, 0.8, 0.8, 1.0, 0.50, 1.0)
    -- KF: offset=(20,80,-20), range=20, far=200, bias=1e-5
    -- DSE CSM: cascade_splits ×100 of KF far=200 → 20000
    ecs.set_directional_light_shadow(sun, true, 1.0, 800, 4000, 20000)

    -- 2. Sky light
    local sky_light = ecs.create_entity()
    ecs.add_transform(sky_light, 0, 0, 0)
    -- KF 无 sky_light，降低 intensity 补偿 PBR 与 HL 的差异
    ecs.add_sky_light(sky_light, 0.38, 0.45, 0.55, 0.12, 0.11, 0.10, 0.3)

    -- 3. Skybox (全景图) — rotate 180° to compensate for camera Z-flip
    local skybox_ent = ecs.create_entity()
    ecs.add_transform(skybox_ent, 0, 0, 0)
    ecs.set_transform_rotation(skybox_ent, 0, 180, 0)
    ecs.add_skybox(skybox_ent, ASSET.skybox_pano)

    -- 4. Ground (demoField.mesh from KF, converted to .dmesh)
    -- kf_to_gltf.py already flipped Z in vertex data; no need to flip again
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 100, 100, 100)
    ecs.add_mesh_renderer(ground, 1.0, 1.0, 1.0, 1.0)
    ecs.set_mesh_path(ground, "cooked/demoField.dmesh")
    ecs.set_mesh_shader_variant(ground, "MESH_HALFLAMBERT_STATIC")
    ecs.set_mesh_material(ground, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, true, false)
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
        ["cooked/Mesh1_0.dmesh"]       = "assets/textures/stage/Medieval house_wall.jpg",
        ["cooked/Mesh1_1.dmesh"]       = "assets/textures/stage/Medieval house_wood.jpg",
        ["cooked/Mesh1_2.dmesh"]       = "assets/textures/stage/Medieval house_door.jpg",
        ["cooked/Mesh1_3.dmesh"]       = "assets/textures/stage/Medieval house_end.jpg",
        ["cooked/Mesh1_4.dmesh"]       = "assets/textures/stage/Medieval house_window.jpg",
        ["cooked/Mesh1_5.dmesh"]       = "assets/textures/stage/Medieval house_concrete.jpg",
        ["cooked/Mesh1_6.dmesh"]       = "assets/textures/stage/Medieval house_roof.jpg",
        ["cooked/Model_0.dmesh"]       = "assets/textures/stage/Bridge_1.jpg",
        ["cooked/Model_1.dmesh"]       = "assets/textures/stage/Bridge_Main.jpg",
        ["cooked/Model_2.dmesh"]       = "assets/textures/stage/Bridge_3.jpg",
        ["cooked/Model_3.dmesh"]       = "assets/textures/stage/Bridge_2.jpg",
        ["cooked/Ornament_0.dmesh"]    = "assets/textures/stage/WindmillAtlas.tga",
        ["cooked/Rock-1_0.dmesh"]      = "assets/textures/stage/pine_diffuse.jpg",
        ["cooked/Rock-2_0.dmesh"]      = "assets/textures/stage/pine_diffuse.jpg",
        ["cooked/Rock-3_0.dmesh"]      = "assets/textures/stage/Stone2_diffuse.jpg",
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

    -- 6. 建筑碰撞体 (BoxCollider3D, 替代 player.lua 硬编码 AABB)
    local building_colliders = {
        {cx=2500, cz=900,  sx=1000, sy=1500, sz=1000},  -- Medieval house (castle)
        {cx=4900, cz=-750, sx=800,  sy=1500, sz=900},   -- Baker_house
        {cx=6150, cz=-1750,sx=900,  sy=1500, sz=900},   -- Medieval_house_1
        {cx=1650, cz=-7950,sx=900,  sy=1500, sz=900},   -- Fancy_Tavern
        {cx=4750, cz=-8200,sx=900,  sy=1500, sz=800},   -- House
        {cx=35,   cz=-6006,sx=370,  sy=500,  sz=390},   -- cartoon_well
    }
    for _, b in ipairs(building_colliders) do
        local coll = ecs.create_entity()
        ecs.add_transform(coll, b.cx, b.sy * 0.5, b.cz)
        ecs.add_box_collider_3d(coll, b.sx, b.sy, b.sz)
    end

    -- 7. Post-processing
    local pp = ecs.create_entity()
    ecs.add_post_process(pp, false, 1.0, 0.8, 1.0)
end

return Scene
