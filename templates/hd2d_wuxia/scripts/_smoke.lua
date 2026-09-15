-- ============================================================================
-- _smoke.lua  渲染冒烟自检（验证：分层大地图 / 精灵朝向 / 中文位图字体 / Bloom）
-- 运行：bin\dsengine_lua_relwithdebinfo.exe --script=templates\hd2d_wuxia\scripts\_smoke.lua
-- ============================================================================
local ecs = dse.ecs
local TILE_W, TILE_H = 44, 30
local W, H = TILE_W, TILE_H

local function file_exists(p)
    local f = io.open(p, "rb")
    if f then f:close() return true end
    return false
end
local ROOT = "templates/hd2d_wuxia/"
local function resolve(p)
    for _, c in ipairs({ p, ROOT .. p, "assets/" .. p, ROOT .. "assets/" .. p }) do
        if file_exists(c) then return c end
    end
    return p
end
local T = {}
local function load_tex(p) return dse.assets.load_texture(resolve(p)) end

local frame = 0
local text_objs = {}

local function spawn_sprite(tex, x, y, sx, sy, order, r, g, b, a)
    local e = ecs.create_entity()
    ecs.add_transform(e, x, y, 0.0, sx, sy, 1.0)
    ecs.add_sprite(e, r or 1, g or 1, b or 1, a or 1, order or 0, tex)
    return e
end

-- 用位图字体图集拼一行中文（验证 uv 约定）
local function make_text(str, x, y, atlas, fontdata, cr, cg, cb)
    local objs = {}
    local pen = x
    for _, cp in utf8.codes(str) do
        local gl = fontdata.glyphs[cp]
        if gl then
            local e = ecs.create_entity()
            ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
            dse.ui.add_renderer(e, atlas, cr or 1, cg or 1, cb or 1, 1.0, 100000, gl.w, gl.h)
            dse.ui.set_anchor(e, 0, 0)
            dse.ui.set_uv(e, gl.u0, gl.v0, gl.u1, gl.v1)
            dse.ui.set_position(e, pen + gl.ox + gl.w * 0.5, y + gl.oy - gl.h * 0.5)
            pen = pen + gl.adv
            objs[#objs + 1] = e
        end
    end
    return objs
end

function Awake()
    T.sky = load_tex("maps/village_sky.png")
    T.far = load_tex("maps/village_far.png")
    T.ground = load_tex("maps/village_ground.png")
    T.glow = load_tex("fx/glow_warm.png")
    T.hero_idle = load_tex("char/hero/hero_d_idle_0.png")
    T.hero_walk = load_tex("char/hero/hero_d_walk_0.png")
    T.fg = load_tex("maps/village_fg.png")
    print(string.format("[smoke] tex sky=%d far=%d ground=%d glow=%d hero=%d fg=%d",
        T.sky, T.far, T.ground, T.glow, T.hero_idle, T.fg))

    local cam = ecs.create_entity()
    ecs.add_transform(cam, W * 0.5, H * 0.5, 0, 1, 1, 1)
    ecs.add_camera(cam, 6.25)
    ecs.add_camera_controller_2d(cam)
    ecs.camera_set_zoom(cam, 1.0)
    ecs.camera_set_bounds(cam, 0, 0, W, H)
    ecs.add_post_process(cam, true, 0.82, 0.75, 1.0)     -- Bloom
    ecs.set_post_process_color(cam, true, 1.0, 1.05)

    spawn_sprite(T.sky, W * 0.5, H * 0.5, W, H, -40)
    spawn_sprite(T.far, W * 0.5, H * 0.68, W, H, -30)
    spawn_sprite(T.ground, W * 0.5, H * 0.5, W, H, -20)
    -- 灯笼光晕（应在地面之上、角色之下）
    spawn_sprite(T.glow, 15.5, 14.2, 5, 5, -10, 1, 0.85, 0.5, 0.85)
    -- 角色：脚底在 y，中心 = y + h/2
    spawn_sprite(T.hero_idle, 20.5, 15.0 + 52 / 64.0, 40 / 32.0, 52 / 32.0, 0)
    spawn_sprite(T.hero_walk, 24.5, 15.0 + 52 / 64.0, 40 / 32.0, 52 / 32.0, 0)
    spawn_sprite(T.fg, W * 0.5, H * 0.5, W, H, 20)

    local fs = require("font_small")
    local fb = require("font_big")
    local ta = load_tex("ui/font_small.png")
    local tb = load_tex("ui/font_big.png")
    make_text("青溪问剑 HD-2D", 40, 660, tb, fb, 1.0, 0.94, 0.78)
    make_text("WASD 移动　J 攻击　K 闪避　U/I 技能　E 交互", 40, 600, ta, fs, 0.92, 0.94, 1.0)
    make_text("生命 100/100　内力 60/60　等级 1　金钱 128", 40, 40, ta, fs, 0.9, 0.95, 0.9)
    print(string.format("[smoke] font small=%d glyphs big=%d glyphs, atlas=%d/%d", #fs.glyphs, #fb.glyphs, ta, tb))
end

function Update(dt)
    frame = frame + 1
    if frame % 30 == 0 then
        print(string.format("[smoke] frame=%d fps=%.1f sprites=%d draws=%d", frame,
            dse.metrics.get_fps(), dse.metrics.get_sprite_count(), dse.metrics.get_draw_calls()))
    end
end