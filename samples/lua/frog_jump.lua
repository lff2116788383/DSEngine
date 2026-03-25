local FrogJump = {}

local GameState = {
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
    total_leaf_spawned = 0,
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
    combo_value = 0,
    last_pass_time = 0.0,
    gen_once_num = 0.0,
    down_ofs = 0.0,
    wave_anim = { active = false, time = 0, x = 0, y = 0 },
    water_anim = { active = false, time = 0, x = 0, y = 0 },
    combo_anim = { active = false, time = 0 },
    gameover_anim = { active = false, time = 0 },
    frame_index = 0,
    menu_star_density = 1.0,
    menu_star_density_target = 1.0,
    combo_star_density = 0.0,
    input_prev_left = false,
    input_prev_right = false,
    screen_w = 1280,
    screen_h = 720,
    ortho_size = 7.0,
    aspect = 16.0 / 9.0,
    ui = { lives_icons = {} }
}

-- Helpers
local function rnd(a, b) return a + (b - a) * math.random() end

local function load_tex(path)
    local handle = dse.assets.load_texture(path)
    if handle == 0 then handle = dse.assets.load_texture("data/" .. path) end
    return handle
end

local function make_sprite(tex, x, y, sx, sy, order, r, g, b, a)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, 0.0, sx, sy, 1.0)
    dse.ecs.add_sprite(e, r or 1.0, g or 1.0, b or 1.0, a or 1.0, order or 0, tex or 0)
    return e
end

local function play_sfx(key, pitch)
    local e = GameState.snd[key]
    if not e then return end
    dse.audio.set_pitch(e, pitch or 1.0)
    dse.audio.restart(e)
end

-- Core Engine
local Engine = {}
function Engine:refresh_screen_metrics()
    local w = dse.app.get_screen_width()
    local h = dse.app.get_screen_height()
    if not w or w <= 0 then w = 800 end
    if not h or h <= 0 then h = 600 end
    GameState.screen_w = w
    GameState.screen_h = h
    GameState.aspect = GameState.screen_w / GameState.screen_h
end

function Engine:world_mouse()
    self:refresh_screen_metrics()
    local mx = dse.app.get_mouse_x()
    local my = dse.app.get_mouse_y()
    local nx = (mx / GameState.screen_w - 0.5) * 2.0
    local ny = (0.5 - my / GameState.screen_h) * 2.0
    local camera_y = GameState.page ~= "start" and GameState.frog_camera_y or 0.0
    return nx * GameState.ortho_size * GameState.aspect, ny * GameState.ortho_size + camera_y
end

function Engine:process_insect_events()
    if not GameState.entities.insect then return end
    -- 添加一个计数器，防止在同一帧无限循环
    local safe_counter = 0
    while safe_counter < 10 do
        local insect_event = dse.ecs.pop_animation_event(GameState.entities.insect)
        if insect_event == "" or insect_event == nil then break end
        
        if GameState.page == "play" and insect_event == "laugh" then
            play_sfx("insect")
        end
        safe_counter = safe_counter + 1
    end
end

-- UI Manager
local UIManager = {}
function UIManager:update_numbers()
    dse.ui.set_label_number(GameState.ui.score_label, GameState.score)
    dse.ui.set_label_number(GameState.ui.max_label, GameState.max_score)
    dse.ui.set_label_number(GameState.ui.max_label_l1, GameState.max_score)
    dse.ui.set_label_number(GameState.ui.max_label_l2, GameState.max_score)
    dse.ui.set_label_number(GameState.ui.pass_label, GameState.pass_num)
    dse.ui.set_label_number(GameState.ui.life_label, math.max(GameState.life, 0))
    dse.ui.set_label_number(GameState.ui.combo_label, GameState.combo_value > 0 and GameState.combo_value or 0)
end

function UIManager:set_visible(visible)
    local alpha = visible and 1.0 or 0.0
    dse.ui.add_label(GameState.ui.score_label, tostring(GameState.score), GameState.tex.number, 1.0, 1.0, 1.0, alpha, 20.0, 32.0, 2.0, 10, 1, 48, -340.0, 280.0)
    dse.ui.add_label(GameState.ui.max_label, tostring(GameState.max_score), GameState.tex.number, 1.0, 0.94, 0.69, alpha, 20.0, 32.0, 2.0, 10, 1, 48, -338.0, 239.0)
    dse.ui.add_label(GameState.ui.max_label_l1, tostring(GameState.max_score), GameState.tex.number, 1.0, 1.0, 1.0, alpha, 20.0, 32.0, 2.0, 10, 1, 48, -340.0, 238.0)
    dse.ui.add_label(GameState.ui.max_label_l2, tostring(GameState.max_score), GameState.tex.number, 1.0, 1.0, 1.0, alpha, 20.0, 32.0, 2.0, 10, 1, 48, -341.0, 238.0)
    dse.ui.add_label(GameState.ui.pass_label, tostring(GameState.pass_num), GameState.tex.number, 1.0, 1.0, 1.0, alpha, 20.0, 32.0, 2.0, 10, 1, 48, -340.0, 196.0)
    dse.ui.add_label(GameState.ui.life_label, tostring(math.max(GameState.life, 0)), GameState.tex.number, 1.0, 0.85, 0.82, alpha, 20.0, 32.0, 2.0, 10, 1, 48, 332.0, 280.0)
    
    for i = 1, 6 do
        local e = GameState.ui.lives_icons[i]
        local a = (visible and i <= GameState.life) and 1.0 or 0.0
        dse.ecs.add_sprite(e, 1, 1, 1, a, 1000, GameState.tex.dragonfly)
    end
