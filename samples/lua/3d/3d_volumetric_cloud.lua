local M = {}
M._meta = { name = "3D Volumetric Cloud", category = "environment" }

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
    -- Camera looking up at sky
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 2, -5, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 1000.0)
    state.cam = cam

    -- Volumetric cloud layer
    local cloud = ecs.create_entity()
    ecs.add_transform(cloud, 0, 50, 0, 1, 1, 1)
    ecs.add_volumetric_cloud(cloud)

    -- Skybox / atmosphere
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0, 1, 1, 1)
    ecs.add_atmosphere(sky)

    -- Ground reference
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 100, 0.1, 100)
    ecs.add_mesh_renderer(ground, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

    -- Some terrain features
    for i = 1, 8 do
        local hill = ecs.create_entity()
        local x = math.cos(i * 0.79) * 20.0
        local z = math.sin(i * 0.79) * 20.0
        local h = 2.0 + math.random() * 3.0
        ecs.add_transform(hill, x, h * 0.5, z, 3, h, 3)
        ecs.add_mesh_renderer(hill, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
    end

    -- Sun light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 50, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.92, 0.8, 2.0, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M