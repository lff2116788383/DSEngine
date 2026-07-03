local M = {}
M._meta = { name = "3D Behavior Tree AI", category = "ai" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 12, -15, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 100.0)

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 30, 0.1, 30)
    ecs.add_mesh_renderer(ground, "cube")

    -- AI Agents with behavior trees
    for i = 1, 4 do
        local agent = ecs.create_entity()
        local x = math.cos(i * 1.57) * 5.0
        local z = math.sin(i * 1.57) * 5.0
        ecs.add_transform(agent, x, 1, z, 0.8, 1.6, 0.8)
        ecs.add_mesh_renderer(agent, "capsule")
        ecs.add_rigidbody3d(agent, 1)
        ecs.add_capsule_collider3d(agent, 0.4, 0.8)

        -- Attach behavior tree
        dse.ai.add_behavior_tree(agent, "patrol_seek")
        dse.ai.set_blackboard(agent, "patrol_radius", 8.0)
        dse.ai.set_blackboard(agent, "speed", 3.0 + i * 0.5)
    end

    -- Target (what AI seeks)
    local target = ecs.create_entity()
    ecs.add_transform(target, 0, 0.5, 0, 0.6, 0.6, 0.6)
    ecs.add_mesh_renderer(target, "sphere")
    state.target = target

    -- Waypoints
    for i = 1, 8 do
        local wp = ecs.create_entity()
        local angle = (i / 8) * math.pi * 2
        ecs.add_transform(wp, math.cos(angle) * 10, 0.2, math.sin(angle) * 10, 0.3, 0.3, 0.3)
        ecs.add_mesh_renderer(wp, "sphere")
    end

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 15, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.9, 1.5)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Move target to test AI reactivity
    local x = math.sin(state.elapsed * 0.4) * 6.0
    local z = math.cos(state.elapsed * 0.3) * 6.0
    ecs.add_transform(state.target, x, 0.5, z, 0.6, 0.6, 0.6)
end

return M