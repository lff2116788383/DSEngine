-- ============================================================================
-- DSEngine 2D Platformer Template — 完整 3 关平台跳跃游戏
-- ----------------------------------------------------------------------------
-- 操作：
--   A / D（或 ← →）移动    Space 跳跃（按住时间越长跳得越高）
--   R 重开当前关卡
-- 玩法：收集金币与宝石、踩头消灭敌人、避开尖刺、站在弹簧上弹跳、
--       抵达关卡尽头的旗帜过关；生命耗尽则 Game Over。
--
-- 关卡数据在下方 LEVELS 表中（纯 Lua 声明式），改坐标即可改关卡。
-- 物理为手写 AABB（可读、可调、平台跳跃手感可控），不依赖 Box2D 调参。
-- ============================================================================

local app = dse.app

-- ── 键码（GLFW）───────────────────────────────────────────────────────────
local KEY_A, KEY_D, KEY_LEFT, KEY_RIGHT = 65, 68, 263, 262
local KEY_SPACE, KEY_R = 32, 82

-- ── 物理 / 手感参数 ────────────────────────────────────────────────────────
local MOVE_SPEED = 7.0      -- 水平速度
local JUMP_VEL   = 15.5     -- 起跳速度
local GRAVITY    = -38.0    -- 重力加速度
local MAX_FALL   = -26.0    -- 最大下落速度
local COYOTE     = 0.08     -- 土狼时间（离开平台后仍可起跳）
local BUFFER     = 0.10     -- 跳跃缓冲（落地前按下的跳跃仍生效）
local PW, PH     = 0.45, 0.95   -- 玩家碰撞盒半宽 / 半高
local P_SCALE    = 1.1      -- 玩家精灵显示缩放
local ENEMY_STOMP_BOUNCE = 9.0 -- 踩头后反弹速度
local SPRING_BOOST = 26.0   -- 弹簧弹起速度
local START_LIVES = 3

-- ── 资产路径解析（兼容：相对路径 / data:/assets: 前缀）─────────────────────
local function file_exists(path)
    local f = io.open(path, "rb")
    if f then f:close(); return true end
    return false
end
local function resolve_path(path)
    if file_exists(path) then return path end
    for _, p in ipairs({ "data/" .. path, "assets/" .. path }) do
        if file_exists(p) then return p end
    end
    return path -- 找不到时交给引擎资产系统（打包后从 game.dpak 挂载）
end
local function load_tex(path)  return dse.assets.load_texture(resolve_path(path)) end
local function audio_path(path) return resolve_path(path) end

