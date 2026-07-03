local M = {}
M._meta = { name = "3D GOAP Planner", category = "ai" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 15, -18, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 100.0)

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 40, 0.1, 40)
    ecs.add_mesh_renderer(ground, "cube")

    -- GOAP Agent (NPC with goals)
    local npc = ecs.create_entity()
    ecs.add_transform(npc, 0, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(npc, "capsule")
    ecs.add_rigidbody3d(npc, 1)
    ecs.add_capsule_collider3d(npc, 0.5, 1.0)
    dse.ai.add_goap(npc)
    -- Define goals and actions
    dse.ai.goap_add_goal(npc, "survive", 1.0)
    dse.ai.goap_add_goal(npc, "collect_resources", 0.5)
    dse.ai.goap_add_action(npc, "find_food", "hungry", "fed")
    dse.ai.goap_add_action(npc, "gather_wood", "need_wood", "has_wood")
    state.npc = npc

    -- Resources scattered in the world
    for i = 1, 6 do
        local res = ecs.create_entity()
        local x = math.cos(i * 1.05) * 8.0
        local z = math.sin(i * 1.05) * 8.0
        ecs.add_transform(res, x, 0.4, z, 0.5, 0.8, 0.5)
        ecs.add_mesh_renderer(res, "cylinder")
    end

    -- Shelter (goal destination)
    local shelter = ecs.create_entity()
    ecs.add_transform(shelter, 8, 1.5, 8, 3, 3, 3)
    ecs.add_mesh_renderer(shelter, "cube")

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 20, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.9, 0.8, 1.5)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M