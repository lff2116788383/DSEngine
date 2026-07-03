local M = {}
M._meta = { name = "3D Meshlet Rendering", category = "rendering" }

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
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 5, -10, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)
    ecs.add_free_camera_controller(cam)

    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 20, 0.1, 20)
    ecs.add_mesh_renderer(ground, 0.35, 0.35, 0.4, 1.0, cube_vertices(), cube_indices())

    for ring = 1, 3 do
        local r = ring * 1.5
        local count = ring * 8
        for i = 1, count do
            local angle = (i / count) * math.pi * 2
            local e = ecs.create_entity()
            local x = math.cos(angle) * r
            local z = math.sin(angle) * r
            local s = 0.3 + (4 - ring) * 0.1
            ecs.add_transform(e, x, 1.5 + ring * 0.5, z, s, s, s)
            local c = 0.4 + ring * 0.15
            ecs.add_mesh_renderer(e, c, c * 0.8, 0.9, 1.0, cube_vertices(), cube_indices())
        end
    end

    local center = ecs.create_entity()
    ecs.add_transform(center, 0, 2.5, 0, 2.0, 2.0, 2.0)
    ecs.add_mesh_renderer(center, 0.8, 0.7, 0.3, 1.0, cube_vertices(), cube_indices())

    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 10, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.9, 1.8, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M
