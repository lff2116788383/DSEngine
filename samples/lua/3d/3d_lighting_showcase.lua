-- 3D P0 sample: lighting showcase
-- 目标：同屏展示 DirectionalLight、PointLight、SpotLight 对 lit mesh 的影响。
local LightingShowcase3D = {}


LightingShowcase3D._meta = {
    name     = "lighting showcase",
    category = "rendering",
    config   = { camera_distance=11.0 },
}

local state = {
    camera = nil,
    directional = nil,
    point_light = nil,
    point_marker = nil,
    spot_light = nil,
    spot_marker = nil,
    cubes = {},
    time = 0.0
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

local function add_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 11.0
    dse.ecs.add_transform(camera, 0.0, 4.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -24.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera, 6.0, 0.12)
    elseif dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 6.0, 0.12)
    end
    state.camera = camera
end

local function add_lit_cube(x, y, z, sx, sy, sz, color)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    table.insert(state.cubes, e)
    return e
end

local function add_emissive_marker(x, y, z, color)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 0.22, 0.22, 0.22)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.35, 1.0, color[1] * 0.9, color[2] * 0.9, color[3] * 0.9, 1.0, false, true)
    return e
end

local function add_scene_meshes()
    add_lit_cube(0.0, -0.55, 0.0, 8.0, 0.12, 5.5, {0.44, 0.48, 0.48, 1.0})
    for x = -3, 3, 2 do
        add_lit_cube(x, 0.25, 0.0, 0.85, 0.85, 0.85, {0.75, 0.75, 0.78, 1.0})
    end
    add_lit_cube(-2.5, 0.4, -1.8, 0.75, 1.2, 0.75, {0.9, 0.42, 0.22, 1.0})
    add_lit_cube(2.5, 0.4, -1.8, 0.75, 1.2, 0.75, {0.25, 0.62, 1.0, 1.0})
end

local function add_lights()
    local dir = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(dir, -0.45, -1.0, -0.25, 1.0, 0.95, 0.86, 0.8, 0.12, 0.25)
    state.directional = dir

    local point = dse.ecs.create_entity()
    dse.ecs.add_transform(point, 0.0, 1.8, 1.4, 1.0, 1.0, 1.0)
    dse.ecs.add_point_light_3d(point, 1.0, 0.25, 0.12, 3.0, 5.0)
    state.point_light = point
    state.point_marker = add_emissive_marker(0.0, 1.8, 1.4, {1.0, 0.2, 0.1})

    local spot = dse.ecs.create_entity()
    dse.ecs.add_transform(spot, -2.8, 3.0, 2.4, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(spot, -45.0, -25.0, 0.0)
    dse.ecs.add_spot_light_3d(spot, 0.0, -1.0, -0.25, 0.3, 0.55, 1.0, 2.2, 7.0, 14.0, 26.0)
    state.spot_light = spot
    state.spot_marker = add_emissive_marker(-2.8, 3.0, 2.4, {0.3, 0.55, 1.0})

    print("[3D][Lighting] DirectionalLight + animated red PointLight + blue SpotLight created.")
end

function LightingShowcase3D.Setup(config)
    print("[3D][Lighting] setup: observe directional/point/spot light contribution. Use free camera to inspect.")
    add_camera(config or {})
    add_scene_meshes()
    add_lights()
end

function LightingShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then
        dt = 0.1
    end
    state.time = state.time + dt
    if state.point_light ~= nil then
        local x = math.cos(state.time * 0.9) * 2.8
        local z = math.sin(state.time * 0.9) * 1.8 + 0.6
        dse.ecs.set_transform_position(state.point_light, x, 1.8, z)
        if state.point_marker ~= nil then
            dse.ecs.set_transform_position(state.point_marker, x, 1.8, z)
            dse.ecs.set_transform_rotation(state.point_marker, state.time * 45.0, state.time * 80.0, 0.0)
        end
    end
    for i, cube in ipairs(state.cubes) do
        if i > 1 then
            dse.ecs.set_transform_rotation(cube, 0.0, state.time * (8.0 + i), 0.0)
        end
    end
end

return LightingShowcase3D