-- ── 关卡数据（改这里即可设计新关卡）────────────────────────────────────────
-- platforms: 静态平台 { x,y, hw,hh }（中心点 + 半宽高）
-- moving:    移动平台 { x,y,hw,hh, axis="x"|"y", dist, speed }
-- coins/gems/springs: { x,y }
-- spikes: { x,y, w,h }（尖刺碰撞盒，中心点 + 半宽高，比贴图小一点）
-- enemies: { x,y, kind="slime"|"fly"|"slime_static", range }
--          slime 地面巡逻；slime_static 原地不动；fly 空中水平往返
-- flag: 终点旗帜 { x, y }
-- start: 出生点 { x, y }
-- bounds: 相机边界 { minx, miny, maxx, maxy }
--
-- 坐标说明：主地面顶面（grass_mid 顶部）在 y = -1.0，地面上的物体应以其为基准：
--   出生点/旗帜：y = -0.05（玩家中心 = 地面 + 碰撞半高 0.95）
--   史莱姆：     y = -0.45（精灵半高 0.55，脚踩在地面上）
--   尖刺：       y = -1.35（碰撞盒上沿恰好到地面，玩家踩上才受伤）
--   弹簧：       y = -0.6（精灵半高 0.4，底部在地面上）
local LEVELS = {
    {
        name = "Level 1 - Meadow",
        start = { x = -13.0, y = -0.05 },
        flag  = { x = 13.0, y = -0.05 },
        bounds = { -22.0, -10.0, 22.0, 10.0 },
        platforms = {
            { x = -16.0, y = -2.5, hw = 14.0, hh = 1.5 },   -- 主地面
            { x = 12.0,  y = -2.5, hw = 10.0, hh = 1.5 },   -- 第二段地面
            { x = -1.0,  y = 1.0,  hw = 2.4,  hh = 0.4 },   -- 空中平台
            { x = 5.0,   y = 2.2,  hw = 1.6,  hh = 0.4 },
        },
        moving = {
            { x = 5.0, y = -1.2, hw = 1.5, hh = 0.35, axis = "x", dist = 3.0, speed = 2.2 },
        },
        coins = {
            { x = -10.0, y = -0.5 }, { x = -7.0, y = -0.5 }, { x = -4.0, y = -0.5 },
            { x = -1.0,  y = 2.2 },  { x = 3.0,  y = 0.2 },
            { x = 5.0,   y = 3.4 },  { x = 9.0,  y = -0.5 },
        },
        gems = { { x = 1.0, y = 3.0 } },
        springs = { { x = 7.5, y = -0.6 } },
        spikes = { { x = 2.0, y = -1.35, w = 0.55, h = 0.35 } },
        enemies = {
            { x = -8.0, y = -0.45, kind = "slime", range = 3.0 },
            { x = 11.0, y = -0.45, kind = "slime_static" },
        },
    },
    {
        name = "Level 2 - Sky Bridges",
        start = { x = -16.0, y = -0.05 },
        flag  = { x = 16.0, y = -0.05 },
        bounds = { -24.0, -12.0, 24.0, 12.0 },
        platforms = {
            { x = -18.0, y = -2.5, hw = 7.0,  hh = 1.5 },
            { x = 18.0,  y = -2.5, hw = 7.0,  hh = 1.5 },
            { x = -10.0, y = 0.8,  hw = 2.2,  hh = 0.4 },
            { x = -4.0,  y = 2.6,  hw = 2.2,  hh = 0.4 },
            { x = 2.0,   y = 1.6,  hw = 2.0,  hh = 0.4 },
            { x = 8.0,   y = 2.6,  hw = 2.2,  hh = 0.4 },
            { x = 13.0,  y = 0.6,  hw = 2.0,  hh = 0.4 },
        },
        moving = {
            { x = 5.0, y = 0.4,  hw = 1.6, hh = 0.35, axis = "y", dist = 2.6, speed = 1.8 },
            { x = 10.0, y = 3.6, hw = 1.4, hh = 0.35, axis = "x", dist = 2.5, speed = 2.6 },
        },
        coins = {
            { x = -10.0, y = 2.0 }, { x = -4.0, y = 3.8 }, { x = -1.5, y = 3.8 },
            { x = 2.0,   y = 3.4 },  { x = 6.5,  y = 2.2 }, { x = 10.5, y = 4.6 },
            { x = 13.0,  y = 1.8 },  { x = 16.0, y = -0.5 },
        },
        gems = { { x = -1.5, y = 4.6 } },
        springs = {},
        spikes = {
            { x = -13.0, y = -1.35, w = 0.8, h = 0.35 },
            { x = 15.0,  y = -1.35, w = 0.8, h = 0.35 },
        },
        enemies = {
            -- 中央缺口无地面：用空中巡逻的 fly 替代原漂浮的 slime
            { x = -7.0, y = 1.5, kind = "fly", range = 4.0 },
            { x = 2.0,  y = 2.8, kind = "fly", range = 4.0 },
            { x = 12.0, y = -0.45, kind = "slime", range = 2.0 },
        },
    },
    {
        name = "Level 3 - Final Trial",
        start = { x = -16.0, y = -0.05 },
        flag  = { x = 16.0, y = -0.05 },
        bounds = { -24.0, -14.0, 24.0, 14.0 },
        platforms = {
            { x = -18.0, y = -2.5, hw = 6.0,  hh = 1.5 },
            { x = 18.0,  y = -2.5, hw = 6.0,  hh = 1.5 },
            { x = -12.0, y = 0.6,  hw = 2.0,  hh = 0.4 },
            { x = -6.0,  y = 2.4,  hw = 2.0,  hh = 0.4 },
            { x = -6.0,  y = -0.2, hw = 2.0,  hh = 0.4 },
            { x = 0.0,   y = 1.4,  hw = 2.2,  hh = 0.4 },
            { x = 6.0,   y = 2.6,  hw = 2.0,  hh = 0.4 },
            { x = 12.0,  y = 0.8,  hw = 2.0,  hh = 0.4 },
            { x = 12.0,  y = -1.6, hw = 2.0,  hh = 0.4 },
        },
        moving = {
            { x = -3.0, y = -0.4, hw = 1.6, hh = 0.35, axis = "y", dist = 3.0, speed = 2.2 },
            { x = 3.0,  y = 3.6,  hw = 1.5, hh = 0.35, axis = "x", dist = 3.5, speed = 2.8 },
            { x = 9.0,  y = 4.2,  hw = 1.5, hh = 0.35, axis = "y", dist = 3.4, speed = 2.4 },
        },
        coins = {
            { x = -12.0, y = 1.8 }, { x = -6.0, y = 3.6 }, { x = -4.5, y = 1.8 },
            { x = -3.0,  y = 1.6 },  { x = 0.0,  y = 3.0 }, { x = 3.0,  y = 5.0 },
            { x = 6.0,   y = 4.0 },  { x = 9.0,  y = 5.6 }, { x = 12.0, y = 2.0 },
            { x = 16.0,  y = -0.5 },
        },
        gems = { { x = 0.0, y = 3.0 }, { x = 9.0, y = 5.6 } },
        springs = { { x = 15.0, y = -0.6 } },
        spikes = {
            { x = -15.0, y = -1.35, w = 0.8, h = 0.35 },
            { x = 3.0,   y = -0.4, w = 0.8, h = 0.35 },   -- 空中悬浮尖刺（低位跨越障碍）
            { x = 14.0,  y = -1.35, w = 0.8, h = 0.35 },
        },
        enemies = {
            { x = -14.0, y = -0.45, kind = "slime", range = 2.0 },
            { x = -6.0,  y = 3.0,   kind = "fly",   range = 5.0 },
            { x = 6.0,   y = 3.4,   kind = "fly",   range = 4.0 },
            { x = 15.0,  y = -0.45, kind = "slime", range = 2.0 },
        },
    },
}

