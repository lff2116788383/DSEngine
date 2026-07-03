local M = {}
M._meta = { name = "3D Ocean Rendering", category = "environment" }

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
    ecs.add_transform(cam, 0, 8, -15, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 500.0)
    state.cam = cam

    -- Ocean surface
    local ocean = ecs.create_entity()
    ecs.add_transform(ocean, 0, 0, 0, 200, 1, 200)
    ecs.add_water(ocean) -- uses ocean system internally

    -- Island/terrain above water
    local island = ecs.create_entity()
    ecs.add_transform(island, 10, 1.5, 10, 8, 3, 8)
    ecs.add_mesh_renderer(island, 0.7, 0.7, 0.7, 1.0, cube_vertices(), cube_indices())

    -- Floating objects
    for i = 1, 5 do
        local obj = ecs.create_entity()
        local x = math.cos(i * 1.26) * 8.0
        local z = math.sin(i * 1.26) * 8.0
        ecs.add_transform(obj, x, 0.3, z, 0.5, 0.5, 0.5)
        ecs.add_mesh_renderer(obj, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
        ecs.add_rigidbody_3d(obj, 1)
        ecs.add_buoyancy(obj, 1.0, 0.5)
    end

    -- Skybox
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0, 1, 1, 1)
    ecs.add_atmosphere(sky)

    -- Sun
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 30, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.85, 2.0, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M