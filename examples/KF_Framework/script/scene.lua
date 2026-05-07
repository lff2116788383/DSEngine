--------------------------------------------------------------------------------
-- KF_Framework — 场景搭建 (Phase 1)
-- 从 demo.stage 二进制精确提取的位置 + 旋转
--------------------------------------------------------------------------------
local Config = require("script.config")
local ASSET = Config.ASSET

local ecs = dse.ecs

local Scene = {}

-- 辅助
local function cube_verts()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
             -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end
local function cube_idx()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local decorations = {}

local function add_mesh(mesh_path, x, y, z, sx, sy, sz, ry, tex_path)
    local e = ecs.create_entity()
    ecs.add_transform(e, x, y, z, sx, sy, sz)
    if ry and ry ~= 0 then
        ecs.set_transform_rotation(e, 0, ry, 0)
    end
    ecs.add_mesh_renderer(e, 1.0, 1.0, 1.0, 1.0)
    ecs.set_mesh_path(e, mesh_path)
    ecs.set_mesh_shader_variant(e, "MESH_LIT")
    ecs.set_mesh_material(e, 0.0, 0.55, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    if tex_path then
        ecs.set_mesh_texture(e, "albedo", tex_path)
    end
    table.insert(decorations, e)
    return e
end

function Scene.setup()
    -- 1. Directional light
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 0, 0)
    local ld = math.sqrt(1+16+1)
    ecs.add_directional_light_3d(sun,
        -1.0/ld, -4.0/ld, -1.0/ld,
         0.8, 0.8, 0.8, 1.5, 0.3, 0.35)
    ecs.set_directional_light_shadow(sun, true, 0.4, 800, 3000, 15000)

    -- 2. Sky light
    local sky_light = ecs.create_entity()
    ecs.add_transform(sky_light, 0, 0, 0)
    ecs.add_sky_light(sky_light, 0.35, 0.45, 0.60, 0.10, 0.10, 0.08, 1.2)

    -- 3. Skybox (全景图)
    local skybox_ent = ecs.create_entity()
    ecs.add_transform(skybox_ent, 0, 0, 0)
    ecs.add_skybox(skybox_ent, ASSET.skybox_pano)

    -- 4. Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, -2, 0, 20000, 4, 20000)
    ecs.add_mesh_renderer(ground, 0.28, 0.35, 0.22, 1.0, cube_verts(), cube_idx())
    ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    ecs.set_mesh_material(ground, 0.0, 0.8, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    ecs.set_mesh_texture(ground, "albedo", ASSET.ground_tex)

    -- 5. 场景装饰物 (demo.stage 精确位置, 战斗区中心偏移 (10,0,15))
    -- Buildings
    add_mesh(ASSET.baker_house, 3915, 0, 743, 1, 1, 1, -145, ASSET.tex_baker)
    add_mesh(ASSET.tavern, 661, 0, -6455, 1, 1, 1, -63, ASSET.tex_tavern)
    add_mesh(ASSET.med_house1, 5145, 0, -287, 1, 1, 1, -114, ASSET.tex_med_house1)
    add_mesh(ASSET.windmill, 8449, 1920, -7212, 1, 1, 1, 38, ASSET.tex_windmill)
    add_mesh(ASSET.med_house, 1499, 0, 2371, 1, 1, 1, 19, nil)
    add_mesh(ASSET.med_house1, 3741, 0, -6697, 1, 1, 1, -157, ASSET.tex_med_house1)
    add_mesh(ASSET.bridge, -3774, 0, 3767, 1, 1, 1, -38, nil)
    add_mesh(ASSET.well, -965, 318, -4507, 1, 1, 1, 0, nil)

    -- Fences
    add_mesh(ASSET.fence, -1284, 0, -1943, 1, 1, 1, -44, ASSET.tex_fence)
    add_mesh(ASSET.fence, -732, 51, -2534, 1, 1, 1, -44, ASSET.tex_fence)
    add_mesh(ASSET.fence, -186, 128, -3118, 1, 1, 1, -44, ASSET.tex_fence)
    add_mesh(ASSET.fence, 306, 156, -3644, 1, 1, 1, -56, ASSET.tex_fence)

    -- Rocks
    add_mesh(ASSET.rock1, 2241, 0, -4877, 1, 1, 1, 0, ASSET.tex_rock)
    add_mesh(ASSET.rock2, 2657, 407, -250, 1, 1, 1, 0, ASSET.tex_rock)
    add_mesh(ASSET.rock3, -1002, 165, -3674, 1, 1, 1, 0, ASSET.tex_rock)

    -- Trees (pine FBX 异常大, scale=0.1)
    local ts = 0.1
    add_mesh(ASSET.pine_tree, 2793, 333, 326, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, -282, 0, -5523, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, -1860, 0, -3863, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, -2206, 0, -823, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, -373, 2, 2058, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, 7813, 1274, -2270, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, 322, 654, 4383, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, -1031, 434, 6239, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, -2030, 0, -1627, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, -1898, 0, -2719, ts, ts, ts, 0, ASSET.tex_pine)
    add_mesh(ASSET.pine_tree, -944, 84, -3107, ts, ts, ts, 0, ASSET.tex_pine)

    -- 6. Post-processing
    local pp = ecs.create_entity()
    ecs.add_post_process(pp, true, 1.0, 0.8, 1.0)
    ecs.set_post_process_color(pp, true, 1.0, 2.2)
end

return Scene
