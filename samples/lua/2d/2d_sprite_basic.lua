local M = {}
M._meta = { name = "2D Sprite Basic", category = "2d" }

local state = { entities = {} }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 6.0)

    local tex = dse.assets.load_texture("data/textures/white.png")

    -- Red sprite top-left
    local s1 = dse.ecs.create_entity()
    dse.ecs.add_transform(s1, -2.0, 1.5, 0, 1.0, 1.0, 1)
    dse.ecs.add_sprite(s1, 1.0, 0.2, 0.2, 1.0, 0, tex)
    state.entities.s1 = s1

    -- Green sprite center
    local s2 = dse.ecs.create_entity()
    dse.ecs.add_transform(s2, 0.0, 0.0, 0, 1.2, 1.2, 1)
    dse.ecs.add_sprite(s2, 0.2, 1.0, 0.2, 1.0, 1, tex)
    state.entities.s2 = s2

    -- Blue sprite bottom-right
    local s3 = dse.ecs.create_entity()
    dse.ecs.add_transform(s3, 2.0, -1.5, 0, 0.8, 0.8, 1)
    dse.ecs.add_sprite(s3, 0.2, 0.2, 1.0, 1.0, 2, tex)
    state.entities.s3 = s3

    -- Yellow sprite with different layer order (behind green)
    local s4 = dse.ecs.create_entity()
    dse.ecs.add_transform(s4, 0.3, 0.3, 0, 1.5, 1.5, 1)
    dse.ecs.add_sprite(s4, 1.0, 1.0, 0.0, 0.7, -1, tex)
    state.entities.s4 = s4
end

function M.Update(dt)
end

return M