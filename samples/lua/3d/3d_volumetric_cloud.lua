local M = {}
M._meta = { name = "3D Volumetric Cloud", category = "environment" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera looking up at sky
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 2, -5, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 1000.0)
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
    ecs.add_mesh_renderer(ground, "cube")

    -- Some terrain features
    for i = 1, 8 do
        local hill = ecs.create_entity()
        local x = math.cos(i * 0.79) * 20.0
        local z = math.sin(i * 0.79) * 20.0
        local h = 2.0 + math.random() * 3.0
        ecs.add_transform(hill, x, h * 0.5, z, 3, h, 3)
        ecs.add_mesh_renderer(hill, "cube")
    end

    -- Sun light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 50, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.92, 0.8, 2.0)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M