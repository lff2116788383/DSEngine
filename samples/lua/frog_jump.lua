local FrogJump = {}

local state = {
    initialized = false,
    page = "start",
    tex = {},
    snd = {},
    entities = {},
    leaves = {},
    leaf_count = 22,
    leaf_move_l = -4.8,
    leaf_move_r = 4.8,
    leaf_min_dist = 1.25,
    highest_leaf_y = 0.0,
    frog_x = 0.0,
    frog_y = 0.0,
    frog_vy = 0.0,
    frog_grounded = true,
    frog_charge_start = -1.0,
    frog_rebirth_at = -1.0,
    frog_camera_y = 0.0,
    pass_num = 0,
    last_statist_num = 0,
    score = 0,
    max_score = 0,
    life = 6,
    combo_keep_until = 0.0,
    combo_value = 0,
    last_pass_time = 0.0,
    screen_w = 1280,
    screen_h = 720,
    ortho_size = 7.0,
    aspect = 16.0 / 9.0,
    ui = {}
}

local function refresh_screen_metrics()
    local w = dse.app.get_screen_width()
    local h = dse.app.get_screen_height()
    if not w or w <= 0 then
        w = 800
    end
    if not h or h <= 0 then
        h = 600
    end
    state.screen_w = w
    state.screen_h = h
    state.aspect = state.screen_w / state.screen_h
end

local function rnd(a, b)
    return a + (b - a) * math.random()
end

local function load_tex(path)
    local handle = dse.assets.load_texture(path)
    if handle == 0 then
        handle = dse.assets.load_texture("data/" .. path)
    end
    return handle
end

local function make_sprite(tex, x, y, sx, sy, order, r, g, b, a)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, 0.0, sx, sy, 1.0)
    dse.ecs.add_sprite(e, r or 1.0, g or 1.0, b or 1.0, a or 1.0, order or 0, tex or 0)
    return e
end

local function make_ui(tex, x, y, order, r, g, b, a)
    local e = dse.ecs.create_entity()
    dse.ui.add_renderer(e, tex, r or 1.0, g or 1.0, b or 1.0, a or 1.0, order or 0)
    local scale_x = 256.0
    local scale_y = 96.0
    dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    return e, scale_x, scale_y, x, y
end

local function play_sfx(key, pitch)
    local e = state.snd[key]
    if not e then
        return
    end
    if pitch then
        dse.audio.set_pitch(e, pitch)
    else
        dse.audio.set_pitch(e, 1.0)
    end
    dse.audio.set_playing(e, true)
end

local function update_ui_numbers()
    dse.ui.set_label_number(state.ui.score_label, state.score)
    dse.ui.set_label_number(state.ui.max_label, state.max_score)
    dse.ui.set_label_number(state.ui.pass_label, state.pass_num)
    dse.ui.set_label_number(state.ui.life_label, math.max(state.life, 0))
    dse.ui.set_label_number(state.ui.combo_label, state.combo_value)
end

local function create_leaf(i, y_base)
    local leaf = {}
    leaf.entity = make_sprite(state.tex.leaf, rnd(state.leaf_move_l, state.leaf_move_r), y_base, 1.4, 0.65, 10 + i)
    leaf.x = rnd(state.leaf_move_l, state.leaf_move_r)
    leaf.y = y_base
    leaf.s = rnd(0.5, 1.2) * (math.random(0, 1) == 0 and -1.0 or 1.0)
    return leaf
end

local function reset_game()
    state.page = "play"
    state.pass_num = 0
    state.last_statist_num = 0
    state.score = 0
    state.combo_value = 0
    state.combo_keep_until = 0.0
    state.life = 6
    state.frog_vy = 0.0
    state.frog_grounded = true
    state.frog_charge_start = -1.0
    state.frog_rebirth_at = -1.0
    state.highest_leaf_y = 0.0
    state.leaves = {}
    local y = -1.0
    for i = 1, state.leaf_count do
        local leaf = create_leaf(i, y)
        table.insert(state.leaves, leaf)
        y = y + rnd(state.leaf_min_dist, state.leaf_min_dist * 1.8)
        if y > state.highest_leaf_y then
            state.highest_leaf_y = y
        end
    end
    local first = state.leaves[1]
    state.frog_x = first.x
    state.frog_y = first.y + 0.45
    dse.ecs.add_transform(state.entities.frog, state.frog_x, state.frog_y, 0.0, 1.2, 1.2, 1.0)
    dse.ecs.play_animation(state.entities.frog, "stand")
    update_ui_numbers()
    dse.ecs.set_particle_density(state.entities.star_up, 0.0)
