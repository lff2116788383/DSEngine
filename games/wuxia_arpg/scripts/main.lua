-- New HD-2D wuxia ARPG R4: combat, skills, equipment, elite/champion/boss.
local core = require("core")
local A = require("assets")
local D = require("data")
local Terrain = require("terrain")
local P = require("player")
local E = require("enemy")
local Fx = require("fx")
local UI = require("ui")
local Loot = require("loot")
local Save = require("save")
local ecs = dse.ecs
local G = { t=0.0, seed=20260918, autosaved=false, cam=nil, dead_t=0, inventory_open=false, inv_sel=1 }

local function setup_camera()
    G.cam = ecs.create_entity()
    ecs.add_transform(G.cam, P.x, 6.2, P.z + 6.2, 1, 1, 1)
    ecs.set_transform_rotation(G.cam, -48.0, 0.0, 0.0)
    ecs.add_camera_3d(G.cam, 38.0, 0)
    ecs.set_post_process_bloom_enabled(G.cam, true)
    ecs.set_post_process_bloom_threshold(G.cam, 0.82)
    ecs.set_post_process_bloom_intensity(G.cam, 0.9)
    ecs.set_post_process_fxaa_enabled(G.cam, true)
    ecs.set_post_process_vignette_enabled(G.cam, true)
    ecs.set_post_process_vignette_intensity(G.cam, 0.32)
    ecs.set_post_process_exposure(G.cam, 1.05)
    ecs.set_post_process_tilt_shift(G.cam, true, 3.5, 1.6, 6.0)
end
local function nearest_enemy()
    local best, bd = nil, 1e9
    for _,e in ipairs(E.list) do
        if not e.dead then
            local d = core.dist(P.x,P.z,e.x,e.z)
            if d < bd then bd, best = d, e end
        end
    end
    if best then return best.x, best.z, best end
    return nil
end
local function resolve_hit()
    if not P.attack_hit then return end
    P.attack_hit = nil
    local targets = 0
    for _,e in ipairs(E.list) do
        if not e.dead then
            local d = core.dist(P.x,P.z,e.x,e.z)
            if d <= P.attack_range then
                local dx,dz = e.x-P.x, e.z-P.z
                local dirs = { d={0,1}, u={0,-1}, l={-1,0}, r={1,0} }
                local f = dirs[P.dir] or dirs.d
                if P.attack_arc >= 360 or dx*f[1] + dz*f[2] > -0.15 then
                    E.damage(e, P.attack_dmg, P.attack_crit)
                    targets = targets + 1
                end
            end
        end
    end
    core.accept_log("attack combo=%d dir=%s targets=%d dmg=%d", P.combo, P.dir, targets, P.attack_dmg)
end
local function resolve_skill()
    if not P.skill_hit then return end
    local sk = P.skill_hit
    P.skill_hit = nil
    if sk.kind == "fenhua" then
        local n = 0
        for _,e in ipairs(E.list) do
            if not e.dead and core.dist(P.x,P.z,e.x,e.z) <= sk.radius then
                E.damage(e, sk.dmg, false); n = n + 1
            end
        end
        A.sfx("slash", 0.7)
        core.accept_log("skill_hit=fenhua targets=%d dmg=%d", n, sk.dmg)
    end
end
function Awake()
    Loot.init(G.seed)
    math.randomseed(G.seed)
    A.load(); Terrain.build(); P.spawn()
    P.items, P.item_seq = {}, 0
    P.equip = { weapon=nil, armor=nil, accessory=nil }
    for _,s in ipairs(D.spawns) do E.spawn(s.kind, s.x, s.z) end
    setup_camera(); UI.init()
    if os.getenv("DSE_WUXIA_DEMO") == "1" then
        Fx.demo_target = nearest_enemy
        Fx.demo_aoe = (#E.list >= 4)
        core.accept_log("demo=1")
    end
    core.accept_log("map=qingxi_village enemies=%d lights=%s rank_boss=1",
        #E.list, tostring(os.getenv("DSE_WUXIA_LIGHTS") ~= "0"))
end
function Update(dt)
    dt = core.clamp(dt or 0.016, 0.0, 0.05)
    G.t = G.t + dt
    if core.key_down(core.KEY.TAB) then
        G.inventory_open = not G.inventory_open
        G.inv_sel = 1
        A.sfx("click", 0.4)
    end
    if G.inventory_open then
        local n = math.max(1, #P.items)
        if core.key_down(core.KEY.W) or core.key_down(265) then G.inv_sel = ((G.inv_sel - 2) % n) + 1 end
        if core.key_down(core.KEY.S) or core.key_down(264) then G.inv_sel = (G.inv_sel % n) + 1 end
        if core.key_down(core.KEY.J) then
            local it = P.items[G.inv_sel]
            if it then P.auto_equip(it) end
        end
        UI.update(dt, P, G)
        return
    end
    if core.key_down(core.KEY.J) then P.try_attack() end
    if core.key_down(core.KEY.K) then P.try_dodge() end
    if core.key_down(core.KEY.U) then P.try_skill("fenhua") end
    if core.key_down(core.KEY.I) then P.try_skill("zixia") end
    if Fx.demo_target then Fx.demo_aoe = (#E.list >= 4) end
    P.update(dt)
    resolve_hit(); resolve_skill(); E.update(dt, P); Fx.update(dt)
    ecs.set_transform_position(G.cam, P.x, 6.2, P.z + 6.2)
    if os.getenv("DSE_WUXIA_AUTOSAVE") == "1" and not G.autosaved and G.t > 4.0 then
        G.autosaved = true
        local ok, err = Save.save(P, G.seed)
        core.accept_log("autosave ok=%s err=%s", tostring(ok), tostring(err or ""))
    end
    if P.state == "dead" then
        G.dead_t = G.dead_t + dt
        if G.dead_t > 2.0 then
            P.state = "idle"; local s=P.stats(); P.hp=s.hp_max; P.mp=s.mp_max; G.dead_t=0
            P.x, P.z = D.spawn.x, D.spawn.z
            P.play("idle", true)
        end
    end
    UI.update(dt, P, G)
end