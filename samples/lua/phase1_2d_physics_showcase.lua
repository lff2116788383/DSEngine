local Runtime2DPhysicsShowcase = {}

local initialized = false
local state = {
    entities = {},
    elapsed = 0.0,
    ray_timer = 0.0,
    info_timer = 0.0,
    logs = {},
    ui = {},
    last_raycast_hit = 0,
    last_highlight_hit = 0,
    drop_count = 0,
    ray_points = {}
}

local function load_tex(path)
    local handle = dse.assets.load_texture(path)
    if handle == 0 then
        handle = dse.assets.load_texture("data/" .. path)
    end
    return handle
end

local function push_log(text)
    table.insert(state.logs, 1, text)
    while #state.logs > 4 do
        table.remove(state.logs)
    end
    if state.ui.log1 ~= nil then dse.ui.set_label_text(state.ui.log1, state.logs[1] or "") end
    if state.ui.log2 ~= nil then dse.ui.set_label_text(state.ui.log2, state.logs[2] or "") end
    if state.ui.log3 ~= nil then dse.ui.set_label_text(state.ui.log3, state.logs[3] or "") end
    if state.ui.log4 ~= nil then dse.ui.set_label_text(state.ui.log4, state.logs[4] or "") end
end

local function set_box_tint(entity, r, g, b, a)
    dse.ecs.add_sprite(entity, r, g, b, a, 1, state.entities.box_tex)
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local size = 9.0
    if type(config) == "table" and type(config.camera_ortho_size) == "number" then
        size = config.camera_ortho_size
    end
    dse.ecs.add_transform(camera, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(camera, size)
    state.entities.camera = camera
end

local function add_box(entity, tex, x, y, sx, sy, color, order)
    dse.ecs.add_transform(entity, x, y, 0.0, sx, sy, 1.0)
    dse.ecs.add_sprite(entity, color[1], color[2], color[3], color[4], order or 1, tex)
end

local function setup_raycast_visual(tex)
    state.entities.ray_markers = {}
    for i = 1, 13 do
        local marker = dse.ecs.create_entity()
        add_box(marker, tex, -100.0, -100.0, 0.14, 0.14, {0.92, 0.36, 0.18, 0.90}, 3)
        state.entities.ray_markers[i] = marker
    end

    local hit_marker = dse.ecs.create_entity()
    add_box(hit_marker, tex, -100.0, -100.0, 0.30, 0.30, {1.0, 0.20, 0.20, 1.0}, 4)
    state.entities.hit_marker = hit_marker
end

local function setup_world(box_tex)
    state.entities.box_tex = box_tex

    local ground = dse.ecs.create_entity()
    add_box(ground, box_tex, 0.0, -4.8, 10.0, 1.0, {0.28, 0.35, 0.50, 1.0})
    dse.ecs.add_rigid_body(ground, 0, 1.0, 1)
    dse.ecs.add_box_collider(ground, 10.0, 1.0, 1.0, 0.6, 0.0)

    local trigger = dse.ecs.create_entity()
    add_box(trigger, box_tex, 0.0, -1.6, 4.0, 0.45, {0.20, 0.85, 0.45, 0.40})
    dse.ecs.add_rigid_body(trigger, 0, 1.0, 1)
    dse.ecs.add_box_collider(trigger, 4.0, 0.45, 1.0, 0.0, 0.0)
    dse.ecs.set_box_collider_trigger(trigger, true)

    local dynamic_box = dse.ecs.create_entity()
    add_box(dynamic_box, box_tex, 0.0, 4.5, 0.9, 0.9, {0.95, 0.90, 0.82, 1.0})
    dse.ecs.add_rigid_body(dynamic_box, 2, 1.0, 0)
    dse.ecs.add_box_collider(dynamic_box, 0.9, 0.9, 1.0, 0.35, 0.15)

    state.entities.ground = ground
    state.entities.trigger = trigger
    state.entities.dynamic_box = dynamic_box

    setup_raycast_visual(box_tex)
end

local function make_label(font_tex, text, x, y, r, g, b)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(e, text, font_tex, r, g, b, 1.0, 8.0, 12.0, 1.0, 16, 6, 32, x, y)
    return e
end

local function setup_ui(font_tex, ui_tex)
    local root = dse.ecs.create_entity()
    dse.ecs.add_transform(root, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_renderer(root, 0, 0.06, 0.08, 0.12, 0.80, 950, 720.0, 170.0)
    dse.ui.add_panel(root, true)

    state.ui.title = make_label(font_tex, "2D Physics Showcase", -340.0, -64.0, 1.0, 0.95, 0.72)
    state.ui.desc = make_label(font_tex, "drop / trigger / collision / raycast / highlight", -340.0, -44.0, 0.78, 0.90, 1.0)
    state.ui.stats = make_label(font_tex, "waiting...", 130.0, -64.0, 1.0, 1.0, 1.0)
    state.ui.raycast = make_label(font_tex, "raycast: none", 130.0, -44.0, 0.98, 0.80, 0.42)
    state.ui.log1 = make_label(font_tex, "", -340.0, -16.0, 0.90, 1.0, 0.90)
    state.ui.log2 = make_label(font_tex, "", -340.0, 2.0, 0.90, 1.0, 0.90)
    state.ui.log3 = make_label(font_tex, "", -340.0, 20.0, 0.90, 1.0, 0.90)
    state.ui.log4 = make_label(font_tex, "", -340.0, 38.0, 0.90, 1.0, 0.90)
    state.ui.root = root
end

local function clear_previous_highlight()
    if state.last_highlight_hit == state.entities.dynamic_box then
        set_box_tint(state.entities.dynamic_box, 0.95, 0.90, 0.82, 1.0)
    elseif state.last_highlight_hit == state.entities.trigger then
        set_box_tint(state.entities.trigger, 0.20, 0.85, 0.45, 0.40)
    elseif state.last_highlight_hit == state.entities.ground then
        set_box_tint(state.entities.ground, 0.28, 0.35, 0.50, 1.0)
    end
    state.last_highlight_hit = 0
end

local function highlight_hit(entity)
    clear_previous_highlight()
    if entity == state.entities.dynamic_box or entity == state.entities.trigger or entity == state.entities.ground then
        set_box_tint(entity, 1.0, 0.96, 0.20, 1.0)
        state.last_highlight_hit = entity
    end
end

local function handle_contact_events()
    while true do
        local ok, other, is_trigger, is_enter = dse.ecs.poll_collision_event(state.entities.dynamic_box)
        if not ok then
            break
        end
        local event_type = is_trigger and "trigger" or "collision"
        local phase = is_enter and "enter" or "exit"
        push_log(string.format("[%s] %s other=%d", event_type, phase, other))
    end
end

local function update_raycast()
    local start_x = -6.0
    local start_y = -1.6
    local end_x = 6.0
    local end_y = -1.6
    local segment_count = #state.entities.ray_markers
    for i = 1, segment_count do
        local t = (i - 1) / (segment_count - 1)
        local x = start_x + (end_x - start_x) * t
        local y = start_y + (end_y - start_y) * t
        dse.ecs.set_transform_position(state.entities.ray_markers[i], x, y, 0.0)
    end

    local hit, entity, px, py, nx, ny = dse.ecs.raycast_2d(start_x, start_y, end_x, end_y)
    if hit then
        state.last_raycast_hit = entity
        highlight_hit(entity)
        dse.ecs.set_transform_position(state.entities.hit_marker, px, py, 0.0)
        dse.ui.set_label_text(state.ui.raycast, string.format("raycast: hit=%d point=(%.2f, %.2f) normal=(%.2f, %.2f)", entity, px, py, nx, ny))
    else
        state.last_raycast_hit = 0
        clear_previous_highlight()
        dse.ecs.set_transform_position(state.entities.hit_marker, -100.0, -100.0, 0.0)
        dse.ui.set_label_text(state.ui.raycast, "raycast: none")
    end
end

function Runtime2DPhysicsShowcase.Setup(config)
    if initialized then
        return
    end

    local box_tex = load_tex("models/CesiumLogoFlat.png")
    local ui_tex = load_tex("models/CesiumLogoFlat.png")
    local font_tex = load_tex("font/bitmap_font.png")

    setup_camera(config)
    setup_world(box_tex ~= 0 and box_tex or ui_tex)
    setup_ui(font_tex, ui_tex ~= 0 and ui_tex or box_tex)

    push_log("[boot] physics showcase ready")
    push_log("[hint] yellow tint means raycast hit")
    push_log("[hint] orange dots visualize the ray")
    update_raycast()
    initialized = true
end

function Runtime2DPhysicsShowcase.Update(delta_time)
    if not initialized then
        return
    end

    local dt = delta_time or 0.0
    state.elapsed = state.elapsed + dt
    state.ray_timer = state.ray_timer + dt
    state.info_timer = state.info_timer + dt

    handle_contact_events()

    if state.ray_timer >= 0.20 then
        state.ray_timer = state.ray_timer - 0.20
        update_raycast()
    end

    local bx, by = dse.ecs.get_transform_position(state.entities.dynamic_box)
    if bx ~= nil and by ~= nil then
        dse.ui.set_label_text(state.ui.stats, string.format("box=(%.2f, %.2f) hit=%d drops=%d", bx, by, state.last_raycast_hit, state.drop_count))
        if by < -8.0 then
            state.drop_count = state.drop_count + 1
            local reset_x = ((state.drop_count % 3) - 1) * 1.2
            dse.ecs.set_transform_position(state.entities.dynamic_box, reset_x, 4.8, 0.0)
            dse.ecs.set_rigid_body_velocity(state.entities.dynamic_box, 0.0, 0.0)
            push_log(string.format("[reset] drop=%d x=%.1f", state.drop_count, reset_x))
        end
    end

    if state.info_timer >= 2.5 then
        state.info_timer = state.info_timer - 2.5
        push_log("[raycast] probe + highlight refreshed")
    end
end

return Runtime2DPhysicsShowcase