end

-- Gameplay Systems
local LeafSystem = {}
function LeafSystem:create(i, y_base, global_id)
    local leaf = {}
    leaf.entity = make_sprite(GameState.tex.leaf, rnd(GameState.leaf_move_l, GameState.leaf_move_r), y_base, 1.4, 0.65, 10 + i)
    leaf.shadow = make_sprite(GameState.tex.leaf, rnd(GameState.leaf_move_l, GameState.leaf_move_r), y_base - 0.15, 1.4, 0.65, 9 + i, 0.66, 0.31, 0.46, 0.66)
    leaf.x = rnd(GameState.leaf_move_l, GameState.leaf_move_r)
    leaf.y = y_base
    leaf.global_id = global_id or i
    local diff_speed = math.floor(leaf.global_id / 32) * 0.1
    leaf.s = (rnd(0.5, 1.2) + diff_speed) * (math.random(0, 1) == 0 and -1.0 or 1.0)
    return leaf
end

function LeafSystem:update(dt)
    local cam_anchor_y = GameState.frog_camera_y
    for i = 1, #GameState.leaves do
        local leaf = GameState.leaves[i]
        leaf.x = leaf.x + leaf.s * dt
        if leaf.x <= GameState.leaf_move_l then
            leaf.x = GameState.leaf_move_l
            leaf.s = math.abs(leaf.s)
        elseif leaf.x >= GameState.leaf_move_r then
            leaf.x = GameState.leaf_move_r
            leaf.s = -math.abs(leaf.s)
        end
        if leaf.y < cam_anchor_y - 10.0 then
            GameState.total_leaf_spawned = GameState.total_leaf_spawned + 1
            local id = GameState.total_leaf_spawned
            local min_dist = GameState.leaf_min_dist + math.floor(id / 4) * 0.05
            local max_dist = GameState.leaf_min_dist * 2.0 + math.floor(id / 2) * 0.05
            leaf.y = GameState.highest_leaf_y + rnd(min_dist, max_dist)
            leaf.x = rnd(GameState.leaf_move_l, GameState.leaf_move_r)
            leaf.global_id = id
            local diff_speed = math.floor(id / 32) * 0.1
            leaf.s = (rnd(0.5, 1.2) + diff_speed) * (leaf.s > 0 and 1.0 or -1.0)
            GameState.highest_leaf_y = leaf.y
        end
        
        local y_render = leaf.y
        if GameState.pass_num % GameState.leaf_count + 1 == i then
            y_render = y_render - GameState.down_ofs
        end
        
        dse.ecs.add_transform(leaf.entity, leaf.x, y_render, 0.0, 1.4, 0.65, 1.0)
        dse.ecs.add_transform(leaf.shadow, leaf.x, y_render - 0.15, 0.0, 1.4, 0.65, 1.0)
    end
end

local function setup_assets()
    local load = load_tex
    GameState.tex.bg = load("frog_jump/jump/bg0000.png")
    GameState.tex.start_bg = load("frog_jump/jump/start_bg0000.png")
    GameState.tex.start = load("frog_jump/jump/start0000.png")
    GameState.tex.dragonfly = load("frog_jump/jump/dragonfly0000.png")
    GameState.tex.number = load("frog_jump/jump/number0000.png")
    GameState.tex.grassL = load("frog_jump/jump/grassL0000.png")
    GameState.tex.grassR = load("frog_jump/jump/grassR0000.png")
    GameState.tex.leaf = load("frog_jump/jump/leaf0000.png")
    GameState.tex.gameover = load("frog_jump/jump/gameover0000.png")
    GameState.tex.wave = load("frog_jump/jump/wave0000.png")
    GameState.tex.water = {
        load("frog_jump/jump/water0000.png"), load("frog_jump/jump/water0001.png"),
        load("frog_jump/jump/water0002.png"), load("frog_jump/jump/water0003.png"),
        load("frog_jump/jump/water0004.png")
    }
    GameState.tex.fg = {
        load("frog_jump/jump/fg0_0000.png"), load("frog_jump/jump/fg1_0000.png"),
        load("frog_jump/jump/fg2_0000.png"), load("frog_jump/jump/fg3_0000.png"),
        load("frog_jump/jump/fg4_0000.png"), load("frog_jump/jump/fg5_0000.png")
    }
    GameState.tex.act = {
        load("frog_jump/jump/act0000.png"), load("frog_jump/jump/act0001.png"),
        load("frog_jump/jump/act0002.png"), load("frog_jump/jump/act0003.png"),
        load("frog_jump/jump/act0004.png"), load("frog_jump/jump/act0005.png"),
        load("frog_jump/jump/act0006.png")
    }
    GameState.tex.insect = {
        load("frog_jump/image/chong0000.png"), load("frog_jump/image/chong0001.png"),
        load("frog_jump/image/chong0002.png"), load("frog_jump/image/chong0003.png"),
        load("frog_jump/image/chong0004.png"), load("frog_jump/image/chong0005.png"),
        load("frog_jump/image/chong0006.png")
    }
    GameState.tex.combo = {
        load("frog_jump/image/combo0000.png"), load("frog_jump/image/combo0001.png")
    }
    GameState.tex.star = load("frog_jump/image/star4_0000.png")
end

