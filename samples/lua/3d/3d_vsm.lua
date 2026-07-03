local M = {}
M._meta = { name = "3D Virtual Shadow Maps", category = "rendering_advanced" }

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
    ecs.add_transform(cam, 0, 8, -12, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 200.0)

    -- Ground (large, receives shadows)
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 60, 0.1, 60)
    ecs.add_mesh_renderer(ground, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

    -- Shadow casters at various distances
    for i = 1, 15 do
        local e = ecs.create_entity()
        local x = math.cos(i * 0.42) * (3.0 + i * 1.5)
        local z = math.sin(i * 0.42) * (3.0 + i * 1.5)
        local h = 1.0 + math.random() * 3.0
        ecs.add_transform(e, x, h * 0.5, z, 1, h, 1)
        ecs.add_mesh_renderer(e, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
    end

    -- Directional light with VSM enabled
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 30, 0, 1, 1, 1)
    ecs.add_directional_light_3d(sun, -0.45, -1.0, -0.25, 1.0, 0.95, 0.85, 2.0, 0.15, 0.25)
    state.sun = sun

    -- Sphere cluster to show detailed shadow contact
    for i = 1, 6 do
        local s = ecs.create_entity()
        ecs.add_transform(s, -2 + i * 0.8, 0.5, 5, 0.8, 0.8, 0.8)
        ecs.add_mesh_renderer(s, 0.7, 0.7, 0.7, 1.0, cube_vertices(), cube_indices())
    end
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M