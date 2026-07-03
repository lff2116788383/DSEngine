local M = {}
M._meta = { name = "2D Physics Colliders", category = "2d" }
local state = { entities = {} }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 9.0)

    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0, -5.0, 0, 14.0, 1.0, 1)
    dse.ecs.add_sprite(ground, 0.3, 0.3, 0.35, 1.0, 0)

    for i = 1, 3 do
        local box = dse.ecs.create_entity()
        dse.ecs.add_transform(box, -3.0 + i * 2.0, 3.0 + i * 0.5, 0, 0.8, 0.8, 1)
        dse.ecs.add_sprite(box, 0.8, 0.4, 0.2, 1.0, 1)
        table.insert(state.entities, box)
    end

    for i = 1, 3 do
        local circle = dse.ecs.create_entity()
        dse.ecs.add_transform(circle, -4.0 + i * 2.5, 5.0 + i * 0.3, 0, 0.7, 0.7, 1)
        dse.ecs.add_sprite(circle, 0.2, 0.6, 0.8, 1.0, 1)
        table.insert(state.entities, circle)
    end

    local poly = dse.ecs.create_entity()
    dse.ecs.add_transform(poly, 0, 7.0, 0, 1.2, 1.0, 1)
    dse.ecs.add_sprite(poly, 0.8, 0.2, 0.8, 1.0, 2)

    local ramp = dse.ecs.create_entity()
    dse.ecs.add_transform(ramp, 4.0, -2.5, 0, 5.0, 0.3, 1)
    dse.ecs.add_sprite(ramp, 0.5, 0.5, 0.3, 1.0, 0)
end

function M.Update(dt)
end

return M
