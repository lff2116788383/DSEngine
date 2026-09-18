-- New HD-2D wuxia ARPG R3 vertical slice (new game, new assets).
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
local G = { t = 0.0, seed = 20260918, autosaved = false, cam = nil, dead_t = 0 }

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
            if d <= 1.45 then
                local dx,dz = e.x-P.x, e.z-P.z
                local dirs = { d={0,1}, u={0,-1}, l={-1,0}, r={1,0} }
                local f = dirs[P.dir] or dirs.d
                if dx*f[1] + dz*f[2] > -0.15 then
                    E.damage(e, P.atk + 5)
                    targets = targets + 1
                end
            end
        end
    end
    core.accept_log("attack dir=%s targets=%d", P.dir, targets)
end

function Awake()
    Loot.init(G.seed)
    math.randomseed(G.seed)
    A.load()
    Terrain.build()
    P.spawn()
    P.items, P.item_seq = {}, 0
    for _,s in ipairs(D.spawns) do E.spawn(s.kind, s.x, s.z) end
    setup_camera()
    UI.init()
    if os.getenv("DSE_WUXIA_DEMO") == "1" then
        Fx.demo_target = nearest_enemy
        core.accept_log("demo=1")
    end
    core.accept_log("map=qingxi_village enemies=%d lights=%s", #E.list, tostring(os.getenv("DSE_WUXIA_LIGHTS") ~= "0"))
end

function Update(dt)
    dt = core.clamp(dt or 0.016, 0.0, 0.05)
    G.t = G.t + dt
    if core.key_down(core.KEY.J) then P.try_attack() end
    if core.key_down(core.KEY.K) then P.try_dodge() end
    P.update(dt)
    resolve_hit()
    E.update(dt, P)
    Fx.update(dt)
    -- camera follow
    ecs.set_transform_position(G.cam, P.x, 6.2, P.z + 6.2)
    -- demo autosave marker
    if os.getenv("DSE_WUXIA_AUTOSAVE") == "1" and not G.autosaved and G.t > 3.0 then
        G.autosaved = true
        local ok, err = Save.save(P, G.seed)
        core.accept_log("autosave ok=%s err=%s", tostring(ok), tostring(err or ""))
    end
    if P.state == "dead" then
        G.dead_t = G.dead_t + dt
        if G.dead_t > 2.0 then
            P.state = "idle"; P.hp = P.hp_max; P.mp = P.mp_max; G.dead_t = 0
            P.x, P.z = D.spawn.x, D.spawn.z
            P.play("idle", true)
        end
    end
    UI.update(dt, P)
end