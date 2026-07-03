local M = {}
M._meta = { name = "2D Camera Controller", category = "2d" }

local state = { elapsed = 0 }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 6.0)
    pcall(dse.ecs.add_camera_controller_2d, cam)
    pcall(dse.ecs.camera_set_zoom, cam, 1.0)
    pcall(dse.ecs.camera_set_bounds, cam, -15, 15, -10, 10)
    pcall(dse.ecs.camera_set_look_ahead, cam, 2.0)
    state.cam = cam

    local tex = (dse.assets and dse.assets.load_texture) and dse.assets.load_texture("data/textures/white.png") or 0

    -- Player character (camera target)
    local player = dse.ecs.create_entity()
    dse.ecs.add_transform(player, 0, 0, 0, 0.8, 1.2, 1)
    dse.ecs.add_sprite(player, 0.2, 0.8, 0.4, 1.0, 5, tex)
    state.player = player

    -- World objects spread around
    for i = 1, 20 do
        local x = math.cos(i * 1.3) * (3.0 + i * 0.6)
        local y = math.sin(i * 0.9) * (2.0 + i * 0.4)
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, x, y, 0, 0.5 + math.random() * 0.5, 0.5 + math.random() * 0.5, 1)
        local r = 0.3 + math.random() * 0.5
        local g = 0.3 + math.random() * 0.5
        local b = 0.3 + math.random() * 0.5
        dse.ecs.add_sprite(e, r, g, b, 1.0, 0, tex)
    end

    -- Ground grid reference
    for i = -15, 15, 3 do
        local line = dse.ecs.create_entity()
        dse.ecs.add_transform(line, i, 0, 0, 0.05, 20.0, 1)
        dse.ecs.add_sprite(line, 0.15, 0.15, 0.2, 0.5, -5, tex)
    end
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Simulate player movement
    local px = math.sin(state.elapsed * 0.8) * 5.0
    local py = math.cos(state.elapsed * 0.5) * 3.0
    dse.ecs.add_transform(state.player, px, py, 0, 0.8, 1.2, 1)

    -- Trigger camera shake periodically
    if math.floor(state.elapsed) % 5 == 0 and state.elapsed - math.floor(state.elapsed) < dt * 2 then
        pcall(dse.ecs.camera_shake, state.cam, 0.3, 0.5)
    end
end

return M