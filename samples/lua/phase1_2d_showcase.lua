local Runtime2DShowcase = {}

local initialized = false
local state = {
    score = 128,
    combo = 3,
    burst_timer = 0.0,
    tip_timer = 0.0,
    pulse = 0.0,
    hero_x = -3.5,
    hero_dir = 1.0,
    ui = {},
    entities = {},
    audio = {}
}

local function load_tex(path)
    local handle = dse.assets.load_texture(path)
    if handle == 0 then
        handle = dse.assets.load_texture("data/" .. path)
    end
    return handle
end

local function file_exists(path)
    local file = io.open(path, "rb")
    if file ~= nil then
        file:close()
        return true
    end
    return false
end

local function resolve_audio_path(path)
    if file_exists(path) then
        return path
    end
    local data_path = "data/" .. path
    if file_exists(data_path) then
        return data_path
    end
    return nil
end

local function clamp(v, min_v, max_v)
    if v < min_v then
        return min_v
    end
    if v > max_v then
        return max_v
    end
    return v
end

local function add_sprite_entity(tex, x, y, sx, sy, order, color)
    local entity = dse.ecs.create_entity()
    dse.ecs.add_transform(entity, x, y, 0.0, sx, sy, 1.0)
    dse.ecs.add_sprite(entity, color[1], color[2], color[3], color[4], order, tex)
    return entity
end

