-- 3D P0 sample: textured cube
-- 目标：使用 .dmesh + .dmat 验证 UV/texture/material 链路，并与纯色 cube 对比。
local TexturedCube3D = {}

local state = {
    camera = nil,
    light = nil,
    solid_cube = nil,
    textured_cube = nil,
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
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 7.5
    dse.ecs.add_transform(camera, 0.0, 2.2, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -17.0, 0.0, 0.0)
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
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.3, 1.0, 0.96, 0.9, 1.35, 0.22, 0.35)
    state.light = light
end

local function setup_solid_cube()
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, -1.7, 0.0, 0.0, 1.45, 1.45, 1.45)
    dse.ecs.add_mesh_renderer(e, 0.18, 0.62, 1.0, 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.02, 0.58, 1.0, 0.02, 0.04, 0.10, 1.0, true, true)
    state.solid_cube = e
end

local function setup_textured_cube(config)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, 1.7, 0.0, 0.0, 1.45, 1.45, 1.45)
    dse.ecs.add_mesh_renderer(e, 1.0, 1.0, 1.0, 1.0)
    local mesh_path = (type(config) == "table" and type(config.mesh_path) == "string") and config.mesh_path or "models/cube.dmesh"
    local material_path = (type(config) == "table" and type(config.material_path) == "string") and config.material_path or "models/cube.dmat"
    dse.ecs.set_mesh_path(e, mesh_path)
    dse.ecs.set_mesh_material(e, material_path)
    state.textured_cube = e
    print(string.format("[3D][TexturedCube] textured cube uses mesh=%s material=%s; expected albedo texture from dmat.", mesh_path, material_path))
end

function TexturedCube3D.Setup(config)
    print("[3D][TexturedCube] setup: left=solid hand-written cube, right=.dmesh/.dmat textured cube. Use free camera to inspect UV/texture.")
    setup_camera(config or {})
    setup_light()
    setup_solid_cube()
    setup_textured_cube(config or {})
end

function TexturedCube3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then
        dt = 0.1
    end
    state.time = state.time + dt
    if state.solid_cube ~= nil then
        dse.ecs.set_transform_rotation(state.solid_cube, state.time * 12.0, state.time * 25.0, 0.0)
    end
    if state.textured_cube ~= nil then
        dse.ecs.set_transform_rotation(state.textured_cube, state.time * 12.0, -state.time * 25.0, state.time * 5.0)
    end
end

return TexturedCube3D
