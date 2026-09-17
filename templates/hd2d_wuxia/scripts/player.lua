-- ============================================================================
-- player.lua  主角：八向移动 / 三段连招 / 闪避无敌帧 / 技能 / 背包 / 成长
-- ============================================================================
local core = require("core")
local A = require("assets")
local D = require("data")
local W = require("world")
local F = require("fx")
local B = require("bplus")

local P = {}

local ecs = dse.ecs
local KEY = core.KEY

P.ent = nil
P.x, P.y, P.vx, P.vy = 0, 0, 0, 0
P.dir = "d"
P.hp, P.hp_max = 130, 130
P.mp, P.mp_max = 60, 60
P.stam, P.stam_max = 100, 100
P.atk, P.def = 12, 5
P.level, P.exp, P.exp_next = 1, 0, 45
P.gold = 0
P.inv = { hp_potion = 3, mp_potion = 2 }
P.equip = { iron_sword = false, leather_armor = false }
P.skill_unlocked = { fenhua = true, zixia = true }
P.cd = { fenhua = 0, zixia = 0 }
P.attack = { t = -1, combo = 1, active_hit = false }
P.dodge = { t = -1, invuln = 0 }
P.hurt_t, P.invuln, P.buff_t = 0, 0, 0
P.stam_delay = 0
P.dead = false
P.anim = ""
P.pending_hit = nil
P.step_t = 0
P.hw, P.hh = 0.40, 0.30

local SPEED = 4.4
local SPRINT = 6.8
local DODGE_TIME = 0.30
local DODGE_DIST = 3.4
local INVULN_HIT = 0.65

function P.setup(dir)
    P.dir = dir
    if B.enabled then
        B.set_actor_dir(P.ent, dir)
        P.play("idle", true)
        return
    end
    for _, a in ipairs({ "idle", "walk", "attack", "dodge", "cast", "hurt", "die" }) do
        local fr = A.frames_for("hero", dir, a)
        if fr then ecs.add_animation_state(P.ent, a, a == "walk" and 10 or (a == "attack" and 16 or 8),
                                           a ~= "attack" and a ~= "hurt" and a ~= "die", fr) end
    end
    if dir == "l" or dir == "r" then
        ecs.add_animation_state(P.ent, "cast", 10, false, A.frames_for("hero", dir, "cast"))
    end
    P.play("idle")
end

function P.play(name, force)
    if P.anim == name and not force then return end
    P.anim = name
    if B.enabled then
        local loop = (name == "idle" or name == "walk" or name == "cast")
        B.play_actor(P.ent, "hero", P.dir, name, B.ACTION_FPS[name] or 8.0, loop)
        return
    end
    ecs.play_animation(P.ent, name)
end

function P.spawn(map)
    P.dead = false
    P.attack.t = -1
    P.dodge.t = -1
    P.invuln = 0
    P.hurt_t = 0
    P.buff_t = 0
    P.hp = math.max(1, math.floor(P.hp_max * 0.6))
    P.mp = P.mp_max
    P.stam = P.stam_max
    P.x, P.y = map.spawn.x, map.spawn.y
    if not P.ent then
        P.ent = ecs.create_entity()
        ecs.add_transform(P.ent, P.x, P.y, 0, 40 / 32, 52 / 32, 1)
        if B.enabled then
            B.register_actor(P.ent, "hero", P.dir, "idle", 40, 52)
        else
            ecs.add_sprite(P.ent, 1, 1, 1, 1, 100, A.frames_for("hero", P.dir, "idle")[1])
            ecs.add_animator(P.ent)
            P.setup(P.dir)
        end
        W.register(P.ent)
    end
    local _, h = A.size_of("hero")
    ecs.set_transform_position(P.ent, P.x, P.y + h / 64.0, 0)
    ecs.set_transform_scale(P.ent, 40 / 32, 52 / 32, 1)
    return P.ent
end

function P.stats()
    local atk = P.atk + (P.equip.iron_sword and 6 or 0)
    local def = P.def + (P.equip.leather_armor and 4 or 0)
    if P.buff_t > 0 then atk = math.floor(atk * 1.35) end
    return atk, def
end

function P.add_item(kind, n)
    n = n or 1
    if D.items[kind] and D.items[kind].quest then
        P.inv[kind] = (P.inv[kind] or 0) + n
        return
    end
    P.inv[kind] = (P.inv[kind] or 0) + n
end

function P.gain_gold(v) P.gold = P.gold + v end

function P.gain_exp(v)
    P.exp = P.exp + v
    F.popup("+" .. v .. " 经验", P.x, P.y + 1.6, "exp")
    while P.exp >= P.exp_next do
        P.exp = P.exp - P.exp_next
        P.level = P.level + 1
        P.exp_next = math.floor(P.exp_next * 1.45)
        P.hp_max = P.hp_max + 16
        P.mp_max = P.mp_max + 8
        P.atk = P.atk + 3
        P.def = P.def + 2
        P.hp, P.mp = P.hp_max, P.mp_max
        F.levelup(P.x, P.y)
        F.popup("升级！等级 " .. P.level, P.x, P.y + 2.4, "info")
        dse.audio.play_sfx(A.snd.levelup, 0.85, 0)
        W.shake(0.35)
    end
