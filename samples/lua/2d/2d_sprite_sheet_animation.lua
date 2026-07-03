local M = {}
M._meta = { name = "2D Sprite Sheet Animation", category = "2d" }

local state = { elapsed = 0, frame_idx = 0, sheet = 0, entity = 0 }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 5.0)

    local tex = (dse.assets and dse.assets.load_texture) and dse.assets.load_texture("data/textures/white.png") or 0

    -- Create animated sprite using sprite sheet
    local sheet = dse.ecs.load_sprite_sheet("data/textures/sprite_sheet.png")
    state.sheet = sheet
    if sheet == 0 then
        -- Fallback: use basic sprite with color cycling to demonstrate animation
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, 0, 0, 0, 2.0, 2.0, 1)
        dse.ecs.add_sprite(e, 1.0, 1.0, 1.0, 1.0, 0, tex)
        dse.ecs.add_animator(e)
        dse.ecs.add_animation_state(e, "pulse", 0.5, true, {tex, tex, tex, tex})
        state.entity = e
    else
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, 0, 0, 0, 2.0, 2.0, 1)
        dse.ecs.add_sprite(e, 1.0, 1.0, 1.0, 1.0, 0, tex)
        state.entity = e
    end

    -- Background reference
    local bg = dse.ecs.create_entity()
    dse.ecs.add_transform(bg, 0, 0, 0, 8.0, 6.0, 1)
    dse.ecs.add_sprite(bg, 0.1, 0.1, 0.15, 1.0, -10, tex)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M