-- ── 纹理 / 音频句柄 ────────────────────────────────────────────────────────
local T, S = {}, {}
local function LoadAssets()
    T.player_stand = load_tex("assets/textures/player_stand.png")
    T.player_walk1 = load_tex("assets/textures/player_walk1.png")
    T.player_walk2 = load_tex("assets/textures/player_walk2.png")
    T.player_jump  = load_tex("assets/textures/player_jump.png")
    T.player_hit   = load_tex("assets/textures/player_hit.png")
    T.slime        = load_tex("assets/textures/slime.png")
    T.slime_move   = load_tex("assets/textures/slime_move.png")
    T.slime_dead   = load_tex("assets/textures/slime_dead.png")
    T.fly          = load_tex("assets/textures/fly.png")
    T.fly_move     = load_tex("assets/textures/fly_move.png")
    T.coin         = load_tex("assets/textures/coin.png")
    T.gem          = load_tex("assets/textures/gem.png")
    T.star         = load_tex("assets/textures/star.png")
    T.flag1        = load_tex("assets/textures/flag1.png")
    T.flag2        = load_tex("assets/textures/flag2.png")
    T.flag_down    = load_tex("assets/textures/flag_down.png")
    T.spikes       = load_tex("assets/textures/spikes.png")
    T.spring       = load_tex("assets/textures/spring.png")
    T.sprung       = load_tex("assets/textures/sprung.png")
    T.grass_mid    = load_tex("assets/textures/grass_mid.png")
    T.dirt         = load_tex("assets/textures/dirt.png")
    T.background   = load_tex("assets/textures/background.png")
    T.bush         = load_tex("assets/textures/bush.png")
    T.sign         = load_tex("assets/textures/sign.png")
    T.hud_coin     = load_tex("assets/textures/hud_coin.png")
    T.heart_full   = load_tex("assets/textures/heart_full.png")
    T.heart_empty  = load_tex("assets/textures/heart_empty.png")
    T.font         = load_tex("assets/font/bitmap_font.png")

    S.jump   = audio_path("assets/audio/jump.wav")
    S.coin   = audio_path("assets/audio/coin.wav")
    S.hurt   = audio_path("assets/audio/hurt.wav")
    S.stomp  = audio_path("assets/audio/stomp.wav")
    S.spring = audio_path("assets/audio/spring.wav")
    S.win    = audio_path("assets/audio/win.wav")
    S.bgm    = audio_path("assets/audio/bgm.wav")
end

-- play_sfx 的第三个参数 loop 在引擎侧是 int（luaL_checkinteger），
-- 不能传布尔值 false/true，必须传 0/1。
local function play_sfx(path, vol)
    if path then dse.audio.play_sfx(path, vol, 0) end
end

-- ── 运行时状态 ─────────────────────────────────────────────────────────────
local state = {
    mode = "play",          -- play / level_complete / game_over / win
    level_index = 1,
    lives = START_LIVES,
    coins = 0,
    total_coins = 0,
    time = 0.0,
    hud = {},
}

local function CurrentLevel() return LEVELS[state.level_index] end

-- 小工具
local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end
local function aabb(ax, ay, ahw, ahh, bx, by, bhw, bhh)
    return math.abs(ax - bx) < (ahw + bhw) and math.abs(ay - by) < (ahh + bhh)
end

-- 实体创建辅助：精灵（中心点 + 半宽高 -> 全尺寸）
local function spawn_sprite(tex, x, y, sx, sy, order)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, 0.0, sx, sy, 1.0)
    dse.ecs.add_sprite(e, 1.0, 1.0, 1.0, 1.0, order or 0, tex)
    return e
end

-- 实体创建辅助：纯色块（无纹理，用于天空/土壤/山丘等背景层）
local function spawn_solid(x, y, sx, sy, order, r, g, b)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, 0.0, sx, sy, 1.0)
    dse.ecs.add_sprite(e, r, g, b, 1.0, order or 0, 0)
    return e
end

-- ============================================================================
-- 关卡构建
-- ============================================================================
local level_entities = { solids = {}, moving = {}, coins = {}, gems = {}, enemies = {},
                         springs = {}, spikes = {}, decor = {}, flag = nil, player = nil, particles = {} }
local flag_anim_entity = nil

-- 粒子：手写微型尘埃（跳跃/落地时喷出，纯色小方块，短寿命）
local function spawn_dust(x, y, count)
    for _ = 1, count do
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, x, y, 0.0, 0.14, 0.14, 1.0)
        dse.ecs.add_sprite(e, 0.85, 0.82, 0.75, 0.9, 5, 0)
        table.insert(level_entities.particles, {
            e = e,
            x = x, y = y,
            vx = (math.random() - 0.5) * 4.0,
            vy = math.random() * 2.0 + 0.5,
            life = 0.35 + math.random() * 0.2,
            t = 0.0,
        })
    end
end

