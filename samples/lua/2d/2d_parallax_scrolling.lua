local M = {}
M._meta = { name = "2D Parallax Scrolling", category = "2d" }

local state = { elapsed = 0, layers = {} }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 6.0)
    dse.ecs.add_camera_controller_2d(cam)
    dse.ecs.camera_set_bounds(cam, -20, 20, -10, 10)

    local tex = (dse.assets and dse.assets.load_texture) and dse.assets.load_texture("data/textures/white.png") or 0

    -- Create parallax entity
    local parallax = dse.ecs.create_entity()
    dse.ecs.add_transform(parallax, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_parallax(parallax)

    -- Layer 0: far background (slow scroll)
    dse.ecs.parallax_add_layer(parallax, tex, 0.1, 0.1, 0.2, 0.15, 1.0)
    dse.ecs.parallax_set_layer_auto_scroll(parallax, 0, 0.3, 0.0)

    -- Layer 1: mid background
    dse.ecs.parallax_add_layer(parallax, tex, 0.2, 0.25, 0.35, 0.8, 1.0)
    dse.ecs.parallax_set_layer_auto_scroll(parallax, 1, 0.6, 0.0)

    -- Layer 2: near foreground (fast scroll)
    dse.ecs.parallax_add_layer(parallax, tex, 0.15, 0.4, 0.2, 0.6, 1.0)
    dse.ecs.parallax_set_layer_auto_scroll(parallax, 2, 1.2, 0.0)

    state.parallax = parallax

    -- Ground reference
    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0, -4.0, 0, 16.0, 1.0, 1)
    dse.ecs.add_sprite(ground, 0.3, 0.5, 0.2, 1.0, 5, tex)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M