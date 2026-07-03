local M = {}
M._meta = { name = "3D Foliage System", category = "vegetation" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 5, -10, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 200.0)

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 50, 0.1, 50)
    ecs.add_mesh_renderer(ground, "cube")

    -- Foliage patches (grass, bushes)
    for i = 1, 20 do
        local e = ecs.create_entity()
        local x = (math.random() - 0.5) * 30.0
        local z = (math.random() - 0.5) * 30.0
        ecs.add_transform(e, x, 0.3, z, 1, 0.6, 1)
        ecs.add_mesh_renderer(e, "cube")
        ecs.add_foliage(e, "grass")
    end

    -- Bushes (larger foliage)
    for i = 1, 10 do
        local e = ecs.create_entity()
        local x = (math.random() - 0.5) * 25.0
        local z = (math.random() - 0.5) * 25.0
        ecs.add_transform(e, x, 0.5, z, 1.5, 1.0, 1.5)
        ecs.add_mesh_renderer(e, "sphere")
        ecs.add_foliage(e, "bush")
    end

    -- Wind source
    local wind = ecs.create_entity()
    ecs.add_transform(wind, 0, 2, 0, 1, 1, 1)

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 20, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.85, 1.5)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M