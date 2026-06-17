-- DSSL 材质系统端到端 Demo
-- 验证: load_material → set_float/color/texture → get_float/color → apply_material → create_instance
--
-- 独立运行:
--   dsengine_game_release.exe --script=samples/lua/dssl/dssl_material_demo.lua
--------------------------------------------------------------------------------
local ecs = dse.ecs
local app = dse.app

-- ============================================================================
-- 内联 cube mesh 顶点 (samples/lua/3d 惯例)
-- ============================================================================
local function cube_vertices()
    return {
        -0.5, -0.5,  0.5,
         0.5, -0.5,  0.5,
         0.5,  0.5,  0.5,
        -0.5,  0.5,  0.5,
        -0.5, -0.5, -0.5,
         0.5, -0.5, -0.5,
         0.5,  0.5, -0.5,
        -0.5,  0.5, -0.5
    }
end

local function cube_indices()
    return {
        0, 1, 2, 2, 3, 0,
        1, 5, 6, 6, 2, 1,
        5, 4, 7, 7, 6, 5,
        4, 0, 3, 3, 7, 4,
        3, 2, 6, 6, 7, 3,
        4, 5, 1, 1, 0, 4
    }
end

local function make_cube(x, y, z, sx, sy, sz, r, g, b, a)
    local e = ecs.create_entity()
    ecs.add_transform(e, x, y, z, sx or 1, sy or 1, sz or 1)
    ecs.add_mesh_renderer(e, r or 1, g or 1, b or 1, a or 1, cube_vertices(), cube_indices())
    ecs.set_mesh_shader_variant(e, "MESH_PBR")
    return e
end

-- ============================================================================
-- 测试框架
-- ============================================================================
local passed = 0
local failed = 0

local function check(name, cond)
    if cond then
        passed = passed + 1
        print("  [PASS] " .. name)
    else
        failed = failed + 1
        print("  [FAIL] " .. name)
    end
end

-- ============================================================================
-- DSSL 材质文件 (内联写入)
-- ============================================================================
local dssl_dir = "samples/lua/dssl/"

-- ============================================================================
-- 场景搭建
-- ============================================================================
local function setup_scene()
    -- 方向光
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 0, 0)
    local ld = math.sqrt(1 + 16 + 1)
    ecs.add_directional_light_3d(sun,
        -1.0/ld, -4.0/ld, -1.0/ld,
         0.8, 0.8, 0.8, 1.0, 0.5, 1.0)

    -- 天空光
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0)
    ecs.add_sky_light(sky, 0.4, 0.45, 0.55, 0.1, 0.1, 0.1, 0.4)

    -- 相机 (fov, priority, near_clip, far_clip)
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 3, 10)
    ecs.set_transform_rotation(cam, -10, 0, 0)
    ecs.add_camera_3d(cam, 60.0, 0, 0.1, 1000.0)

    -- 地面
    local ground = make_cube(0, -0.5, 0, 20, 1, 20)
    ecs.set_mesh_shader_variant(ground, "MESH_PBR")
    ecs.set_mesh_material(ground, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, false, false)
end

