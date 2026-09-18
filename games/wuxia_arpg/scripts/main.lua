-- New HD-2D wuxia ARPG R5: three maps, transitions, weather, light polish.
local core = require("core")
local A = require("assets")
local D = require("data")
local Terrain = require("terrain")
local P = require("player")
local E = require("enemy")
local Fx = require("fx")
local UI = require("ui")
local Weather = require("weather")
local Loot = require("loot")
local Save = require("save")
local ecs = dse.ecs
local G = { t=0.0, seed=20260918, autosaved=false, cam=nil, dead_t=0, inventory_open=false, inv_sel=1, transition_cd=0.0, current_map="qingxi_village", tour_wait=0.0 }

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
local function load_map(id, entry, weather_override)
    D.load_map(id)
    G.current_map = D.current_id
    Terrain.clear(); E.clear(); Terrain.build()
    local sp = { x = D.w * 0.5, z = D.h - 2.5 }
    if entry == "north" then sp.z = 1.6 elseif entry == "south" then sp.z = D.h - 1.6 end
    P.teleport(sp.x, sp.z)
    for _,s in ipairs(D.spawns or {}) do E.spawn(s.kind, s.x, s.z) end
    local weather = weather_override or D.weather or "clear"
    Weather.set(weather, true)
    G.transition_cd = 1.0
    core.accept_log("map=%s weather=%s spawn=%.1f,%.1f enemies=%d", D.current_id, weather, P.x, P.z, #E.list)
    if os.getenv("DSE_WUXIA_TOUR") == "1" then G.tour_wait = 0.35 end
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
                    E.damage(e, P.attack_dmg, P.attack_crit); targets = targets + 1
                end
            end
        end
    end
    core.accept_log("attack combo=%d dir=%s targets=%d dmg=%d", P.combo, P.dir, targets, P.attack_dmg)
end
local function resolve_skill()
    if not P.skill_hit then return end
    local sk = P.skill_hit; P.skill_hit = nil
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
local function check_exits()
    if G.transition_cd > 0 then return end
    for _,ex in ipairs(D.exits or {}) do
        if P.x > ex.x - ex.w/2 and P.x < ex.x + ex.w/2
           and P.z > ex.z - ex.h/2 and P.z < ex.z + ex.h/2 then
            core.accept_log("transition from=%s to=%s entry=%s", D.current_id, ex.to, tostring(ex.entry))
            load_map(ex.to, ex.entry, nil)
            return
        end
    end
end
function Awake()
    Loot.init(G.seed); math.randomseed(G.seed)
    A.load(); setup_camera(); P.spawn(); UI.init()
    P.items, P.item_seq = {}, 0
    P.equip = { weapon=nil, armor=nil, accessory=nil }
    Weather.init(G.cam)
    local map_env = os.getenv("DSE_WUXIA_MAP")
    local weather_env = os.getenv("DSE_WUXIA_WEATHER")
    load_map(map_env ~= "" and map_env or "qingxi_village", nil, weather_env ~= "" and weather_env or nil)
    if os.getenv("DSE_WUXIA_DEMO") == "1" then
        Fx.demo_target = nearest_enemy
        Fx.demo_aoe = (#E.list >= 4)
        core.accept_log("demo=1")
    end
    core.accept_log("map_order=%d lights=%s", #D.map_order, tostring(os.getenv("DSE_WUXIA_LIGHTS") ~= "0"))
end
function Update(dt)
    dt = core.clamp(dt or 0.016, 0.0, 0.05)
    G.t = G.t + dt
    if G.transition_cd > 0 then G.transition_cd = G.transition_cd - dt end
    if core.key_down(core.KEY.TAB) then
        G.inventory_open = not G.inventory_open; G.inv_sel = 1; A.sfx("click",0.4)
    end
    if G.inventory_open then
        local n = math.max(1, #P.items)
        if core.key_down(core.KEY.W) or core.key_down(265) then G.inv_sel = ((G.inv_sel - 2) % n) + 1 end
        if core.key_down(core.KEY.S) or core.key_down(264) then G.inv_sel = (G.inv_sel % n) + 1 end
        if core.key_down(core.KEY.J) then local it=P.items[G.inv_sel]; if it then P.auto_equip(it) end end
        UI.update(dt,P,G); return
    end
    if core.key_down(core.KEY.J) then P.try_attack() end
    if core.key_down(core.KEY.K) then P.try_dodge() end
    if core.key_down(core.KEY.U) then P.try_skill("fenhua") end
    if core.key_down(core.KEY.I) then P.try_skill("zixia") end
    if Fx.demo_target then Fx.demo_aoe = (#E.list >= 4) end
    P.update(dt); resolve_hit(); resolve_skill(); E.update(dt,P)
    if os.getenv("DSE_WUXIA_TOUR") == "1" then
        G.tour_wait = math.max(0.0, G.tour_wait - dt)
        if G.tour_wait <= 0.0 then
            local idx = 1
            for i,id in ipairs(D.map_order) do if id == D.current_id then idx = i end end
            local next_id = D.map_order[idx+1]
            if next_id then
                for _,ex in ipairs(D.exits or {}) do
                    if ex.to == next_id then P.teleport(ex.x, ex.z); G.tour_wait = 0.6; break end
                end
            end
        end
    end    Weather.update(dt,P); Fx.update(dt)
    ecs.set_transform_position(G.cam, P.x, 6.2, P.z + 6.2)
    check_exits()
    if os.getenv("DSE_WUXIA_AUTOSAVE") == "1" and not G.autosaved and G.t > 4.0 then
        G.autosaved = true
        local ok, err = Save.save(P, G.seed)
        core.accept_log("autosave ok=%s err=%s", tostring(ok), tostring(err or ""))
    end
    if P.state == "dead" then
        G.dead_t = G.dead_t + dt
        if G.dead_t > 2.0 then
            P.state="idle"; local s=P.stats(); P.hp=s.hp_max; P.mp=s.mp_max; G.dead_t=0
            P.teleport(D.spawn.x, D.spawn.z)
        end
    end
    UI.update(dt,P,G)
end