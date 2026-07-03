local M = {}
M._meta = { name = "3D Foliage System", category = "vegetation" }

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
    ecs.add_transform(cam, 0, 5, -10, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 200.0)

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 50, 0.1, 50)
    ecs.add_mesh_renderer(ground, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

    -- Foliage patches (grass, bushes)
    for i = 1, 20 do
        local e = ecs.create_entity()
        local x = (math.random() - 0.5) * 30.0
        local z = (math.random() - 0.5) * 30.0
        ecs.add_transform(e, x, 0.3, z, 1, 0.6, 1)
        ecs.add_mesh_renderer(e, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
        ecs.add_foliage(e, "grass")
    end

    -- Bushes (larger foliage)
    for i = 1, 10 do
        local e = ecs.create_entity()
        local x = (math.random() - 0.5) * 25.0
        local z = (math.random() - 0.5) * 25.0
        ecs.add_transform(e, x, 0.5, z, 1.5, 1.0, 1.5)
        ecs.add_mesh_renderer(e, 0.7, 0.7, 0.7, 1.0, cube_vertices(), cube_indices())
        ecs.add_foliage(e, "bush")
    end

    -- Wind source
    local wind = ecs.create_entity()
    ecs.add_transform(wind, 0, 2, 0, 1, 1, 1)

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 20, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.85, 1.5, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M