end

local function setup_assets()
    local load = load_tex
    state.tex.bg = load("frog_jump/jump/bg0000.png")
    state.tex.start_bg = load("frog_jump/jump/start_bg0000.png")
    state.tex.start = load("frog_jump/jump/start0000.png")
    state.tex.dragonfly = load("frog_jump/jump/dragonfly0000.png")
    state.tex.number = load("frog_jump/jump/number0000.png")
    state.tex.grassL = load("frog_jump/jump/grassL0000.png")
    state.tex.grassR = load("frog_jump/jump/grassR0000.png")
    state.tex.leaf = load("frog_jump/jump/leaf0000.png")
    state.tex.gameover = load("frog_jump/jump/gameover0000.png")
    state.tex.wave = load("frog_jump/jump/wave0000.png")
    state.tex.water = {
        load("frog_jump/jump/water0000.png"),
        load("frog_jump/jump/water0001.png"),
        load("frog_jump/jump/water0002.png"),
        load("frog_jump/jump/water0003.png"),
        load("frog_jump/jump/water0004.png")
    }
    state.tex.fg = {
        load("frog_jump/jump/fg0_0000.png"),
        load("frog_jump/jump/fg1_0000.png"),
        load("frog_jump/jump/fg2_0000.png"),
        load("frog_jump/jump/fg3_0000.png"),
        load("frog_jump/jump/fg4_0000.png"),
        load("frog_jump/jump/fg5_0000.png")
    }
    state.tex.act = {
        load("frog_jump/jump/act0000.png"),
        load("frog_jump/jump/act0001.png"),
        load("frog_jump/jump/act0002.png"),
        load("frog_jump/jump/act0003.png"),
        load("frog_jump/jump/act0004.png"),
        load("frog_jump/jump/act0005.png"),
        load("frog_jump/jump/act0006.png")
    }
    state.tex.insect = {
        load("frog_jump/image/chong0000.png"),
        load("frog_jump/image/chong0001.png"),
        load("frog_jump/image/chong0002.png"),
        load("frog_jump/image/chong0003.png"),
        load("frog_jump/image/chong0004.png"),
        load("frog_jump/image/chong0005.png"),
        load("frog_jump/image/chong0006.png")
    }
    state.tex.combo = {
        load("frog_jump/image/combo0000.png"),
        load("frog_jump/image/combo0001.png")
    }
    state.tex.star = load("frog_jump/image/star4_0000.png")
end

local function setup_audio()
    local function add(path, loop, volume)
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.audio.add_source(e, path, false, loop or false, volume or 1.0)
        return e
    end
    state.snd.down = add("frog_jump/jump/wa.wav", false, 1.0)
    state.snd.jump = add("frog_jump/jump/jump.wav", false, 1.0)
    state.snd.water = add("frog_jump/jump/water.wav", false, 1.0)
    state.snd.over = add("frog_jump/jump/over.wav", false, 1.0)
    state.snd.combo = add("frog_jump/jump/combo.wav", false, 1.0)
    state.snd.insect = add("frog_jump/sound/hhhh.wav", false, 0.7)
    state.snd.bgm = add("frog_jump/jump/bgm.mp3", true, 0.45)
    dse.audio.set_playing(state.snd.bgm, true)
end

