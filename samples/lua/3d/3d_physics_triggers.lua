-- 3D P3 sample: physics triggers & collision events
-- 目标：验证 2D 碰撞事件系统（trigger / poll_collision_event）在混合场景中的使用，
--       以及 3D 物理 raycast + impulse 的组合交互。
--       由于 3D 碰撞事件回调暂无 Lua 绑定，本范例同时展示 2D trigger 和 3D raycast
--       两条路径，供用户对照理解。
-- 覆盖 API: dse.ecs.set_box_collider_trigger, poll_collision_event (2D),
--           dse.ecs.add_box_collider_3d, add_rigidbody_3d, physics_3d_raycast,
--           rigidbody_3d_add_impulse, rigidbody_3d_get_velocity (3D)
local PhysicsTriggers3D = {}

local state = {
    camera = nil,
    time = 0.0,
    -- 2D 碰撞
    trigger_zones = {},
    dynamic_2d = nil,
    collision_logs = {},
    -- 3D 物理
    cubes_3d = {},
    hit_marker = nil,
    impulse_done = false,
    phase = 0,
    logged_api = false,
    logged_summary = false
}

-- ============================================================
-- 几何数据
-- ============================================================

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

-- ============================================================
-- 场景搭建
-- ============================================================

local function add_cube_3d(name, x, y, z, sx, sy, sz, color, dynamic, mass)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.50, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    dse.ecs.add_box_collider_3d(e, sx, sy, sz)
    dse.ecs.add_rigidbody_3d(e, dynamic and 2 or 0, mass or 1.0)
    table.insert(state.cubes_3d, { name = name, entity = e, dynamic = dynamic, color = color })
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 14.0
    dse.ecs.add_transform(camera, 0.0, 6.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -28.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.5, 0.12)
    end
    state.camera = camera
end

