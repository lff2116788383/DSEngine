-- 3D P0 sample: static model loading
-- 目标：从手写 cube 过渡到 .dmesh/.dmat 资源化 Mesh/Material。
local StaticModel3D = {}


StaticModel3D._meta = {
    name     = "static model loading",
    category = "rendering",
    config   = { camera_distance=8.0,
    mesh_path="models/cube.dmesh",
    material_path="models/cube.dmat" },
}

local state = {
    camera = nil,
    light = nil,
    handwritten_cube = nil,
    resource_cube = nil,
    time = 0.0
}

local function add_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 8.0
    dse.ecs.add_transform(camera, 0.0, 2.2, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -16.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera, 5.0, 0.12)
    elseif dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.0, 0.12)
    end
    state.camera = camera
end

local function add_light()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.35, 1.0, 0.96, 0.88, 1.25, 0.25, 0.35)
    state.light = light
end

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

local function add_handwritten_cube()
    local entity = dse.ecs.create_entity()
    dse.ecs.add_transform(entity, -1.8, 0.0, 0.0, 1.5, 1.5, 1.5)
    dse.ecs.add_mesh_renderer(entity, 0.25, 0.65, 1.0, 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(entity, "MESH_LIT")
    dse.ecs.set_mesh_material(entity, 0.03, 0.55, 1.0, 0.02, 0.03, 0.10, 0.12, true, true)
    state.handwritten_cube = entity
end

local function add_resource_cube(config)
    local entity = dse.ecs.create_entity()
    dse.ecs.add_transform(entity, 1.8, 0.0, 0.0, 1.5, 1.5, 1.5)
    dse.ecs.add_mesh_renderer(entity, 1.0, 1.0, 1.0, 1.0)
    local mesh_path = (type(config) == "table" and type(config.mesh_path) == "string") and config.mesh_path or "models/cube.dmesh"
    local material_path = (type(config) == "table" and type(config.material_path) == "string") and config.material_path or "models/cube.dmat"
    dse.ecs.set_mesh_path(entity, mesh_path)
    dse.ecs.set_mesh_material(entity, material_path)
    state.resource_cube = entity
    print(string.format("[3D][StaticModel] loaded %s + %s", mesh_path, material_path))
end

function StaticModel3D.Setup(config)
    print("[3D][StaticModel] setup: left=handwritten cube, right=.dmesh/.dmat resource cube. Use W/A/S/D + right mouse to inspect.")
    add_camera(config or {})
    add_light()
    add_handwritten_cube()
    add_resource_cube(config or {})
end

function StaticModel3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then
        dt = 0.1
    end
    state.time = state.time + dt
    if state.handwritten_cube ~= nil then
        dse.ecs.set_transform_rotation(state.handwritten_cube, state.time * 16.0, state.time * 26.0, 0.0)
    end
    if state.resource_cube ~= nil then
        dse.ecs.set_transform_rotation(state.resource_cube, state.time * 14.0, -state.time * 24.0, state.time * 8.0)
    end
end

return StaticModel3D
