-- 3D P1 sample: scene showcase
-- 目标：整合资源 cube、手写 cube、多光源、移动物体与 free camera，形成小型综合 3D 场景。
local SceneShowcase3D = {}

local state = {
    camera = nil,
    directional = nil,
    point_light = nil,
    point_marker = nil,
    objects = {},
    movers = {},
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

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 12.0
    dse.ecs.add_transform(camera, 0.0, 4.2, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -24.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera, 6.0, 0.12)
    elseif dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 6.0, 0.12)
    end
    state.camera = camera
end

local function add_lit_cube(name, x, y, z, sx, sy, sz, color, material)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    local mat = material or {}
    dse.ecs.set_mesh_material(e, mat.metallic or 0.0, mat.roughness or 0.5, mat.ao or 1.0, mat.er or 0.0, mat.eg or 0.0, mat.eb or 0.0, mat.normal_strength or 1.0, mat.receive_shadow ~= false, mat.double_sided == true)
    table.insert(state.objects, { name = name, entity = e })
    return e
end

local function add_resource_cube(name, x, y, z, sx, sy, sz, mesh_path, material_path)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, 1.0, 1.0, 1.0, 1.0)
    dse.ecs.set_mesh_path(e, mesh_path)
    dse.ecs.set_mesh_material(e, material_path)
    table.insert(state.objects, { name = name, entity = e })
    return e
end

local function add_emissive_marker(name, x, y, z, color)
    local e = add_lit_cube(name, x, y, z, 0.22, 0.22, 0.22, color, {
        roughness = 0.35,
        er = color[1] * 0.8,
        eg = color[2] * 0.8,
        eb = color[3] * 0.8,
        receive_shadow = false
    })
    return e
end

local function setup_lights()
    local dir = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(dir, -0.35, -1.0, -0.30, 1.0, 0.94, 0.82, 1.05, 0.20, 0.32)
    state.directional = dir

    local point = dse.ecs.create_entity()
    dse.ecs.add_transform(point, 0.0, 2.2, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_point_light_3d(point, 1.0, 0.38, 0.16, 3.2, 6.0)
    state.point_light = point
    state.point_marker = add_emissive_marker("moving_point_light_marker", 0.0, 2.2, 0.0, {1.0, 0.35, 0.12, 1.0})

    local spot = dse.ecs.create_entity()
    dse.ecs.add_transform(spot, -4.2, 4.0, 3.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(spot, -48.0, -32.0, 0.0)
    dse.ecs.add_spot_light_3d(spot, 0.28, 0.56, 1.0, 0.35, 0.58, 1.0, 2.4, 8.0, 13.0, 28.0)
    add_emissive_marker("blue_spot_marker", -4.2, 4.0, 3.0, {0.25, 0.55, 1.0, 1.0})

    print("[3D][Scene] lights: DirectionalLight + animated PointLight + blue SpotLight")
end

local function setup_scene(config)
    local mesh_path = (type(config) == "table" and type(config.mesh_path) == "string") and config.mesh_path or "models/cube.dmesh"
    local material_path = (type(config) == "table" and type(config.material_path) == "string") and config.material_path or "models/cube.dmat"

    add_lit_cube("ground", 0.0, -0.58, 0.0, 11.0, 0.12, 7.0, {0.35, 0.39, 0.40, 1.0}, { roughness = 0.78, double_sided = true })
    add_lit_cube("back_wall", 0.0, 1.2, -3.6, 11.0, 3.6, 0.10, {0.18, 0.20, 0.23, 1.0}, { roughness = 0.7, double_sided = true })

    local placements = {
        {"resource_center", 0.0, 0.15, 0.0, 1.15, 1.15, 1.15},
        {"resource_left", -2.2, 0.05, -1.0, 0.9, 0.9, 0.9},
        {"resource_right", 2.2, 0.05, -1.0, 0.9, 0.9, 0.9},
        {"resource_back", -3.6, 0.35, -2.3, 0.75, 1.45, 0.75},
        {"resource_tall", 3.6, 0.55, -2.1, 0.8, 1.8, 0.8}
    }
    for _, p in ipairs(placements) do
        local e = add_resource_cube(p[1], p[2], p[3], p[4], p[5], p[6], p[7], mesh_path, material_path)
        table.insert(state.movers, { entity = e, base_y = p[3], speed = 0.4 + #state.movers * 0.07, orbit = false })
    end

    local mover = add_lit_cube("handwritten_orbit_cube", 0.0, 0.45, 1.8, 0.65, 0.65, 0.65, {0.95, 0.45, 0.18, 1.0}, { roughness = 0.28, er = 0.05 })
    table.insert(state.movers, { entity = mover, base_y = 0.45, speed = 0.9, orbit = true })

    add_lit_cube("cyan_pillar", -4.5, 0.4, 1.7, 0.55, 1.25, 0.55, {0.15, 0.85, 1.0, 1.0}, { roughness = 0.35, er = 0.0, eg = 0.05, eb = 0.08 })
    add_lit_cube("green_pillar", 4.5, 0.4, 1.7, 0.55, 1.25, 0.55, {0.25, 1.0, 0.45, 1.0}, { roughness = 0.45 })

    print(string.format("[3D][Scene] scene: objects=%d resource_mesh=%s material=%s", #state.objects, mesh_path, material_path))
end

function SceneShowcase3D.Setup(config)
    print("[3D][Scene] setup: P1 integrated scene with ground, 5+ models, multi lights, moving cube. Use right mouse + W/A/S/D/Q/E to inspect.")
    setup_camera(config or {})
    setup_scene(config or {})
    setup_lights()
end

function SceneShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then
        dt = 0.1
    end
    state.time = state.time + dt

    if state.point_light ~= nil then
        local x = math.cos(state.time * 0.75) * 3.5
        local z = math.sin(state.time * 0.75) * 2.1 + 0.2
        dse.ecs.set_transform_position(state.point_light, x, 2.2, z)
        if state.point_marker ~= nil then
            dse.ecs.set_transform_position(state.point_marker, x, 2.2, z)
            dse.ecs.set_transform_rotation(state.point_marker, state.time * 60.0, state.time * 90.0, 0.0)
        end
    end

    for i, item in ipairs(state.movers) do
        if item.orbit then
            local x = math.cos(state.time * item.speed) * 2.8
            local z = math.sin(state.time * item.speed) * 1.8 + 1.1
            dse.ecs.set_transform_position(item.entity, x, item.base_y + math.sin(state.time * 1.8) * 0.25, z)
        end
        dse.ecs.set_transform_rotation(item.entity, math.sin(state.time + i) * 10.0, state.time * (12.0 + i * 2.0), 0.0)
    end
end

return SceneShowcase3D
