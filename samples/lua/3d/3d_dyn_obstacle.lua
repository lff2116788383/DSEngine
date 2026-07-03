local M = {}
M._meta = { name = "3D Dynamic Obstacle Avoidance", category = "ai" }

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
    ecs.add_transform(cam, 0, 15, -15, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)

    -- Ground + NavMesh
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 30, 0.1, 30)
    ecs.add_mesh_renderer(ground, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

    -- Static obstacles (part of navmesh)
    local statics = {
        {-5, 1, -5, 2, 2, 2},
        {5, 1, 5, 2, 2, 2},
        {-3, 1, 4, 1.5, 2, 3},
        {4, 1, -3, 3, 2, 1.5},
    }
    for _, s in ipairs(statics) do
        local e = ecs.create_entity()
        ecs.add_transform(e, s[1], s[2], s[3], s[4], s[5], s[6])
        ecs.add_mesh_renderer(e, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
        ecs.add_rigidbody_3d(e, 0)
        ecs.add_box_collider_3d(e, s[4], s[5], s[6])
    end

    -- Dynamic obstacles (move and agents must avoid them)
    for i = 1, 3 do
        local dyn = ecs.create_entity()
        local x = math.cos(i * 2.1) * 6.0
        local z = math.sin(i * 2.1) * 6.0
        ecs.add_transform(dyn, x, 0.75, z, 1.5, 1.5, 1.5)
        ecs.add_mesh_renderer(dyn, 0.7, 0.7, 0.7, 1.0, cube_vertices(), cube_indices())
        ecs.add_dynamic_obstacle(dyn, 1.0) -- obstacle radius
        table.insert(state, dyn)
    end
    state.obstacles = state

    -- Navigation agents
    for i = 1, 4 do
        local agent = ecs.create_entity()
        local angle = (i / 4) * math.pi * 2
        ecs.add_transform(agent, math.cos(angle) * 10, 1, math.sin(angle) * 10, 0.8, 1.6, 0.8)
        ecs.add_mesh_renderer(agent, 0.5, 0.7, 0.5, 1.0, cube_vertices(), cube_indices())
    end

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 15, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.9, 1.4, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M