local M = {}
M._meta = { name = "3D Character Movement", category = "3c" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 5, -8, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 100.0)

    -- Ground plane
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 20, 0.1, 20)
    ecs.add_mesh_renderer(ground, "cube")
    ecs.add_rigidbody3d(ground, 0) -- static

    -- Character with movement component
    local char = ecs.create_entity()
    ecs.add_transform(char, 0, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(char, "capsule")
    ecs.add_rigidbody3d(char, 1) -- dynamic
    ecs.add_capsule_collider3d(char, 0.5, 1.0)
    ecs.add_character_movement(char)

    -- Configure movement
    ecs.set_character_movement_speed(char, 5.0)
    ecs.set_character_movement_jump_force(char, 8.0)
    ecs.set_character_movement_gravity(char, -20.0)

    state.char = char

    -- Obstacles
    for i = 1, 5 do
        local obs = ecs.create_entity()
        ecs.add_transform(obs, -4 + i * 2, 0.5, 3, 1, 1, 1)
        ecs.add_mesh_renderer(obs, "cube")
        ecs.add_rigidbody3d(obs, 0)
        ecs.add_box_collider3d(obs, 1, 1, 1)
    end

    -- Directional light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 10, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.9, 1.5)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Simulate movement input
    local move_x = math.sin(state.elapsed * 0.8) * 0.7
    local move_z = math.cos(state.elapsed * 0.6) * 0.5
    ecs.character_movement_input(state.char, move_x, move_z)
end

return M