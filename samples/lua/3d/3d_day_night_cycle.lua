local M = {}
M._meta = { name = "3D Day Night Cycle", category = "environment" }

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
    ecs.add_transform(cam, 0, 5, -12, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 200.0)

    -- Day/Night controller
    local dn = ecs.create_entity()
    ecs.add_transform(dn, 0, 0, 0, 1, 1, 1)
    ecs.add_day_night_cycle(dn)
    state.dn = dn

    -- Atmosphere/Sky
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0, 1, 1, 1)
    ecs.add_atmosphere(sky)

    -- Sun light (controlled by day/night)
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 30, 0, 1, 1, 1)
    ecs.add_directional_light_3d(sun, -0.45, -1.0, -0.25, 1.0, 0.95, 0.85, 2.0, 0.15, 0.25)
    state.sun = sun

    -- Scene: village-like setup
    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 40, 0.1, 40)
    ecs.add_mesh_renderer(ground, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

    -- Buildings
    for i = 1, 6 do
        local bldg = ecs.create_entity()
        local x = math.cos(i * 1.05) * 6.0
        local z = math.sin(i * 1.05) * 6.0
        local h = 2.0 + math.random() * 3.0
        ecs.add_transform(bldg, x, h * 0.5, z, 2, h, 2)
        ecs.add_mesh_renderer(bldg, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
    end

    -- Point lights (street lamps, visible at night)
    for i = 1, 4 do
        local lamp = ecs.create_entity()
        local angle = (i / 4) * math.pi * 2
        ecs.add_transform(lamp, math.cos(angle) * 4, 3, math.sin(angle) * 4, 1, 1, 1)
        ecs.add_point_light_3d(lamp, 1.0, 0.8, 0.4, 1.5, 8.0)
    end

    -- Trees
    for i = 1, 8 do
        local tree = ecs.create_entity()
        local x = math.cos(i * 0.79) * 12.0
        local z = math.sin(i * 0.79) * 12.0
        ecs.add_transform(tree, x, 2, z, 1, 4, 1)
        ecs.add_mesh_renderer(tree, 0.7, 0.5, 0.3, 1.0, cube_vertices(), cube_indices())
    end
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M