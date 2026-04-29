-- 3D basic sample: cube
-- 目标：从 2D 面片进入 3D 体，观察 8 个顶点和 12 个三角形组成的立方体。
local Cube3D = {}

local state = {
    camera = nil,
    light = nil,
    cube = nil,
    time = 0.0
}

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 6.5
    dse.ecs.add_transform(camera, 0.0, 2.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -18.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera)
    end
    state.camera = camera
end

local function setup_light()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.5, -1.0, -0.4, 1.0, 0.96, 0.88, 1.25, 0.2, 0.35)
    state.light = light
end

local function setup_cube()
    local entity = dse.ecs.create_entity()
    dse.ecs.add_transform(entity, 0.0, 0.0, 0.0, 2.0, 2.0, 2.0)

    -- 立方体：8 个共享顶点，6 个面，每个面 2 个三角形。
    local vertices = {
        -0.5, -0.5,  0.5,
         0.5, -0.5,  0.5,
         0.5,  0.5,  0.5,
        -0.5,  0.5,  0.5,
        -0.5, -0.5, -0.5,
         0.5, -0.5, -0.5,
         0.5,  0.5, -0.5,
        -0.5,  0.5, -0.5
    }
    local indices = {
        0, 1, 2, 2, 3, 0, -- front
        1, 5, 6, 6, 2, 1, -- right
        5, 4, 7, 7, 6, 5, -- back
        4, 0, 3, 3, 7, 4, -- left
        3, 2, 6, 6, 7, 3, -- top
        4, 5, 1, 1, 0, 4  -- bottom
    }

    dse.ecs.add_mesh_renderer(entity, 0.38, 1.0, 0.35, 1.0, vertices, indices)
    dse.ecs.set_mesh_shader_variant(entity, "MESH_LIT")
    dse.ecs.set_mesh_material(entity, 0.05, 0.52, 1.0, 0.06, 0.02, 0.12, 0.02, true)
    state.cube = entity
end

function Cube3D.Setup(config)
    print("[3D-Basic][Cube] setup: 8 vertices, 12 triangles. Use W/A/S/D + mouse to inspect.")
    setup_camera(config or {})
    setup_light()
    setup_cube()
end

function Cube3D.Update(delta_time)
    state.time = state.time + (delta_time or 0.0)
    if state.cube ~= nil then
        dse.ecs.set_transform_rotation(state.cube, state.time * 18.0, state.time * 28.0, state.time * 10.0)
    end
end

return Cube3D
