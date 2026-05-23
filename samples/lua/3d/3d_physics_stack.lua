-- 3D P6 sample: physics stack showcase（PhysX 后端验证）
-- 目标：验证 3D RigidBody/BoxCollider 绑定 + PhysX 物理模拟
-- 若 PhysX 未启用，仍显示静态堆叠画面并记录 fallback。
local PhysicsStack3D = {}


PhysicsStack3D._meta = {
    name     = "physics stack showcase（PhysX 后端验证）",
    category = "physics",
    config   = { camera_distance=10.0,
    rows=5 },
}

local state = {
    camera = nil,
    cubes = {},
    time = 0.0,
    fallback = false,
    logged_physics = false,
    logged_velocity = false,
    impulse_applied = false
}

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 10.0
    dse.ecs.add_transform(camera, 0.0, 3.8, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -24.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera, 5.5, 0.12)
    elseif dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.5, 0.12)
    end
    state.camera = camera
end

local function add_cube(name, x, y, z, sx, sy, sz, color, dynamic, mass)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.48, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    dse.ecs.add_box_collider_3d(e, sx, sy, sz)
    dse.ecs.add_rigidbody_3d(e, dynamic and 2 or 0, mass or 1.0)
    table.insert(state.cubes, { name = name, entity = e, dynamic = dynamic, initial_y = y })
    return e
end

local function setup_scene(config)
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.86, 1.05, 0.16, 0.30)

    -- 静态地面
    add_cube("static_ground", 0.0, -0.62, 0.0, 8.0, 0.18, 5.0, {0.32, 0.35, 0.36, 1.0}, false, 0.0)

    -- 动态堆叠方块
    local rows = config.rows or 5
    for i = 1, rows do
        local x = ((i % 2) == 0) and 0.32 or -0.32
        local z = ((i % 3) - 1) * 0.18
        add_cube("dynamic_box_" .. tostring(i), x, 1.1 + i * 0.72, z, 0.58, 0.58, 0.58,
            {0.25 + i * 0.10, 0.45, 1.0 - i * 0.08, 1.0}, true, 1.0)
    end

    -- 检测 PhysX 是否可用
    state.fallback = not (dse.ecs.physics_3d_raycast ~= nil)
    print(string.format("[3D][PhysicsStack] setup: static ground + %d dynamic boxes (PhysX %s)",
        rows, state.fallback and "DISABLED" or "ENABLED"))
end

function PhysicsStack3D.Setup(config)
    setup_camera(config or {})
    setup_scene(config or {})
end

function PhysicsStack3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 0.5s 后对第一个动态方块施加一个向上的冲量，验证力 API
    if not state.impulse_applied and state.time > 0.5 and #state.cubes >= 2 then
        state.impulse_applied = true
        local first_dynamic = state.cubes[2] -- cubes[1] 是地面
        if first_dynamic and first_dynamic.dynamic and dse.ecs.rigidbody_3d_add_impulse then
            dse.ecs.rigidbody_3d_add_impulse(first_dynamic.entity, 0.0, 5.0, 0.0)
            print(string.format("[3D][Physics] applied upward impulse (0,5,0) to %s", first_dynamic.name))
        end
    end

    -- 1.0s 后检测物理模拟结果
    if not state.logged_physics and state.time > 1.0 then
        state.logged_physics = true
        for _, item in ipairs(state.cubes) do
            if item.dynamic then
                local x, y, z = dse.ecs.get_transform_position(item.entity)
                local dy = (y or 0.0) - item.initial_y
                print(string.format("[3D][Physics] %s pos=(%.3f,%.3f,%.3f) dy=%.3f %s",
                    item.name, x or 0.0, y or 0.0, z or 0.0, dy,
                    (dy < -0.05) and "FELL" or "STABLE"))
            end
        end
    end

    -- 2.0s 后尝试获取速度
    if not state.logged_velocity and state.time > 2.0 and #state.cubes >= 2 then
        state.logged_velocity = true
        local first_dynamic = state.cubes[2]
        if first_dynamic and first_dynamic.dynamic and dse.ecs.rigidbody_3d_get_velocity then
            local vx, vy, vz = dse.ecs.rigidbody_3d_get_velocity(first_dynamic.entity)
            print(string.format("[3D][Physics] %s velocity=(%.3f,%.3f,%.3f)",
                first_dynamic.name, vx or 0.0, vy or 0.0, vz or 0.0))
        end
    end
end

return PhysicsStack3D
