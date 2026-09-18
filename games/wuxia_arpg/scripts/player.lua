-- New game player (R4): combo, skills, equipment stats, deterministic growth.
local core = require("core")
local A = require("assets")
local D = require("data")
local Fx = require("fx")
local Loot = require("loot")
local P = {}
local ecs = dse.ecs
P.x, P.z = D.spawn.x, D.spawn.z
P.dir = "d"
P.hp_max, P.hp = 120, 120
P.mp_max, P.mp = 60, 60
P.atk, P.def = 10, 3
P.level, P.exp, P.exp_next = 1, 0, 40
P.gold = 0
P.items, P.item_seq = {}, 0
P.equip = { weapon = nil, armor = nil, accessory = nil }
P.combo, P.combo_t = 0, 0.0
P.speed = 4.2
P.attack_cd, P.attack_t = 0.0, -1.0
P.dodge_t, P.invuln = -1.0, 0.0
P.state, P.anim_dir, P.anim_action = "idle", "d", "idle"
P.ent = nil
P.hw, P.hh = 0.34, 0.28
P.buff_t = 0.0
P.skill_cd = { fenhua = 0.0, zixia = 0.0 }
P.skill_hit = nil

local function box_solid(x, z)
    local col, row = math.floor(x) + 1, math.floor(z) + 1
    if row < 1 or row > D.h or col < 1 or col > D.w then return true end
    return D.solid[row][col] == true
end
local function move(x, z, dx, dz)
    local nx, nz = x + dx, z + dz
    if not box_solid(nx + core.sign(dx) * P.hw, z + core.sign(dz) * P.hh) then x = nx end
    if not box_solid(x, nz + core.sign(dz) * P.hh) then z = nz end
    return x, z
end
function P.play(action, force)
    if P.anim_action == action and not force then return end
    P.anim_action, P.anim_dir = action, P.dir
    local fps = ({ idle=4.0, walk=8.0, attack=12.0, dodge=10.0, hurt=4.0, die=8.0 })[action] or 8.0
    A.play(P.ent, "hero", P.dir, action, fps, action=="idle" or action=="walk")
end
function P.set_dir(dir)
    if P.dir == dir then return end
    P.dir = dir
    P.play(P.anim_action, true)
end
function P.stats()
    local s = { atk = P.atk, def = P.def, hp_max = P.hp_max, mp_max = P.mp_max,
                crit = 0.02, move = 0.0, leech = 0.0, luck = 0.0 }
    for _,item in pairs(P.equip) do
        if item and item.stats then
            s.atk = s.atk + (item.stats.atk or 0)
            s.def = s.def + (item.stats.def or 0)
            s.hp_max = s.hp_max + (item.stats.hp or 0)
            s.mp_max = s.mp_max + (item.stats.mp or 0)
            s.crit = s.crit + (item.stats.crit or 0) / 100.0
            s.move = s.move + (item.stats.move or 0) / 100.0
            s.leech = s.leech + (item.stats.leech or 0) / 100.0
            s.luck = s.luck + (item.stats.luck or 0) / 100.0
        end
    end
    return s
end
function P.recalc()
    local s = P.stats()
    P.hp = math.min(P.hp, s.hp_max)
    P.mp = math.min(P.mp, s.mp_max)
end
function P.equip_item(item)
    if not item or not item.slot then return false end
    P.equip[item.slot] = item
    P.recalc()
    core.accept_log("equip slot=%s name=%s power=%.1f atk=%d def=%d hp=%d",
        item.slot, item.name, Loot.power(item), item.stats.atk or 0, item.stats.def or 0, item.stats.hp or 0)
    return true
end
function P.auto_equip(item)
    local current = P.equip[item.slot]
    if not current or Loot.power(item) >= Loot.power(current) then
        P.equip_item(item)
        Fx.toast("装备 " .. item.name)
        return true
    end
    core.accept_log("keep slot=%s name=%s power=%.1f", item.slot, item.name, Loot.power(item))
    return false
end
function P.spawn()
    P.ent = ecs.create_entity()
    ecs.add_transform(P.ent, P.x, 0, P.z, 1, 1, 1)
    ecs.add_sprite3d(P.ent, 0, 1.0, 1.5, {billboard="yaw",anchor=0.0,lit=true,receive_shadow=true})
    ecs.set_sprite3d_lit(P.ent, true)
    ecs.set_sprite3d_receive_shadow(P.ent, true)
    ecs.set_sprite3d_contact_shadow(P.ent, true, 0.45, 0.45)
    P.play("idle", true)
    ecs.set_transform_position(P.ent, P.x, 0, P.z)
