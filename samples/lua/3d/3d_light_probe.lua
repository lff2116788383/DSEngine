-- Light Probe (SH 间接漫反射) Demo
-- 验证: LightProbeComponent 的 SH 间接光照（与 ReflectionProbe 互补）
-- 场景: 彩色房间 + 多个光照探针 + 漫反射物体
local LightProbeDemo = {}


LightProbeDemo._meta = {
    name     = "Light Probe (SH 间接漫反射) Demo",
    category = "rendering",
    config   = { camera_distance=10.0 },
}

local state = {
    camera = nil,
    light  = nil,
    probes = {},
    objects = {},
    time = 0.0,
}

local cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local cube_i = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local function make_box(x, y, z, sx, sy, sz, r, g, b, metallic, roughness)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, r, g, b, 1.0, cube_v, cube_i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, metallic or 0.0, roughness or 0.7, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
    return e
end

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 10.0
    dse.ecs.add_transform(e, 0, 3, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -12, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 60)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 5.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, 0.0, -1.0, -0.3, 1.0, 0.98, 0.92, 0.8, 0.08, 0.20)
    state.light = light

    local s = 6.0
    local h = 5.0

    -- 房间墙壁（低环境光，突出间接光照）
    make_box(0, -0.05, 0, s*2, 0.1, s*2, 0.8, 0.8, 0.8, 0, 0.9)       -- 地板
    make_box(0, h+0.05, 0, s*2, 0.1, s*2, 0.8, 0.8, 0.8, 0, 0.9)      -- 天花板
    make_box(0, h/2, -s, s*2, h, 0.1, 0.8, 0.8, 0.8, 0, 0.9)          -- 后墙
    make_box(-s, h/2, 0, 0.1, h, s*2, 0.9, 0.20, 0.15, 0, 0.85)       -- 左墙 暖红
    make_box( s, h/2, 0, 0.1, h, s*2, 0.15, 0.20, 0.9, 0, 0.85)       -- 右墙 冷蓝
    make_box(0, h/2, s, s*2, h, 0.1, 0.2, 0.8, 0.25, 0, 0.85)         -- 前墙 绿（相机背后）

    -- 漫反射测试物体（高 roughness，低 metallic → 接收间接漫反射为主）
    local obj_positions = {
        {-2.5, 1.0, -1.5},
        { 0.0, 1.0,  0.0},
        { 2.5, 1.0, -1.5},
        {-1.5, 1.0,  2.0},
        { 1.5, 1.0,  2.0},
    }
    for i, pos in ipairs(obj_positions) do
        local e = make_box(pos[1], pos[2], pos[3], 1.4, 1.4, 1.4, 0.85, 0.85, 0.85, 0.0, 0.85)
        table.insert(state.objects, e)
    end

    -- 光照探针网格（3x2 分布）
    local probe_positions = {
        {-3.0, 2.5, -2.0},
        { 0.0, 2.5, -2.0},
        { 3.0, 2.5, -2.0},
        {-3.0, 2.5,  2.0},
        { 0.0, 2.5,  2.0},
        { 3.0, 2.5,  2.0},
    }
    for _, pp in ipairs(probe_positions) do
        local probe = dse.ecs.create_entity()
        dse.ecs.add_transform(probe, pp[1], pp[2], pp[3], 1, 1, 1)
        -- add_light_probe(entity, influence_radius)
        dse.ecs.add_light_probe(probe, 8.0)
        -- set_light_probe(entity, influence_radius, needs_rebake)
        dse.ecs.set_light_probe(probe, 8.0, true)
        dse.ecs.set_light_probe_enabled(probe, true)
        table.insert(state.probes, probe)

        -- 探针位置可视化标记
        make_box(pp[1], pp[2], pp[3], 0.15, 0.15, 0.15, 1.0, 1.0, 0.3, 0, 0.3)
    end

    print(string.format("[LightProbe] 场景: 彩色房间 + %d 个 SH 光照探针 + %d 个漫反射体",
        #state.probes, #state.objects))
    print("[LightProbe] 红墙(左)/蓝墙(右)/绿墙(前) → 白色物体应呈现色彩溢出")
    print("[LightProbe] WASD 移动相机观察间接漫反射效果")
end

function LightProbeDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function LightProbeDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 物体缓慢旋转
    for i, e in ipairs(state.objects) do
        dse.ecs.set_transform_rotation(e, 0, state.time * 8 + i * 72, 0)
    end
end

return LightProbeDemo
