-- Steering Behavior 转向行为 Demo
-- 验证: SteeringComponent 的 seek / flee / arrive 三种行为
-- 场景: 3 个 agent 分别演示不同转向模式
local SteeringDemo = {}


SteeringDemo._meta = {
    name     = "Steering Behavior 转向行为 Demo",
    category = "ai",
    config   = { camera_distance=20.0 },
}

local state = {
    camera = nil,
    light  = nil,
    agents = {},
    target_entity = nil,
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
    local dist = (config and config.camera_distance) or 20.0
    dse.ecs.add_transform(e, 0, 18, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -50, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 8.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.4, 1.0, 0.95, 0.88, 1.1, 0.18, 0.30)
    state.light = light

    -- 地面
    make_box(0, -0.15, 0, 30, 0.1, 30, 0.30, 0.35, 0.30)

    -- 目标标记（黄色立柱）
    local target = make_box(0, 0.5, 0, 0.6, 1.0, 0.6, 1.0, 0.9, 0.2)
    -- 发光标记
    dse.ecs.set_mesh_material(target, 0.0, 0.3, 1.0, 0.8, 0.7, 0.1, 1.0, true, false)
    state.target_entity = target

    -- Agent 配置
    local agent_configs = {
        {
            name = "Seeker",
            color = {0.2, 0.8, 0.3},
            start = {-8, 0.5, -8},
            behavior = "seek",
            max_vel = 4.0, max_force = 8.0, mass = 1.0,
        },
        {
            name = "Fleer",
            color = {0.9, 0.3, 0.2},
            start = {0, 0.5, 5},
            behavior = "flee",
            max_vel = 3.5, max_force = 6.0, mass = 1.2,
        },
        {
            name = "Arriver",
            color = {0.3, 0.4, 0.9},
            start = {8, 0.5, -8},
            behavior = "arrive",
            max_vel = 5.0, max_force = 10.0, mass = 0.8,
        },
    }

    for _, cfg in ipairs(agent_configs) do
        local e = make_box(cfg.start[1], cfg.start[2], cfg.start[3],
                           0.8, 0.8, 0.8,
                           cfg.color[1], cfg.color[2], cfg.color[3])

        -- add_steering(entity, max_velocity, max_force, mass)
        dse.ecs.add_steering(e, cfg.max_vel, cfg.max_force, cfg.mass)
        -- set_steering_target(entity, behavior, tx, ty, tz)
        dse.ecs.set_steering_target(e, cfg.behavior, 0, 0.5, 0)

        table.insert(state.agents, {
            entity = e,
            name = cfg.name,
            behavior = cfg.behavior,
        })
    end

    -- 障碍物装饰
    local obstacles = {
        { 4, 0.4, 3, 1.2, 0.8, 1.2},
        {-5, 0.4, 4, 1.0, 0.8, 1.0},
        { 2, 0.4,-5, 1.5, 0.8, 1.5},
        {-3, 0.4,-3, 0.8, 0.8, 0.8},
    }
    for _, o in ipairs(obstacles) do
        make_box(o[1], o[2], o[3], o[4], o[5], o[6], 0.5, 0.5, 0.5)
    end

    print(string.format("[Steering] 3 个 Agent: Seek(绿) / Flee(红) / Arrive(蓝)"))
    print("[Steering] 黄色标记为目标，目标沿圆弧移动")
    print("[Steering] WASD 移动相机，俯视观察转向行为")
end

function SteeringDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function SteeringDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 目标沿圆弧移动
    local radius = 6.0
    local tx = math.cos(state.time * 0.4) * radius
    local tz = math.sin(state.time * 0.4) * radius
    if state.target_entity then
        dse.ecs.set_transform_position(state.target_entity, tx, 0.5, tz)
    end

    -- 更新每个 agent 的转向目标
    for _, ag in ipairs(state.agents) do
        dse.ecs.set_steering_target(ag.entity, ag.behavior, tx, 0.5, tz)

        -- get_steering_state 返回: ok, enabled, seek_en, flee_en, arrive_en,
        --   vx, vy, vz, speed, max_vel, max_force, mass, decel_r,
        --   seek_tx, seek_ty, seek_tz, flee_tx, flee_ty, flee_tz, arr_tx, arr_ty, arr_tz
        local ok, _, _, _, _, vx, vy, vz, speed = dse.ecs.get_steering_state(ag.entity)
        if ok and type(vx) == "number" and speed > 0.01 then
            local yaw = math.deg(math.atan(vx, vz))
            dse.ecs.set_transform_rotation(ag.entity, 0, yaw, 0)
        end
    end
end

return SteeringDemo
