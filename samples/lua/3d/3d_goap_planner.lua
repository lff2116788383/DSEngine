local M = {}
M._meta = { name = "3D GOAP Planner", category = "ai" }

local ecs = dse.ecs
local state = { elapsed = 0, npc = 0, targets = {} }

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
    ecs.add_transform(cam, 0, 10, -12, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)
    ecs.add_free_camera_controller(cam)

    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 20, 0.1, 20)
    ecs.add_mesh_renderer(ground, 0.4, 0.45, 0.35, 1.0, cube_vertices(), cube_indices())

    -- NPC with GOAP-like behavior (simulated)
    local npc = ecs.create_entity()
    ecs.add_transform(npc, 0, 1.0, 0, 0.7, 1.6, 0.7)
    ecs.add_mesh_renderer(npc, 0.3, 0.7, 0.9, 1.0, cube_vertices(), cube_indices())
    state.npc = npc

    -- Goal targets (food, water, rest)
    local goals = {{4,0,3,1.0,0.8,0.2},{-3,0,5,0.2,0.5,1.0},{-5,0,-2,0.5,1.0,0.5}}
    for _, g in ipairs(goals) do
        local t = ecs.create_entity()
        ecs.add_transform(t, g[1], 0.5, g[3], 1.0, 1.0, 1.0)
        ecs.add_mesh_renderer(t, g[4], g[5], g[6], 1.0, cube_vertices(), cube_indices())
        table.insert(state.targets, {entity = t, x = g[1], z = g[3]})
    end

    -- Obstacles
    for i = 1, 5 do
        local obs = ecs.create_entity()
        local angle = (i / 5) * math.pi * 2
        ecs.add_transform(obs, math.cos(angle) * 7, 0.6, math.sin(angle) * 7, 1.2, 1.2, 1.2)
        ecs.add_mesh_renderer(obs, 0.6, 0.3, 0.3, 1.0, cube_vertices(), cube_indices())
    end

    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 12, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.9, 1.5, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Simulate GOAP: NPC moves between goal targets
    local idx = math.floor(state.elapsed * 0.3) % #state.targets + 1
    local tgt = state.targets[idx]
    local t = state.elapsed * 0.8
    local x = tgt.x + math.sin(t) * 0.5
    local z = tgt.z + math.cos(t) * 0.5
    ecs.add_transform(state.npc, x, 1.0, z, 0.7, 1.6, 0.7)
end

return M