local function setup_scene()
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(cam, state.ortho_size)
    state.entities.camera = cam

    local cam_target = dse.ecs.create_entity()
    dse.ecs.add_transform(cam_target, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    state.entities.camera_target = cam_target
    dse.ecs.set_camera_follow(cam, cam_target, 0.12, 0.0, 0.5, 0.0, 0.0)

    state.entities.bg = make_sprite(state.tex.bg, 0.0, 0.0, 20.0, 16.0, -100)
    state.entities.start_bg = make_sprite(state.tex.start_bg, 0.0, 0.0, 20.0, 16.0, -99)
    state.entities.grass_l = make_sprite(state.tex.grassL, -8.0, 0.0, 2.6, 16.0, -90)
    state.entities.grass_r = make_sprite(state.tex.grassR, 8.0, 0.0, 2.6, 16.0, -90)
    dse.ecs.set_sprite_uv_scroll(state.entities.grass_l, 0.0, 0.02)
    dse.ecs.set_sprite_uv_scroll(state.entities.grass_r, 0.0, 0.02)
    state.entities.fg_left_top = make_sprite(state.tex.fg[1], -8.5, 5.4, 2.8, 3.2, -85, 1.0, 1.0, 1.0, 0.85)
    state.entities.fg_left_bottom = make_sprite(state.tex.fg[2], -8.5, -5.5, 2.8, 3.2, -85, 1.0, 1.0, 1.0, 0.85)
    state.entities.fg_right_top = make_sprite(state.tex.fg[4], 8.5, 5.4, 2.8, 3.2, -85, 1.0, 1.0, 1.0, 0.85)
    state.entities.fg_right_bottom = make_sprite(state.tex.fg[5], 8.5, -5.5, 2.8, 3.2, -85, 1.0, 1.0, 1.0, 0.85)

    state.entities.wave = make_sprite(state.tex.wave, 0.0, -50.0, 1.2, 1.2, 40, 1.0, 1.0, 1.0, 0.0)
    state.entities.water = make_sprite(state.tex.water[1], 0.0, -50.0, 1.4, 1.4, 41, 1.0, 1.0, 1.0, 0.0)
    dse.ecs.add_animator(state.entities.water)
    dse.ecs.add_animation_state(state.entities.water, "splash", 12.0, true, state.tex.water)
    dse.ecs.play_animation(state.entities.water, "splash")

    state.entities.insect = make_sprite(state.tex.insect[1], 5.8, 4.3, 1.0, 1.0, 120)
    dse.ecs.add_animator(state.entities.insect)
    dse.ecs.add_animation_state(state.entities.insect, "idle", 8.0, true, state.tex.insect)
    dse.ecs.play_animation(state.entities.insect, "idle")

    state.entities.frog = make_sprite(state.tex.act[1], 0.0, 0.0, 1.2, 1.2, 100)
    dse.ecs.add_animator(state.entities.frog)
    dse.ecs.add_animation_state(state.entities.frog, "stand", 9.0, true, {state.tex.act[1], state.tex.act[2], state.tex.act[3], state.tex.act[3], state.tex.act[4]})
    dse.ecs.add_animation_state(state.entities.frog, "jump", 12.0, false, {state.tex.act[5]})
    dse.ecs.add_animation_state(state.entities.frog, "water", 18.0, false, {state.tex.act[6], state.tex.act[7]})
    dse.ecs.play_animation(state.entities.frog, "stand")

    state.entities.star_menu = dse.ecs.create_entity()
    dse.ecs.add_transform(state.entities.star_menu, 0.0, 4.8, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_particle_emitter(state.entities.star_menu, state.tex.star, 180, 14.0)
    dse.ecs.set_particle_density(state.entities.star_menu, 1.0)

    state.entities.star_up = dse.ecs.create_entity()
    dse.ecs.add_transform(state.entities.star_up, 0.0, -3.8, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_particle_emitter(state.entities.star_up, state.tex.star, 260, 40.0)
    dse.ecs.set_particle_density(state.entities.star_up, 0.0)

    state.entities.start_button = make_sprite(state.tex.start, 0.0, 0.2, 3.0, 1.1, 300)
    state.entities.gameover = make_sprite(state.tex.gameover, 0.0, 1.5, 4.0, 1.3, 301, 1.0, 1.0, 1.0, 0.0)

    state.ui.panel = dse.ecs.create_entity()
    dse.ui.add_renderer(state.ui.panel, state.tex.number, 1.0, 1.0, 1.0, 0.0, 1000)
    dse.ui.add_panel(state.ui.panel, false)
    state.ui.score_label = dse.ecs.create_entity()
    dse.ui.add_renderer(state.ui.score_label, state.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(state.ui.score_label, "0", state.tex.number, 1.0, 1.0, 1.0, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, -340.0, 280.0)
    state.ui.max_label = dse.ecs.create_entity()
    dse.ui.add_renderer(state.ui.max_label, state.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(state.ui.max_label, "0", state.tex.number, 1.0, 0.9, 0.7, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, -340.0, 238.0)
    state.ui.pass_label = dse.ecs.create_entity()
    dse.ui.add_renderer(state.ui.pass_label, state.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(state.ui.pass_label, "0", state.tex.number, 1.0, 1.0, 1.0, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, -340.0, 196.0)
    state.ui.life_label = dse.ecs.create_entity()
    dse.ui.add_renderer(state.ui.life_label, state.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(state.ui.life_label, "6", state.tex.number, 1.0, 1.0, 1.0, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, 300.0, 280.0)
    state.ui.combo_label = dse.ecs.create_entity()
    dse.ui.add_renderer(state.ui.combo_label, state.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(state.ui.combo_label, "0", state.tex.number, 1.0, 0.9, 0.4, 1.0, 24.0, 36.0, 2.0, 10, 1, 48, 0.0, 220.0)
    update_ui_numbers()
end

local function set_page_visual()
    if state.page == "start" then
        state.frog_camera_y = 0.0
        dse.ecs.add_transform(state.entities.camera_target, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.ecs.add_transform(state.entities.bg, 0.0, 0.0, 0.0, 20.0, 16.0, 1.0)
        dse.ecs.add_transform(state.entities.fg_left_top, -8.5, 5.4, 0.0, 2.8, 3.2, 1.0)
        dse.ecs.add_transform(state.entities.fg_left_bottom, -8.5, -5.5, 0.0, 2.8, 3.2, 1.0)
        dse.ecs.add_transform(state.entities.fg_right_top, 8.5, 5.4, 0.0, 2.8, 3.2, 1.0)
        dse.ecs.add_transform(state.entities.fg_right_bottom, 8.5, -5.5, 0.0, 2.8, 3.2, 1.0)
        dse.ecs.add_sprite(state.entities.start_bg, 1.0, 1.0, 1.0, 1.0, -99, state.tex.start_bg)
        dse.ecs.add_sprite(state.entities.start_button, 1.0, 1.0, 1.0, 1.0, 300, state.tex.start)
        dse.ecs.set_particle_density(state.entities.star_menu, 1.0)
        dse.ecs.set_particle_density(state.entities.star_up, 0.0)
    elseif state.page == "gameover" then
        state.frog_camera_y = 0.0
        dse.ecs.add_transform(state.entities.camera_target, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.ecs.add_transform(state.entities.bg, 0.0, 0.0, 0.0, 20.0, 16.0, 1.0)
        dse.ecs.add_sprite(state.entities.start_button, 1.0, 1.0, 1.0, 1.0, 302, state.tex.start)
        dse.ecs.add_sprite(state.entities.gameover, 1.0, 1.0, 1.0, 1.0, 301, state.tex.gameover)
        dse.ecs.set_particle_density(state.entities.star_menu, 0.2)
    else
        dse.ecs.add_sprite(state.entities.start_bg, 1.0, 1.0, 1.0, 0.0, -99, state.tex.start_bg)
        dse.ecs.add_sprite(state.entities.start_button, 1.0, 1.0, 1.0, 0.0, 300, state.tex.start)
        dse.ecs.add_sprite(state.entities.gameover, 1.0, 1.0, 1.0, 0.0, 301, state.tex.gameover)
        dse.ecs.set_particle_density(state.entities.star_menu, 0.0)
    end
end

local function update_leaves(dt)
    local cam_anchor_y = state.frog_camera_y
    for i = 1, #state.leaves do
        local leaf = state.leaves[i]
        leaf.x = leaf.x + leaf.s * dt
        if leaf.x <= state.leaf_move_l or leaf.x >= state.leaf_move_r then
            leaf.s = -leaf.s
        end
        if leaf.y < cam_anchor_y - 10.0 then
            leaf.y = state.highest_leaf_y + rnd(state.leaf_min_dist, state.leaf_min_dist * 2.0)
            leaf.x = rnd(state.leaf_move_l, state.leaf_move_r)
            state.highest_leaf_y = leaf.y
        end
        dse.ecs.add_transform(leaf.entity, leaf.x, leaf.y, 0.0, 1.4, 0.65, 1.0)
    end
end

local function nearest_landing_leaf(px, py, pprev)
    local best = nil
    local best_dy = 9999.0
    for i = 1, #state.leaves do
        local leaf = state.leaves[i]
        if pprev >= leaf.y + 0.3 and py <= leaf.y + 0.3 then
            local dx = math.abs(px - leaf.x)
            if dx <= 0.9 then
                local dy = math.abs(py - leaf.y)
                if dy < best_dy then
                    best_dy = dy
                    best = leaf
                end
            end
        end
    end
    return best
end

local function world_mouse()
    refresh_screen_metrics()
    local mx = dse.app.get_mouse_x()
    local my = dse.app.get_mouse_y()
    local nx = (mx / state.screen_w - 0.5) * 2.0
    local ny = (0.5 - my / state.screen_h) * 2.0
    local camera_y = 0.0
    if state.page ~= "start" then
        camera_y = state.frog_camera_y
    end
    return nx * state.ortho_size * state.aspect, ny * state.ortho_size + camera_y
end

local function start_button_hit()
    local wx, wy = world_mouse()
    return math.abs(wx - 0.0) <= 1.8 and math.abs(wy - 0.2) <= 0.7
end

local function update_play(dt)
    local now = dse.app.time_since_startup()
    update_leaves(dt)

    if state.frog_rebirth_at > 0.0 and now >= state.frog_rebirth_at then
        state.frog_rebirth_at = -1.0
        if state.life < 0 then
            state.page = "gameover"
            play_sfx("over")
            set_page_visual()
            return
        end
        local base_leaf = state.leaves[1]
        state.frog_x = base_leaf.x
        state.frog_y = base_leaf.y + 0.45
        state.frog_vy = 0.0
        state.frog_grounded = true
        dse.ecs.play_animation(state.entities.frog, "stand")
        play_sfx("down")
    end

    if state.frog_grounded and state.frog_charge_start < 0.0 and dse.app.get_mouse_left_down() then
        state.frog_charge_start = now
    end

    if state.frog_grounded and state.frog_charge_start >= 0.0 and dse.app.get_mouse_left_up() then
        local hold = now - state.frog_charge_start
        local jump_speed = math.sqrt(math.max(hold, 0.0)) * 8.5
        if jump_speed > 11.0 then
            jump_speed = 11.0
        end
        state.frog_vy = jump_speed
        state.frog_grounded = false
        state.frog_charge_start = -1.0
        dse.ecs.play_animation(state.entities.frog, "jump")
        play_sfx("jump", 1.25 - jump_speed / 11.0)
    end

    if state.frog_grounded then
        local anchor = state.leaves[(state.pass_num % #state.leaves) + 1]
        state.frog_x = state.frog_x + (anchor.x - state.frog_x) * 0.18
    else
        local prev_y = state.frog_y
        state.frog_y = state.frog_y + state.frog_vy * dt
        state.frog_vy = state.frog_vy - 18.0 * dt
        if state.frog_vy <= 0.0 then
            local leaf = nearest_landing_leaf(state.frog_x, state.frog_y, prev_y)
            if leaf then
                state.frog_grounded = true
                state.frog_vy = 0.0
                state.frog_y = leaf.y + 0.45
                dse.ecs.play_animation(state.entities.frog, "stand")
                play_sfx("down")
                local old_pass = state.pass_num
                state.pass_num = state.pass_num + 1
                if state.pass_num - state.last_statist_num > 1 and (now - state.last_pass_time) < 3.2 then
                    state.combo_value = state.pass_num - state.last_statist_num
                    state.combo_keep_until = now + 1.2
                    play_sfx("combo")
                    dse.ecs.particle_burst(state.entities.star_up, 10 + state.combo_value * 2)
                else
                    state.last_statist_num = state.pass_num - 1
                    state.combo_value = 0
                end
                state.last_pass_time = now
                local diff = state.pass_num - old_pass
                local bonus = diff * math.max(1, state.pass_num - state.last_statist_num) * 5
                state.score = state.score + bonus
                if state.score > state.max_score then
                    state.max_score = state.score
                end
                update_ui_numbers()
            end
        end
        if state.frog_y < state.frog_camera_y - 8.0 and state.frog_rebirth_at < 0.0 then
            state.life = state.life - 1
            update_ui_numbers()
            state.frog_rebirth_at = now + 1.0
            dse.ecs.play_animation(state.entities.frog, "water")
            play_sfx("water")
            dse.ecs.particle_burst(state.entities.star_up, 24)
        end
    end

    if state.combo_keep_until > 0.0 and now > state.combo_keep_until then
        state.combo_keep_until = 0.0
        state.combo_value = 0
        update_ui_numbers()
    end

    state.frog_camera_y = state.frog_camera_y + ((state.frog_y + 1.8) - state.frog_camera_y) * 0.06
    dse.ecs.add_transform(state.entities.camera_target, 0.0, state.frog_camera_y, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_transform(state.entities.bg, 0.0, state.frog_camera_y, 0.0, 20.0, 16.0, 1.0)
    dse.ecs.add_transform(state.entities.start_bg, 0.0, state.frog_camera_y, 0.0, 20.0, 16.0, 1.0)
    dse.ecs.add_transform(state.entities.frog, state.frog_x, state.frog_y, 0.0, 1.2, 1.2, 1.0)
    dse.ecs.add_transform(state.entities.wave, state.frog_x, state.frog_y - 0.45, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_transform(state.entities.water, state.frog_x, state.frog_y - 0.4, 0.0, 1.2, 1.2, 1.0)
    dse.ecs.add_transform(state.entities.grass_l, -8.0, state.frog_camera_y, 0.0, 2.6, 16.0, 1.0)
    dse.ecs.add_transform(state.entities.grass_r, 8.0, state.frog_camera_y, 0.0, 2.6, 16.0, 1.0)
    dse.ecs.add_transform(state.entities.fg_left_top, -8.5, state.frog_camera_y + 5.4, 0.0, 2.8, 3.2, 1.0)
    dse.ecs.add_transform(state.entities.fg_left_bottom, -8.5, state.frog_camera_y - 5.5, 0.0, 2.8, 3.2, 1.0)
    dse.ecs.add_transform(state.entities.fg_right_top, 8.5, state.frog_camera_y + 5.4, 0.0, 2.8, 3.2, 1.0)
    dse.ecs.add_transform(state.entities.fg_right_bottom, 8.5, state.frog_camera_y - 5.5, 0.0, 2.8, 3.2, 1.0)

    local density = math.min(1.0, math.max(0.0, state.combo_value / 6.0))
    dse.ecs.set_particle_density(state.entities.star_up, density)
end

function FrogJump.Setup(cfg)
    if state.initialized then
        return
    end
    if cfg and cfg.camera_ortho_size then
        state.ortho_size = cfg.camera_ortho_size
    end
    refresh_screen_metrics()
    math.randomseed(13)
    setup_assets()
    setup_audio()
    setup_scene()
    reset_game()
    state.page = "start"
    set_page_visual()
    state.initialized = true
end

function FrogJump.Update(delta_time)
    if not state.initialized then
        return
    end
    refresh_screen_metrics()
    if state.page == "start" then
        if dse.app.get_mouse_left_up() and start_button_hit() then
            reset_game()
            state.page = "play"
            set_page_visual()
            play_sfx("jump", 0.9)
        end
        return
    end
    if state.page == "gameover" then
        if dse.app.get_mouse_left_up() and start_button_hit() then
            reset_game()
            state.page = "play"
            set_page_visual()
            play_sfx("jump", 0.9)
        end
        return
    end
    update_play(delta_time)
end

return FrogJump
