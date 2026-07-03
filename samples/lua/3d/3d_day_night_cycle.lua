local M = {}
M._meta = { name = "3D Day Night Cycle", category = "environment" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 5, -12, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 200.0)

    -- Day/Night controller
    local dn = ecs.create_entity()
    ecs.add_transform(dn, 0, 0, 0, 1, 1, 1)
    ecs.add_day_night(dn)
    state.dn = dn

    -- Atmosphere/Sky
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0, 1, 1, 1)
    ecs.add_atmosphere(sky)

    -- Sun light (controlled by day/night)
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 30, 0, 1, 1, 1)
    ecs.add_dir_light(sun, 1.0, 0.95, 0.85, 2.0)
    state.sun = sun

    -- Scene: village-like setup
    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 40, 0.1, 40)
    ecs.add_mesh_renderer(ground, "cube")

    -- Buildings
    for i = 1, 6 do
        local bldg = ecs.create_entity()
        local x = math.cos(i * 1.05) * 6.0
        local z = math.sin(i * 1.05) * 6.0
        local h = 2.0 + math.random() * 3.0
        ecs.add_transform(bldg, x, h * 0.5, z, 2, h, 2)
        ecs.add_mesh_renderer(bldg, "cube")
    end

    -- Point lights (street lamps, visible at night)
    for i = 1, 4 do
        local lamp = ecs.create_entity()
        local angle = (i / 4) * math.pi * 2
        ecs.add_transform(lamp, math.cos(angle) * 4, 3, math.sin(angle) * 4, 1, 1, 1)
        ecs.add_point_light(lamp, 1.0, 0.8, 0.4, 1.5, 8.0)
    end

    -- Trees
    for i = 1, 8 do
        local tree = ecs.create_entity()
        local x = math.cos(i * 0.79) * 12.0
        local z = math.sin(i * 0.79) * 12.0
        ecs.add_transform(tree, x, 2, z, 1, 4, 1)
        ecs.add_mesh_renderer(tree, "cylinder")
    end
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M