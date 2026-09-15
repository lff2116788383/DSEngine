-- ============================================================================
-- assets.lua  资源加载。**加载顺序 = 绘制顺序**（引擎精灵批处理按 texture 分组，
-- order_in_layer 只在同纹理内生效，所以跨纹理层次必须靠加载顺序保证）：
--   1 背景(天空/远景/地面)  2 光晕/阴影  3 道具  4 角色/敌人  5 特效  6 前景遮挡
-- ============================================================================
local core = require("core")

local A = { tex = {}, snd = {}, frames = {}, size = {}, order_log = {} }

local function load_tex(p)
    local path = core.resolve(p)
    local h = dse.assets.load_texture(path)
    if not h or h == 0 then
        print("[assets] MISSING texture: " .. tostring(path))
    end
    A.order_log[#A.order_log + 1] = { path = path, handle = h }
    return h
end
function A.get(p) return load_tex(p) end

local function load_snd(p)
    return core.resolve(p)
end

-- 角色帧表：{ key = { dir = { action = {handle,...} } } }
local function load_actor(key, dirs, anims, fmt)
    A.frames[key] = {}
    for _, d in ipairs(dirs) do
        A.frames[key][d] = {}
        for a, n in pairs(anims) do
            local list = {}
            for i = 0, n - 1 do
                list[#list + 1] = load_tex(string.format(fmt, d, a, i))
            end
            A.frames[key][d][a] = list
        end
    end
end

local function load_npc(key, dirs, n, fmt)
    A.frames[key] = {}
    for _, d in ipairs(dirs) do
        local list = {}
        for i = 0, n - 1 do list[#list + 1] = load_tex(string.format(fmt, d, i)) end
        A.frames[key][d] = { idle = list }
    end
end

local ACTOR_ANIM = { idle = 4, walk = 6, attack = 5, dodge = 4, cast = 5, hurt = 2, die = 5 }
local BANDIT_ANIM = { idle = 3, walk = 4, attack = 3, hurt = 1, die = 4 }
local NPC_ANIM = { idle = 3 }
local WOLF_ANIM = { idle = 3, walk = 6, attack = 3, hurt = 1, die = 4 }
local GHOST_ANIM = { idle = 4, walk = 6, attack = 4, hurt = 1, die = 4 }
local BOSS_ANIM = { idle = 4, walk = 6, attack = 4, hurt = 1, die = 6 }

function A.load(mapdata)
    -- 1) 背景层
    for _, m in ipairs(mapdata) do
        A.tex["bg_" .. m.id] = {
            sky = load_tex("maps/" .. m.bg.sky),
            far = load_tex("maps/" .. m.bg.far),
            ground = load_tex("maps/" .. m.bg.ground),
        }
    end
    -- 2) 光晕 / 阴影
    A.tex.glow_warm = load_tex("fx/glow_warm.png")
    A.tex.glow_cool = load_tex("fx/glow_cool.png")
    A.tex.glow_torch = load_tex("fx/glow_torch.png")
    A.tex.shadow_small = load_tex("fx/shadow_small.png")
    A.tex.shadow_mid = load_tex("fx/shadow_mid.png")
    A.tex.shadow_big = load_tex("fx/shadow_big.png")
    -- 3) 道具
    A.tex.chest = load_tex("ui/icon_armor.png")
    A.tex.pickup_coin = load_tex("ui/icon_coin.png")
    A.tex.pickup_hp = load_tex("ui/icon_hp_potion.png")
    A.tex.pickup_mp = load_tex("ui/icon_mp_potion.png")
    A.tex.quest_marker = load_tex("ui/quest_marker.png")
    -- 4) 角色 / 敌人
    load_actor("hero", { "d", "u", "l", "r" }, ACTOR_ANIM, "char/hero/hero_%s_%s_%d.png")
    load_actor("bandit", { "d", "u", "l", "r" }, BANDIT_ANIM, "enemy/bandit/bandit_%s_%s_%d.png")
    load_actor("boss", { "d", "u", "l", "r" }, BOSS_ANIM, "enemy/boss/boss_%s_%s_%d.png")
    load_actor("wolf", { "l", "r" }, WOLF_ANIM, "enemy/wolf_%s_%s_%d.png")
    load_actor("ghost", { "l", "r" }, GHOST_ANIM, "enemy/ghost_%s_%s_%d.png")
    load_npc("villager", { "d", "u", "r" }, 3, "npc/villager/villager_%s_idle_%d.png")
    load_npc("smith", { "d", "u", "r" }, 3, "npc/smith/smith_%s_idle_%d.png")
    load_npc("elder", { "d", "u", "r" }, 3, "npc/elder/elder_%s_idle_%d.png")
    -- 5) 特效帧
    local function seq(name, n)
        local l = {}
        for i = 0, n - 1 do l[#l + 1] = load_tex(string.format("fx/%s_%d.png", name, i)) end
        A.tex[name] = l
    end
    seq("slash", 5); seq("hit", 4); seq("dust", 4); seq("qi", 5); seq("heal", 5)
    seq("levelup", 6); seq("leaf", 3)
    A.tex.firefly = load_tex("fx/firefly.png")
    -- 6) 前景遮挡层（必须最后加载，保证覆盖角色）
    for _, m in ipairs(mapdata) do
        A.tex["fg_" .. m.id] = load_tex("maps/" .. m.bg.fg)
    end
    -- 尺寸表（美术像素  世界单位）
    A.size = {
        hero = { 40, 52 }, bandit = { 35, 46 }, boss = { 65, 84 },
        wolf = { 51, 34 }, ghost = { 39, 46 },
        villager = { 35, 46 }, smith = { 35, 46 }, elder = { 37, 48 },
    }
    -- 音效
    A.snd.bgm_title = load_snd("audio/bgm_title.wav")
    A.snd.bgm_field = load_snd("audio/bgm_field.wav")
    A.snd.bgm_battle = load_snd("audio/bgm_battle.wav")
    A.snd.bgm_boss = load_snd("audio/bgm_boss.wav")
    for _, k in ipairs({ "slash", "hit", "crit", "die", "dodge", "skill", "heal", "pickup",
                         "coin_use", "levelup", "talk", "ui", "gate" }) do
        A.snd[k] = load_snd("audio/sfx_" .. k .. ".wav")
    end
    print(string.format("[assets] loaded %d textures (%d ordered), audio ready", #A.order_log, #A.order_log))
end

function A.frames_for(key, dir, action)
    local a = A.frames[key]
    if not a then return nil end
    local d = a[dir] or a["r"] or a["d"]
    if not d then return nil end
    return d[action] or d["idle"] or d["walk"]
end

function A.size_of(key)
    local s = A.size[key] or { 32, 48 }
    return s[1], s[2]
end

return A