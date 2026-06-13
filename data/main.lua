-- DSEngine minimal 2D Web demo (A1 M3 "sprite on screen" + input + audio).
-- Self-contained: only the repo data/ dir is preloaded into the Web MEMFS,
-- so this entry must NOT require() the sample modules under samples/lua.
-- Lifecycle: the engine calls global Awake() once, then Update(dt) per frame.
--
-- Controls (these verify the Web input/audio wiring):
--   WASD / Arrow keys : move the white hero sprite
--   Hold Left Mouse   : tint the hero red
--   Click (press)     : play a one-shot SFX
--   M                 : toggle the looping background music
--   Mouse Wheel       : grow / shrink the hero

-- GLFW key codes (no symbolic key constants are exposed to Lua).
local KEY_W, KEY_A, KEY_S, KEY_D = 87, 65, 83, 68
local KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT = 265, 264, 263, 262
local KEY_M = 77

-- The only audio clip shipped under data/ (preloaded into the Web MEMFS).
local AUDIO_CLIP = "audio/spatial/spatial_ping.wav"

local hero
local hero_tex = 0
local hx, hy = 0.0, -2.0
local hscale = 1.0
local t = 0.0
local prev_lmb = false
local prev_m = false
local bgm_on = true

local function load_tex(path)
    local h = dse.assets.load_texture(path)
    if h == 0 then
        h = dse.assets.load_texture("data/" .. path)
    end
    return h
end

local function make_sprite(tex, x, y, sx, sy, order, r, g, b, a)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, 0.0, sx, sy, 1.0)
    dse.ecs.add_sprite(e, r, g, b, a, order, tex)
    return e
end

function Awake()
    dse.app.set_window_title("DSEngine Web - 2D Demo (WASD/Arrows move, LMB tint, click=SFX, M=BGM, wheel zoom)")

    -- Orthographic 2D camera at the origin.
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(cam, 5.0)

    -- A texture that ships in data/ (preloaded into /data on Web).
    hero_tex = load_tex("icon/dse_icon.png")

    -- Static tinted backdrop row.
    make_sprite(hero_tex, -2.4, 0.0, 1.2, 1.2, 0, 0.35, 0.45, 0.70, 1.0)
    make_sprite(hero_tex,  0.0, 0.0, 1.2, 1.2, 0, 0.55, 0.42, 0.78, 1.0)
    make_sprite(hero_tex,  2.4, 0.0, 1.2, 1.2, 0, 0.30, 0.70, 0.68, 1.0)

    -- The interactive hero sprite (driven by input in Update).
    hero = make_sprite(hero_tex, hx, hy, 1.0, 1.0, 1, 1.0, 1.0, 1.0, 1.0)

    -- Looping background music at low volume. WebAudio starts suspended until
    -- the first user gesture; shell.html calls _dse_resume_audio() then, so
    -- playback begins on the first click/keypress.
    if dse.audio and dse.audio.play_bgm then
        dse.audio.play_bgm(AUDIO_CLIP, 0.2, true)
    end
end

local function key(code)
    return dse.app.get_key and dse.app.get_key(code)
end

function Update(delta_time)
    local dt = delta_time or 0.0
    t = t + dt
    if not hero then return end

    -- Keyboard movement (WASD + arrow keys).
    local speed = 4.0
    local dx, dy = 0.0, 0.0
    if key(KEY_A) or key(KEY_LEFT)  then dx = dx - 1.0 end
    if key(KEY_D) or key(KEY_RIGHT) then dx = dx + 1.0 end
    if key(KEY_W) or key(KEY_UP)    then dy = dy + 1.0 end
    if key(KEY_S) or key(KEY_DOWN)  then dy = dy - 1.0 end
    hx = hx + dx * speed * dt
    hy = hy + dy * speed * dt

    -- Mouse wheel zoom.
    if dse.app.get_mouse_scroll_dy then
        local sc = dse.app.get_mouse_scroll_dy()
        if sc and sc ~= 0 then
            hscale = math.max(0.3, math.min(3.0, hscale + sc * 0.1))
        end
    end

    -- Idle pulse so the sprite always shows it is live.
    local s = hscale * (0.95 + math.sin(t * 3.0) * 0.05)
    dse.ecs.add_transform(hero, hx, hy, 0.0, s, s, 1.0)

    -- Left mouse tints the hero red (verifies mouse-button input).
    local pressed = dse.app.get_mouse_left and dse.app.get_mouse_left()
    if pressed then
        dse.ecs.add_sprite(hero, 1.0, 0.25, 0.25, 1.0, 1, hero_tex)
    else
        dse.ecs.add_sprite(hero, 1.0, 1.0, 1.0, 1.0, 1, hero_tex)
    end

    -- A click (press edge) plays a one-shot SFX (verifies Web audio output).
    if pressed and not prev_lmb and dse.audio and dse.audio.play_sfx then
        dse.audio.play_sfx(AUDIO_CLIP, 0.8)
    end
    prev_lmb = pressed and true or false

    -- 'M' (press edge) toggles the background music (verifies pause/resume).
    local m_down = key(KEY_M)
    if m_down and not prev_m and dse.audio then
        bgm_on = not bgm_on
        if bgm_on then
            if dse.audio.resume_bgm then dse.audio.resume_bgm() end
        else
            if dse.audio.pause_bgm then dse.audio.pause_bgm() end
        end
    end
    prev_m = m_down and true or false
end

function exit() end

function main()
    Awake()
end
