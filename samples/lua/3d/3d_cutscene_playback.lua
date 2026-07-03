local M = {}
M._meta = { name = "3D Scene Playback", category = "gameplay" }

local ecs = dse.ecs
local state = { elapsed = 0, actors = {}, camera = 0 }

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
    ecs.add_transform(cam, 5, 3, -8, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)
    state.camera = cam

    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 15, 0.1, 10)
    ecs.add_mesh_renderer(ground, 0.3, 0.25, 0.2, 1.0, cube_vertices(), cube_indices())

    local a1 = ecs.create_entity()
    ecs.add_transform(a1, -2, 1.0, 0, 0.6, 2.0, 0.6)
    ecs.add_mesh_renderer(a1, 0.8, 0.3, 0.2, 1.0, cube_vertices(), cube_indices())
    table.insert(state.actors, a1)

    local a2 = ecs.create_entity()
    ecs.add_transform(a2, 2, 1.0, 0, 0.6, 1.8, 0.6)
    ecs.add_mesh_renderer(a2, 0.2, 0.5, 0.8, 1.0, cube_vertices(), cube_indices())
    table.insert(state.actors, a2)

    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 10, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.3, -1.0, -0.2, 0.9, 0.85, 0.8, 1.2, 0.1, 0.2)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    local t = state.elapsed * 0.5
    ecs.add_transform(state.actors[1], -2 + math.min(t, 2.0), 1.0, 0, 0.6, 2.0, 0.6)
    ecs.add_transform(state.actors[2], 2 - math.min(t, 2.0), 1.0, 0, 0.6, 1.8, 0.6)
    local cx = math.cos(t * 0.3) * 8
    local cz = math.sin(t * 0.3) * 8
    ecs.add_transform(state.camera, cx, 3, cz, 1, 1, 1)
end

return M