end
function P.add_equipment(item)
    if not item then return end
    P.item_seq = P.item_seq + 1
    item.uid = P.item_seq
    P.items[#P.items+1] = item
    Fx.toast("获得 " .. item.name .. "（" .. Loot.rarity(item.rarity) .. "）")
    core.accept_log("drop uid=%d name=%s rarity=%s affixes=%d", item.uid, item.name, item.rarity, #(item.affixes or {}))
    P.auto_equip(item)
end
function P.gain_gold(v)
    P.gold = P.gold + v
    core.accept_log("gold+%d total=%d", v, P.gold)
end
function P.gain_exp(v)
    P.exp = P.exp + v
    while P.exp >= P.exp_next do
        P.exp = P.exp - P.exp_next
        P.level = P.level + 1
        P.exp_next = math.floor(P.exp_next * 1.45)
        P.hp_max = P.hp_max + 14; P.mp_max = P.mp_max + 7
        P.atk = P.atk + 3; P.def = P.def + 2
        local s = P.stats(); P.hp, P.mp = s.hp_max, s.mp_max
        Fx.toast("升级！等级 " .. P.level)
        A.sfx("levelup", 0.7)
        core.accept_log("levelup level=%d", P.level)
    end
    core.accept_log("exp+%d level=%d", v, P.level)
end
function P.damage(amount, from_x, from_z)
    if P.invuln > 0 or P.dodge_t >= 0 then return end
    local s = P.stats()
    local dmg = math.max(1, math.floor(amount - s.def * 0.5))
    P.hp = P.hp - dmg
    Fx.popup("-"..dmg, P.x, P.z)
    if P.hp <= 0 then P.hp = 0; P.state = "dead"; P.play("die", true) end
end
function P.try_attack()
    if P.state == "dead" or P.dodge_t >= 0 then return end
    if P.attack_cd > 0 then return end
    if P.combo_t > 0 and P.combo < 3 then P.combo = P.combo + 1 else P.combo = 1 end
    P.combo_t = 0.62
    local par = ({ {dmg=8, range=1.25, arc=130}, {dmg=11, range=1.35, arc=140}, {dmg=18, range=1.65, arc=180} })[P.combo]
    P.attack_cd = 0.42
    P.attack_t = 0
    P.attack_hit = true
    P.attack_dmg = par.dmg + P.stats().atk
    P.attack_range = par.range
    P.attack_arc = par.arc
    P.attack_crit = P.combo == 3 or math.random() < P.stats().crit
    P.play("attack", true)
    A.sfx("slash", 0.5)
    core.accept_log("combo=%d dmg=%d", P.combo, P.attack_dmg)
end
function P.try_skill(id)
    if P.state == "dead" or P.dodge_t >= 0 then return false end
    if P.skill_cd[id] and P.skill_cd[id] > 0 then return false end
    if id == "fenhua" then
        if P.mp < 18 then return false end
        P.mp = P.mp - 18; P.skill_cd.fenhua = 4.0
        P.skill_hit = { kind="fenhua", radius=3.4, dmg=math.floor(P.stats().atk * 1.6 + 15) }
        A.sfx("slash", 0.7)
        core.accept_log("skill=fenhua dmg=%d", P.skill_hit.dmg)
        return true
    elseif id == "zixia" then
        if P.mp < 22 then return false end
        P.mp = P.mp - 22; P.skill_cd.zixia = 10.0
        local s = P.stats()
        P.hp = math.min(s.hp_max, P.hp + math.floor(s.hp_max * 0.35))
        P.buff_t = 8.0
        A.sfx("levelup", 0.6)
        core.accept_log("skill=zixia hp=%d buff=1.35", P.hp)
        return true
    end
    return false
end
function P.update(dt)
    if P.attack_cd > 0 then P.attack_cd = P.attack_cd - dt end
    if P.combo_t > 0 then P.combo_t = P.combo_t - dt end
    if P.skill_cd.fenhua > 0 then P.skill_cd.fenhua = P.skill_cd.fenhua - dt end
    if P.skill_cd.zixia > 0 then P.skill_cd.zixia = P.skill_cd.zixia - dt end
    if P.invuln > 0 then P.invuln = P.invuln - dt end
    if P.buff_t > 0 then P.buff_t = P.buff_t - dt end
    if P.state == "dead" then
        ecs.set_transform_position(P.ent, P.x, 0, P.z)
        return
    end
    local ix, iz = 0, 0
    if os.getenv("DSE_WUXIA_DEMO") == "1" and Fx.demo_target then
        local tx, tz = Fx.demo_target()
        if tx then
            local dx, dz = tx - P.x, tz - P.z
            local d = math.max(0.001, math.sqrt(dx*dx+dz*dz))
            ix, iz = dx/d, dz/d
            if d < 1.05 then
                if P.hp < P.stats().hp_max * 0.45 and P.try_skill("zixia") then
                elseif Fx.demo_aoe and P.try_skill("fenhua") then
                else P.try_attack() end
            end
        end
    else
        ix, iz = core.axis_x(), core.axis_z()
    end
    local len = math.sqrt(ix*ix+iz*iz)
    if len > 0 then
        ix, iz = ix/len, iz/len
        P.set_dir(core.dir_from_vec(ix,iz))
    end
    local speed = P.speed * (1.0 + P.stats().move)
    P.x, P.z = move(P.x, P.z, ix*speed*dt, iz*speed*dt)
    if P.attack_t >= 0 then
        P.attack_t = P.attack_t + dt
        if P.attack_t >= 0.38 then P.attack_t = -1 end
    elseif P.dodge_t < 0 and len > 0 then
        P.play("walk")
    elseif P.dodge_t < 0 then
        P.play("idle")
    end
    if P.dodge_t >= 0 then
        P.dodge_t = P.dodge_t + dt
        local dx,dz = 0,0
        if P.dir=="u" then dz=-1 elseif P.dir=="d" then dz=1 elseif P.dir=="l" then dx=-1 else dx=1 end
        P.x, P.z = move(P.x, P.z, dx*7.0*dt, dz*7.0*dt)
        if P.dodge_t > 0.25 then P.dodge_t = -1 P.invuln = 0.35 end
    end
    ecs.set_transform_position(P.ent, P.x, 0, P.z)
end
function P.try_dodge()
    if P.state=="dead" or P.dodge_t >= 0 or P.attack_cd > 0.5 then return end
    P.dodge_t, P.invuln = 0, 0.5
    P.play("dodge", true)
    A.sfx("click", 0.35)
end
return P