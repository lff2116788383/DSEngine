-- ============================================================================
-- enemy.lua  敌人 AI / 攻击判定 / 掉落 / Boss 分阶段
-- ============================================================================
local core = require("core")
local A = require("assets")
local D = require("data")
local W = require("world")
local F = require("fx")
local AU = require("audio")
local B = require("bplus")

local E = { list = {}, shots = {} }
local ecs = dse.ecs

local function set_dir(en, dir)
    if en.dir == dir then return end
    en.dir = dir
    if B.enabled then
        B.set_actor_dir(en.ent, dir)
        return
    end
    for _, a in ipairs({ "idle", "walk", "attack", "hurt", "die" }) do
        local fr = A.frames_for(en.kind, dir, a)
        if fr then
            ecs.add_animation_state(en.ent, a, a == "walk" and 9 or (a == "attack" and 12 or 7),
                                    a ~= "attack" and a ~= "hurt" and a ~= "die", fr)
        end
    end
end

local function play(en, name)
    if en.anim == name then return end
    en.anim = name
    if B.enabled then
        local loop = (name == "idle" or name == "walk")
        B.play_actor(en.ent, en.kind, en.dir, name, B.ACTION_FPS[name] or 8.0, loop)
        return
    end
    ecs.play_animation(en.ent, name)
end

