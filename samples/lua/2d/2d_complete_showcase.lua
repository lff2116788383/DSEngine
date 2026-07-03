local M = {}
M._meta = { name = "2D Complete Showcase", category = "2d" }

local state = { elapsed = 0, player_x = 0, player_y = 0 }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 8.0)
    pcall(dse.ecs.add_camera_controller_2d, cam)
    pcall(dse.ecs.camera_set_bounds, cam, -12, 12, -8, 8)
    state.cam = cam

    local tex = (dse.assets and dse.assets.load_texture) and dse.assets.load_texture("data/textures/white.png") or 0

    -- Set 2D ambient lighting
    pcall(dse.ecs.set_ambient_2d, 0.15, 0.15, 0.2)

    -- Parallax background
    local parallax = dse.ecs.create_entity()
    dse.ecs.add_transform(parallax, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_parallax(parallax)
    dse.ecs.parallax_add_layer(parallax, tex, 0.05, 0.05, 0.1, 0.3, 1.0)
    dse.ecs.parallax_set_layer_auto_scroll(parallax, 0, 0.2, 0.0)

    -- Tilemap ground
    local tilemap = dse.ecs.create_entity()
    dse.ecs.add_transform(tilemap, -10.0, -5.0, 0, 1, 1, 1)
    dse.ecs.add_tilemap(tilemap, 20, 3, 1.0, tex)
    for x = 0, 19 do
        for y = 0, 2 do
            dse.ecs.set_tile(tilemap, x, y, 1)
        end
    end

    -- Player with physics
    local player = dse.ecs.create_entity()
    dse.ecs.add_transform(player, 0, 0, 0, 0.7, 1.0, 1)
    dse.ecs.add_sprite(player, 0.3, 0.9, 0.4, 1.0, 5, tex)
    pcall(dse.ecs.add_rigid_body, player, 2, 1.0, 0)
    pcall(dse.ecs.add_box_collider, player, 0.7, 1.0, 1.0, 0.2, 0.5)
    dse.ecs.add_trail_renderer(player)
    dse.ecs.set_trail_emitting(player, true)
    dse.ecs.set_trail_colors(player, 0.3, 0.9, 0.4, 0.8, 0.1, 0.3, 0.1)
    state.player = player

    -- Point light following player
    local light = dse.ecs.create_entity()
    dse.ecs.add_transform(light, 0, 1.0, 0, 1, 1, 1)
    pcall(dse.ecs.add_light_2d, light)
    pcall(dse.ecs.set_light_2d_color, light, 1.0, 0.9, 0.6)
    pcall(dse.ecs.set_light_2d_intensity, light, 2.0)
    pcall(dse.ecs.set_light_2d_range, light, 7.0)
    pcall(dse.ecs.set_light_2d_shadow, light, true)
    state.light = light

    -- Obstacles with physics
    for i = 1, 5 do
        local obs = dse.ecs.create_entity()
        dse.ecs.add_transform(obs, -6.0 + i * 2.5, -1.5, 0, 0.6, 0.6, 1)
        dse.ecs.add_sprite(obs, 0.6, 0.3, 0.2, 1.0, 2, tex)
        pcall(dse.ecs.add_rigid_body, obs, 2, 0.8, 0)
        pcall(dse.ecs.add_box_collider, obs, 0.6, 0.6, 1.0, 0.3, 0.6)
    end

    -- Platforms
    local plat1 = dse.ecs.create_entity()
    dse.ecs.add_transform(plat1, -4.0, 0.5, 0, 3.0, 0.4, 1)
    dse.ecs.add_sprite(plat1, 0.4, 0.4, 0.5, 1.0, 0, tex)
    pcall(dse.ecs.add_rigid_body, plat1, 0, 1.0, 0)
    pcall(dse.ecs.add_box_collider, plat1, 3.0, 0.4, 1.0, 0.3, 0.1)

    local plat2 = dse.ecs.create_entity()
    dse.ecs.add_transform(plat2, 4.0, 2.0, 0, 2.5, 0.4, 1)
    dse.ecs.add_sprite(plat2, 0.4, 0.4, 0.5, 1.0, 0, tex)
    pcall(dse.ecs.add_rigid_body, plat2, 0, 1.0, 0)
    pcall(dse.ecs.add_box_collider, plat2, 2.5, 0.4, 1.0, 0.3, 0.1)

    -- Ground collider
    local ground_col = dse.ecs.create_entity()
    dse.ecs.add_transform(ground_col, 0, -4.0, 0, 20.0, 1.0, 1)
    pcall(dse.ecs.add_rigid_body, ground_col, 0, 1.0, 0)
    pcall(dse.ecs.add_box_collider, ground_col, 20.0, 1.0, 1.0, 0.4, 0.1)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Auto-move player
    state.player_x = math.sin(state.elapsed * 0.6) * 5.0
    state.player_y = -2.0 + math.abs(math.sin(state.elapsed * 2.0)) * 2.0
end

return M