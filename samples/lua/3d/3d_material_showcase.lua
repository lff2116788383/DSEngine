-- 3D P0 sample: material showcase
-- 目标：展示 metallic、roughness、emissive、double-sided 等 MeshRenderer 材质参数差异。
local MaterialShowcase3D = {}


MaterialShowcase3D._meta = {
    name     = "material showcase",
    category = "rendering",
    config   = { camera_distance=10.0 },
}

local state = {
    camera = nil,
    light = nil,
    cubes = {},
    animated = nil,
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
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 10.0
    dse.ecs.add_transform(camera, 0.0, 3.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -20.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera, 5.0, 0.12)
    elseif dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.0, 0.12)
    end
    state.camera = camera
end

local function setup_light()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.28, 1.0, 0.96, 0.9, 1.45, 0.28, 0.35)
    state.light = light
end

local function add_cube(label, x, z, color, metallic, roughness, emissive, normal_strength, receive_shadow, double_sided)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, 0.0, z, 1.05, 1.05, 1.05)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, metallic, roughness, 1.0, emissive[1], emissive[2], emissive[3], normal_strength or 1.0, receive_shadow ~= false, double_sided == true)
    table.insert(state.cubes, { entity = e, label = label })
    print(string.format("[3D][Material] %s metallic=%.2f roughness=%.2f emissive=(%.2f,%.2f,%.2f) double_sided=%s", label, metallic, roughness, emissive[1], emissive[2], emissive[3], tostring(double_sided == true)))
    return e
end

function MaterialShowcase3D.Setup(config)
    print("[3D][Material] setup: material parameter grid. Animated cube changes roughness/emissive over time.")
    setup_camera(config or {})
    setup_light()

    add_cube("matte_red", -3.0, 1.4, {1.0, 0.25, 0.18, 1.0}, 0.0, 0.92, {0.0, 0.0, 0.0}, 1.0, true, false)
    add_cube("glossy_blue", 0.0, 1.4, {0.15, 0.45, 1.0, 1.0}, 0.0, 0.12, {0.0, 0.0, 0.0}, 1.0, true, false)
    add_cube("metal_gold", 3.0, 1.4, {1.0, 0.72, 0.22, 1.0}, 0.95, 0.25, {0.0, 0.0, 0.0}, 1.0, true, false)
    add_cube("emissive_green", -3.0, -1.4, {0.1, 0.9, 0.35, 1.0}, 0.0, 0.55, {0.0, 0.45, 0.10}, 1.0, false, false)
    state.animated = add_cube("animated_pulse", 0.0, -1.4, {0.85, 0.35, 1.0, 1.0}, 0.1, 0.5, {0.15, 0.02, 0.18}, 1.0, true, true)
    add_cube("double_sided_white", 3.0, -1.4, {0.92, 0.92, 0.88, 1.0}, 0.0, 0.45, {0.0, 0.0, 0.0}, 1.0, true, true)
end

function MaterialShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then
        dt = 0.1
    end
    state.time = state.time + dt
    for i, item in ipairs(state.cubes) do
        dse.ecs.set_transform_rotation(item.entity, math.sin(state.time * 0.5 + i) * 8.0, state.time * (12.0 + i * 3.0), 0.0)
    end
    if state.animated ~= nil then
        local pulse = (math.sin(state.time * 1.6) + 1.0) * 0.5
        local roughness = 0.08 + pulse * 0.88
        dse.ecs.set_mesh_material_scalar(state.animated, "roughness", roughness)
        dse.ecs.set_mesh_emissive(state.animated, 0.10 + pulse * 0.35, 0.02, 0.18 + pulse * 0.28)
    end
end

return MaterialShowcase3D
