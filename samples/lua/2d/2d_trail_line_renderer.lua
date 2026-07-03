local M = {}
M._meta = { name = "2D Trail and Line Renderer", category = "2d" }

local state = { elapsed = 0 }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 7.0)

    local tex = (dse.assets and dse.assets.load_texture) and dse.assets.load_texture("data/textures/white.png") or 0

    -- Trail renderer: moving object that leaves a trail
    local mover = dse.ecs.create_entity()
    dse.ecs.add_transform(mover, 0, 0, 0, 0.4, 0.4, 1)
    dse.ecs.add_sprite(mover, 1.0, 0.8, 0.2, 1.0, 5, tex)
    dse.ecs.add_trail_renderer(mover)
    dse.ecs.set_trail_emitting(mover, true)
    dse.ecs.set_trail_colors(mover, 1.0, 0.6, 0.1, 1.0, 0.2, 0.0, 0.0)
    state.mover = mover

    -- Line renderer: draw a polygon shape
    local line_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(line_entity, 3.0, 0, 0, 1, 1, 1)
    dse.ecs.add_line_renderer(line_entity)
    -- Pentagon shape
    local points = {}
    for i = 0, 4 do
        local angle = (i / 5) * math.pi * 2 - math.pi / 2
        table.insert(points, math.cos(angle) * 2.0)
        table.insert(points, math.sin(angle) * 2.0)
    end
    dse.ecs.line_renderer_set_points(line_entity, points)
    dse.ecs.line_renderer_set_width(line_entity, 0.08)
    dse.ecs.line_renderer_set_color(line_entity, 0.3, 0.8, 1.0, 1.0)
    dse.ecs.line_renderer_set_closed(line_entity, true)

    -- Another line renderer: sine wave
    local wave = dse.ecs.create_entity()
    dse.ecs.add_transform(wave, -3.0, -3.0, 0, 1, 1, 1)
    dse.ecs.add_line_renderer(wave)
    local wave_pts = {}
    for i = 0, 30 do
        local x = (i / 30) * 8.0 - 4.0
        local y = math.sin(x * 1.5) * 1.0
        table.insert(wave_pts, x)
        table.insert(wave_pts, y)
    end
    dse.ecs.line_renderer_set_points(wave, wave_pts)
    dse.ecs.line_renderer_set_width(wave, 0.05)
    dse.ecs.line_renderer_set_color(wave, 0.8, 0.4, 1.0, 0.8)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Move the trail emitter in a figure-8
    local x = math.sin(state.elapsed * 1.2) * 3.0
    local y = math.sin(state.elapsed * 2.4) * 1.5
    dse.ecs.add_transform(state.mover, x - 3.0, y + 1.5, 0, 0.4, 0.4, 1)
end

return M