local function ClearLevelEntities()
    local seen = {}
    local function kill(e)
        if e ~= 0 and e ~= nil and not seen[e] then
            seen[e] = true
            pcall(dse.ecs.destroy_entity, e)
        end
    end
    for _, s in ipairs(level_entities.solids) do if s.e then kill(s.e) end end
    for _, m in ipairs(level_entities.moving) do if m.e then kill(m.e) end end
    for _, c in ipairs(level_entities.coins) do if c.e then kill(c.e) end end
    for _, g in ipairs(level_entities.gems) do if g.e then kill(g.e) end end
    for _, en in ipairs(level_entities.enemies) do if en.e then kill(en.e) end end
    for _, sp in ipairs(level_entities.springs) do if sp.e then kill(sp.e) end end
    for _, sk in ipairs(level_entities.spikes) do if sk.e then kill(sk.e) end end
    for _, d in ipairs(level_entities.decor) do if d then kill(d) end end
    if level_entities.flag then kill(level_entities.flag) end
    if flag_anim_entity then kill(flag_anim_entity) end
    for _, p in ipairs(level_entities.particles) do if p.e then kill(p.e) end end
    if level_entities.player then kill(level_entities.player) end
    level_entities = { solids = {}, moving = {}, coins = {}, gems = {}, enemies = {},
                       springs = {}, spikes = {}, decor = {}, flag = nil, player = nil, particles = {} }
end

local function BuildLevel(level)
    -- 背景（全部为纯色/细带，不再拉伸 128x128 背景图 —— 放大几十倍会严重变形发白）：
    --   order -4  天空底板：浅蓝，覆盖关卡边界 + 边距，保证相机视野内无黑边
    --   order -3  地下土壤：深棕，从地面略下延伸到视野底部，避免地面之下露出天空
    --   order -2  地平线山丘：绿色细带，略高于地面，营造纵深
    local bx0, by0, bx1, by1 = level.bounds[1], level.bounds[2], level.bounds[3], level.bounds[4]
    local cx, cy = (bx0 + bx1) * 0.5, (by0 + by1) * 0.5
    local bw = (bx1 - bx0) + 12.0
    local bh = (by1 - by0) + 12.0
    table.insert(level_entities.decor, spawn_solid(cx, cy, bw, bh, -4, 0.49, 0.78, 1.0))   -- 天空（浅蓝）
    table.insert(level_entities.decor, spawn_solid(cx, (by0 - 1.0) * 0.5, bw, -1.0 - by0, -3, 0.30, 0.20, 0.12))   -- 地下土壤
    table.insert(level_entities.decor, spawn_solid(cx, 0.0, bw, 3.0, -2, 0.42, 0.72, 0.22))   -- 远景山丘（向下覆盖到草地顶面，避免露出天空缝）

    -- 静态平台：中心地面用 dirt 铺底 + grass_mid 顶面；空中平台用 grass_mid
    for _, p in ipairs(level.platforms) do
        local is_ground = (p.y < 0.0 and p.hh >= 1.0)
        local top = spawn_sprite(T.grass_mid, p.x, p.y + p.hh - 0.35, p.hw * 2.0, 0.7, 1)
        table.insert(level_entities.solids, { e = top, x = p.x, y = p.y, hw = p.hw, hh = p.hh })
        if is_ground then
            -- 泥土块上沿延伸到草地底部（-1.7），避免草皮与泥土之间露出土壤缝隙
            local body = spawn_sprite(T.dirt, p.x, p.y - p.hh + 1.15, p.hw * 2.0, p.hh * 2.0 - 0.7, 0)
            table.insert(level_entities.solids, { e = body })
        end
    end

    -- 移动平台
    for _, m in ipairs(level.moving or {}) do
        local e = spawn_sprite(T.grass_mid, m.x, m.y, m.hw * 2.0, m.hh * 2.0, 1)
        table.insert(level_entities.moving, {
            e = e, x = m.x, y = m.y, base_x = m.x, base_y = m.y,
            hw = m.hw, hh = m.hh, axis = m.axis, dist = m.dist, speed = m.speed, t = math.random() * 10.0,
        })
    end

    -- 金币 / 宝石
    for _, c in ipairs(level.coins or {}) do
        local e = spawn_sprite(T.coin, c.x, c.y, 0.7, 0.7, 3)
        table.insert(level_entities.coins, { e = e, x = c.x, y = c.y, collected = false })
    end
    for _, g in ipairs(level.gems or {}) do
        local e = spawn_sprite(T.gem, g.x, g.y, 0.8, 0.8, 3)
        table.insert(level_entities.gems, { e = e, x = g.x, y = g.y, collected = false })
    end

    -- 敌人
    for _, en in ipairs(level.enemies or {}) do
        local tex, move_tex, dead_tex
        if en.kind == "fly" then
            tex, move_tex, dead_tex = T.fly, T.fly_move, T.fly_dead
        else
            tex, move_tex, dead_tex = T.slime, T.slime_move, T.slime_dead
        end
        local e = spawn_sprite(tex, en.x, en.y, 1.1, 1.1, 2)
        local is_fly = (en.kind == "fly")
        local hw, hh = 0.5, 0.45
        if is_fly then hw, hh = 0.55, 0.4 end
        table.insert(level_entities.enemies, {
            e = e, kind = en.kind, x = en.x, y = en.y,
            base_x = en.x, base_y = en.y, range = en.range or 0.0,
            hw = hw, hh = hh, dead = false, dir = 1.0, t = math.random() * 10.0,
        })
        dse.ecs.add_animator(e)
        if move_tex and tex then
            dse.ecs.add_animation_state(e, "move", is_fly and 8.0 or 6.0, true, { tex, move_tex })
            dse.ecs.play_animation(e, "move")
        end
        if dead_tex then
            dse.ecs.add_animation_state(e, "dead", 1.0, true, { dead_tex })
        end
    end

    -- 尖刺
    for _, sk in ipairs(level.spikes or {}) do
        local e = spawn_sprite(T.spikes, sk.x, sk.y, 1.3, 1.0, 2)
        table.insert(level_entities.spikes, { e = e, x = sk.x, y = sk.y, w = sk.w or 0.5, h = sk.h or 0.3 })
    end

    -- 弹簧
    for _, sp in ipairs(level.springs or {}) do
        local e = spawn_sprite(T.spring, sp.x, sp.y, 1.0, 0.8, 2)
        dse.ecs.add_animator(e)
        if T.spring and T.sprung then
            dse.ecs.add_animation_state(e, "bounce", 10.0, false, { T.sprung, T.spring })
        end
        table.insert(level_entities.springs, { e = e, x = sp.x, y = sp.y, activated = 0.0 })
    end

    -- 终点旗帜（旗杆 + 飘动旗面动画）
    local flag = level.flag
    local pole = spawn_sprite(T.flag_down, flag.x, flag.y, 0.5, 2.2, 1)
    flag_anim_entity = spawn_sprite(T.flag1, flag.x + 0.35, flag.y + 0.85, 1.0, 0.9, 2)
    dse.ecs.add_animator(flag_anim_entity)
    dse.ecs.add_animation_state(flag_anim_entity, "wave", 6.0, true, { T.flag1, T.flag2 })
    dse.ecs.play_animation(flag_anim_entity, "wave")
    level_entities.flag = pole
    level_entities.flag_x, level_entities.flag_y = flag.x, flag.y

    -- 起点指示牌 + 灌木装饰（灌木种在地面上，而非相机视野下方）
    table.insert(level_entities.decor, spawn_sprite(T.sign, level.start.x + 1.2, level.start.y - 0.5, 1.0, 1.0, 1))
    for i = 1, 6 do
        local bx = level.bounds[1] + 2.0 + (i - 1) * ((level.bounds[3] - level.bounds[1] - 4.0) / 5)
        table.insert(level_entities.decor, spawn_sprite(T.bush, bx, -0.9, 1.4, 1.2, 0))
    end

    -- 玩家
    local p = dse.ecs.create_entity()
    dse.ecs.add_transform(p, level.start.x, level.start.y, 0.0, P_SCALE, P_SCALE, 1.0)
    dse.ecs.add_sprite(p, 1.0, 1.0, 1.0, 1.0, 4, T.player_stand)
    dse.ecs.add_animator(p)
    dse.ecs.add_animation_state(p, "idle", 1.0, true, { T.player_stand })
    dse.ecs.add_animation_state(p, "walk", 10.0, true, { T.player_walk1, T.player_walk2 })
    dse.ecs.add_animation_state(p, "jump", 1.0, true, { T.player_jump })
    dse.ecs.add_animation_state(p, "hurt", 1.0, true, { T.player_hit })
    dse.ecs.play_animation(p, "idle")
    level_entities.player = p
