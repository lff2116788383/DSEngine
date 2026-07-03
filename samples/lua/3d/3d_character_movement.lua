local M = {}
M._meta = { name = "3D Character Movement", category = "3c" }

local ecs = dse.ecs
local state = { elapsed = 0, char = 0 }

local function cube_vertices()
    return {
        -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
        -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
    }
end
local function cube_indices()
    return {
        0,1,2, 2,3,0, 1,5,6, 6,2,1, 5,4,7, 7,6,5, 4,0,3, 3,7,4, 3,2,6, 6,7,3, 4,5,1, 1,0,4
    }
end

function M.Setup(config)
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 8, -12, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)
    ecs.add_free_camera_controller(cam)

    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 20, 0.1, 20)
    ecs.add_mesh_renderer(ground, 0.4, 0.5, 0.4, 1.0, cube_vertices(), cube_indices())

    local char = ecs.create_entity()
    ecs.add_transform(char, 0, 1.0, 0, 0.6, 1.8, 0.6)
    ecs.add_mesh_renderer(char, 0.2, 0.6, 0.9, 1.0, cube_vertices(), cube_indices())
    state.char = char

    local obs_positions = {{3,0,2},{-4,0,1},{1,0,-3},{-2,0,4},{5,0,-2}}
    for _, p in ipairs(obs_positions) do
        local obs = ecs.create_entity()
        ecs.add_transform(obs, p[1], 0.75, p[3], 1.5, 1.5, 1.5)
        ecs.add_mesh_renderer(obs, 0.7, 0.3, 0.2, 1.0, cube_vertices(), cube_indices())
    end

    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 10, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.9, 1.5, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    local t = state.elapsed * 0.6
    local x = math.sin(t) * 4.0
    local z = math.sin(t * 2.0) * 3.0
    ecs.add_transform(state.char, x, 1.0, z, 0.6, 1.8, 0.6)
end

return M
