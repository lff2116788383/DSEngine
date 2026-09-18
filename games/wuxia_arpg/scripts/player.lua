-- New game player: 3D lit Sprite3D hero, movement, collision, combo-like attack, dodge, growth.
local core = require("core")
local A = require("assets")
local D = require("data")
local Fx = require("fx")
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
P.speed = 4.2
P.attack_cd, P.attack_t = 0.0, -1.0
P.dodge_t, P.invuln = -1.0, 0.0
P.state, P.anim_dir, P.anim_action = "idle", "d", "idle"
P.ent = nil
P.hw, P.hh = 0.34, 0.28
local attack_total, attack_active, attack_range, attack_arc = 0.38, 0.16, 1.35, 130

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

function P.stats()
    return P.atk, P.def
end

function P.add_equipment(item)
    if not item then return end
    P.item_seq = P.item_seq + 1
    item.uid = P.item_seq
    P.items[#P.items+1] = item
    Fx.toast("获得 " .. item.name .. "（" .. require("loot").rarity(item.rarity) .. "）")
    core.accept_log("drop uid=%d name=%s rarity=%s affixes=%d", item.uid, item.name, item.rarity, #(item.affixes or {}))
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
        P.hp, P.mp = P.hp_max, P.mp_max
        Fx.toast("升级！等级 " .. P.level)
        A.sfx("levelup", 0.7)
        core.accept_log("levelup level=%d", P.level)
    end
    core.accept_log("exp+%d level=%d", v, P.level)
end

function P.damage(amount, from_x, from_z)
    if P.invuln > 0 or P.dodge_t >= 0 then return end
    local dmg = math.max(1, math.floor(amount - P.def * 0.5))
    P.hp = P.hp - dmg
    Fx.popup("-"..dmg, P.x, P.z)
    if P.hp <= 0 then P.hp = 0; P.state = "dead"; P.play("die", true) end
end

function P.update(dt)
    if P.attack_cd > 0 then P.attack_cd = P.attack_cd - dt end
    if P.invuln > 0 then P.invuln = P.invuln - dt end
    if P.state == "dead" then
        ecs.set_transform_position(P.ent, P.x, 0, P.z)
        return
    end
    local ix, iz = 0, 0
    if os.getenv("DSE_WUXIA_DEMO") == "1" and Fx.demo_target then
        local tx, tz = Fx.demo_target()
        local dx, dz = tx - P.x, tz - P.z
        local d = math.max(0.001, math.sqrt(dx*dx+dz*dz))
        ix, iz = dx/d, dz/d
        if d < 1.0 and P.attack_cd <= 0 then
            P.attack_t = 0; P.attack_cd = 0.52; P.attack_hit = true
            P.set_dir(core.dir_from_vec(dx,dz)); A.sfx("slash",0.45)
        end
    else
        ix, iz = core.axis_x(), core.axis_z()
    end
    local len = math.sqrt(ix*ix+iz*iz)
    if len > 0 then
        ix, iz = ix/len, iz/len
        P.set_dir(core.dir_from_vec(ix,iz))
    end
    local speed = P.speed
    P.x, P.z = move(P.x, P.z, ix*speed*dt, iz*speed*dt)
    -- attack timer
    if P.attack_t >= 0 then
        P.attack_t = P.attack_t + dt
        if P.attack_t >= attack_total then P.attack_t = -1 end
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

function P.try_attack()
    if P.state=="dead" or P.attack_cd > 0 or P.dodge_t >= 0 then return end
    P.attack_t, P.attack_cd, P.attack_hit = 0, 0.52, true
    P.play("attack", true)
    A.sfx("slash", 0.5)
end

function P.try_dodge()
    if P.state=="dead" or P.dodge_t >= 0 or P.attack_cd > 0.5 then return end
    P.dodge_t, P.invuln = 0, 0.5
    P.play("dodge", true)
    A.sfx("click", 0.35)
end

return P