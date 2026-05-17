-- Reflection Probe 反射探针 Demo
-- 验证: ReflectionProbeComponent 捕获环境 cubemap 用于 PBR 反射
-- 场景: 高反射金属球 + 反射探针 + 彩色环境物体
local ReflProbeDemo = {}

local state = {
    camera = nil,
    light  = nil,
    probes = {},
    metal_spheres = {},
    time = 0.0,
}

local cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local cube_i = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local function make_box(x, y, z, sx, sy, sz, r, g, b)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, r, g, b, 1.0, cube_v, cube_i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
    return e
end

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 16.0
    dse.ecs.add_transform(e, 0, 5, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -15, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 120)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 6.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.4, 1.0, 0.97, 0.90, 1.2, 0.18, 0.30)
    state.light = light

    -- 地面
    make_box(0, -0.15, 0, 24, 0.1, 24, 0.25, 0.25, 0.28)

    -- 彩色环境墙壁（提供反射内容）
    local wall_colors = {
        {-8, 3.0, 0,  0.5, 6.0, 16, 0.85, 0.2, 0.15},  -- 左 红
        { 8, 3.0, 0,  0.5, 6.0, 16, 0.15, 0.2, 0.85},  -- 右 蓝
        { 0, 3.0, -8, 16, 6.0, 0.5, 0.2, 0.75, 0.2},   -- 后 绿
    }
    for _, w in ipairs(wall_colors) do
        make_box(w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], w[9])
    end

    -- 高反射金属球（用立方体代替，高 metallic 低 roughness）
    local sphere_positions = {
        {-3.0, 1.2, 0},
        { 0.0, 1.2, 0},
        { 3.0, 1.2, 0},
    }
    local roughness_values = {0.05, 0.25, 0.6}  -- 镜面 → 模糊

    for i, pos in ipairs(sphere_positions) do
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, pos[1], pos[2], pos[3], 1.8, 1.8, 1.8)
        dse.ecs.add_mesh_renderer(e, 0.9, 0.9, 0.92, 1.0, cube_v, cube_i)
        dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
        -- 高 metallic + 不同 roughness
        dse.ecs.set_mesh_material(e, 1.0, roughness_values[i], 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
        table.insert(state.metal_spheres, e)
    end

    -- 反射探针（放置在场景中心）
    local probe = dse.ecs.create_entity()
    dse.ecs.add_transform(probe, 0, 2, 0, 1, 1, 1)
    dse.ecs.add_reflection_probe(probe, 20.0)
    dse.ecs.set_reflection_probe(probe, 20.0, 18, 8, 18, 256)
    table.insert(state.probes, probe)

    -- 第二个探针偏移位置
    local probe2 = dse.ecs.create_entity()
    dse.ecs.add_transform(probe2, -5, 2, -3, 1, 1, 1)
    dse.ecs.add_reflection_probe(probe2, 10.0)
    dse.ecs.set_reflection_probe(probe2, 10.0, 10, 6, 10, 128)
    table.insert(state.probes, probe2)

    print(string.format("[ReflProbe] 场景: 3 面彩色墙 + 3 个金属体 (roughness=%.2f/%.2f/%.2f) + %d 个探针",
        roughness_values[1], roughness_values[2], roughness_values[3], #state.probes))
    print("[ReflProbe] WASD 移动相机, 观察金属体表面环境反射")
end

function ReflProbeDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function ReflProbeDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 金属体缓慢旋转
    for i, e in ipairs(state.metal_spheres) do
        local angle = state.time * 12 + i * 120
        dse.ecs.set_transform_rotation(e, 0, angle, 0)
    end
end

return ReflProbeDemo
