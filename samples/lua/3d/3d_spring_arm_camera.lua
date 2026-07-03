local M = {}
M._meta = { name = "3D Spring Arm Camera", category = "3c" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Target character
    local char = ecs.create_entity()
    ecs.add_transform(char, 0, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(char, "capsule")
    state.char = char

    -- Spring arm camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 3, -5, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 100.0)
    ecs.add_spring_arm(cam, char)

    -- Configure spring arm
    ecs.set_spring_arm_length(cam, 6.0)
    ecs.set_spring_arm_offset(cam, 0, 2.0, 0)
    ecs.set_spring_arm_lag_speed(cam, 5.0)
    ecs.set_spring_arm_collision(cam, true)

    state.cam = cam

    -- Environment: walls for collision testing
    local wall1 = ecs.create_entity()
    ecs.add_transform(wall1, -5, 2, 0, 0.5, 4, 10)
    ecs.add_mesh_renderer(wall1, "cube")
    ecs.add_rigidbody3d(wall1, 0)
    ecs.add_box_collider3d(wall1, 0.5, 4, 10)

    local wall2 = ecs.create_entity()
    ecs.add_transform(wall2, 5, 2, 0, 0.5, 4, 10)
    ecs.add_mesh_renderer(wall2, "cube")
    ecs.add_rigidbody3d(wall2, 0)
    ecs.add_box_collider3d(wall2, 0.5, 4, 10)

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 20, 0.1, 20)
    ecs.add_mesh_renderer(ground, "cube")

    -- Directional light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 10, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.85, 1.5)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Move character around
    local x = math.sin(state.elapsed * 0.5) * 3.0
    local z = math.cos(state.elapsed * 0.5) * 3.0
    ecs.add_transform(state.char, x, 1, z, 1, 2, 1)
end

return M