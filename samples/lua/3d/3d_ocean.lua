local M = {}
M._meta = { name = "3D Ocean Rendering", category = "environment" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 8, -15, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 500.0)
    state.cam = cam

    -- Ocean surface
    local ocean = ecs.create_entity()
    ecs.add_transform(ocean, 0, 0, 0, 200, 1, 200)
    ecs.add_water(ocean) -- uses ocean system internally

    -- Island/terrain above water
    local island = ecs.create_entity()
    ecs.add_transform(island, 10, 1.5, 10, 8, 3, 8)
    ecs.add_mesh_renderer(island, "sphere")

    -- Floating objects
    for i = 1, 5 do
        local obj = ecs.create_entity()
        local x = math.cos(i * 1.26) * 8.0
        local z = math.sin(i * 1.26) * 8.0
        ecs.add_transform(obj, x, 0.3, z, 0.5, 0.5, 0.5)
        ecs.add_mesh_renderer(obj, "cube")
        ecs.add_rigidbody3d(obj, 1)
        ecs.add_buoyancy(obj, 1.0, 0.5)
    end

    -- Skybox
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0, 1, 1, 1)
    ecs.add_atmosphere(sky)

    -- Sun
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 30, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.85, 2.0)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M