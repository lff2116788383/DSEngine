local M = {}
M._meta = { name = "3D Impostor Rendering", category = "rendering_advanced" }

local ecs = dse.ecs
local state = { elapsed = 0 }

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
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 10, -20, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 500.0)
    state.cam = cam

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 100, 0.1, 100)
    ecs.add_mesh_renderer(ground, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

    -- Near objects (full mesh)
    for i = 1, 5 do
        local e = ecs.create_entity()
        ecs.add_transform(e, -4 + i * 2, 1.5, 0, 1, 3, 1)
        ecs.add_mesh_renderer(e, 0.7, 0.5, 0.3, 1.0, cube_vertices(), cube_indices())
    end

    -- Far objects with impostor (billboard replacement at distance)
    for i = 1, 20 do
        local e = ecs.create_entity()
        local x = math.cos(i * 0.31) * (20.0 + i * 2.0)
        local z = math.sin(i * 0.31) * (20.0 + i * 2.0)
        ecs.add_transform(e, x, 2, z, 1.5, 4, 1.5)
        ecs.add_mesh_renderer(e, 0.7, 0.5, 0.3, 1.0, cube_vertices(), cube_indices())
        pcall(ecs.add_impostor,e, 30.0) -- switch to impostor at 30 units
    end

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 30, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.85, 1.5, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Pan camera to show impostor transitions
    local x = math.sin(state.elapsed * 0.2) * 10.0
    ecs.add_transform(state.cam, x, 10, -20, 1, 1, 1)
end

return M