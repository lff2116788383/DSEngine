-- DSEngine minimal 2D Web demo (A1 M3 "sprite on screen").
-- Self-contained: only the repo data/ dir is preloaded into the Web MEMFS,
-- so this entry must NOT require() the sample modules under samples/lua.
-- Lifecycle: the engine calls global Awake() once, then Update(dt) per frame.

local hero
local t = 0.0

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
    dse.app.set_window_title("DSEngine Web - 2D Demo")

    -- Orthographic 2D camera at the origin.
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(cam, 5.0)

    -- A texture that ships in data/ (preloaded into /data on Web).
    local tex = load_tex("icon/dse_icon.png")

    -- Static tinted backdrop row.
    make_sprite(tex, -2.4, 0.0, 1.2, 1.2, 0, 0.35, 0.45, 0.70, 1.0)
    make_sprite(tex,  0.0, 0.0, 1.2, 1.2, 0, 0.55, 0.42, 0.78, 1.0)
    make_sprite(tex,  2.4, 0.0, 1.2, 1.2, 0, 0.30, 0.70, 0.68, 1.0)

    -- A moving / pulsing sprite on top so motion is visible each frame.
    hero = make_sprite(tex, 0.0, -2.0, 1.0, 1.0, 1, 1.0, 1.0, 1.0, 1.0)
end

function Update(delta_time)
    t = t + (delta_time or 0.0)
    if hero then
        local x = math.sin(t * 1.5) * 2.5
        local s = 0.9 + math.sin(t * 3.0) * 0.2
        -- add_transform acts as an upsert: re-applying updates the entity.
        dse.ecs.add_transform(hero, x, -2.0, 0.0, s, s, 1.0)
    end
end

function exit() end

function main()
    Awake()
end
