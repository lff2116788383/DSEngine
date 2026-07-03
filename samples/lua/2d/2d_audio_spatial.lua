local M = {}
M._meta = { name = "2D Spatial Audio", category = "2d" }

local state = { elapsed = 0 }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 8.0)

    local tex = dse.assets.load_texture("data/textures/white.png")

    -- Audio listener at center
    local listener = dse.ecs.create_entity()
    dse.ecs.add_transform(listener, 0, 0, 0, 0.6, 0.6, 1)
    dse.ecs.add_sprite(listener, 0.2, 0.8, 0.2, 1.0, 5, tex)
    dse.ecs.add_audio_listener_2d(listener)
    state.listener = listener

    -- Audio source 1 (left, moving)
    local src1 = dse.ecs.create_entity()
    dse.ecs.add_transform(src1, -4.0, 0, 0, 0.5, 0.5, 1)
    dse.ecs.add_sprite(src1, 1.0, 0.4, 0.2, 1.0, 3, tex)
    dse.ecs.add_audio_spatial_2d(src1)
    dse.ecs.set_audio_spatial_2d_range(src1, 6.0)
    dse.ecs.set_audio_spatial_2d_attenuation(src1, 1.5)
    state.src1 = src1

    -- Audio source 2 (right, stationary)
    local src2 = dse.ecs.create_entity()
    dse.ecs.add_transform(src2, 4.0, 2.0, 0, 0.5, 0.5, 1)
    dse.ecs.add_sprite(src2, 0.2, 0.4, 1.0, 1.0, 3, tex)
    dse.ecs.add_audio_spatial_2d(src2)
    dse.ecs.set_audio_spatial_2d_range(src2, 5.0)
    dse.ecs.set_audio_spatial_2d_attenuation(src2, 2.0)
    state.src2 = src2

    -- Range indicators (circles approximated by sprites)
    local range1 = dse.ecs.create_entity()
    dse.ecs.add_transform(range1, -4.0, 0, 0, 12.0, 12.0, 1)
    dse.ecs.add_sprite(range1, 1.0, 0.4, 0.2, 0.1, -2, tex)

    local range2 = dse.ecs.create_entity()
    dse.ecs.add_transform(range2, 4.0, 2.0, 0, 10.0, 10.0, 1)
    dse.ecs.add_sprite(range2, 0.2, 0.4, 1.0, 0.1, -2, tex)

    -- Background
    local bg = dse.ecs.create_entity()
    dse.ecs.add_transform(bg, 0, 0, 0, 16.0, 12.0, 1)
    dse.ecs.add_sprite(bg, 0.06, 0.06, 0.1, 1.0, -10, tex)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Move source 1 in a circle
    local x = math.cos(state.elapsed * 0.8) * 4.0
    local y = math.sin(state.elapsed * 0.8) * 3.0
    dse.ecs.add_transform(state.src1, x, y, 0, 0.5, 0.5, 1)
end

return M