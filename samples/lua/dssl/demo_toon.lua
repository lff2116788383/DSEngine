-- demo_toon.lua
-- Toon / Cel Shading 演示：展示 toon_*.dssl 材质的各参数效果
--
-- 独立运行:
--   dsengine_game_release.exe --script=samples/lua/dssl/demo_toon.lua
--------------------------------------------------------------------------------
local ecs = dse.ecs
local app = dse.app

local dssl_dir = "samples/lua/dssl/"

-- ============================================================================
-- 内联 cube mesh（samples/lua/3d 惯例：8 顶点 / 36 索引）
-- ============================================================================
local function cube_vertices()
    return {
        -0.5, -0.5,  0.5,  0.5, -0.5,  0.5,  0.5,  0.5,  0.5, -0.5,  0.5,  0.5,
        -0.5, -0.5, -0.5,  0.5, -0.5, -0.5,  0.5,  0.5, -0.5, -0.5,  0.5, -0.5,
    }
end

local function cube_indices()
    return {
        0, 1, 2, 2, 3, 0,  1, 5, 6, 6, 2, 1,  5, 4, 7, 7, 6, 5,
        4, 0, 3, 3, 7, 4,  3, 2, 6, 6, 7, 3,  4, 5, 1, 1, 0, 4,
    }
end

local function make_cube(x, y, z, sx, sy, sz)
    local e = ecs.create_entity()
    ecs.add_transform(e, x, y, z, sx or 1, sy or 1, sz or 1)
    ecs.add_mesh_renderer(e, 1, 1, 1, 1, cube_vertices(), cube_indices())
    ecs.set_mesh_shader_variant(e, "MESH_PBR")
    return e
end

-- ============================================================================
-- 场景：方向光 + 天空光 + 相机 + 地面
-- ============================================================================
local function setup_scene()
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 0, 0)
    local ld = math.sqrt(1 + 16 + 1)
    ecs.add_directional_light_3d(sun, -1.0/ld, -4.0/ld, -1.0/ld, 0.95, 0.95, 0.9, 1.2, 0.5, 1.0)

    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0)
    ecs.add_sky_light(sky, 0.4, 0.45, 0.55, 0.1, 0.1, 0.1, 0.4)

    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 2, 9)
    ecs.set_transform_rotation(cam, -8, 0, 0)
    ecs.add_camera_3d(cam, 60.0, 0, 0.1, 1000.0)

    local ground = make_cube(0, -1.0, 0, 24, 1, 24)
    ecs.set_mesh_material(ground, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, false, false)
end

-- ============================================================================
-- 入口
-- ============================================================================
function Awake()
    app.set_window_title("DSSL Toon / Cel Shading")
    setup_scene()

    -- 1) toon_basic — 紫色阴影 / 自然亮区（左）
    local hero = make_cube(-3.0, 1.0, 0, 1.6, 1.6, 1.6)
    local mat_basic = dssl.load_material(dssl_dir .. "toon_basic.dssl")
    dssl.set_color(mat_basic, "albedo_color",     0.85, 0.5, 0.45, 1.0)
    dssl.set_color(mat_basic, "shadow_color",     0.12, 0.08, 0.22, 1.0)
    dssl.set_float(mat_basic, "shadow_threshold", 0.38)
    dssl.set_float(mat_basic, "shadow_softness",  0.04)
    dssl.set_float(mat_basic, "specular_size",     0.65)
    dssl.set_float(mat_basic, "specular_strength", 1.2)
    dssl.set_float(mat_basic, "rim_strength",      0.5)
    dssl.apply_material(hero, mat_basic)

    -- 2) toon_metal — 卡通金属（中）
    local metal = make_cube(0.0, 1.0, 0, 1.6, 1.6, 1.6)
    local mat_metal = dssl.load_material(dssl_dir .. "toon_metal.dssl")
    dssl.set_color(mat_metal, "albedo_color",     0.7, 0.72, 0.78, 1.0)
    dssl.set_color(mat_metal, "shadow_color",     0.08, 0.06, 0.12, 1.0)
    dssl.set_float(mat_metal, "shadow_threshold", 0.3)
    dssl.set_float(mat_metal, "specular_size",     0.85)
    dssl.set_float(mat_metal, "specular_strength", 1.5)
    dssl.apply_material(metal, mat_metal)

    -- 3) toon_rim — 强边缘光（右）
    local rim = make_cube(3.0, 1.0, 0, 1.6, 1.6, 1.6)
    local mat_rim = dssl.load_material(dssl_dir .. "toon_rim.dssl")
    dssl.set_color(mat_rim, "albedo_color",     0.4, 0.55, 0.8, 1.0)
    dssl.set_color(mat_rim, "shadow_color",     0.1, 0.1, 0.12, 1.0)
    dssl.set_float(mat_rim, "shadow_threshold", 0.4)
    dssl.set_float(mat_rim, "specular_strength", 0.3)
    dssl.set_float(mat_rim, "rim_strength",      0.7)
    dssl.apply_material(rim, mat_rim)

    print("[demo_toon] Toon shading demo loaded.")
    print("  left:   toon_basic (purple shadow / warm rim / discrete specular)")
    print("  center: toon_metal (cartoon metal / strong specular)")
    print("  right:  toon_rim   (strong rim light)")
end

function Update(dt)
end
