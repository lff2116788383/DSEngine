-- 3D P1 sample: physics stack showcase
-- 目标：验证 3D RigidBody/BoxCollider 绑定；若 PhysX 未启用，仍显示静态堆叠目标画面并记录 fallback。
local PhysicsStack3D = {}

local state = { camera = nil, cubes = {}, time = 0.0, fallback = false, logged = false }

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
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 5.5, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 5.5, 0.12) end
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
    add_cube("static_ground", 0.0, -0.62, 0.0, 8.0, 0.18, 5.0, {0.32, 0.35, 0.36, 1.0}, false, 0.0)
    local rows = config.rows or 5
    for i = 1, rows do
        local x = ((i % 2) == 0) and 0.32 or -0.32
        local z = ((i % 3) - 1) * 0.18
        add_cube("dynamic_box_" .. tostring(i), x, 1.1 + i * 0.72, z, 0.58, 0.58, 0.58, {0.25 + i * 0.10, 0.45, 1.0 - i * 0.08, 1.0}, true, 1.0)
    end
    print(string.format("[3D][Physics] setup: static ground + %d dynamic boxes with RigidBody3D/BoxCollider3D", rows))
    print("[3D][Physics] if PhysX is disabled in this build, boxes remain visible as fallback stack markers.")
end

function PhysicsStack3D.Setup(config)
    setup_camera(config or {})
    setup_scene(config or {})
end

function PhysicsStack3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    if not state.logged and state.time > 1.0 then
        state.logged = true
        for _, item in ipairs(state.cubes) do
            if item.dynamic then
                local x, y, z = dse.ecs.get_transform_position(item.entity)
                print(string.format("[3D][Physics] sample %s y=%.3f initial_y=%.3f", item.name, y or -999.0, item.initial_y))
                break
            end
        end
    end
end

return PhysicsStack3D
