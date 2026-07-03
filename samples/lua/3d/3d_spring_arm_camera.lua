local M = {}
M._meta = { name = "3D Spring Arm Camera", category = "3c" }

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
    -- Target character
    local char = ecs.create_entity()
    ecs.add_transform(char, 0, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(char, 0.5, 0.7, 0.5, 1.0, cube_vertices(), cube_indices())
    state.char = char

    -- Spring arm camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 3, -5, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)
    pcall(ecs.add_spring_arm,cam, char)

    -- Configure spring arm
    pcall(ecs.set_spring_arm_length,cam, 6.0)
    pcall(ecs.set_spring_arm_offset,cam, 0, 2.0, 0)
    pcall(ecs.set_spring_arm_lag_speed,cam, 5.0)
    pcall(ecs.set_spring_arm_collision,cam, true)

    state.cam = cam

    -- Environment: walls for collision testing
    local wall1 = ecs.create_entity()
    ecs.add_transform(wall1, -5, 2, 0, 0.5, 4, 10)
    ecs.add_mesh_renderer(wall1, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
    pcall(ecs.add_rigidbody_3d,wall1, 0)
    pcall(ecs.add_box_collider_3d,wall1, 0.5, 4, 10)

    local wall2 = ecs.create_entity()
    ecs.add_transform(wall2, 5, 2, 0, 0.5, 4, 10)
    ecs.add_mesh_renderer(wall2, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
    pcall(ecs.add_rigidbody_3d,wall2, 0)
    pcall(ecs.add_box_collider_3d,wall2, 0.5, 4, 10)

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 20, 0.1, 20)
    ecs.add_mesh_renderer(ground, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

    -- Directional light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 10, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.85, 1.5, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Move character around
    local x = math.sin(state.elapsed * 0.5) * 3.0
    local z = math.cos(state.elapsed * 0.5) * 3.0
    ecs.add_transform(state.char, x, 1, z, 1, 2, 1)
end

return M