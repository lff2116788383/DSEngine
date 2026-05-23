-- 3D P0 sample: camera showcase
-- 目标：展示 free camera、orbit camera、top camera 的相机切换与观察差异。
local CameraShowcase3D = {}


CameraShowcase3D._meta = {
    name     = "camera showcase",
    category = "ui",
    config   = { camera_distance=7.0 },
}

local state = {
    cameras = {},
    active_index = 1,
    switch_timer = 0.0,
    time = 0.0,
    target = nil,
    objects = {}
}

local function cube_vertices()
    return {
        -0.5, -0.5,  0.5,
         0.5, -0.5,  0.5,
         0.5,  0.5,  0.5,
        -0.5,  0.5,  0.5,
        -0.5, -0.5, -0.5,
         0.5, -0.5, -0.5,
         0.5,  0.5, -0.5,
        -0.5,  0.5, -0.5
    }
end

local function cube_indices()
    return {
        0, 1, 2, 2, 3, 0,
        1, 5, 6, 6, 2, 1,
        5, 4, 7, 7, 6, 5,
        4, 0, 3, 3, 7, 4,
        3, 2, 6, 6, 7, 3,
        4, 5, 1, 1, 0, 4
    }
end

local function add_cube(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.05, 0.48, 1.0, emissive and emissive[1] or 0.0, emissive and emissive[2] or 0.0, emissive and emissive[3] or 0.0, 1.0, true, true)
    table.insert(state.objects, { name = name, entity = e })
    return e
end

local function add_camera(name, x, y, z, rx, ry, rz, fov, priority, free)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(e, rx, ry, rz)
    dse.ecs.add_camera_3d(e, fov or 60.0, priority or 0)
    if free then
        if Ecs and Ecs.add_free_camera_controller then
            Ecs.add_free_camera_controller(e, 5.5, 0.12)
        elseif dse.ecs.add_free_camera_controller then
            dse.ecs.add_free_camera_controller(e, 5.5, 0.12)
        end
    end
    table.insert(state.cameras, { name = name, entity = e, free = free == true })
    return e
end

local function set_active_camera(index)
    state.active_index = index
    for i, cam in ipairs(state.cameras) do
        dse.ecs.set_camera_enabled(cam.entity, i == index)
        dse.ecs.set_camera_priority(cam.entity, i == index and 100 or 0)
    end
    print(string.format("[3D][Camera] active camera: %s", state.cameras[index].name))
end

local function setup_scene()
    add_cube("ground", 0.0, -0.55, 0.0, 7.0, 0.12, 5.0, {0.42, 0.46, 0.48, 1.0})
    state.target = add_cube("center_target", 0.0, 0.25, 0.0, 1.1, 1.1, 1.1, {0.9, 0.35, 0.18, 1.0}, {0.04, 0.01, 0.0})
    add_cube("left", -2.4, 0.1, -0.8, 0.8, 0.8, 0.8, {0.2, 0.75, 1.0, 1.0})
    add_cube("right", 2.4, 0.1, -0.8, 0.8, 0.8, 0.8, {0.25, 1.0, 0.45, 1.0})
    add_cube("back", 0.0, 0.1, -2.0, 0.8, 0.8, 0.8, {0.9, 0.85, 0.25, 1.0})
end

function CameraShowcase3D.Setup(config)
    print("[3D][Camera] setup: free/orbit/top cameras auto-switch every 4 seconds. Free camera supports right mouse + W/A/S/D/Q/E.")
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.35, 1.0, 0.96, 0.88, 1.25, 0.24, 0.32)
    setup_scene()
    add_camera("free_camera", 0.0, 2.2, 7.0, -16.0, 0.0, 0.0, 60.0, 100, true)
    add_camera("orbit_camera", 5.0, 2.6, 5.0, -22.0, 42.0, 0.0, 55.0, 0, false)
    add_camera("top_camera", 0.0, 8.0, 0.1, -88.0, 0.0, 0.0, 45.0, 0, false)
    set_active_camera(1)
end

function CameraShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then
        dt = 0.1
    end
    state.time = state.time + dt
    state.switch_timer = state.switch_timer + dt

    if state.target ~= nil then
        dse.ecs.set_transform_rotation(state.target, state.time * 16.0, state.time * 34.0, 0.0)
    end

    local orbit = state.cameras[2]
    if orbit ~= nil then
        local radius = 6.0
        local x = math.cos(state.time * 0.35) * radius
        local z = math.sin(state.time * 0.35) * radius
        dse.ecs.set_transform_position(orbit.entity, x, 2.7, z)
        dse.ecs.set_transform_rotation(orbit.entity, -24.0, -state.time * 20.0 + 45.0, 0.0)
    end

    if state.switch_timer >= 4.0 then
        state.switch_timer = 0.0
        local next_index = state.active_index + 1
        if next_index > #state.cameras then
            next_index = 1
        end
        set_active_camera(next_index)
    end
end

return CameraShowcase3D