end

function P.use_item(kind)
    local it = D.items[kind]
    if not it or (P.inv[kind] or 0) <= 0 then return false end
    if kind == "hp_potion" then
        if P.hp >= P.hp_max then return false end
        P.hp = core.clamp(P.hp + it.heal, 0, P.hp_max)
        F.popup("+" .. it.heal, P.x, P.y + 1.8, "heal")
        F.heal(P.x, P.y)
        dse.audio.play_sfx(A.snd.coin_use, 0.6, 0)
    elseif kind == "mp_potion" then
        if P.mp >= P.mp_max then return false end
        P.mp = core.clamp(P.mp + it.mana, 0, P.mp_max)
        F.popup("+" .. it.mana, P.x, P.y + 1.8, "mp")
        dse.audio.play_sfx(A.snd.coin_use, 0.6, 0)
    else
        return false
    end
    P.inv[kind] = P.inv[kind] - 1
    return true
end

function P.damage(amount, from_x, from_y)
    if P.dead or P.invuln > 0 or P.dodge.t >= 0 then return false end
    local _, def = P.stats()
    local dmg = math.max(1, math.floor(amount - def * 0.6))
    P.hp = P.hp - dmg
    P.hurt_t = 0.28
    P.invuln = INVULN_HIT
    F.popup("-" .. dmg, P.x, P.y + 1.7, "hurt")
    F.hit(P.x, P.y + 0.6, 1.0, false)
    dse.audio.play_sfx(A.snd.hit, 0.75, 0)
    local dx = core.sign(P.x - (from_x or P.x))
    local dy = core.sign(P.y - (from_y or P.y))
    if dx == 0 and dy == 0 then dx = -1 end
    P.vx, P.vy = dx * 7.0, dy * 4.0
    W.shake(0.45)
    if P.hp <= 0 then
        P.hp = 0
        P.dead = true
        P.play("die", true)
    end
    return true
end

function P.heal(v)
    P.hp = core.clamp(P.hp + v, 0, P.hp_max)
end

-- 技能：分花拂柳（AoE） / 紫霞真气（治疗+增益）
function P.cast_skill(id)
    local sk
    for _, s in ipairs(D.skills) do if s.id == id then sk = s end end
    if not sk or P.dead then return end
    if P.cd[id] and P.cd[id] > 0 then
        F.toast("冷却中：" .. sk.name, 1.0, 0.7, 0.4)
        dse.audio.play_sfx(A.snd.ui, 0.4, 0)
        return
    end
    if P.mp < sk.mp then
        F.toast("内力不足", 0.6, 0.8, 1.0)
        dse.audio.play_sfx(A.snd.ui, 0.4, 0)
        return
    end
    P.mp = P.mp - sk.mp
    P.cd[id] = sk.cd
    if id == "fenhua" then
        P.pending_hit = { id = id, dmg = sk.dmg + P.atk, range = sk.range, arc = 360, kb = sk.kb,
                          crit = false, hitstop = sk.hitstop }
        F.spawn("qi", P.x, P.y + 0.5, { scale = 2.4, life = 0.5, fps = 18, r = 0.7, g = 1.0, b = 1.0 })
        F.slash(P.x, P.y + 0.5, 0, 0, true)
        P.play("cast", true)
        P.attack.t = 0
        dse.audio.play_sfx(A.snd.skill, 0.8, 0)
        W.shake(0.4)
    elseif id == "zixia" then
        P.heal(math.floor(P.hp_max * sk.heal))
        P.buff_t = sk.buff_time
        F.heal(P.x, P.y)
        F.popup("紫霞真气", P.x, P.y + 2.2, "heal")
        P.play("cast", true)
        P.attack.t = 0
        dse.audio.play_sfx(A.snd.heal, 0.8, 0)
    end
end