end

-- ============================================================================
-- 玩家
-- ============================================================================
local player = {
    x = 0, y = 0, vx = 0, vy = 0,
    on_ground = false, face = 1.0,
    coyote = 0.0, buffer = 0.0,
    invuln = 0.0,      -- 受击无敌计时
    hurt_flash = 0.0,
    anim = "idle",
}

local function ResetPlayer()
    local level = CurrentLevel()
    player.x, player.y = level.start.x, level.start.y
    player.vx, player.vy = 0.0, 0.0
    player.on_ground = false
    player.coyote, player.buffer = 0.0, 0.0
    player.invuln, player.hurt_flash = 0.0, 0.0
    player.anim = "idle"
end

local function SetPlayerAnim(name)
    if player.anim ~= name then
        player.anim = name
        dse.ecs.play_animation(level_entities.player, name)
    end
end

local function DamagePlayer(from_x)
    if player.invuln > 0.0 or state.mode ~= "play" then return end
    state.lives = state.lives - 1
    player.invuln = 1.5
    player.hurt_flash = 0.5
    play_sfx(S.hurt, 1.0)
    -- 击退：远离伤害来源
    local dir = 1.0
    if from_x then dir = (player.x < from_x) and -1.0 or 1.0 end
    player.vx = dir * 6.0
    player.vy = 6.0
    player.on_ground = false
    if state.lives <= 0 then
        state.mode = "game_over"
    end
end

