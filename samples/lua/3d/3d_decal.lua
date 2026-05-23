-- Decal 贴花投影 Demo
-- 验证: DecalComponent 投影到几何体表面
-- 场景: 地面 + 墙壁 + 多个彩色贴花
local DecalDemo = {}


DecalDemo._meta = {
    name     = "Decal 贴花投影 Demo",
    category = "rendering",
    config   = { camera_distance=14.0 },
}

local state = {
    camera = nil,
    light  = nil,
    decals = {},
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
    local dist = (config and config.camera_distance) or 14.0
    dse.ecs.add_transform(e, 0, 5, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -18, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 6.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.3, 1.0, 0.95, 0.88, 1.1, 0.18, 0.30)
    state.light = light

    -- 地面
    make_box(0, -0.15, 0, 20, 0.1, 20, 0.30, 0.32, 0.35)

    -- 墙壁
    make_box(0, 2.5, -5, 12, 5.0, 0.3, 0.55, 0.53, 0.50)
    make_box(-6, 2.5, 0, 0.3, 5.0, 10, 0.52, 0.50, 0.48)

    -- 地面上的阶梯
    for i = 0, 3 do
        make_box(3, 0.2 * (i + 1), i * 1.2, 3, 0.2 * (i + 1), 1.0, 0.45, 0.42, 0.40)
    end

    -- 贴花
    local decal_configs = {
        {x=0,   y=0.05, z=0,   sx=2.0, sy=0.5, sz=2.0, r=0.9, g=0.2, b=0.15, a=0.8, angle=0.6},
        {x=-2,  y=0.05, z=2,   sx=1.5, sy=0.5, sz=1.5, r=0.2, g=0.7, b=0.2,  a=0.7, angle=0.4},
        {x=2,   y=0.05, z=-1,  sx=1.8, sy=0.5, sz=1.8, r=0.15,g=0.3, b=0.85, a=0.75,angle=0.5},
        {x=0,   y=2.0,  z=-4.7,sx=2.5, sy=2.5, sz=0.5, r=0.9, g=0.8, b=0.1,  a=0.6, angle=0.3}, -- 墙上
        {x=-5.7,y=2.5,  z=2,   sx=0.5, sy=2.0, sz=2.0, r=0.8, g=0.2, b=0.7,  a=0.7, angle=0.5}, -- 侧墙
        {x=3,   y=0.5,  z=0,   sx=1.2, sy=1.0, sz=1.2, r=0.9, g=0.5, b=0.1,  a=0.65,angle=0.4}, -- 阶梯上
    }

    for i, dc in ipairs(decal_configs) do
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, dc.x, dc.y, dc.z, dc.sx, dc.sy, dc.sz)
        dse.ecs.add_decal(e)
        -- set_decal: enabled, albedo_tex(0=procedural), r, g, b, a, angle_fade
        dse.ecs.set_decal(e, true, 0, dc.r, dc.g, dc.b, dc.a, dc.angle)
        table.insert(state.decals, e)
    end

    print(string.format("[Decal] 场景: 地面+墙壁+阶梯 + %d 个贴花", #state.decals))
    print("[Decal] WASD 移动相机，观察贴花投影到不同表面")
end

function DecalDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function DecalDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 地面贴花缓慢旋转
    for i = 1, 3 do
        local e = state.decals[i]
        if e then
            local angle = state.time * 10 + i * 120
            dse.ecs.set_transform_rotation(e, 0, angle, 0)
        end
    end
end

return DecalDemo