function P.update(dt, mode)
    if not P.ent then return end
    -- 计时器
    for k, v in pairs(P.cd) do if v > 0 then P.cd[k] = math.max(0, v - dt) end end
    if P.invuln > 0 then P.invuln = P.invuln - dt end
    if P.buff_t > 0 then P.buff_t = P.buff_t - dt end
    if P.hurt_t > 0 then P.hurt_t = P.hurt_t - dt end
    if P.stam_delay > 0 then P.stam_delay = P.stam_delay - dt end

    local ix, iy = core.axis_x(), core.axis_y()
    local moving = (ix ~= 0 or iy ~= 0)

    if P.dead then
        P.x, P.y = W.move(P.x, P.y, P.hw, P.hh, P.vx * dt, P.vy * dt)
        P.vx, P.vy = P.vx * 0.86, P.vy * 0.86
        ecs.set_transform_position(P.ent, P.x, P.y + 52 / 64.0, 0)
        return
    end

    -- 攻击动作推进
    if P.attack.t >= 0 then
        P.attack.t = P.attack.t + dt
        local windup, active, total = 0.10, 0.16, 0.34
        if P.attack.t >= windup and not P.attack.hit_done then
            P.attack.hit_done = true
            local sk = D.skills[P.attack.combo]
            P.pending_hit = { id = sk.id, dmg = sk.dmg + P.atk, range = sk.range, arc = sk.arc,
                              kb = sk.kb, crit = math.random() < 0.18, hitstop = sk.hitstop,
                              dir = P.dir }
            local fx_, fy_ = P.x, P.y + 0.55
            local dx, dy = 0, 0
            if P.dir == "d" then dy = -0.35 elseif P.dir == "u" then dy = 0.35
            elseif P.dir == "l" then dx = -0.5 else dx = 0.5 end
            F.slash(P.x + dx, P.y + 0.55 + dy, dx, dy, P.attack.combo == 3)
            dse.audio.play_sfx(A.snd.slash, 0.55, 0)
        end
        if P.attack.t >= total then
            P.attack.t = -1
            P.attack.hit_done = false
        end
    end

    -- 闪避
    if P.dodge.t >= 0 then
        P.dodge.t = P.dodge.t + dt
        local k = core.clamp(P.dodge.t / DODGE_TIME, 0, 1)
        local sp = DODGE_DIST / DODGE_TIME * (1.0 - k * 0.65)
        local dx, dy = 0, 0
        if P.dir == "d" then dy = -sp elseif P.dir == "u" then dy = sp
        elseif P.dir == "l" then dx = -sp else dx = sp end
        P.x, P.y = W.move(P.x, P.y, P.hw, P.hh, dx * dt, dy * dt)
        if P.dodge.t >= DODGE_TIME then P.dodge.t = -1 end
    else
        -- 普通移动
        local sp = SPEED
        if core.key(340) or core.key(KEY.H) then sp = SPRINT end
        local len = math.sqrt(ix * ix + iy * iy)
        if len > 0 then ix, iy = ix / len, iy / len end
        P.vx = core.lerp(P.vx, ix * sp, 1 - math.exp(-14 * dt))
        P.vy = core.lerp(P.vy, iy * sp, 1 - math.exp(-14 * dt))
        P.x, P.y = W.move(P.x, P.y, P.hw, P.hh, P.vx * dt, P.vy * dt)
        if moving then P.set_dir(core.dir_from_vec(ix, iy)) end
        -- 走路上限（避免 vx 残留）
        if not moving then P.vx, P.vy = P.vx * 0.72, P.vy * 0.72 end
    end

    -- 输入动作
    if mode == "play" then
        if P.attack.t < 0 and P.dodge.t < 0 then
            if (core.key_down(KEY.J) or core.demo_attack) then
                P.attack.combo = (P.attack.combo % 3) + 1
                P.attack.t = 0
                P.attack.hit_done = false
                P.play("attack", true)
            end
            if (core.key_down(KEY.K) or core.demo_dodge) and P.stam >= 20 then
                P.stam = P.stam - 20
                P.stam_delay = 0.7
                P.dodge.t = 0
                P.play("dodge", true)
                dse.audio.play_sfx(A.snd.dodge, 0.6, 0)
                F.dust(P.x, P.y, 1.2)
            end
            if core.key_down(KEY.U) then P.cast_skill("fenhua") end
            if core.key_down(KEY.I) then P.cast_skill("zixia") end
            if core.key_down(KEY.ONE) then P.use_item("hp_potion") end
            if core.key_down(KEY.TWO) then P.use_item("mp_potion") end
        end
    end

    -- 体力回复
    if P.stam_delay <= 0 then P.stam = math.min(P.stam_max, P.stam + 26 * dt) end

    -- 动画
    if P.hurt_t > 0 then P.play("hurt", true)
    elseif P.attack.t >= 0 and P.anim ~= "cast" then P.play("attack")
    elseif P.dodge.t >= 0 then P.play("dodge")
    elseif P.anim == "cast" and P.attack.t >= 0 then
    elseif math.abs(P.vx) + math.abs(P.vy) > 0.6 then P.play("walk")
    else P.play("idle") end

    -- 脚步灰尘
    if (math.abs(P.vx) + math.abs(P.vy)) > 3.0 then
        P.step_t = P.step_t + dt
        if P.step_t > 0.26 then
            P.step_t = 0
            F.dust(P.x, P.y, 0.7)
        end
    end

    ecs.set_transform_position(P.ent, P.x, P.y + 52 / 64.0, 0)
    local sx = (P.dir == "l") and -(40 / 32) or (40 / 32)
    ecs.set_transform_scale(P.ent, sx, 52 / 32, 1)
end

function P.set_dir(dir)
    if P.dir == dir then return end
    P.dir = dir
    if B.enabled then
        B.set_actor_dir(P.ent, dir)
        local a = P.anim
        P.anim = ""
        P.play(a == "" and "idle" or a, true)
        return
    end
    local a = P.anim
    P.anim = ""
    P.setup(dir)
    P.play(a == "" and "idle" or a, true)
end

return P