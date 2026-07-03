local M = {}
M._meta = { name = "2D Spine Animation", category = "2d" }

local state = { elapsed = 0 }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 6.0)

    local tex = dse.assets.load_texture("data/textures/white.png")

    -- Spine character (if spine data available)
    local spine_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(spine_entity, 0, -1.0, 0, 1.5, 1.5, 1)
    dse.ecs.add_sprite(spine_entity, 1.0, 1.0, 1.0, 1.0, 0, tex)

    -- Try to add spine renderer
    local ok = pcall(function()
        dse.spine.add_renderer(spine_entity, "data/spine/character.json")
        dse.spine.set_animation(spine_entity, "idle", true)
    end)

    if not ok then
        -- Fallback: basic animated sprite to show something
        dse.ecs.add_animator(spine_entity)
        dse.ecs.add_animation_state(spine_entity, "idle", 0.8, true, {tex, tex, tex, tex})
    end

    state.spine_entity = spine_entity

    -- Background
    local bg = dse.ecs.create_entity()
    dse.ecs.add_transform(bg, 0, 0, 0, 10.0, 8.0, 1)
    dse.ecs.add_sprite(bg, 0.08, 0.1, 0.15, 1.0, -10, tex)

    -- Ground
    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0, -3.5, 0, 10.0, 0.5, 1)
    dse.ecs.add_sprite(ground, 0.25, 0.3, 0.2, 1.0, -1, tex)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M