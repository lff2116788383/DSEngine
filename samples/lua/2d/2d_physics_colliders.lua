local M = {}
M._meta = { name = "2D Physics Colliders", category = "2d" }

local state = { entities = {} }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 9.0)

    local tex = dse.assets.load_texture("data/textures/white.png")

    -- Ground (static)
    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0, -5.0, 0, 14.0, 1.0, 1)
    dse.ecs.add_sprite(ground, 0.3, 0.3, 0.35, 1.0, 0, tex)
    dse.ecs.add_rigid_body(ground, 0, 1.0, 0) -- static
    dse.ecs.add_box_collider(ground, 14.0, 1.0, 1.0, 0.4, 0.2)

    -- Box collider objects (dynamic)
    for i = 1, 3 do
        local box = dse.ecs.create_entity()
        dse.ecs.add_transform(box, -3.0 + i * 2.0, 3.0 + i * 0.5, 0, 0.8, 0.8, 1)
        dse.ecs.add_sprite(box, 0.8, 0.4, 0.2, 1.0, 1, tex)
        dse.ecs.add_rigid_body(box, 2, 1.0, 0) -- dynamic
        dse.ecs.add_box_collider(box, 0.8, 0.8, 1.0, 0.3, 0.5)
        table.insert(state.entities, box)
    end

    -- Circle collider objects
    for i = 1, 3 do
        local circle = dse.ecs.create_entity()
        dse.ecs.add_transform(circle, -4.0 + i * 2.5, 5.0 + i * 0.3, 0, 0.7, 0.7, 1)
        dse.ecs.add_sprite(circle, 0.2, 0.6, 0.8, 1.0, 1, tex)
        dse.ecs.add_rigid_body(circle, 2, 1.0, 0)
        dse.ecs.add_circle_collider(circle, 0.35, 0.4, 0.6)
        table.insert(state.entities, circle)
    end

    -- Polygon collider (triangle-shaped platform)
    local poly = dse.ecs.create_entity()
    dse.ecs.add_transform(poly, 0, -2.0, 0, 3.0, 2.0, 1)
    dse.ecs.add_sprite(poly, 0.5, 0.7, 0.3, 1.0, 0, tex)
    dse.ecs.add_rigid_body(poly, 0, 1.0, 0) -- static
    dse.ecs.add_polygon_collider(poly, {-1.5, -1.0, 1.5, -1.0, 0.0, 1.0}, 0.3, 0.2)
end

function M.Update(dt)
end

return M