local function setup_scene(config)
    -- 光照
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.30, 1.0, 0.96, 0.88, 1.15, 0.20, 0.32)

    -- 静态地面
    add_cube_3d("ground", 0.0, -0.55, 0.0, 12.0, 0.12, 8.0, {0.30, 0.34, 0.38, 1.0}, false, 0.0)

    -- === 3D 触发区域标记（半透明方块，无 rigidbody，仅用于 raycast 检测演示）===
    -- 左侧红色触发区
    local zone_left = dse.ecs.create_entity()
    dse.ecs.add_transform(zone_left, -3.0, 0.5, 0.0, 2.0, 1.0, 2.0)
    dse.ecs.add_mesh_renderer(zone_left, 1.0, 0.25, 0.15, 0.35, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(zone_left, "MESH_LIT")
    dse.ecs.set_mesh_material(zone_left, 0.0, 0.55, 1.0, 0.15, 0.03, 0.0, 0.35, true, true)
    dse.ecs.add_box_collider_3d(zone_left, 2.0, 1.0, 2.0)
    dse.ecs.add_rigidbody_3d(zone_left, 0, 0.0)  -- 静态碰撞体
    table.insert(state.trigger_zones, { name = "zone_left", entity = zone_left, color = {1.0, 0.25, 0.15} })

    -- 右侧蓝色触发区
    local zone_right = dse.ecs.create_entity()
    dse.ecs.add_transform(zone_right, 3.0, 0.5, 0.0, 2.0, 1.0, 2.0)
    dse.ecs.add_mesh_renderer(zone_right, 0.15, 0.35, 1.0, 0.35, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(zone_right, "MESH_LIT")
    dse.ecs.set_mesh_material(zone_right, 0.0, 0.55, 1.0, 0.03, 0.06, 0.18, 0.35, true, true)
    dse.ecs.add_box_collider_3d(zone_right, 2.0, 1.0, 2.0)
    dse.ecs.add_rigidbody_3d(zone_right, 0, 0.0)  -- 静态碰撞体
    table.insert(state.trigger_zones, { name = "zone_right", entity = zone_right, color = {0.15, 0.35, 1.0} })

    -- 中央绿色触发区
    local zone_center = dse.ecs.create_entity()
    dse.ecs.add_transform(zone_center, 0.0, 0.5, -2.5, 2.5, 1.0, 2.0)
    dse.ecs.add_mesh_renderer(zone_center, 0.15, 1.0, 0.30, 0.30, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(zone_center, "MESH_LIT")
    dse.ecs.set_mesh_material(zone_center, 0.0, 0.55, 1.0, 0.03, 0.15, 0.02, 0.30, true, true)
    dse.ecs.add_box_collider_3d(zone_center, 2.5, 1.0, 2.0)
    dse.ecs.add_rigidbody_3d(zone_center, 0, 0.0)  -- 静态碰撞体
    table.insert(state.trigger_zones, { name = "zone_center", entity = zone_center, color = {0.15, 1.0, 0.30} })

    -- === 3D 动态方块（受物理影响，可被 raycast 命中后施加 impulse）===
    add_cube_3d("dynamic_1", -3.0, 2.5, 0.0, 0.65, 0.65, 0.65, {0.95, 0.40, 0.18, 1.0}, true, 1.0)
    add_cube_3d("dynamic_2", 3.0, 2.5, 0.0, 0.65, 0.65, 0.65, {0.25, 0.55, 1.0, 1.0}, true, 1.0)
    add_cube_3d("dynamic_3", 0.0, 2.5, -2.5, 0.65, 0.65, 0.65, {0.30, 0.95, 0.45, 1.0}, true, 1.0)

    -- 命中标记
    state.hit_marker = dse.ecs.create_entity()
    dse.ecs.add_transform(state.hit_marker, 0.0, 10.0, 0.0, 0.22, 0.22, 0.22)
    dse.ecs.add_mesh_renderer(state.hit_marker, 1.0, 1.0, 0.15, 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(state.hit_marker, "MESH_LIT")
    dse.ecs.set_mesh_material(state.hit_marker, 0.0, 0.5, 1.0, 0.8, 0.7, 0.0, 1.0, true, true)

    -- 2D 碰撞 API 检测（仅验证绑定可用性，2D 触发器在 3D 场景中不直接适用）
    local has_2d_trigger = dse.ecs.set_box_collider_trigger ~= nil
    local has_2d_collision = dse.ecs.poll_collision_event ~= nil
    print(string.format("[3D][PhysicsTriggers] api: 2d_trigger=%s 2d_collision=%s 3d_raycast=%s 3d_impulse=%s 3d_velocity=%s",
        tostring(has_2d_trigger), tostring(has_2d_collision),
        tostring(dse.ecs.physics_3d_raycast ~= nil),
        tostring(dse.ecs.rigidbody_3d_add_impulse ~= nil),
        tostring(dse.ecs.rigidbody_3d_get_velocity ~= nil)))
end

-- ============================================================
-- 交互阶段
-- ============================================================

--- 阶段 1：对左侧红色区域上方的方块施加向右的冲量
local function phase_impulse_left()
    if state.impulse_done then return end
    if #state.cubes_3d < 2 then return end
    local target = state.cubes_3d[2]  -- dynamic_1
    if not target or not target.dynamic then return end
    if not dse.ecs.rigidbody_3d_add_impulse then return end

    dse.ecs.rigidbody_3d_add_impulse(target.entity, 5.0, 3.0, 0.0)
    state.impulse_done = true
    print(string.format("[3D][PhysicsTriggers] phase1: impulse (5,3,0) on %s", target.name))
end

--- 阶段 2：从上方做 raycast 检测碰撞区域
local function phase_raycast()
    if state.raycast_done then return end
    if not dse.ecs.physics_3d_raycast then return end

    -- 对三个触发区域上方各射一条射线
    local rays = {
        { "left",   -3.0, 8.0, 0.0,   0.0, -1.0, 0.0 },
        { "right",   3.0, 8.0, 0.0,   0.0, -1.0, 0.0 },
        { "center",  0.0, 8.0, -2.5,  0.0, -1.0, 0.0 },
    }

    for _, ray in ipairs(rays) do
        local hit, entity, hx, hy, hz, nx, ny, nz, distance =
            dse.ecs.physics_3d_raycast(ray[2], ray[3], ray[4], ray[5], ray[6], ray[7], 20.0)
        if hit then
            table.insert(state.collision_logs, string.format("ray_%s:hit entity=%s pos=(%.2f,%.2f,%.2f) dist=%.2f",
                ray[1], tostring(entity), hx or 0.0, hy or 0.0, hz or 0.0, distance or 0.0))
            -- 在首次命中点放置标记
            if state.hit_marker and not state.raycast_done then
                dse.ecs.set_transform_position(state.hit_marker, hx or 0.0, (hy or 0.0) + 0.22, hz or 0.0)
            end
        else
            table.insert(state.collision_logs, string.format("ray_%s:miss", ray[1]))
        end
    end

    state.raycast_done = true
    for _, log in ipairs(state.collision_logs) do
        print(string.format("[3D][PhysicsTriggers] phase2: %s", log))
    end
end
state.raycast_done = false

--- 阶段 3：查询动态方块的速度
local function phase_velocity()
    if state.velocity_done then return end
    if not dse.ecs.rigidbody_3d_get_velocity then return end

    for _, cube in ipairs(state.cubes_3d) do
        if cube.dynamic then
            local vx, vy, vz = dse.ecs.rigidbody_3d_get_velocity(cube.entity)
            print(string.format("[3D][PhysicsTriggers] phase3: %s velocity=(%.2f,%.2f,%.2f)", cube.name, vx or 0.0, vy or 0.0, vz or 0.0))
        end
    end
    state.velocity_done = true
end
state.velocity_done = false

--- 阶段 4：2D 碰撞事件轮询演示（验证 API 可用性）
local function phase_2d_collision_poll()
    if state.collision_poll_done then return end
    if dse.ecs.poll_collision_event then
        -- 轮询一次 2D 碰撞事件（在纯 3D 场景中通常不会有 2D 碰撞，仅验证 API 可调用）
        local has_event, entity_a, entity_b, normal_x, normal_y = dse.ecs.poll_collision_event()
        print(string.format("[3D][PhysicsTriggers] phase4: 2d_poll_collision_event has_event=%s (3d_scene_has_no_2d_bodies)", tostring(has_event)))
    else
        print("[3D][PhysicsTriggers] phase4: poll_collision_event API not available (2D only)")
    end
    state.collision_poll_done = true
end
state.collision_poll_done = false

-- ============================================================
-- 生命周期
-- ============================================================

function PhysicsTriggers3D.Setup(config)
    print("[3D][PhysicsTriggers] setup: 3D trigger zones + raycast detection + impulse interaction. 2D collision API availability check included.")
    setup_camera(config or {})
    setup_scene(config or {})
end

function PhysicsTriggers3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 阶段 1: 0.5s 后施加冲量
    if state.time > 0.5 and not state.impulse_done then
        phase_impulse_left()
    end

    -- 阶段 2: 1.0s 后 raycast 检测
    if state.time > 1.0 and not state.raycast_done then
        phase_raycast()
    end

    -- 阶段 3: 1.5s 后查询速度
    if state.time > 1.5 and not state.velocity_done then
        phase_velocity()
    end

    -- 阶段 4: 2.0s 后 2D 碰撞 API 可用性检查
    if state.time > 2.0 and not state.collision_poll_done then
        phase_2d_collision_poll()
    end

    -- 触发区域脉冲动画
    for i, zone in ipairs(state.trigger_zones) do
        local pulse = 0.30 + math.sin(state.time * 2.5 + i * 1.2) * 0.08
        dse.ecs.set_mesh_material(zone.entity, 0.0, 0.55, 1.0,
            zone.color[1] * pulse, zone.color[2] * pulse, zone.color[3] * pulse,
            0.30 + math.sin(state.time * 3.0 + i) * 0.08, true, true)
    end

    -- 命中标记旋转
    if state.hit_marker then
        dse.ecs.set_transform_rotation(state.hit_marker, state.time * 45.0, state.time * 90.0, 0.0)
    end

    -- 最终汇总
    if not state.logged_summary and state.time > 2.5 then
        state.logged_summary = true
        print(string.format("[3D][PhysicsTriggers] summary: impulse=%s raycast=%s velocity=%s collision_poll=%s trigger_zones=%d",
            tostring(state.impulse_done), tostring(state.raycast_done),
            tostring(state.velocity_done), tostring(state.collision_poll_done),
            #state.trigger_zones))
    end
end

return PhysicsTriggers3D
