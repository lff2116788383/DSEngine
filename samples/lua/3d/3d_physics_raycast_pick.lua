-- 3D P3 sample: physics raycast pick showcase
-- 目标：验证 Physics3D raycast/拾取专项入口；当前优先使用真实 PhysX Raycast，未启用 PhysX 时使用 ECS 3D collider 几何 fallback。
local PhysicsRaycastPick3D = {}

local state = {
    camera = nil,
    time = 0.0,
    targets = {},
    beam = nil,
    hit_marker = nil,
    selected = 1,
    logged = false
}

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 9.5
    dse.ecs.add_transform(camera, 0.0, 2.8, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -18.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 5.5, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 5.5, 0.12) end
    state.camera = camera
end

local function add_box(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.45, 1.0, emissive and emissive[1] or 0.0, emissive and emissive[2] or 0.0, emissive and emissive[3] or 0.0, 1.0, true, true)
    return e
end

local function setup_scene(config)
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.22, 1.0, 0.95, 0.86, 1.20, 0.18, 0.32)

    add_box("ground", 0.0, -0.65, 0.0, 7.5, 0.16, 4.6, {0.22, 0.25, 0.25, 1.0})

    local count = (type(config) == "table" and type(config.target_count) == "number") and config.target_count or 5
    for i = 1, count do
        local x = (i - (count + 1) * 0.5) * 1.15
        local z = -0.25 + math.sin(i * 1.7) * 0.55
        local color = {0.18 + i * 0.11, 0.42 + (i % 2) * 0.22, 0.95 - i * 0.08, 1.0}
        local target = add_box("pick_target_" .. tostring(i), x, 0.05, z, 0.62, 0.62, 0.62, color)
        dse.ecs.add_box_collider_3d(target, 0.62, 0.62, 0.62)
        dse.ecs.add_rigidbody_3d(target, 0, 0.0)
        table.insert(state.targets, { entity = target, x = x, z = z, base_color = color })
    end

    state.beam = add_box("ray_beam", 0.0, 0.32, 2.1, 0.055, 0.055, 4.0, {0.1, 0.8, 1.0, 0.8}, {0.0, 0.35, 0.5})
    state.hit_marker = add_box("hit_marker", 0.0, 0.78, 0.0, 0.34, 0.34, 0.34, {1.0, 0.75, 0.12, 1.0}, {0.45, 0.24, 0.0})

    print(string.format("[3D][PhysicsRaycastPick] setup: %d pick targets with BoxCollider3D; visual beam shows raycast direction.", count))
    print("[3D][PhysicsRaycastPick] physics_3d_raycast now returns hit/entity/position via PhysX when available or ECS collider geometry fallback.")
end

local function try_raycast()
    if dse.ecs.physics_3d_raycast then
        local hit, entity, hx, hy, hz, nx, ny, nz, distance = dse.ecs.physics_3d_raycast(0.0, 0.55, 5.0, 0.0, -0.08, -1.0, 12.0)
        return hit, entity, hx, hy, hz, nx, ny, nz, distance
    end
    return false, nil, nil, nil, nil, nil, nil, nil, nil
end

function PhysicsRaycastPick3D.Setup(config)
    setup_camera(config or {})
    setup_scene(config or {})
end

function PhysicsRaycastPick3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    local count = #state.targets
    if count > 0 then
        state.selected = (math.floor(state.time * 0.75) % count) + 1
        for i, item in ipairs(state.targets) do
            local bob = (i == state.selected) and math.sin(state.time * 6.0) * 0.08 or 0.0
            local scale = (i == state.selected) and 0.78 or 0.62
            dse.ecs.add_transform(item.entity, item.x, 0.05 + bob, item.z, scale, scale, scale)
        end
        local selected = state.targets[state.selected]
        if selected and state.hit_marker then
            dse.ecs.set_transform_position(state.hit_marker, selected.x, 0.82, selected.z)
            dse.ecs.set_transform_rotation(state.hit_marker, state.time * 18.0, state.time * 72.0, 0.0)
        end
    end

    if state.beam then
        dse.ecs.set_transform_rotation(state.beam, 0.0, math.sin(state.time * 0.85) * 10.0, 0.0)
    end

    if not state.logged and state.time > 0.2 then
        state.logged = true
        local hit, entity, hx, hy, hz, nx, ny, nz, distance = try_raycast()
        print(string.format("[3D][PhysicsRaycastPick] raycast sample hit=%s entity=%s pos=(%.2f,%.2f,%.2f) normal=(%.2f,%.2f,%.2f) distance=%.2f selected=%d", tostring(hit), tostring(entity), hx or 0.0, hy or 0.0, hz or 0.0, nx or 0.0, ny or 0.0, nz or 0.0, distance or 0.0, state.selected))
        if hit and state.hit_marker ~= nil then
            dse.ecs.set_transform_position(state.hit_marker, hx or 0.0, (hy or 0.0) + 0.35, hz or 0.0)
        end
    end
end

return PhysicsRaycastPick3D