-- ============================================================================
-- 主测试
-- ============================================================================
local function run_tests()
    print("========================================")
    print("  DSSL Material Demo — 端到端验证")
    print("========================================")

    -- ------------------------------------------------------------------
    -- Test 1: 加载 PBR 材质
    -- ------------------------------------------------------------------
    local pbr_id = dssl.load_material(dssl_dir .. "demo_pbr.dssl")
    check("load_material(demo_pbr.dssl) 返回非 nil", pbr_id ~= nil)
    check("load_material 返回正整数 ID", pbr_id ~= nil and pbr_id > 0)

    if pbr_id then
        -- Test 2: 默认值
        local roughness = dssl.get_float(pbr_id, "roughness")
        check("get_float(roughness) 默认 0.5", math.abs(roughness - 0.5) < 0.01)

        local metallic = dssl.get_float(pbr_id, "metallic")
        check("get_float(metallic) 默认 0.0", math.abs(metallic - 0.0) < 0.01)

        local r, g, b, a = dssl.get_color(pbr_id, "albedo_color")
        check("get_color(albedo_color) 默认白色",
            math.abs(r - 1.0) < 0.01 and math.abs(g - 1.0) < 0.01 and
            math.abs(b - 1.0) < 0.01 and math.abs(a - 1.0) < 0.01)

        -- Test 3: set + get 一致性
        dssl.set_float(pbr_id, "roughness", 0.8)
        check("set_float + get_float (0.8)", math.abs(dssl.get_float(pbr_id, "roughness") - 0.8) < 0.01)

        dssl.set_color(pbr_id, "albedo_color", 0.9, 0.2, 0.1, 1.0)
        local nr, ng, nb, _ = dssl.get_color(pbr_id, "albedo_color")
        check("set_color + get_color (0.9,0.2,0.1)",
            math.abs(nr - 0.9) < 0.01 and math.abs(ng - 0.2) < 0.01 and math.abs(nb - 0.1) < 0.01)

        -- Test 4: apply_material — 红色粗糙 cube
        local cube1 = make_cube(-3, 1.5, 0, 2, 2, 2)
        dssl.apply_material(cube1, pbr_id)
        check("apply_material(PBR) 不崩溃", true)
    end

    -- ------------------------------------------------------------------
    -- Test 5: 加载自发光材质
    -- ------------------------------------------------------------------
    local emissive_id = dssl.load_material(dssl_dir .. "demo_emissive.dssl")
    check("load_material(demo_emissive.dssl) 成功", emissive_id ~= nil)

    if emissive_id then
        local cube2 = make_cube(0, 1.5, 0, 2, 2, 2)
        dssl.apply_material(cube2, emissive_id)
        check("apply_material(Emissive) 不崩溃", true)
    end

    -- ------------------------------------------------------------------
    -- Test 6: 加载 Unlit 材质
    -- ------------------------------------------------------------------
    local unlit_id = dssl.load_material(dssl_dir .. "demo_unlit.dssl")
    check("load_material(demo_unlit.dssl) 成功", unlit_id ~= nil)

    if unlit_id then
        local cube3 = make_cube(3, 1.5, 0, 2, 2, 2)
        dssl.apply_material(cube3, unlit_id)
        check("apply_material(Unlit) 不崩溃", true)
    end

    -- ------------------------------------------------------------------
    -- Test 7: create_instance 独立性
    -- ------------------------------------------------------------------
    if pbr_id then
        local pbr_id2 = dssl.create_instance(dssl_dir .. "demo_pbr.dssl")
        check("create_instance 返回新 ID", pbr_id2 ~= nil and pbr_id2 ~= pbr_id)

        if pbr_id2 then
            dssl.set_color(pbr_id2, "albedo_color", 0.0, 1.0, 0.0, 1.0)
            dssl.set_float(pbr_id2, "metallic", 1.0)
            dssl.set_float(pbr_id2, "roughness", 0.1)

            -- 原实例不受影响
            local orig_r, _, _, _ = dssl.get_color(pbr_id, "albedo_color")
            check("实例独立性 (原 R 仍 0.9)", math.abs(orig_r - 0.9) < 0.01)

            local cube4 = make_cube(-1.5, 4, 0, 1.5, 1.5, 1.5)
            dssl.apply_material(cube4, pbr_id2)
            check("apply_material(PBR#2 绿色金属) 不崩溃", true)
        end
    end

    -- ------------------------------------------------------------------
    -- 结果
    -- ------------------------------------------------------------------
    print("========================================")
    print(string.format("  DSSL Demo: %d passed, %d failed", passed, failed))
    if failed == 0 then
        print("  ALL PASSED")
    else
        print("  FAILED: " .. failed)
    end
    print("========================================")
end

-- ============================================================================
-- 入口
-- ============================================================================
function Awake()
    app.set_window_title("DSSL Material Demo")
    setup_scene()
    run_tests()
end

function Update(dt)
end
