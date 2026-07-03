local M = {}
M._meta = { name = "3D Open World Complete", category = "comprehensive" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera with spring arm on player
    local player = ecs.create_entity()
    ecs.add_transform(player, 0, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(player, "capsule")
    ecs.add_rigidbody3d(player, 1)
    ecs.add_capsule_collider3d(player, 0.5, 1.0)
    ecs.add_character_movement(player)
    state.player = player

    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 5, -8, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 500.0)
    ecs.add_spring_arm(cam, player)
    state.cam = cam

    -- World partition
    local world = ecs.create_entity()
    ecs.add_transform(world, 0, 0, 0, 1, 1, 1)
    ecs.add_world_partition(world, 64.0, 4, 4)

    -- Terrain
    local terrain = ecs.create_entity()
    ecs.add_transform(terrain, 0, 0, 0, 200, 0.5, 200)
    ecs.add_mesh_renderer(terrain, "cube")

    -- Day/night
    local dn = ecs.create_entity()
    ecs.add_transform(dn, 0, 0, 0, 1, 1, 1)
    ecs.add_day_night(dn)

    -- Atmosphere + clouds
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0, 1, 1, 1)
    ecs.add_atmosphere(sky)
    ecs.add_volumetric_cloud(sky)

    -- Forest (trees + foliage)
    for i = 1, 20 do
        local tree = ecs.create_entity()
        local x = (math.random() - 0.5) * 60.0
        local z = (math.random() - 0.5) * 60.0
        local h = 5.0 + math.random() * 4.0
        ecs.add_transform(tree, x, h * 0.5, z, 1.5, h, 1.5)
        ecs.add_mesh_renderer(tree, "cylinder")
        ecs.add_tree(tree)
    end

    -- AI NPCs
    for i = 1, 3 do
        local npc = ecs.create_entity()
        local x = math.cos(i * 2.1) * 15.0
        local z = math.sin(i * 2.1) * 15.0
        ecs.add_transform(npc, x, 1, z, 0.8, 1.6, 0.8)
        ecs.add_mesh_renderer(npc, "capsule")
        dse.ai.add_behavior_tree(npc, "patrol")
    end

    -- Water body
    local water = ecs.create_entity()
    ecs.add_transform(water, 30, -0.5, 30, 40, 1, 40)
    ecs.add_water(water)

    -- Sun
    local sun = ecs.create_entity()
    ecs.add_transform(sun, 0, 50, 0, 1, 1, 1)
    ecs.add_dir_light(sun, 1.0, 0.95, 0.85, 2.0)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M