-- demo_npr_light.lua
-- NPR 自定义光照模型演示：展示 DSSL light() 函数的三种预设
-- hatching (交叉线影) / gradient_ramp (渐变色带) / minnaert (月球散射)
--
-- 独立运行:
--   dsengine_game_release.exe --script=samples/lua/dssl/demo_npr_light.lua
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
    ecs.add_directional_light_3d(sun, -1.0/ld, -4.0/ld, -1.0/ld, 0.9, 0.9, 0.85, 1.2, 0.5, 1.0)

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
    app.set_window_title("DSSL NPR Light Models")
    setup_scene()

    -- 1) Hatching — 素描/版画风格（左）
    local hatching_entity = make_cube(-3.0, 1.0, 0, 1.6, 1.6, 1.6)
    local mat_hatching = dssl.load_material(dssl_dir .. "hatching.dssl")
    dssl.set_color(mat_hatching, "ink_color",   0.1, 0.08, 0.05, 1.0)
    dssl.set_color(mat_hatching, "paper_color", 0.95, 0.92, 0.85, 1.0)
    dssl.set_float(mat_hatching, "hatch_density",   30.0)
    dssl.set_float(mat_hatching, "hatch_thickness",  0.4)
    dssl.apply_material(hatching_entity, mat_hatching)

    -- 2) Gradient Ramp — 渐变色带光照（中）
    local ramp_entity = make_cube(0.0, 1.0, 0, 1.6, 1.6, 1.6)
    local mat_ramp = dssl.load_material(dssl_dir .. "gradient_ramp.dssl")
    dssl.set_color(mat_ramp, "albedo_color", 1.0, 0.85, 0.7, 1.0)
    dssl.set_color(mat_ramp, "warm_color",   1.0, 0.9,  0.7, 1.0)
    dssl.set_color(mat_ramp, "cool_color",   0.3, 0.4,  0.6, 1.0)
    dssl.set_float(mat_ramp, "ramp_smoothness", 0.1)
    dssl.set_float(mat_ramp, "ramp_bands",      4.0)
    dssl.apply_material(ramp_entity, mat_ramp)

    -- 3) Minnaert — 月球表面散射（右）
    local minnaert_entity = make_cube(3.0, 1.0, 0, 1.6, 1.6, 1.6)
    local mat_minnaert = dssl.load_material(dssl_dir .. "minnaert.dssl")
    dssl.set_color(mat_minnaert, "albedo_color", 0.75, 0.73, 0.7, 1.0)
    dssl.set_float(mat_minnaert, "darkness", 1.5)
    dssl.apply_material(minnaert_entity, mat_minnaert)

    print("[demo_npr_light] NPR light() models loaded:")
    print("  left:   hatching  (cross-hatch sketch style)")
    print("  center: gradient_ramp (warm/cool color bands)")
    print("  right:  minnaert  (lunar limb darkening)")
end

function Update(dt)
end