local function RespawnLevel()
    ClearLevelEntities()
    state.time = 0.0
    state.coins = 0
    state.mode = "play"
    local level = CurrentLevel()
    local n = 0
    for _, c in ipairs(level.coins or {}) do n = n + 1 end
    for _, g in ipairs(level.gems or {}) do n = n + 1 end
    state.total_coins = n
    BuildLevel(level)
    ResetPlayer()
    -- 关卡重建后玩家是新实体：重新绑定相机跟随与边界（并直接摆到出生点）
    if state.cam then
        dse.ecs.set_transform_position(state.cam, player.x, player.y, 0.0)
        dse.ecs.set_camera_follow(state.cam, level_entities.player, 0.10, 0.0, 0.0, 0.0, 0.0)
        dse.ecs.camera_set_bounds(state.cam, level.bounds[1], level.bounds[2], level.bounds[3], level.bounds[4])
    end
end

local function AdvanceLevel()
    if state.level_index >= #LEVELS then
        state.mode = "win"
        play_sfx(S.win, 1.0)
        return
    end
    state.level_index = state.level_index + 1
    RespawnLevel()
    if S.bgm then dse.audio.play_bgm(S.bgm, 0.7, true) end
end

-- ============================================================================
-- 碰撞解析（手写 AABB）
-- ============================================================================
local function CollideSolids(dt)
    local level = CurrentLevel()

    -- 移动平台运动
    for _, m in ipairs(level_entities.moving) do
        m.t = m.t + dt * m.speed
        local off = math.sin(m.t) * m.dist
        if m.axis == "y" then
            m.y = m.base_y + off
        else
            m.x = m.base_x + off
        end
        dse.ecs.set_transform_position(m.e, m.x, m.y, 0.0)
    end

    -- X 轴碰撞
    player.x = player.x + player.vx * dt
    local solids = level_entities.solids
    for _, s in ipairs(solids) do
        if not s.x then goto continue_x end
        if aabb(player.x, player.y, PW, PH, s.x, s.y, s.hw, s.hh) then
            if player.vx > 0 then player.x = s.x - s.hw - PW
            elseif player.vx < 0 then player.x = s.x + s.hw + PW end
            player.vx = 0.0
        end
        ::continue_x::
    end
    for _, m in ipairs(level_entities.moving) do
        if aabb(player.x, player.y, PW, PH, m.x, m.y, m.hw, m.hh) then
            if player.vx > 0 then player.x = m.x - m.hw - PW
            elseif player.vx < 0 then player.x = m.x + m.hw + PW end
            player.vx = 0.0
        end
    end

    -- Y 轴碰撞
    local was_ground = player.on_ground
    player.y = player.y + player.vy * dt
    player.on_ground = false
    for _, s in ipairs(solids) do
        if not s.x then goto continue_y end
        if aabb(player.x, player.y, PW, PH, s.x, s.y, s.hw, s.hh) then
            if player.vy <= 0.0 then
                player.y = s.y + s.hh + PH
                player.on_ground = true
            else
                player.y = s.y - s.hh - PH
            end
            player.vy = 0.0
        end
        ::continue_y::
    end
    for _, m in ipairs(level_entities.moving) do
        if aabb(player.x, player.y, PW, PH, m.x, m.y, m.hw, m.hh) then
            if player.vy <= 0.0 then
                player.y = m.y + m.hh + PH
                player.on_ground = true
            else
                player.y = m.y - m.hh - PH
            end
            player.vy = 0.0
        end
    end

    -- 落地检测（喷尘土）
    if player.on_ground and not was_ground and player.vy <= 0.0 then
        spawn_dust(player.x, player.y - PH, 4)
    end
end

-- ============================================================================
-- 交互（金币 / 敌人 / 尖刺 / 弹簧 / 旗帜）
-- ============================================================================
local function UpdateInteractions(dt)
    -- 金币
    for _, c in ipairs(level_entities.coins) do
        if not c.collected and aabb(player.x, player.y, PW, PH, c.x, c.y, 0.4, 0.4) then
            c.collected = true
            if c.e then pcall(dse.ecs.destroy_entity, c.e) end
            c.e = nil
            state.coins = state.coins + 1
            play_sfx(S.coin, 1.0)
        end
    end
    -- 宝石（+2 金币）
    for _, g in ipairs(level_entities.gems) do
        if not g.collected and aabb(player.x, player.y, PW, PH, g.x, g.y, 0.45, 0.45) then
            g.collected = true
            if g.e then pcall(dse.ecs.destroy_entity, g.e) end
            g.e = nil
            state.coins = state.coins + 2
            play_sfx(S.coin, 1.0)
        end
    end

    -- 弹簧
    for _, sp in ipairs(level_entities.springs) do
        if aabb(player.x, player.y, PW, PH, sp.x, sp.y + 0.3, 0.5, 0.5) and player.vy <= 0.0 then
            player.vy = SPRING_BOOST
            player.on_ground = false
            if sp.e then pcall(dse.ecs.play_animation, sp.e, "bounce") end
            play_sfx(S.spring, 1.0)
            spawn_dust(player.x, player.y - PH, 6)
        end
    end

    -- 敌人
    for _, en in ipairs(level_entities.enemies) do
        if en.dead then goto continue_en end
        if aabb(player.x, player.y, PW, PH, en.x, en.y, en.hw, en.hh) then
            -- 踩头判定：玩家脚底需接近敌人顶面（下落中）才算踩；横向贴脸算撞伤
            local stomp = player.vy < 0.0 and (player.y - PH) > (en.y + en.hh - 0.45)
            if stomp then
                en.dead = true
                player.vy = ENEMY_STOMP_BOUNCE
                player.on_ground = false
                play_sfx(S.stomp, 1.0)
                if en.e then pcall(dse.ecs.play_animation, en.e, "dead") end
                en.death_timer = 0.4
            else
                DamagePlayer(en.x)
            end
        end
        if en.death_timer then
            en.death_timer = en.death_timer - dt
            if en.death_timer <= 0.0 and en.e then
                pcall(dse.ecs.destroy_entity, en.e)
                en.e = nil
            end
        end
        ::continue_en::
    end

    -- 尖刺
    if player.invuln <= 0.0 then
        for _, sk in ipairs(level_entities.spikes) do
            if aabb(player.x, player.y, PW, PH - 0.2, sk.x, sk.y, sk.w, sk.h) then
                DamagePlayer(sk.x)
                break
            end
        end
    end

    -- 旗帜（过关）
    if state.mode == "play" and level_entities.flag_x then
        if aabb(player.x, player.y, PW, PH,
                level_entities.flag_x, level_entities.flag_y + 1.1, 0.6, 1.5) then
            state.mode = "level_complete"
            play_sfx(S.win, 1.0)
        end
    end
