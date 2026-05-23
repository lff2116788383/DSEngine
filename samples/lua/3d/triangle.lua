-- 3D basic sample: triangle
-- 目标：用最少顶点认识 3D Mesh 的三角形面片。
local Triangle3D = {}


Triangle3D._meta = {
    name     = "3D basic sample: triangle",
    category = "basic",
}

local state = {
    camera = nil,
    light = nil,
    triangle = nil,
    time = 0.0
}

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 5.5
    dse.ecs.add_transform(camera, 0.0, 1.2, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -10.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera)
    end
    state.camera = camera
end

local function setup_light()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.25, 1.0, 0.97, 0.9, 1.2, 0.18, 0.28)
    state.light = light
end

local function setup_triangle()
    local entity = dse.ecs.create_entity()
    dse.ecs.add_transform(entity, 0.0, 0.0, 0.0, 2.2, 2.2, 2.2)

    -- 单个三角形：3 个顶点 + 1 个三角面。
    local vertices = {
         0.0,  0.8, 0.0,
        -0.8, -0.7, 0.0,
         0.8, -0.7, 0.0
    }
    local indices = {
        0, 1, 2
    }

    dse.ecs.add_mesh_renderer(entity, 1.0, 0.35, 0.18, 1.0, vertices, indices)
    dse.ecs.set_mesh_shader_variant(entity, "MESH_LIT")
    dse.ecs.set_mesh_material(entity, 0.02, 0.72, 1.0, 0.03, 0.12, 0.02, 0.0, true, true)
    state.triangle = entity
end

function Triangle3D.Setup(config)
    print("[3D-Basic][Triangle] setup: 3 vertices, 1 face. Use W/A/S/D + mouse to inspect.")
    setup_camera(config or {})
    setup_light()
    setup_triangle()
end

function Triangle3D.Update(delta_time)
    state.time = state.time + (delta_time or 0.0)
    if state.triangle ~= nil then
        dse.ecs.set_transform_rotation(state.triangle, 0.0, state.time * 25.0, 0.0)
    end
end

return Triangle3D
