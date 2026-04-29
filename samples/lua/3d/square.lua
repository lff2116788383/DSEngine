-- 3D basic sample: square
-- 目标：用两个三角形拼出一个正方形，理解索引缓冲复用顶点。
local Square3D = {}

local state = {
    camera = nil,
    light = nil,
    square = nil,
    time = 0.0
}

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 6.0
    dse.ecs.add_transform(camera, 0.0, 1.4, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -12.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera)
    end
    state.camera = camera
end

local function setup_light()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.35, 0.9, 1.0, 0.95, 1.15, 0.18, 0.30)
    state.light = light
end

local function setup_square()
    local entity = dse.ecs.create_entity()
    dse.ecs.add_transform(entity, 0.0, 0.0, 0.0, 2.2, 2.2, 2.2)

    -- 正方形：4 个顶点，用 2 个三角形组成一个四边形面。
    local vertices = {
        -0.8,  0.8, 0.0,
        -0.8, -0.8, 0.0,
         0.8, -0.8, 0.0,
         0.8,  0.8, 0.0
    }
    local indices = {
        0, 1, 2,
        2, 3, 0
    }

    dse.ecs.add_mesh_renderer(entity, 0.18, 0.65, 1.0, 1.0, vertices, indices)
    dse.ecs.set_mesh_shader_variant(entity, "MESH_LIT")
    dse.ecs.set_mesh_material(entity, 0.03, 0.62, 1.0, 0.04, 0.01, 0.08, 0.12, true)
    state.square = entity
end

function Square3D.Setup(config)
    print("[3D-Basic][Square] setup: 4 vertices, 2 triangles. Use W/A/S/D + mouse to inspect.")
    setup_camera(config or {})
    setup_light()
    setup_square()
end

function Square3D.Update(delta_time)
    state.time = state.time + (delta_time or 0.0)
    if state.square ~= nil then
        dse.ecs.set_transform_rotation(state.square, math.sin(state.time * 0.6) * 15.0, state.time * 22.0, 0.0)
    end
end

return Square3D
