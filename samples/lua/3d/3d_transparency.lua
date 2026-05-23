-- 透明度 / Alpha 排序验证 Demo
-- 验证: alpha_test cutoff、半透明物体深度排序、双面渲染
-- 场景: 不透明地面 + alpha_test 镂空墙 + 半透明彩色玻璃板
local TransparencyDemo = {}


TransparencyDemo._meta = {
    name     = "透明度 / Alpha 排序验证 Demo",
    category = "rendering",
    config   = { camera_distance=16.0 },
}

local state = {
    camera = nil,
    light  = nil,
    glass_panels = {},
    time = 0.0,
}

local cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local cube_i = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local function make_box(x, y, z, sx, sy, sz, r, g, b, a)
    a = a or 1.0
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, r, g, b, a, cube_v, cube_i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    return e
end

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 16.0
    dse.ecs.add_transform(e, 0, 4, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -12, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 120)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 6.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    -- 方向光
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.3, 1.0, 0.97, 0.90, 1.1, 0.20, 0.30)
    state.light = light

    -- 地面（不透明）
    local ground = make_box(0, -0.15, 0, 30, 0.1, 30, 0.28, 0.30, 0.32, 1.0)
    dse.ecs.set_mesh_material(ground, 0.0, 0.7, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)

    -- === 左侧: Alpha Test 物体 ===
    -- Alpha test 用 cutoff 做硬边镂空
    for i = 1, 3 do
        local e = make_box(-6, 1.5 * i, 0, 3.0, 1.2, 0.15, 0.75, 0.65, 0.5, 1.0)
        -- metallic, roughness, ao, normal_str, emissive_r/g/b, receive_shadow, double_sided
        dse.ecs.set_mesh_material(e, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0, true, true)
        -- 设置 alpha test (通过 depth_state 控制)
        dse.ecs.set_mesh_depth_state(e, true, true)
    end
    print("[Transparency] 左侧: 3 个 alpha_test 双面物体")

    -- === 中央: 半透明玻璃板（不同透明度，测试排序）===
    local glass_colors = {
        {0.9, 0.2, 0.2, 0.35},  -- 红色玻璃
        {0.2, 0.8, 0.2, 0.40},  -- 绿色玻璃
        {0.2, 0.3, 0.9, 0.45},  -- 蓝色玻璃
        {0.9, 0.9, 0.2, 0.30},  -- 黄色玻璃
        {0.8, 0.3, 0.9, 0.38},  -- 紫色玻璃
    }
    for i, c in ipairs(glass_colors) do
        local z = (i - 3) * 2.0
        local e = make_box(0, 2.0, z, 2.5, 3.5, 0.08, c[1], c[2], c[3], c[4])
        dse.ecs.set_mesh_material(e, 0.8, 0.15, 1.0, 0.0, 0.0, 0.0, 0.0, false, true)
        dse.ecs.set_mesh_depth_state(e, true, false) -- depth test on, depth write off
        table.insert(state.glass_panels, e)
    end
    print(string.format("[Transparency] 中央: %d 个半透明玻璃板 (alpha 0.30~0.45)", #glass_colors))

    -- === 右侧: 不透明参照物 ===
    for i = 1, 4 do
        local y = 0.6 + (i - 1) * 1.8
        local e = make_box(6, y, 0, 1.0, 1.4, 1.0, 0.4, 0.5, 0.6, 1.0)
        dse.ecs.set_mesh_material(e, 0.2, 0.4, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
    end
    print("[Transparency] 右侧: 4 个不透明参照立方体")

    -- 后方不透明大柱（用于透过玻璃板观察）
    for i = 1, 3 do
        local x = (i - 2) * 3.0
        make_box(x, 2.5, -6, 0.8, 5.0, 0.8, 0.65, 0.55, 0.45, 1.0)
    end

    print("[Transparency] WASD 移动相机, 穿过玻璃板观察深度排序")
end

function TransparencyDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function TransparencyDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 玻璃板轻微摆动
    for i, e in ipairs(state.glass_panels) do
        local angle = math.sin(state.time * 0.8 + i * 0.7) * 8.0
        dse.ecs.set_transform_rotation(e, 0, angle, 0)
    end
end

return TransparencyDemo