local function setup_audio()
    local function add(path, loop, volume)
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.audio.add_source(e, path, false, loop or false, volume or 1.0)
        return e
    end
    GameState.snd.down = add("frog_jump/jump/wa.wav", false, 1.0)
    GameState.snd.jump = add("frog_jump/jump/jump.wav", false, 1.0)
    GameState.snd.water = add("frog_jump/jump/water.wav", false, 1.0)
    GameState.snd.over = add("frog_jump/jump/over.wav", false, 1.0)
    GameState.snd.combo = add("frog_jump/jump/combo.wav", false, 1.0)
    GameState.snd.insect = add("frog_jump/sound/hhhh.wav", false, 0.7)
    GameState.snd.bgm = add("frog_jump/jump/bgm.mp3", true, 0.45)
    dse.audio.set_playing(GameState.snd.bgm, true)
end

local function setup_scene()
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(cam, GameState.ortho_size)
    GameState.entities.camera = cam

    local cam_target = dse.ecs.create_entity()
    dse.ecs.add_transform(cam_target, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    GameState.entities.camera_target = cam_target
    dse.ecs.set_camera_follow(cam, cam_target, 0.12, 0.0, 0.5, 0.0, 0.0)

    GameState.entities.bg = make_sprite(GameState.tex.bg, 0.0, 0.0, 20.0, 16.0, -100)
    GameState.entities.start_bg = make_sprite(GameState.tex.start_bg, 0.0, 0.0, 20.0, 16.0, -99)
    GameState.entities.grass_l = make_sprite(GameState.tex.grassL, -8.0, 0.0, 2.6, 16.0, -90)
    GameState.entities.grass_r = make_sprite(GameState.tex.grassR, 8.0, 0.0, 2.6, 16.0, -90)
    dse.ecs.set_sprite_uv_scroll(GameState.entities.grass_l, 0.0, 0.0)
    dse.ecs.set_sprite_uv_scroll(GameState.entities.grass_r, 0.0, 0.0)
    GameState.entities.fg_left_top = make_sprite(GameState.tex.fg[1], -8.5, 5.4, 2.8, 3.2, -85, 1.0, 1.0, 1.0, 0.85)
    GameState.entities.fg_left_bottom = make_sprite(GameState.tex.fg[2], -8.5, -5.5, 2.8, 3.2, -85, 1.0, 1.0, 1.0, 0.85)
    GameState.entities.fg_right_top = make_sprite(GameState.tex.fg[4], 8.5, 5.4, 2.8, 3.2, -85, 1.0, 1.0, 1.0, 0.85)
    GameState.entities.fg_right_bottom = make_sprite(GameState.tex.fg[5], 8.5, -5.5, 2.8, 3.2, -85, 1.0, 1.0, 1.0, 0.85)

    GameState.entities.wave = make_sprite(GameState.tex.wave, 0.0, -50.0, 1.2, 1.2, 8, 1.0, 1.0, 1.0, 0.0)
    GameState.entities.water = make_sprite(GameState.tex.water[1], 0.0, -50.0, 1.4, 1.4, 11, 1.0, 1.0, 1.0, 0.0)
    dse.ecs.add_animator(GameState.entities.water)
    dse.ecs.add_animation_state(GameState.entities.water, "splash", 12.0, true, GameState.tex.water)
    dse.ecs.play_animation(GameState.entities.water, "splash")
    
    GameState.entities.combo_img = make_sprite(GameState.tex.combo[1], 0.0, -50.0, 1.2, 1.2, 200, 1.0, 1.0, 1.0, 0.0)

    GameState.entities.insect = make_sprite(GameState.tex.insect[1], 5.8, 4.3, 1.0, 1.0, 120)
    dse.ecs.add_animator(GameState.entities.insect)
    dse.ecs.add_animation_state(GameState.entities.insect, "idle", 8.333333, true, {
        GameState.tex.insect[3], GameState.tex.insect[2], GameState.tex.insect[1], GameState.tex.insect[1],
        GameState.tex.insect[2], GameState.tex.insect[3], GameState.tex.insect[4], GameState.tex.insect[5],
        GameState.tex.insect[4], GameState.tex.insect[5], GameState.tex.insect[4], GameState.tex.insect[5],
        GameState.tex.insect[4], GameState.tex.insect[5]
    })
    dse.ecs.add_animation_event(GameState.entities.insect, "idle", 0.44642857, "laugh")
    dse.ecs.play_animation(GameState.entities.insect, "idle")

    GameState.entities.frog = make_sprite(GameState.tex.act[1], 0.0, 0.0, 1.2, 1.2, 100)
    dse.ecs.add_animator(GameState.entities.frog)
    dse.ecs.add_animation_state(GameState.entities.frog, "stand", 9.0, true, {GameState.tex.act[1], GameState.tex.act[2], GameState.tex.act[3], GameState.tex.act[3], GameState.tex.act[4]})
    dse.ecs.add_animation_state(GameState.entities.frog, "jump", 12.0, false, {GameState.tex.act[5]})
    dse.ecs.add_animation_state(GameState.entities.frog, "water", 18.0, false, {GameState.tex.act[6], GameState.tex.act[7]})
    dse.ecs.play_animation(GameState.entities.frog, "stand")

    GameState.entities.star_menu = dse.ecs.create_entity()
    dse.ecs.add_transform(GameState.entities.star_menu, 0.0, 4.8, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_particle_emitter(GameState.entities.star_menu, GameState.tex.star, 180, 14.0)
    dse.ecs.set_particle_density(GameState.entities.star_menu, 1.0)

    GameState.entities.star_up = dse.ecs.create_entity()
    dse.ecs.add_transform(GameState.entities.star_up, 0.0, -3.8, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_particle_emitter(GameState.entities.star_up, GameState.tex.star, 260, 40.0)
    dse.ecs.set_particle_density(GameState.entities.star_up, 0.0)

    GameState.entities.start_button = make_sprite(GameState.tex.start, 0.0, 0.2, 3.0, 1.1, 300)
    GameState.entities.gameover = make_sprite(GameState.tex.gameover, 0.0, 1.5, 4.0, 1.3, 301, 1.0, 1.0, 1.0, 0.0)

    for i = 1, 6 do
        local e = make_sprite(GameState.tex.dragonfly, 0, 0, 1.0, 1.0, 1000)
        dse.ecs.add_sprite(e, 1, 1, 1, 0, 1000, GameState.tex.dragonfly)
        table.insert(GameState.ui.lives_icons, e)
    end

    GameState.ui.panel = dse.ecs.create_entity()
    dse.ui.add_renderer(GameState.ui.panel, GameState.tex.number, 1.0, 1.0, 1.0, 0.0, 1000)
    dse.ui.add_panel(GameState.ui.panel, false)
    GameState.ui.score_label = dse.ecs.create_entity()
    dse.ui.add_renderer(GameState.ui.score_label, GameState.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(GameState.ui.score_label, "0", GameState.tex.number, 1.0, 1.0, 1.0, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, -340.0, 280.0)
    GameState.ui.max_label = dse.ecs.create_entity()
    dse.ui.add_renderer(GameState.ui.max_label, GameState.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(GameState.ui.max_label, "0", GameState.tex.number, 1.0, 0.94, 0.69, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, -338.0, 239.0)
    GameState.ui.max_label_l1 = dse.ecs.create_entity()
    dse.ui.add_renderer(GameState.ui.max_label_l1, GameState.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(GameState.ui.max_label_l1, "0", GameState.tex.number, 1.0, 1.0, 1.0, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, -340.0, 238.0)
    GameState.ui.max_label_l2 = dse.ecs.create_entity()
    dse.ui.add_renderer(GameState.ui.max_label_l2, GameState.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(GameState.ui.max_label_l2, "0", GameState.tex.number, 1.0, 1.0, 1.0, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, -341.0, 238.0)
    GameState.ui.pass_label = dse.ecs.create_entity()
    dse.ui.add_renderer(GameState.ui.pass_label, GameState.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(GameState.ui.pass_label, "0", GameState.tex.number, 1.0, 1.0, 1.0, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, -340.0, 196.0)
    GameState.ui.life_label = dse.ecs.create_entity()
    dse.ui.add_renderer(GameState.ui.life_label, GameState.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(GameState.ui.life_label, "6", GameState.tex.number, 1.0, 0.85, 0.82, 1.0, 20.0, 32.0, 2.0, 10, 1, 48, 332.0, 280.0)
    GameState.ui.combo_label = dse.ecs.create_entity()
    dse.ui.add_renderer(GameState.ui.combo_label, GameState.tex.number, 1.0, 1.0, 1.0, 0.0, 1002)
    dse.ui.add_label(GameState.ui.combo_label, "0", GameState.tex.number, 1.0, 0.9, 0.4, 0.0, 24.0, 36.0, 2.0, 10, 1, 48, 0.0, 220.0)
    UIManager:update_numbers()
end

local function set_page_visual()
    UIManager:set_visible(GameState.page ~= "start")

    if GameState.page == "start" then
        GameState.frog_camera_y = 0.0
        dse.ecs.add_transform(GameState.entities.camera_target, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.ecs.add_transform(GameState.entities.bg, 0.0, 0.0, 0.0, 20.0, 16.0, 1.0)
        dse.ecs.add_transform(GameState.entities.fg_left_top, -8.5, 5.4, 0.0, 2.8, 3.2, 1.0)
        dse.ecs.add_transform(GameState.entities.fg_left_bottom, -8.5, -5.5, 0.0, 2.8, 3.2, 1.0)
        dse.ecs.add_transform(GameState.entities.fg_right_top, 8.5, 5.4, 0.0, 2.8, 3.2, 1.0)
        dse.ecs.add_transform(GameState.entities.fg_right_bottom, 8.5, -5.5, 0.0, 2.8, 3.2, 1.0)
        dse.ecs.add_sprite(GameState.entities.start_bg, 1.0, 1.0, 1.0, 1.0, -99, GameState.tex.start_bg)
        dse.ecs.add_sprite(GameState.entities.start_button, 1.0, 1.0, 1.0, 1.0, 300, GameState.tex.start)
        GameState.menu_star_density = 1.0
        GameState.menu_star_density_target = 1.0
        GameState.combo_star_density = 0.0
    elseif GameState.page == "gameover" then
        GameState.frog_camera_y = 0.0
        GameState.gameover_anim.time = 0
        GameState.gameover_anim.active = true
        dse.ecs.add_transform(GameState.entities.camera_target, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.ecs.add_transform(GameState.entities.bg, 0.0, 0.0, 0.0, 20.0, 16.0, 1.0)
        dse.ecs.add_sprite(GameState.entities.start_button, 1.0, 1.0, 1.0, 0.0, 302, GameState.tex.start)
        dse.ecs.add_sprite(GameState.entities.gameover, 1.0, 1.0, 1.0, 1.0, 301, GameState.tex.gameover)
        GameState.menu_star_density_target = 0.2
    else
        dse.ecs.add_sprite(GameState.entities.start_bg, 1.0, 1.0, 1.0, 0.0, -99, GameState.tex.start_bg)
        dse.ecs.add_sprite(GameState.entities.start_button, 1.0, 1.0, 1.0, 0.0, 300, GameState.tex.start)
        dse.ecs.add_sprite(GameState.entities.gameover, 1.0, 1.0, 1.0, 0.0, 301, GameState.tex.gameover)
        GameState.menu_star_density_target = 0.0
    end
end

local function update_leaves(dt)
    local cam_anchor_y = state.frog_camera_y
    for i = 1, #state.leaves do
        local leaf = state.leaves[i]
        leaf.x = leaf.x + leaf.s * dt
        if leaf.x <= state.leaf_move_l then
            leaf.x = state.leaf_move_l
            leaf.s = math.abs(leaf.s)
        elseif leaf.x >= state.leaf_move_r then
            leaf.x = state.leaf_move_r
            leaf.s = -math.abs(leaf.s)
        end
        if leaf.y < cam_anchor_y - 10.0 then
            state.total_leaf_spawned = state.total_leaf_spawned + 1
            local id = state.total_leaf_spawned
            local min_dist = state.leaf_min_dist + math.floor(id / 4) * 0.05
            local max_dist = state.leaf_min_dist * 2.0 + math.floor(id / 2) * 0.05
            leaf.y = state.highest_leaf_y + rnd(min_dist, max_dist)
            leaf.x = rnd(state.leaf_move_l, state.leaf_move_r)
            leaf.global_id = id
            local diff_speed = math.floor(id / 32) * 0.1
            leaf.s = (rnd(0.5, 1.2) + diff_speed) * (leaf.s > 0 and 1.0 or -1.0)
            state.highest_leaf_y = leaf.y
        end
        
        -- 渲染荷叶及阴影，加入受力下压表现
        local y_render = leaf.y
        if state.pass_num % state.leaf_count + 1 == i then
            y_render = y_render - state.down_ofs
        end
        
        dse.ecs.add_transform(leaf.entity, leaf.x, y_render, 0.0, 1.4, 0.65, 1.0)
        dse.ecs.add_transform(leaf.shadow, leaf.x, y_render - 0.15, 0.0, 1.4, 0.65, 1.0)
    end
end

local function start_button_hit()
    local wx, wy = Engine:world_mouse()
    return math.abs(wx - 0.0) <= 1.8 and math.abs(wy - 0.2) <= 0.7
end

local function reset_game()
    GameState.page = "play"
    GameState.pass_num = 0
    GameState.last_statist_num = 0
    GameState.score = 0
    GameState.combo_value = 0
    GameState.gen_once_num = 0.0
    GameState.life = 6
    GameState.frog_vy = 0.0
    GameState.frog_grounded = true
    GameState.frog_charge_start = -1.0
    GameState.frog_rebirth_at = -1.0
    GameState.highest_leaf_y = 0.0
    GameState.down_ofs = 0.0
    GameState.frame_index = 0
    GameState.menu_star_density = 0.0
    GameState.menu_star_density_target = 0.0
    GameState.combo_star_density = 0.0
    GameState.input_prev_left = dse.app.get_mouse_left()
    GameState.input_prev_right = dse.app.get_mouse_right()
    
    GameState.wave_anim.active = false
    dse.ecs.add_sprite(GameState.entities.wave, 1, 1, 1, 0, 8, GameState.tex.wave)
    GameState.water_anim.active = false
    dse.ecs.add_sprite(GameState.entities.water, 1, 1, 1, 0, 40, GameState.tex.water[1])
    GameState.combo_anim.active = false
    dse.ecs.add_sprite(GameState.entities.combo_img, 1, 1, 1, 0, 200, GameState.tex.combo[1])
    dse.ui.add_renderer(GameState.ui.combo_label, GameState.tex.number, 1, 1, 1, 0, 1002)

    GameState.total_leaf_spawned = 0
    GameState.leaves = {}
    local y = -1.0
    for i = 1, GameState.leaf_count do
        GameState.total_leaf_spawned = GameState.total_leaf_spawned + 1
        local leaf = LeafSystem:create(i, y, GameState.total_leaf_spawned)
        table.insert(GameState.leaves, leaf)
        local min_dist = GameState.leaf_min_dist + math.floor(GameState.total_leaf_spawned / 4) * 0.05
        local max_dist = GameState.leaf_min_dist * 2.0 + math.floor(GameState.total_leaf_spawned / 2) * 0.05
        y = y + rnd(min_dist, max_dist)
        if y > GameState.highest_leaf_y then
            GameState.highest_leaf_y = y
        end
    end
    
    local first = GameState.leaves[1]
    GameState.frog_x = first.x
    GameState.frog_y = first.y + 0.45
    dse.ecs.add_transform(GameState.entities.frog, GameState.frog_x, GameState.frog_y, 0.0, 1.2, 1.2, 1.0)
    dse.ecs.play_animation(GameState.entities.frog, "stand")
    UIManager:update_numbers()
    dse.ecs.set_particle_density(GameState.entities.star_up, 0.0)
end

local function update_play(dt)
    local now = dse.app.time_since_startup()
    GameState.frame_index = GameState.frame_index + 1
    local mouse_left = dse.app.get_mouse_left()
    local mouse_right = dse.app.get_mouse_right()
    local mouse_left_down = mouse_left and not GameState.input_prev_left
    local mouse_left_up = (not mouse_left) and GameState.input_prev_left
    LeafSystem:update(dt)
    local frame_ratio = dt * 60.0
    GameState.menu_star_density = GameState.menu_star_density + (GameState.menu_star_density_target - GameState.menu_star_density) * math.min(1.0, 0.1 * frame_ratio)
    dse.ecs.set_particle_density(GameState.entities.star_menu, GameState.menu_star_density)

    if GameState.frog_rebirth_at > 0.0 and now >= GameState.frog_rebirth_at then
        GameState.frog_rebirth_at = -1.0
        if GameState.life < 0 then
            GameState.page = "gameover"
            GameState.gameover_anim.time = 0
            GameState.gameover_anim.active = true
            play_sfx("over")
            set_page_visual()
            return
        end
        local base_leaf = GameState.leaves[(GameState.pass_num % GameState.leaf_count) + 1]
        GameState.frog_x = base_leaf.x
        GameState.frog_y = base_leaf.y + 0.45
        GameState.frog_vy = 0.0
        GameState.frog_grounded = true
        dse.ecs.play_animation(GameState.entities.frog, "stand")
        play_sfx("down")
    end

    if GameState.frog_grounded and GameState.frog_charge_start < 0.0 and mouse_left_down then
        GameState.frog_charge_start = now
    end

    if GameState.frog_grounded and GameState.frog_charge_start >= 0.0 then
        if mouse_right then
            GameState.frog_charge_start = -1.0
        elseif mouse_left_up then
            local hold = now - GameState.frog_charge_start
            local jump_speed = math.sqrt(math.max(hold, 0.0)) * 15.0
            if jump_speed < 1.0 then jump_speed = 1.0 end
            if jump_speed > 18.0 then jump_speed = 18.0 end
            GameState.frog_vy = jump_speed
            GameState.frog_grounded = false
            GameState.frog_charge_start = -1.0
            dse.ecs.play_animation(GameState.entities.frog, "jump")
            play_sfx("jump", 1.25 - jump_speed / 18.0)
        elseif not mouse_left then
            GameState.frog_charge_start = -1.0
        end
    end

    Engine:process_insect_events()

    if GameState.frog_grounded then
        local anchor = GameState.leaves[(GameState.pass_num % GameState.leaf_count) + 1]
        GameState.frog_x = GameState.frog_x + (anchor.x - GameState.frog_x) * 0.5
    else
        GameState.frog_y = GameState.frog_y + GameState.frog_vy * frame_ratio
        GameState.frog_vy = GameState.frog_vy - 0.28 * frame_ratio
        
        -- 核心：下落达到一定速度时瞬间进行落地/落水判定（伪3D跳跃感）
        if GameState.frog_vy <= -3.0 then
            GameState.frog_vy = 0.0
            
            local check_pass = GameState.pass_num
            local stand_r = 0.39
            local frog_foot_y = GameState.frog_y - 0.45
            while true do
                local leaf = GameState.leaves[(check_pass % GameState.leaf_count) + 1]
                if frog_foot_y > leaf.y + stand_r then
                    check_pass = check_pass + 1
                else
                    break
                end
            end
            
            local target_leaf = GameState.leaves[(check_pass % GameState.leaf_count) + 1]
            local crs_x = math.abs(GameState.frog_x - target_leaf.x)
            local crs_y = math.abs(frog_foot_y - target_leaf.y)
            
            -- 水波纹动画重置
            GameState.wave_anim.active = true
            GameState.wave_anim.time = 0
            GameState.wave_anim.x = GameState.frog_x
            GameState.wave_anim.y = target_leaf.y
            
            if crs_y > stand_r or crs_x > stand_r then
                -- 落水判定
                GameState.life = GameState.life - 1
                UIManager:update_numbers()
                GameState.frog_rebirth_at = now + 1.0
                GameState.last_statist_num = check_pass
                
                GameState.water_anim.active = true
                GameState.water_anim.time = 0
                GameState.water_anim.x = GameState.wave_anim.x
                GameState.water_anim.y = GameState.wave_anim.y
                
                dse.ecs.play_animation(GameState.entities.frog, "water")
                dse.ecs.play_animation(GameState.entities.water, "splash")
                play_sfx("water")
                dse.ecs.add_transform(GameState.entities.star_up, GameState.frog_x, target_leaf.y, 0.0, 1.0, 1.0, 1.0)
                dse.ecs.particle_burst(GameState.entities.star_up, 24)
            else
                -- 成功着陆
                GameState.frog_grounded = true
                GameState.frog_y = target_leaf.y + 0.45
                dse.ecs.play_animation(GameState.entities.frog, "stand")
                play_sfx("down")
                GameState.down_ofs = 0.25 -- 荷叶受压效果
                
                local old_pass = GameState.pass_num
                GameState.pass_num = check_pass
                
                if GameState.pass_num > old_pass then
                    GameState.gen_once_num = math.min(6.0, GameState.gen_once_num + 1.0)
                    if GameState.pass_num - GameState.last_statist_num > 1 and (now - GameState.last_pass_time) < 3.2 then
                        GameState.combo_value = GameState.pass_num - GameState.last_statist_num
                        play_sfx("combo")
                        dse.ecs.add_transform(GameState.entities.star_up, GameState.frog_x, GameState.frog_y + 0.25, 0.0, 1.0, 1.0, 1.0)
                        dse.ecs.particle_burst(GameState.entities.star_up, 10 + GameState.combo_value * 2)
                        GameState.combo_anim.active = true
                        GameState.combo_anim.time = 0
                    else
                        GameState.last_statist_num = GameState.pass_num - 1
                        GameState.combo_value = 0
                    end
                    GameState.last_pass_time = now
                    
                    -- 精准落点奖励机制
                    local base_score = (crs_x < 0.3) and 10 or 5
                    local diff = GameState.pass_num - old_pass
                    local bonus = diff * math.max(1, GameState.pass_num - GameState.last_statist_num) * base_score
                    GameState.score = GameState.score + bonus
                    if GameState.score > GameState.max_score then GameState.max_score = GameState.score end
                    UIManager:update_numbers()
                end
            end
        end
    end

    if GameState.down_ofs > 0.001 then
        GameState.down_ofs = GameState.down_ofs * 0.925
    else
        GameState.down_ofs = 0.0
    end
    if GameState.gen_once_num > 0.5 then
        GameState.gen_once_num = GameState.gen_once_num - 0.005 * frame_ratio
    elseif GameState.gen_once_num < 0.0 then
        GameState.gen_once_num = 0.0
    end

    if GameState.wave_anim.active then
        GameState.wave_anim.time = GameState.wave_anim.time + dt * 1000
        if GameState.wave_anim.time > 2000 then
            GameState.wave_anim.active = false
            dse.ecs.add_sprite(GameState.entities.wave, 1, 1, 1, 0, 8, GameState.tex.wave)
        else
            local t = GameState.wave_anim.time / 2000.0
            local scale = 0.45 + (3.0 - 0.45) * t
            local alpha = 1.0 - t
            dse.ecs.add_transform(GameState.entities.wave, GameState.wave_anim.x, GameState.wave_anim.y, 0, scale, scale, 1)
            dse.ecs.add_sprite(GameState.entities.wave, 1, 1, 1, alpha, 8, GameState.tex.wave)
        end
    end

    if GameState.water_anim.active then
        GameState.water_anim.time = GameState.water_anim.time + dt * 1000
        if GameState.water_anim.time > 450 then
            GameState.water_anim.active = false
            dse.ecs.add_sprite(GameState.entities.water, 1, 1, 1, 0, 40, GameState.tex.water[1])
        else
            dse.ecs.add_transform(GameState.entities.water, GameState.water_anim.x, GameState.water_anim.y + 0.5, 0, 1.4, 1.4, 1)
            dse.ecs.add_sprite(GameState.entities.water, 1, 1, 1, 1, 40, GameState.tex.water[1])
        end
    end

    if GameState.combo_anim.active then
        GameState.combo_anim.time = GameState.combo_anim.time + dt * 1000
        if GameState.combo_anim.time > 3600 then
            GameState.combo_anim.active = false
            dse.ecs.add_sprite(GameState.entities.combo_img, 1, 1, 1, 0, 200, GameState.tex.combo[1])
            dse.ui.add_renderer(GameState.ui.combo_label, GameState.tex.number, 1, 1, 1, 0, 1002)
        else
            local t = GameState.combo_anim.time
            local ox = t < 1800 and -2.0 * (t / 1800.0) or -2.0 + 2.0 * ((t - 1800) / 1800.0)
            local oy = 2.0 * (t / 3600.0)
            local alpha = 1.0 - (t / 3600.0)
            local flash = math.floor(GameState.frame_index / 4) % 2 == 0
            local tex = flash and GameState.tex.combo[2] or GameState.tex.combo[1]
            local cx = 5.0 + ox
            local cy = GameState.frog_camera_y + 3.0 + oy
            dse.ecs.add_transform(GameState.entities.combo_img, cx, cy, 0, 1.2, 1.2, 1)
            dse.ecs.add_sprite(GameState.entities.combo_img, 1, 1, 1, alpha, 200, tex)
            dse.ui.add_renderer(GameState.ui.combo_label, GameState.tex.number, 1.0, 0.9, 0.4, alpha, 1002)
        end
    end

    GameState.frog_camera_y = GameState.frog_camera_y + ((GameState.frog_y + 1.8) - GameState.frog_camera_y) * 0.06
    dse.ecs.add_transform(GameState.entities.camera_target, 0.0, GameState.frog_camera_y, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_transform(GameState.entities.bg, 0.0, GameState.frog_camera_y, 0.0, 20.0, 16.0, 1.0)
    dse.ecs.add_transform(GameState.entities.start_bg, 0.0, GameState.frog_camera_y, 0.0, 20.0, 16.0, 1.0)
    dse.ecs.add_transform(GameState.entities.frog, GameState.frog_x, GameState.frog_y, 0.0, 1.2, 1.2, 1.0)
    dse.ecs.add_transform(GameState.entities.grass_l, -8.0, GameState.frog_camera_y, 0.0, 2.6, 16.0, 1.0)
    dse.ecs.add_transform(GameState.entities.grass_r, 8.0, GameState.frog_camera_y, 0.0, 2.6, 16.0, 1.0)
    local grass_scroll = GameState.frog_camera_y * 0.018
    dse.ecs.set_sprite_uv_offset(GameState.entities.grass_l, 0.0, grass_scroll)
    dse.ecs.set_sprite_uv_offset(GameState.entities.grass_r, 0.0, grass_scroll)
    dse.ecs.add_transform(GameState.entities.fg_left_top, -8.5, GameState.frog_camera_y + 5.4, 0.0, 2.8, 3.2, 1.0)
    dse.ecs.add_transform(GameState.entities.fg_left_bottom, -8.5, GameState.frog_camera_y - 5.5, 0.0, 2.8, 3.2, 1.0)
    dse.ecs.add_transform(GameState.entities.fg_right_top, 8.5, GameState.frog_camera_y + 5.4, 0.0, 2.8, 3.2, 1.0)
    dse.ecs.add_transform(GameState.entities.fg_right_bottom, 8.5, GameState.frog_camera_y - 5.5, 0.0, 2.8, 3.2, 1.0)

    -- 更新生命值图标UI位置
    local cam_y = GameState.frog_camera_y
    for i = 1, 6 do
        local e = GameState.ui.lives_icons[i]
        if i <= GameState.life then
            dse.ecs.add_transform(e, 8.5 - i * 0.8, cam_y + 4.5, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_sprite(e, 1, 1, 1, 1, 1000, GameState.tex.dragonfly)
        else
            dse.ecs.add_sprite(e, 1, 1, 1, 0, 1000, GameState.tex.dragonfly)
        end
    end

    local target_combo_density = math.min(1.0, math.max(0.0, GameState.gen_once_num / 6.0))
    GameState.combo_star_density = GameState.combo_star_density + (target_combo_density - GameState.combo_star_density) * math.min(1.0, 0.12 * frame_ratio)
    dse.ecs.set_particle_density(GameState.entities.star_up, GameState.combo_star_density)
    GameState.input_prev_left = mouse_left
    GameState.input_prev_right = mouse_right
end

function FrogJump.Setup(cfg)
    if GameState.initialized then return end
    if cfg and cfg.camera_ortho_size then GameState.ortho_size = cfg.camera_ortho_size end
    Engine:refresh_screen_metrics()
    math.randomseed(13)
    setup_assets()
    setup_audio()
    setup_scene()
    reset_game()
    GameState.page = "start"
    set_page_visual()
    GameState.initialized = true
end

function FrogJump.Update(delta_time)
    if not GameState.initialized then return end
    Engine:refresh_screen_metrics()
    local mouse_left = dse.app.get_mouse_left()
    local mouse_left_up = (not mouse_left) and GameState.input_prev_left
    
    local btn_scale_x, btn_scale_y = 3.0, 1.1
    if start_button_hit() then
        btn_scale_x, btn_scale_y = 3.3, 1.2
    end

    if GameState.page == "start" then
        dse.ecs.add_transform(GameState.entities.start_button, 0.0, 0.2, 0.0, btn_scale_x, btn_scale_y, 1.0)
        if mouse_left_up and start_button_hit() then
            -- 在重置游戏之前，强制清理掉所有残留事件
            if GameState.entities.insect then
                local safe_counter = 0
                while safe_counter < 100 do
                    local ev = dse.ecs.pop_animation_event(GameState.entities.insect)
                    if ev == "" or ev == nil then break end
                    safe_counter = safe_counter + 1
                end
            end
            
            reset_game()
            GameState.page = "play"
            set_page_visual()
            play_sfx("jump", 0.7)
            GameState.input_prev_left = mouse_left
            GameState.input_prev_right = dse.app.get_mouse_right()
        end
        GameState.input_prev_left = mouse_left
        GameState.input_prev_right = dse.app.get_mouse_right()
        
        -- 清理可能积累的动画事件，防止在开始界面累积并在进入游戏后瞬间爆发
        if GameState.entities.insect then
            local safe_counter = 0
            while safe_counter < 100 do
                local ev = dse.ecs.pop_animation_event(GameState.entities.insect)
                if ev == "" or ev == nil then break end
                safe_counter = safe_counter + 1
            end
        end
        return
    end
    
    if GameState.page == "gameover" then
        GameState.gameover_anim.time = GameState.gameover_anim.time + delta_time * 1000
        local t = math.min(1.0, GameState.gameover_anim.time / 400.0)
        local scale = 0.2 + 0.8 * t
        dse.ecs.add_transform(GameState.entities.gameover, 0.0, GameState.frog_camera_y + 1.5, 0.0, scale * 4.0, scale * 1.3, 1.0)
        
        local btn_alpha = math.min(1.0, GameState.gameover_anim.time / 1000.0)
        dse.ecs.add_transform(GameState.entities.start_button, 0.0, GameState.frog_camera_y + 0.2, 0.0, btn_scale_x, btn_scale_y, 1.0)
        dse.ecs.add_sprite(GameState.entities.start_button, 1, 1, 1, btn_alpha, 302, GameState.tex.start)
        
        if mouse_left_up and start_button_hit() then
            -- 在重置游戏之前，强制清理掉所有残留事件
            if GameState.entities.insect then
                local safe_counter = 0
                while safe_counter < 100 do
                    local ev = dse.ecs.pop_animation_event(GameState.entities.insect)
                    if ev == "" or ev == nil then break end
                    safe_counter = safe_counter + 1
                end
            end
            
            reset_game()
            GameState.page = "play"
            set_page_visual()
            play_sfx("jump", 0.7)
            GameState.input_prev_left = mouse_left
            GameState.input_prev_right = dse.app.get_mouse_right()
        end
        GameState.input_prev_left = mouse_left
        GameState.input_prev_right = dse.app.get_mouse_right()
        
        -- 清理可能积累的动画事件，防止在结束界面累积
        if GameState.entities.insect then
            local safe_counter = 0
            while safe_counter < 100 do
                local ev = dse.ecs.pop_animation_event(GameState.entities.insect)
                if ev == "" or ev == nil then break end
                safe_counter = safe_counter + 1
            end
        end
        return
    end
    
    update_play(delta_time)
end

return FrogJump

