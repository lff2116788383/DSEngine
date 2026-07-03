local M = {}
M._meta = { name = "3D EQS Query", category = "ai" }

local ecs = dse.ecs
local state = { elapsed = 0, agent = 0, cover_points = {} }

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
    ecs.add_transform(cam, 0, 15, -18, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)
    ecs.add_free_camera_controller(cam)

    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 25, 0.1, 25)
    ecs.add_mesh_renderer(ground, 0.35, 0.4, 0.3, 1.0, cube_vertices(), cube_indices())

    -- Agent seeking cover
    local agent = ecs.create_entity()
    ecs.add_transform(agent, 0, 1.0, 0, 0.6, 1.8, 0.6)
    ecs.add_mesh_renderer(agent, 0.9, 0.3, 0.2, 1.0, cube_vertices(), cube_indices())
    state.agent = agent

    -- Cover points (small green markers)
    for i = 1, 12 do
        local angle = (i / 12) * math.pi * 2
        local r = 6.0 + math.sin(i * 2.1) * 3.0
        local cp = ecs.create_entity()
        ecs.add_transform(cp, math.cos(angle) * r, 0.3, math.sin(angle) * r, 0.4, 0.6, 0.4)
        ecs.add_mesh_renderer(cp, 0.2, 0.8, 0.3, 1.0, cube_vertices(), cube_indices())
        table.insert(state.cover_points, {entity = cp, x = math.cos(angle) * r, z = math.sin(angle) * r})
    end

    -- Walls/obstacles that provide cover
    local walls = {{5,0,0,0.5,3,4},{-4,0,3,4,2.5,0.5},{2,0,-5,0.5,2,6}}
    for _, w in ipairs(walls) do
        local wall = ecs.create_entity()
        ecs.add_transform(wall, w[1], w[5]*0.5, w[3], w[4], w[5], w[6])
        ecs.add_mesh_renderer(wall, 0.5, 0.5, 0.55, 1.0, cube_vertices(), cube_indices())
    end

    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 12, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.9, 1.5, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Agent moves toward best cover point
    local idx = math.floor(state.elapsed * 0.5) % #state.cover_points + 1
    local cp = state.cover_points[idx]
    local t = math.min((state.elapsed % 2.0) / 2.0, 1.0)
    local x = cp.x * t
    local z = cp.z * t
    ecs.add_transform(state.agent, x, 1.0, z, 0.6, 1.8, 0.6)
end

return M