end

-- ============================================================================
-- 敌人更新
-- ============================================================================
local function UpdateEnemies(dt)
    for _, en in ipairs(level_entities.enemies) do
        if en.dead then goto continue_en2 end
        en.t = en.t + dt
        if en.kind == "slime" then
            -- 地面巡逻：range 范围内来回
            en.x = en.x + en.dir * 1.2 * dt
            if en.x > en.base_x + en.range then en.dir = -1.0 end
            if en.x < en.base_x - en.range then en.dir = 1.0 end
        elseif en.kind == "fly" then
            en.x = en.base_x + math.sin(en.t * 1.5) * en.range
            en.y = en.base_y + math.sin(en.t * 2.2) * 0.8
        end
        if en.e then
            dse.ecs.set_transform_position(en.e, en.x, en.y, 0.0)
        end
        ::continue_en2::
    end
end

-- ============================================================================
-- 玩家更新
-- ============================================================================
local function UpdatePlayer(dt)
    if state.mode ~= "play" then return end
    local level = CurrentLevel()

    -- 计时器
    player.coyote = player.coyote - dt
    player.buffer = player.buffer - dt
    if player.invuln > 0.0 then player.invuln = player.invuln - dt end
    if player.hurt_flash > 0.0 then player.hurt_flash = player.hurt_flash - dt end

    -- 输入：水平移动
    local input_x = 0.0
    if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then input_x = input_x - 1.0 end
    if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then input_x = input_x + 1.0 end

    -- 跳跃（缓冲 + 土狼时间 + 可变高度）
    if app.get_key_down(KEY_SPACE) then player.buffer = BUFFER end
    if player.on_ground then player.coyote = COYOTE end
    if player.buffer > 0.0 and player.coyote > 0.0 then
        player.vy = JUMP_VEL
        player.on_ground = false
        player.coyote = 0.0
        player.buffer = 0.0
        play_sfx(S.jump, 1.0)
        spawn_dust(player.x, player.y - PH, 5)
    end
    -- 松开空格截断上升（可变跳跃高度）
    if app.get_key_up(KEY_SPACE) and player.vy > 4.0 then
        player.vy = 4.0
    end

    -- 重力
    player.vx = input_x * MOVE_SPEED
    player.vy = player.vy + GRAVITY * dt
    if player.vy < MAX_FALL then player.vy = MAX_FALL end

    CollideSolids(dt)

    -- 朝向
    if input_x < 0.0 then player.face = -1.0 end
    if input_x > 0.0 then player.face = 1.0 end

    -- 动画状态
    if player.invuln > 0.0 then
        SetPlayerAnim("hurt")
    elseif not player.on_ground then
        SetPlayerAnim("jump")
    elseif math.abs(player.vx) > 0.5 then
        SetPlayerAnim("walk")
    else
        SetPlayerAnim("idle")
    end

    -- 朝向（Kenney alien 正面对称，无需镜像；保留 face 变量供将来扩展）
    local sx = P_SCALE
    dse.ecs.set_transform_scale(level_entities.player, sx, P_SCALE, 1.0)
    dse.ecs.set_transform_position(level_entities.player, player.x, player.y, 0.0)

    UpdateInteractions(dt)

    -- 掉落
    if player.y < level.bounds[2] - 4.0 then
        state.lives = state.lives - 1
        if state.lives <= 0 then
            state.mode = "game_over"
        else
            RespawnLevel()
        end
    end
end

