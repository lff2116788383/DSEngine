-- GI Probe (DDGI) 全局光照探针 Demo
-- 验证: GI Probe 组件的间接光照效果
-- 场景: Cornell Box 变体，色彩溢出展示间接光照
local GIProbeDemo = {}


GIProbeDemo._meta = {
    name     = "GI Probe (DDGI) 全局光照探针 Demo",
    category = "rendering",
    config   = { camera_distance=8.0 },
}

local state = {
    camera = nil,
    light  = nil,
    gi_probe = nil,
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
    local dist = (config and config.camera_distance) or 8.0
    dse.ecs.add_transform(e, 0, 3, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -10, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 50)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 4.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, 0.0, -1.0, -0.3, 1.0, 0.98, 0.92, 1.2, 0.10, 0.25)
    state.light = light

    local s = 5.0  -- 房间半尺寸
    local h = 6.0  -- 房间高

    -- Cornell Box 墙壁
    make_box(0, -0.05, 0, s*2, 0.1, s*2, 0.85, 0.85, 0.85, 0, 0.9)    -- 地板 白
    make_box(0, h+0.05, 0, s*2, 0.1, s*2, 0.85, 0.85, 0.85, 0, 0.9)   -- 天花板 白
    make_box(0, h/2, -s, s*2, h, 0.1, 0.85, 0.85, 0.85, 0, 0.9)       -- 后墙 白
    make_box(-s, h/2, 0, 0.1, h, s*2, 0.85, 0.15, 0.15, 0, 0.8)       -- 左墙 红
    make_box( s, h/2, 0, 0.1, h, s*2, 0.15, 0.85, 0.15, 0, 0.8)       -- 右墙 绿

    -- 场景内物体
    make_box(-1.5, 1.5, -1, 2.0, 3.0, 2.0, 0.85, 0.85, 0.85, 0, 0.7)  -- 高箱
    make_box( 1.5, 0.75, 1, 1.5, 1.5, 1.5, 0.85, 0.85, 0.85, 0, 0.7)  -- 矮箱

    -- 金属球（反射间接光）
    make_box(0, 0.6, 1.5, 1.0, 1.0, 1.0, 0.95, 0.95, 0.95, 1.0, 0.1)

    -- GI Probe
    local probe = dse.ecs.create_entity()
    dse.ecs.add_transform(probe, 0, h/2, 0, 1, 1, 1)
    dse.ecs.add_gi_probe(probe)
    -- set_gi_probe 参数依赖引擎具体实现
    if dse.ecs.set_gi_probe then
        dse.ecs.set_gi_probe(probe)
    end
    dse.ecs.set_gi_probe_enabled(probe, true)
    state.gi_probe = probe

    print("[GIProbe] Cornell Box: 红墙(左)/绿墙(右)/白天花板+地板+后墙")
    print("[GIProbe] 1 个 GI Probe + 2 白箱 + 1 金属球")
    print("[GIProbe] WASD 移动相机, 观察红/绿色彩溢出(间接光)")
end

function GIProbeDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function GIProbeDemo.Update(delta_time)
    -- 静态场景，不需要动态更新
end

return GIProbeDemo
