local M = {}
M._meta = { name = "3D Player Controller", category = "3c" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 8, -10, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 100.0)

    -- Player with full controller
    local player = ecs.create_entity()
    ecs.add_transform(player, 0, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(player, "capsule")
    ecs.add_rigidbody3d(player, 1)
    ecs.add_capsule_collider3d(player, 0.5, 1.0)
    ecs.add_player_controller(player)
    state.player = player

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 30, 0.1, 30)
    ecs.add_mesh_renderer(ground, "cube")
    ecs.add_rigidbody3d(ground, 0)
    ecs.add_box_collider3d(ground, 30, 0.1, 30)

    -- Ramp
    local ramp = ecs.create_entity()
    ecs.add_transform(ramp, 5, 0.5, 0, 4, 0.2, 3)
    ecs.add_mesh_renderer(ramp, "cube")
    ecs.add_rigidbody3d(ramp, 0)
    ecs.add_box_collider3d(ramp, 4, 0.2, 3)

    -- Stairs (stepped boxes)
    for i = 1, 5 do
        local step = ecs.create_entity()
        ecs.add_transform(step, -5, i * 0.3, i * 0.8, 2, 0.3, 0.8)
        ecs.add_mesh_renderer(step, "cube")
        ecs.add_rigidbody3d(step, 0)
        ecs.add_box_collider3d(step, 2, 0.3, 0.8)
    end

    -- Collectibles
    for i = 1, 6 do
        local pickup = ecs.create_entity()
        local angle = (i / 6) * math.pi * 2
        ecs.add_transform(pickup, math.cos(angle) * 5, 1.5, math.sin(angle) * 5, 0.3, 0.3, 0.3)
        ecs.add_mesh_renderer(pickup, "sphere")
    end

    -- Lighting
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 10, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.9, 1.5)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M