-- ============================================================================
-- HUD
-- ============================================================================
local function UpdateHUD()
    local hud = state.hud
    if hud.coins then
        dse.ui.set_label_text(hud.coins, string.format("Coins  %d/%d", state.coins, state.total_coins))
    end
    if hud.lives then
        dse.ui.set_label_text(hud.lives, string.format("Lives  %d", state.lives))
    end
    if hud.level then
        dse.ui.set_label_text(hud.level, CurrentLevel().name)
    end
    if hud.time then
        dse.ui.set_label_text(hud.time, string.format("Time  %d", math.floor(state.time)))
    end
    if hud.status then
        local msg = ""
        if state.mode == "level_complete" then
            msg = (state.level_index >= #LEVELS) and "ALL LEVELS CLEAR!" or ("LEVEL CLEAR!  ->  " .. LEVELS[state.level_index + 1].name)
        elseif state.mode == "game_over" then
            msg = "GAME OVER - Press R to restart"
        elseif state.mode == "win" then
            msg = "YOU WIN!  Press R to play again"
        end
        dse.ui.set_label_text(hud.status, msg)
    end
    if hud.tip then
        dse.ui.set_label_text(hud.tip, "A/D move   Space jump (hold = higher)   R restart")
    end
end

-- ============================================================================
-- 主流程
-- ============================================================================
local level_complete_timer = 0.0

function Awake()
    LoadAssets()
    state.time = 0.0
    state.lives = START_LIVES
    state.coins = 0
    state.level_index = 1
    state.mode = "play"

    -- 相机
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(cam, 6.0)
    -- 不配置后处理：2D 合成走线性直拷，精灵颜色不失真（不创建 PostProcessComponent）。
    -- 相机控制器：camera_set_bounds 依赖它才生效（否则相机跟随无边界，会露出关卡边缘）。
    dse.ecs.add_camera_controller_2d(cam)
    dse.ecs.camera_set_zoom(cam, 5.0 / 6.0)   -- 控制器默认 target_zoom=1 会把视野缩到 5.0，保持 6.0
    state.cam = cam

    -- HUD：直接用标签绘制（add_label 内部会挂载 UIRenderer）。
    -- 坐标是相对屏幕中心（anchor 默认 (0.5,0.5)）的像素偏移，y 向上。
    -- 注意：不要在此创建全屏 add_renderer —— 它会把最终合成画面整个盖住
    -- （尤其纯白 (1,1,1,1) + 全屏尺寸 = 白屏）。
    local function make_label(text, ox, oy, r, g, b, gw, gh)
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.ui.add_label(e, text, T.font, r, g, b, 1.0, gw, gh, 1.0, 16, 6, 32, ox, oy)
        return e
    end

    state.hud.coins   = make_label("Coins 0/0",    -560.0, 320.0, 1.0, 0.85, 0.3, 18.0, 24.0)
    state.hud.lives   = make_label("Lives 3",      -200.0, 320.0, 1.0, 0.4, 0.4, 18.0, 24.0)
    state.hud.level   = make_label("Level 1",       60.0, 320.0, 0.9, 0.9, 1.0, 18.0, 24.0)
    state.hud.time    = make_label("Time 0",       440.0, 320.0, 1.0, 1.0, 1.0, 18.0, 24.0)
    state.hud.status  = make_label("",               0.0,  60.0, 1.0, 1.0, 0.4, 30.0, 40.0)
    state.hud.tip     = make_label("",               0.0, -330.0, 0.8, 0.8, 0.8, 16.0, 22.0)

    local level = CurrentLevel()
    local n = 0
    for _, c in ipairs(level.coins or {}) do n = n + 1 end
    for _, g in ipairs(level.gems or {}) do n = n + 1 end
    state.total_coins = n

    BuildLevel(level)
    ResetPlayer()

    -- 相机跟随 + 边界（相机先摆到出生点，避免开局从 (0,0) 滑行过去）
    dse.ecs.set_transform_position(cam, player.x, player.y, 0.0)
    dse.ecs.set_camera_follow(cam, level_entities.player, 0.10, 0.0, 0.0, 0.0, 0.0)
    dse.ecs.camera_set_bounds(cam, level.bounds[1], level.bounds[2], level.bounds[3], level.bounds[4])

    if S.bgm then dse.audio.play_bgm(S.bgm, 0.7, true) end
    print("[platformer] ready -- 3 levels, " .. tostring(state.total_coins) .. " pickups total")
end

function Update(dt)
    dt = dt or 0.0
    if dt > 0.05 then dt = 0.05 end

    -- 粒子更新（尘埃）
    for i = #level_entities.particles, 1, -1 do
        local p = level_entities.particles[i]
        p.t = p.t + dt
        p.x = p.x + p.vx * dt
        p.y = p.y + p.vy * dt
        p.vy = p.vy - 10.0 * dt
        if p.e then dse.ecs.set_transform_position(p.e, p.x, p.y, 0.0) end
        if p.t >= p.life then
            if p.e then pcall(dse.ecs.destroy_entity, p.e) end
            table.remove(level_entities.particles, i)
        end
    end

    if state.mode == "play" then
        state.time = state.time + dt
        UpdatePlayer(dt)
        UpdateEnemies(dt)
        UpdateHUD()
    elseif state.mode == "level_complete" then
        level_complete_timer = level_complete_timer + dt
        UpdateHUD()
        if level_complete_timer > 1.4 then
            level_complete_timer = 0.0
            AdvanceLevel()
        end
    else
        -- game_over / win：按 R 重开
        if app.get_key_down(KEY_R) then
            state.level_index = 1
            state.lives = START_LIVES
            RespawnLevel()
            if S.bgm then dse.audio.play_bgm(S.bgm, 0.7, true) end
        end
        UpdateHUD()
    end
end
