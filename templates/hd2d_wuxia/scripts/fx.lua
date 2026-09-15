-- ============================================================================
-- fx.lua  特效 / 飘字 / 提示 / 天气
-- ============================================================================
local core = require("core")
local A = require("assets")

local F = { list = {}, pops = {}, toasts = {}, cam = nil, weather = nil }
local to_screen_ref = nil

function F.init(cam_entity, world_to_screen)
    F.cam = cam_entity
    to_screen_ref = world_to_screen
end

function F.set_font(tex, data)
    F.font_tex, F.font_data = tex, data
end

-- 帧动画特效（slash/hit/dust/qi/heal/levelup）
function F.spawn(kind, x, y, cfg)
    cfg = cfg or {}
    local frames = A.tex[kind]
    if not frames then return nil end
    local e = dse.ecs.create_entity()
    local s = cfg.scale or 1.0
    dse.ecs.add_transform(e, x, y, 0.0, s, s, 1.0)
    dse.ecs.add_sprite(e, cfg.r or 1, cfg.g or 1, cfg.b or 1, 1.0, cfg.order or 5000, frames[1])
    dse.ecs.add_animator(e)
    dse.ecs.add_animation_state(e, "play", cfg.fps or 20, false, frames)
    dse.ecs.play_animation(e, "play")
    F.list[#F.list + 1] = {
        e = e, t = 0, life = cfg.life or 0.5, x = x, y = y,
        vx = cfg.vx or 0, vy = cfg.vy or 0, follow = cfg.follow,
        sx = s, sy = s, spin = cfg.spin or 0,
    }
    return e
end

function F.slash(x, y, dir_x, dir_y, big)
    local e = F.spawn("slash", x, y, { scale = big and 1.8 or 1.15, life = 0.28, fps = 26 })
    return e
end

function F.hit(x, y, scale, crit)
    F.spawn("hit", x, y, { scale = (scale or 1.0) * (crit and 1.5 or 1.0), life = 0.3, fps = 24 })
end

function F.dust(x, y, scale)
    F.spawn("dust", x, y, { scale = scale or 1.0, life = 0.35, fps = 18 })
end

function F.qi(x, y, col)
    local e = F.spawn("qi", x, y, { scale = 1.2, life = 0.5, fps = 18 })
    if e and col then dse.ecs.add_sprite(e, col[1], col[2], col[3], 1.0, 5000, A.tex.qi[1]) end
    return e
end

function F.heal(x, y)
    F.spawn("heal", x, y, { scale = 1.1, life = 0.6, fps = 16 })
end

function F.levelup(x, y)
    F.spawn("levelup", x, y + 1.0, { scale = 1.6, life = 1.1, fps = 12 })
end

-- 伤害飘字（世界坐标  屏幕坐标，用位图字体绘制，保证清晰）
function F.popup(text, x, y, kind)
    if not F.font_data then return end
    local t = core.Text.new(F.font_data, F.font_tex, 0, 0, 1, 1, 1, 960000)
    t:set(tostring(text))
    local col = { 1.0, 0.9, 0.6 }
    if kind == "crit" then col = { 1.0, 0.45, 0.25 }
    elseif kind == "heal" then col = { 0.55, 1.0, 0.6 }
    elseif kind == "mp" then col = { 0.6, 0.85, 1.0 }
    elseif kind == "exp" then col = { 1.0, 0.85, 0.35 }
    elseif kind == "hurt" then col = { 1.0, 0.35, 0.35 }
    elseif kind == "info" then col = { 0.95, 0.95, 0.9 } end
    t:set_color(col[1], col[2], col[3])
    F.pops[#F.pops + 1] = { t = t, x = x, y = y, t0 = 0, life = 0.95 }
end

function F.toast(text, r, g, b)
    if not F.font_data then return end
    local t = core.Text.new(F.font_data, F.font_tex, 0, 0, (r or 1) * 0.98, (g or 1) * 0.96, (b or 1) * 0.9, 970000)
    t:set(text)
    F.toasts[#F.toasts + 1] = { t = t, t0 = 0, life = 2.4 }
end

-- 天气：落叶 / 萤火（围绕相机随机生成，飘落后销毁）
function F.set_weather(kind) F.weather = kind end

local weather_t = 0.0
function F.update_weather(dt, cam_x, cam_y)
    if not F.weather then return end
    weather_t = weather_t + dt
    local interval = (F.weather == "leaf") and 0.42 or 0.9
    if weather_t < interval then return end
    weather_t = 0.0
    local rx = cam_x + (math.random() - 0.5) * 22.0
    local ry = cam_y + 7.5
    if F.weather == "leaf" then
        local e = F.spawn("leaf", rx, ry, { scale = 1.0, life = 6.0, fps = 6, spin = 40 })
        local item = F.list[#F.list]
        if item then item.vx = -0.5 - math.random() * 0.6 item.vy = -1.2 - math.random() * 0.8 end
    else
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, rx, ry - math.random() * 6.0, 0.0, 0.7, 0.7, 1.0)
        dse.ecs.add_sprite(e, 1.0, 0.95, 0.75, 0.9, 5200, A.tex.firefly)
        F.list[#F.list + 1] = { e = e, t = 0, life = 4.0, x = rx, y = ry, vx = (math.random() - 0.5) * 0.5,
                                vy = (math.random() - 0.5) * 0.4, sx = 0.7, sy = 0.7 }
    end
end

function F.update(dt)
    for i = #F.list, 1, -1 do
        local it = F.list[i]
        it.t = it.t + dt
        if it.follow and dse.ecs then
            local px, py = dse.ecs.get_transform_position(it.follow)
            it.x, it.y = px, py
        end
        it.x = it.x + (it.vx or 0) * dt
        it.y = it.y + (it.vy or 0) * dt
        dse.ecs.set_transform_position(it.e, it.x, it.y, 0.0)
        if it.spin and it.spin ~= 0 then
            dse.ecs.set_transform_rotation(it.e, 0.0, 0.0, (it.t * it.spin) % 360.0)
        end
        local alpha = core.clamp(1.0 - it.t / it.life, 0.0, 1.0)
        if i and it.e then
            pcall(dse.ecs.add_sprite, it.e, 1.0, 1.0, 1.0, alpha, 5000, nil)
        end
        if it.t >= it.life then
            pcall(dse.ecs.destroy_entity, it.e)
            table.remove(F.list, i)
        end
    end
    -- 飘字
    for i = #F.pops, 1, -1 do
        local p = F.pops[i]
        p.t0 = p.t0 + dt
        local k = p.t0 / p.life
        if k >= 1.0 then
            p.t:destroy()
            table.remove(F.pops, i)
        else
            local sx, sy = to_screen_ref(p.x, p.y + k * 2.0)
            p.t:set_pos(sx - p.t.width * 0.5, sy)
            p.t:visible(true)
        end
    end
    -- 提示条
    for i = #F.toasts, 1, -1 do
        local p = F.toasts[i]
        p.t0 = p.t0 + dt
        if p.t0 >= p.life then
            p.t:destroy()
            table.remove(F.toasts, i)
        else
            local y = 118 + (i - 1) * 26
            p.t:set_pos((core.SCREEN_W - p.t.width) * 0.5, y)
        end
    end
end

return F