function E.spawn(kind, x, y, patrol, map_id)
    local cfg = D.enemy_cfg[kind]
    if not cfg then return nil end
    local dir = A.frames[kind]["d"] and "d" or "r"
    local ent = ecs.create_entity()
    local w, h = cfg.size[1], cfg.size[2]
    ecs.add_transform(ent, x, y + h / 64.0, 0, w / 32, h / 32, 1)
    if B.enabled then
        B.register_actor(ent, kind, dir, "idle", w, h)
    else
        ecs.add_sprite(ent, 1, 1, 1, 1, 200, A.frames_for(kind, dir, "idle")[1])
        ecs.add_animator(ent)
    end
    local en = {
        kind = kind, cfg = cfg, ent = ent, x = x, y = y, home_x = x, home_y = y,
        hp = cfg.hp, hp_max = cfg.hp, dir = dir, anim = "", state = "patrol", t = math.random() * 6.28,
        atk_cd = 0, hurt_t = 0, dead = false, patrol = patrol or 2.0, map_id = map_id,
        w = w, h = h, phase = 1, death_t = 0, drop_done = false,
    }
    set_dir(en, dir)
    play(en, "idle")
    E.list[#E.list + 1] = en
    return en
end

function E.damage(en, dmg, from_x, from_y, crit)
    if en.dead then return end
    local def = en.cfg.def or 0
    local real = math.max(1, math.floor(dmg - def * 0.5))
    if crit then real = math.floor(real * 1.6) end
    en.hp = en.hp - real
    en.hurt_t = 0.18
    F.popup("-" .. real .. (crit and " 暴击" or ""), en.x, en.y + en.h / 32 + 0.4, crit and "crit" or "hurt")
    F.hit(en.x, en.y + en.h / 64.0, 1.0, crit)
    AU.random("hit", 0.6)
    -- 击退
    local dx = core.sign(en.x - (from_x or en.x))
    local dy = core.sign(en.y - (from_y or en.y))
    en.kx, en.ky = dx * 3.2, dy * 1.6
    if en.hp <= 0 then
        en.dead = true
        en.state = "dead"
        en.anim = ""
        play(en, "die")
        AU.random("die", 0.6)
        local P = require("player")
        P.gain_exp(en.cfg.exp)
        P.gain_gold(en.cfg.gold)
        F.popup("+" .. en.cfg.gold .. " 铜钱", en.x, en.y + 1.2, "exp")
        if not en.drop_done then
            en.drop_done = true
            for _, d in ipairs(en.cfg.drops or {}) do
                if math.random() <= d[2] then
                    P.add_item(d[1], 1)
                    F.toast("获得 " .. (D.items[d[1]] and D.items[d[1]].name or d[1]) .. " x1", 1.0, 0.92, 0.6)
                end
            end
        end
        W.shake(0.3)
    end
end

-- 敌人攻击玩家 / 远程弹道
local function try_hit_player(en, P, dmg_scale)
    local d = core.dist(en.x, en.y, P.x, P.y)
    if d <= (en.cfg.atk_range + 0.35) then
        P.damage(en.cfg.atk * (dmg_scale or 1.0), en.x, en.y)
    end
end

function E.spawn_shot(en, target)
    local s = ecs.create_entity()
    ecs.add_transform(s, en.x, en.y + en.h / 64.0, 0, 0.7, 0.7, 1)
    if B.enabled then
        B.register_fx_actor(s, "hit", "hit", 0.7 * 32, 0.7 * 32, 24, false)
    else
        ecs.add_sprite(s, 0.8, 1.0, 1.0, 1.0, 800, A.tex["hit"][1])
    end
    local dx, dy = target.x - en.x, (target.y + 0.6) - (en.y + 0.5)
    local len = math.max(0.001, math.sqrt(dx * dx + dy * dy))
    E.shots[#E.shots + 1] = { e = s, x = en.x, y = en.y + 0.6, vx = dx / len * 6.5, vy = dy / len * 6.5,
                              life = 1.6, dmg = en.cfg.atk }
end

function E.update(dt, P, mode, map_id)
    for i = #E.list, 1, -1 do
        local en = E.list[i]
        if en.map_id == map_id then
            en.atk_cd = math.max(0, en.atk_cd - dt)
            if en.hurt_t > 0 then en.hurt_t = en.hurt_t - dt end
            -- 击退位移
            if en.kx or en.ky then
                en.x, en.y = W.move(en.x, en.y, en.w / 64, en.h / 64, (en.kx or 0) * dt, (en.ky or 0) * dt)
                en.kx = (en.kx or 0) * 0.82
                en.ky = (en.ky or 0) * 0.82
                if math.abs(en.kx) < 0.05 then en.kx = nil end
                if math.abs(en.ky) < 0.05 then en.ky = nil end
            end
            if en.dead then
                en.death_t = en.death_t + dt
                if en.death_t > 1.4 then
                    pcall(ecs.destroy_entity, en.ent)
                    table.remove(E.list, i)
                end
            elseif en.hurt_t > 0 then
                play(en, "hurt")
            else
                local d = core.dist(en.x, en.y, P.x, P.y)
                local see = d <= en.cfg.sight and W.line_clear(en.x, en.y + 0.5, P.x, P.y + 0.6)
                en.t = en.t + dt
                if en.state == "patrol" then
                    local tx = en.home_x + math.sin(en.t * 0.7) * en.patrol
                    local ty = en.home_y
                    set_dir(en, tx > en.x and "r" or "l")
                    en.x, en.y = W.move(en.x, en.y, en.w / 64, en.h / 64, (tx - en.x) * dt * 1.6, (ty - en.y) * dt * 1.6)
                    play(en, "walk")
                    if see and not P.dead then en.state = "chase" end
                elseif en.state == "chase" then
                    if P.dead then en.state = "patrol" end
                    local dx, dy = P.x - en.x, P.y - en.y
                    set_dir(en, core.dir_from_vec(dx, dy))
                    local sp = en.cfg.speed * (en.cfg.boss and (en.hp < en.hp_max * 0.5 and 1.35 or 1.0) or 1.0)
                    en.x, en.y = W.move(en.x, en.y, en.w / 64, en.h / 64,
                                        core.sign(dx) * sp * dt, core.sign(dy) * sp * dt)
                    play(en, "walk")
                    if d <= en.cfg.atk_range and en.atk_cd <= 0 then
                        en.state = "attack"
                        en.t = 0
                        en.hit_done = false
                        play(en, "attack")
                        if en.cfg.ranged then E.spawn_shot(en, P) end
                    elseif d > en.cfg.sight * 1.8 then
                        en.state = "patrol"
                    end
                elseif en.state == "attack" then
                    en.t = en.t + dt
                    if en.t > 0.35 and not en.hit_done then
                        en.hit_done = true
                        if not en.cfg.ranged then try_hit_player(en, P, 1.0) end
                    end
                    if en.t > 0.75 then
                        en.state = "chase"
                        en.atk_cd = en.cfg.atk_cd
                    end
                end
            end
            -- Boss 半血狂暴
            if en.cfg.boss and en.phase == 1 and en.hp <= en.hp_max * 0.5 then
                en.phase = 2
                F.toast("寨主血刀 暴怒！", 1.0, 0.4, 0.3)
                F.qi(en.x, en.y + 0.5, nil)
                W.shake(0.6)
                en.cfg.atk_cd = en.cfg.atk_cd * 0.7
            end
            ecs.set_transform_position(en.ent, en.x, en.y + en.h / 64.0, 0)
            local sx = (en.dir == "l") and -en.w / 32 or en.w / 32
            ecs.set_transform_scale(en.ent, sx, en.h / 32, 1)
        end
    end
    -- 弹道
    for i = #E.shots, 1, -1 do
        local s = E.shots[i]
        s.life = s.life - dt
        s.x, s.y = s.x + s.vx * dt, s.y + s.vy * dt
        ecs.set_transform_position(s.e, s.x, s.y, 0)
        if core.dist(s.x, s.y, P.x, P.y + 0.5) < 0.6 then
            P.damage(s.dmg, s.x, s.y)
            s.life = 0
        end
        if s.life <= 0 or W.solid(s.x, s.y) then
            F.spawn("hit", s.x, s.y, { scale = 0.8, life = 0.25 })
            pcall(ecs.destroy_entity, s.e)
            table.remove(E.shots, i)
        end
    end
end

function E.clear_map(map_id)
    for i = #E.list, 1, -1 do
        if E.list[i].map_id == map_id then
            pcall(ecs.destroy_entity, E.list[i].ent)
            table.remove(E.list, i)
        end
    end
end

function E.clear()
    for _, en in ipairs(E.list) do pcall(ecs.destroy_entity, en.ent) end
    E.list = {}
    for _, s in ipairs(E.shots) do pcall(ecs.destroy_entity, s.e) end
    E.shots = {}
end

function E.count(map_id)
    local n = 0
    for _, en in ipairs(E.list) do
        if en.map_id == map_id and not en.dead then n = n + 1 end
    end
    return n
end

return E