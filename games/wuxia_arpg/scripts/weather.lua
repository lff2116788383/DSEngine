-- Weather system for the new game: particles + postprocess/light mood.
local core = require("core")
local A = require("assets")
local D = require("data")
local ecs = dse.ecs
local Terrain = require("terrain")
local W = { particles = {}, cam = nil, current = nil, cycle_t = 0.0, cycle_i = 0, lightning_t = 0.0, timer = 0.0 }
local NAMES = { clear="晴", leaf="落叶", rain="雨", storm="雷暴", fog="雾", snow="雪" }
local SETTINGS = {
    clear = { count=0,  exposure=1.05, bloom=0.90 },
    leaf  = { count=36, exposure=1.00, bloom=0.92 },
    rain  = { count=52, exposure=0.92, bloom=0.82 },
    storm = { count=72, exposure=0.78, bloom=0.72 },
    fog   = { count=24, exposure=0.96, bloom=0.98 },
    snow  = { count=48, exposure=1.08, bloom=0.88 },
}
local function rnd(a,b) return a + math.random()*(b-a) end
local function clear_particles()
    for _,p in ipairs(W.particles) do if p.e then pcall(ecs.destroy_entity, p.e) end end
    W.particles = {}
end
local function make_particles(kind)
    clear_particles()
    local cfg = SETTINGS[kind] or SETTINGS.clear
    if cfg.count <= 0 then return end
    local tex = A.tex["weather_"..(kind=="storm" and "rain" or kind)]
    if not tex then return end
    for i=1,cfg.count do
        local e = ecs.create_entity()
        ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
        local w,h = 1,1
        if kind=="rain" or kind=="storm" then w,h=0.16,1.1
        elseif kind=="snow" then w,h=0.28,0.28
        elseif kind=="fog" then w,h=5.0,2.4
        elseif kind=="leaf" then w,h=0.32,0.32 end
        ecs.add_sprite3d(e, tex, w, h, {billboard="yaw",anchor=0.5,lit=false,receive_shadow=false})
        ecs.set_sprite3d_lit(e, false)
        ecs.set_sprite3d_opacity(e, kind=="fog" and 0.35 or 0.82)
        W.particles[#W.particles+1] = {
            e=e, ox=rnd(-9,9), oz=rnd(-7,7), oy=rnd(0.2,5.0),
            spd=(kind=="rain" or kind=="storm") and rnd(9,14) or (kind=="snow" and rnd(1.0,2.0) or (kind=="fog" and rnd(0.2,0.6) or rnd(1.4,2.4))),
            drift=rnd(-0.8,0.8), phase=rnd(0,6.28),
        }
    end
end
function W.init(cam)
    W.cam = cam
end
function W.name(kind) return NAMES[kind] or kind or "晴" end
function W.set(kind, immediate)
    kind = kind or "clear"
    if W.current == kind and not immediate then return end
    W.current = kind
    make_particles(kind)
    Terrain.set_weather_light(kind, 1.0)
    local cfg = SETTINGS[kind] or SETTINGS.clear
    if W.cam then
        ecs.set_post_process_exposure(W.cam, cfg.exposure)
        ecs.set_post_process_bloom_intensity(W.cam, cfg.bloom)
    end
    core.accept_log("weather=%s %s", kind, NAMES[kind] or "")
end
function W.update(dt, P)
    if not W.current then W.set("clear", true) end
    local cfg = SETTINGS[W.current] or SETTINGS.clear
    -- optional map-local cycle unless explicit weather was requested
    if (not os.getenv("DSE_WUXIA_WEATHER")) and D.weather_cycle and #D.weather_cycle > 0 then
        W.cycle_t = W.cycle_t + dt
        if W.cycle_t > 25.0 then
            W.cycle_t = 0.0
            W.cycle_i = (W.cycle_i % #D.weather_cycle) + 1
            W.set(D.weather_cycle[W.cycle_i])
        end
    end
    for i,p in ipairs(W.particles) do
        p.oy = p.oy - p.spd*dt
        p.ox = p.ox + p.drift*dt
        if p.oy < 0.0 then p.oy = 5.2; p.ox = rnd(-9,9); p.oz = rnd(-7,7) end
        if p.ox > 10 then p.ox = -10 elseif p.ox < -10 then p.ox = 10 end
        local y = p.oy
        if W.current=="fog" then y = 1.2 + math.sin(p.phase + W.timer*0.3)*0.25 end
        local x = P.x + p.ox
        local z = P.z + p.oz
        ecs.set_transform_position(p.e, x, y, z)
        p.phase = p.phase + dt
    end
    W.timer = W.timer + dt
    if W.current == "storm" then
        W.lightning_t = W.lightning_t - dt
        if W.lightning_t <= 0.0 then W.lightning_t = rnd(2.5,5.0) end
        local flash = math.max(0.0, 1.0 - math.abs(W.lightning_t - 2.2)*8.0)
        if W.cam then ecs.set_post_process_exposure(W.cam, cfg.exposure + flash*0.8) end
    elseif W.cam then
        ecs.set_post_process_exposure(W.cam, cfg.exposure)
    end
end
return W