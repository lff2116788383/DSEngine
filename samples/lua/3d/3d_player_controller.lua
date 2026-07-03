local M = {}
M._meta = { name = "3D Player Controller", category = "3c" }

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
    ecs.add_transform(cam, 0, 8, -10, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)

    -- Player with full controller
    local player = ecs.create_entity()
    ecs.add_transform(player, 0, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(player, 0.5, 0.7, 0.5, 1.0, cube_vertices(), cube_indices())
    pcall(ecs.add_rigidbody_3d,player, 1)
    pcall(ecs.add_capsule_collider_3d,player, 0.5, 1.0)
    pcall(ecs.add_player_controller,player)
    state.player = player

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 30, 0.1, 30)
    ecs.add_mesh_renderer(ground, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
    pcall(ecs.add_rigidbody_3d,ground, 0)
    pcall(ecs.add_box_collider_3d,ground, 30, 0.1, 30)

    -- Ramp
    local ramp = ecs.create_entity()
    ecs.add_transform(ramp, 5, 0.5, 0, 4, 0.2, 3)
    ecs.add_mesh_renderer(ramp, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
    pcall(ecs.add_rigidbody_3d,ramp, 0)
    pcall(ecs.add_box_collider_3d,ramp, 4, 0.2, 3)

    -- Stairs (stepped boxes)
    for i = 1, 5 do
        local step = ecs.create_entity()
        ecs.add_transform(step, -5, i * 0.3, i * 0.8, 2, 0.3, 0.8)
        ecs.add_mesh_renderer(step, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
        pcall(ecs.add_rigidbody_3d,step, 0)
        pcall(ecs.add_box_collider_3d,step, 2, 0.3, 0.8)
    end

    -- Collectibles
    for i = 1, 6 do
        local pickup = ecs.create_entity()
        local angle = (i / 6) * math.pi * 2
        ecs.add_transform(pickup, math.cos(angle) * 5, 1.5, math.sin(angle) * 5, 0.3, 0.3, 0.3)
        ecs.add_mesh_renderer(pickup, 0.7, 0.7, 0.7, 1.0, cube_vertices(), cube_indices())
    end

    -- Lighting
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 10, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.9, 1.5, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M