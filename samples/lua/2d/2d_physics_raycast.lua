local M = {}
M._meta = { name = "2D Physics Raycast", category = "2d" }

local state = { elapsed = 0, ray_points = {} }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 8.0)

    local tex = (dse.assets and dse.assets.load_texture) and dse.assets.load_texture("data/textures/white.png") or 0

    -- Walls and obstacles for raycast testing
    local wall1 = dse.ecs.create_entity()
    dse.ecs.add_transform(wall1, -3.0, 0, 0, 0.5, 4.0, 1)
    dse.ecs.add_sprite(wall1, 0.5, 0.5, 0.6, 1.0, 0, tex)
    dse.ecs.add_rigid_body(wall1, 0, 1.0, 0)
    dse.ecs.add_box_collider(wall1, 0.5, 4.0, 1.0, 0.3, 0.1)

    local wall2 = dse.ecs.create_entity()
    dse.ecs.add_transform(wall2, 2.0, 2.0, 0, 3.0, 0.5, 1)
    dse.ecs.add_sprite(wall2, 0.5, 0.5, 0.6, 1.0, 0, tex)
    dse.ecs.add_rigid_body(wall2, 0, 1.0, 0)
    dse.ecs.add_box_collider(wall2, 3.0, 0.5, 1.0, 0.3, 0.1)

    -- Dynamic boxes
    for i = 1, 4 do
        local box = dse.ecs.create_entity()
        dse.ecs.add_transform(box, -1.0 + i * 1.5, -2.0 + i * 0.8, 0, 0.6, 0.6, 1)
        dse.ecs.add_sprite(box, 0.7, 0.5, 0.2, 1.0, 1, tex)
        dse.ecs.add_rigid_body(box, 2, 1.0, 0)
        dse.ecs.add_box_collider(box, 0.6, 0.6, 1.0, 0.3, 0.5)
    end

    -- Ground
    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0, -5.0, 0, 14.0, 1.0, 1)
    dse.ecs.add_sprite(ground, 0.3, 0.35, 0.3, 1.0, 0, tex)
    dse.ecs.add_rigid_body(ground, 0, 1.0, 0)
    dse.ecs.add_box_collider(ground, 14.0, 1.0, 1.0, 0.4, 0.1)

    -- Ray origin marker
    local ray_origin = dse.ecs.create_entity()
    dse.ecs.add_transform(ray_origin, -6.0, 0, 0, 0.3, 0.3, 1)
    dse.ecs.add_sprite(ray_origin, 1.0, 1.0, 0.0, 1.0, 5, tex)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Cast ray from left sweeping up/down
    local angle = math.sin(state.elapsed) * 0.5
    local hit = dse.ecs.raycast_2d(-6.0, 0, math.cos(angle), math.sin(angle), 12.0)
end

return M