local function setup_camera(config)
    local camera_size = 7.5
    if type(config) == "table" and type(config.camera_ortho_size) == "number" then
        camera_size = config.camera_ortho_size
    end

    local camera = dse.ecs.create_entity()
    dse.ecs.add_transform(camera, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(camera, camera_size)
    state.entities.camera = camera
end

local function setup_tilemap(ground_tex)
    local tilemap = dse.ecs.create_entity()
    dse.ecs.add_transform(tilemap, -6.0, -3.8, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_tilemap(tilemap, 12, 3, 1.0, ground_tex)
    for y = 0, 2 do
        for x = 0, 11 do
            local tile = ((x + y) % 2 == 0) and 0 or 1
            dse.ecs.set_tile(tilemap, x, y, tile)
        end
    end
    state.entities.tilemap = tilemap
end

local function setup_physics_lane(box_tex)
    local floor = dse.ecs.create_entity()
    dse.ecs.add_transform(floor, 0.0, -3.2, 0.0, 8.0, 0.8, 1.0)
    dse.ecs.add_sprite(floor, 0.28, 0.34, 0.48, 1.0, -1, box_tex)
    dse.ecs.add_rigid_body(floor, 0, 1.0, 0)
    dse.ecs.add_box_collider(floor, 8.0, 0.8, 1.0, 0.5, 0.1)

    local hero = dse.ecs.create_entity()
    dse.ecs.add_transform(hero, -3.5, -1.5, 0.0, 0.9, 0.9, 1.0)
    dse.ecs.add_sprite(hero, 1.0, 1.0, 1.0, 1.0, 2, box_tex)
    dse.ecs.add_rigid_body(hero, 2, 1.0, 0)
    dse.ecs.add_box_collider(hero, 0.9, 0.9, 1.0, 0.3, 0.5)

    local crate = dse.ecs.create_entity()
    dse.ecs.add_transform(crate, 2.6, -1.0, 0.0, 0.8, 0.8, 1.0)
    dse.ecs.add_sprite(crate, 0.82, 0.92, 1.0, 1.0, 2, box_tex)
    dse.ecs.add_rigid_body(crate, 2, 1.0, 0)
    dse.ecs.add_box_collider(crate, 0.8, 0.8, 1.0, 0.3, 0.5)

    state.entities.floor = floor
    state.entities.hero = hero
    state.entities.crate = crate
end

local function setup_particles(box_tex)
    local burst = dse.ecs.create_entity()
    dse.ecs.add_transform(burst, 0.0, -0.4, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_particle_emitter(burst, box_tex, 96, 18.0)
    dse.ecs.set_particle_density(burst, 18.0)
    state.entities.burst = burst
end

local function setup_animation(box_tex, ground_tex)
    local anim = dse.ecs.create_entity()
    dse.ecs.add_transform(anim, 4.2, 1.6, 0.0, 1.2, 1.2, 1.0)
    dse.ecs.add_sprite(anim, 1.0, 0.95, 0.95, 1.0, 3, box_tex)
    dse.ecs.add_animator(anim)
    dse.ecs.add_animation_state(anim, "pulse", 1.4, true, {box_tex, ground_tex, box_tex, ground_tex})
    state.entities.anim = anim
end

local function setup_backdrop(ui_tex)
    state.entities.backdrop_a = add_sprite_entity(ui_tex, -4.6, 2.3, 1.4, 1.4, 0, {0.35, 0.45, 0.70, 0.80})
    state.entities.backdrop_b = add_sprite_entity(ui_tex, -1.8, 2.5, 1.1, 1.1, 0, {0.55, 0.42, 0.78, 0.72})
    state.entities.backdrop_c = add_sprite_entity(ui_tex, 1.2, 2.2, 1.3, 1.3, 0, {0.30, 0.70, 0.68, 0.70})
end

local function setup_ui(font_tex, ui_tex)
    local root = dse.ecs.create_entity()
    dse.ecs.add_transform(root, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)

    dse.ui.add_renderer(root, ui_tex, 0.08, 0.10, 0.16, 0.78, 900)
    dse.ui.add_panel(root, true)

    local title = dse.ecs.create_entity()
    dse.ecs.add_transform(title, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(title, "2D Showcase", font_tex, 1.0, 0.95, 0.70, 1.0, 20.0, 28.0, 2.0, 16, 6, 32, -280.0, -42.0)

    local subtitle = dse.ecs.create_entity()
    dse.ecs.add_transform(subtitle, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(subtitle, "tilemap / physics / ui / particle / audio / animator", font_tex, 0.80, 0.88, 1.0, 1.0, 14.0, 20.0, 1.0, 16, 6, 32, -280.0, -6.0)

    local score_text = dse.ecs.create_entity()
    dse.ecs.add_transform(score_text, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(score_text, "Score", font_tex, 1.0, 1.0, 1.0, 1.0, 16.0, 22.0, 1.0, 16, 6, 32, 150.0, -42.0)

    local score_value = dse.ecs.create_entity()
    dse.ecs.add_transform(score_value, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(score_value, "0", font_tex, 1.0, 0.84, 0.35, 1.0, 18.0, 24.0, 1.0, 16, 6, 32, 150.0, -8.0)
    dse.ui.set_label_number(score_value, state.score)

    local combo_text = dse.ecs.create_entity()
    dse.ecs.add_transform(combo_text, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(combo_text, "Combo", font_tex, 1.0, 1.0, 1.0, 1.0, 16.0, 22.0, 1.0, 16, 6, 32, 280.0, -42.0)

    local combo_value = dse.ecs.create_entity()
    dse.ecs.add_transform(combo_value, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(combo_value, "0", font_tex, 0.55, 1.0, 0.72, 1.0, 18.0, 24.0, 1.0, 16, 6, 32, 280.0, -8.0)
    dse.ui.set_label_number(combo_value, state.combo)

    local tip = dse.ecs.create_entity()
    dse.ecs.add_transform(tip, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(tip, "Auto pulse / burst / music loop", font_tex, 0.72, 0.96, 0.82, 1.0, 14.0, 20.0, 1.0, 16, 6, 32, -280.0, 34.0)

    local button = dse.ecs.create_entity()
    dse.ecs.add_transform(button, 4.6, -3.2, 0.0, 1.8, 0.7, 1.0)
    dse.ecs.add_sprite(button, 0.20, 0.38, 0.70, 0.95, 10, ui_tex)
    dse.ui.add_button(button, 0.20, 0.38, 0.70, 0.95)
    dse.ui.set_button_scale(button, 1.06, 0.95, 10.0)

    local button_label = dse.ecs.create_entity()
    dse.ecs.add_transform(button_label, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(button_label, "Showcase Loop", font_tex, 1.0, 1.0, 1.0, 1.0, 14.0, 20.0, 1.0, 16, 6, 32, 205.0, 28.0)

    state.ui.root = root
    state.ui.score_value = score_value
    state.ui.combo_value = combo_value
    state.ui.tip = tip
    state.ui.button = button
end

local function setup_audio()
    local bgm_path = resolve_audio_path("audio/phase1_bgm.wav")
    if not bgm_path then
        return
    end

    local audio = dse.ecs.create_entity()
    dse.audio.add_source(audio, bgm_path, true, true, 0.45)
    dse.audio.set_playing(audio, true)
    state.audio.bgm = audio
end

function Runtime2DShowcase.Setup(config)
    if initialized then
        return
    end

    local box_tex = load_tex("mirror_assets/Resources/item/1.png")
    local ground_tex = load_tex("mirror_assets/Resources/map/0.png")
    local ui_tex = load_tex("mirror_assets/Resources/ui/1.png")
    local font_tex = load_tex("font/bitmap_font.png")

    setup_camera(config)
    setup_backdrop(ui_tex ~= 0 and ui_tex or box_tex)
    setup_tilemap(ground_tex ~= 0 and ground_tex or box_tex)
    setup_physics_lane(box_tex ~= 0 and box_tex or ground_tex)
    setup_particles(box_tex ~= 0 and box_tex or ui_tex)
    setup_animation(box_tex ~= 0 and box_tex or ui_tex, ground_tex ~= 0 and ground_tex or box_tex)
    setup_ui(font_tex, ui_tex ~= 0 and ui_tex or box_tex)
    setup_audio()

    initialized = true
end

function Runtime2DShowcase.Update(delta_time)
    if not initialized then
        return
    end

    local dt = delta_time or 0.0
    state.burst_timer = state.burst_timer + dt
    state.tip_timer = state.tip_timer + dt
    state.pulse = state.pulse + dt

    if state.burst_timer >= 1.25 then
        state.burst_timer = state.burst_timer - 1.25
        state.score = state.score + 12
        state.combo = (state.combo % 9) + 1
        dse.ui.set_label_number(state.ui.score_value, state.score)
        dse.ui.set_label_number(state.ui.combo_value, state.combo)
        dse.ecs.particle_burst(state.entities.burst, 18)
        if state.audio.bgm ~= nil then
            dse.audio.restart(state.audio.bgm)
            dse.audio.set_pitch(state.audio.bgm, 0.92 + state.combo * 0.03)
            dse.audio.set_playing(state.audio.bgm, true)
        end
    end

    state.hero_x = state.hero_x + state.hero_dir * dt * 1.5
    if state.hero_x > -2.0 then
        state.hero_x = -2.0
        state.hero_dir = -1.0
    elseif state.hero_x < -4.9 then
        state.hero_x = -4.9
        state.hero_dir = 1.0
    end

    local hero_y = -1.5 + math.sin(state.pulse * 2.2) * 0.08
    local crate_y = -1.0 + math.cos(state.pulse * 1.7) * 0.05
    dse.ecs.add_transform(state.entities.hero, state.hero_x, hero_y, 0.0, 0.9, 0.9, 1.0)
    dse.ecs.add_transform(state.entities.crate, 2.6 - (state.hero_x + 3.5) * 0.5, crate_y, 0.0, 0.8, 0.8, 1.0)

    local emitter_y = -0.4 + math.sin(state.pulse * 1.4) * 0.25
    dse.ecs.add_transform(state.entities.burst, 0.0, emitter_y, 0.0, 1.0, 1.0, 1.0)

    if state.tip_timer >= 2.0 then
        state.tip_timer = state.tip_timer - 2.0
        if state.combo % 2 == 0 then
            dse.ui.set_label_text(state.ui.tip, "Particle burst + numeric label refresh")
        else
            dse.ui.set_label_text(state.ui.tip, "Animator + tilemap + audio loop are active")
        end
    end

    local pitch = clamp(0.90 + math.sin(state.pulse) * 0.08, 0.82, 1.08)
    if state.audio.bgm ~= nil then
        dse.audio.set_pitch(state.audio.bgm, pitch)
    end
end

return Runtime2DShowcase
