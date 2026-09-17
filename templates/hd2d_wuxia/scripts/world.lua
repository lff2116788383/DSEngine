-- ============================================================================
-- world.lua  关卡构建 / 相机 / 瓦片碰撞 / 视差 / 光晕
-- ============================================================================
local core = require("core")
local A = require("assets")
local F = require("fx")
local B = require("bplus")

local W = {}
W.cam = nil
W.map = nil
W.grid = {}
W.W, W.H = 0, 0
W.entities = {}
W.glows = {}
W.time = 0.0
W.follow_target = nil

local ecs = dse.ecs

--  相机 
local ORTHO = 6.25
function W.setup_camera()
    if B.enabled then
        local cam = B.setup_camera()
        W.cam = cam
        F.init(cam, W.to_screen)
        return cam
    end
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    ecs.add_camera(cam, ORTHO)
    ecs.add_camera_controller_2d(cam)
    ecs.camera_set_zoom(cam, 1.0)
    ecs.camera_set_look_ahead(cam, 0.6, 0.35)
    ecs.add_post_process(cam, true, 0.84, 0.62, 1.02)
    ecs.set_post_process_color_grading_enabled(cam, true)
    ecs.set_post_process_exposure(cam, 1.02)
    ecs.set_post_process_gamma(cam, 1.02)
    ecs.set_post_process_fxaa_enabled(cam, true)
    ecs.set_post_process_vignette_enabled(cam, true)
    ecs.set_post_process_vignette_intensity(cam, 0.42)
    ecs.set_post_process_vignette_radius(cam, 0.72)
    ecs.set_post_process_vignette_softness(cam, 0.55)
    ecs.set_post_process_film_grain_enabled(cam, true)
    ecs.set_post_process_film_grain_intensity(cam, 0.035)
    W.cam = cam
    F.init(cam, W.to_screen)
    return cam
end

function W.follow(target)
    W.follow_target = target
    if B.enabled then B.follow(target) return end
    if W.cam and target then
        ecs.set_camera_follow(W.cam, target, 0.14, 0.0, 0.0, 0.0, 0.35)
    end
end

function W.shake(v) if B.enabled then return end if W.cam then ecs.camera_shake(W.cam, v) end end
function W.zoom(z) if B.enabled then return end if W.cam then ecs.camera_set_zoom(W.cam, z) end end

-- 世界  屏幕（供 UI 飘字使用）
local half_w, half_h = 1, 1
function W.to_screen(wx, wy)
    if B.enabled then
        local sx, sy = B.project(wx, wy)
        return sx or 0.0, sy or 0.0
    end
    local cx, cy, _ = ecs.get_transform_position(W.cam)
    half_h = ORTHO
    half_w = ORTHO * core.SCREEN_W / core.SCREEN_H
    local sx = core.SCREEN_W * 0.5 + (wx - cx) / half_w * (core.SCREEN_W * 0.5)
    local sy = core.SCREEN_H * 0.5 + (wy - cy) / half_h * (core.SCREEN_H * 0.5)
    return sx, sy
end

--  关卡 
function W.clear()
    if B.enabled then B.clear() end
    for _, e in ipairs(W.entities) do pcall(ecs.destroy_entity, e) end
    W.entities = {}
    W.glows = {}
end

function W.register(e) W.entities[#W.entities + 1] = e return e end

local function spawn_sprite(tex, x, y, sw, sh, order, r, g, b, a)
    local e = ecs.create_entity()
    ecs.add_transform(e, x, y, 0.0, sw, sh, 1.0)
    ecs.add_sprite(e, r or 1, g or 1, b or 1, a or 1, order or 0, tex)
    return W.register(e)
end

function W.load_map(map)
    W.clear()
    W.map = map
    W.grid = map.collide
    W.W, W.H = map.w, map.h
    if B.enabled then
        B.load_map(map)
        return
    end
    local bg = A.tex["bg_" .. map.id]
    -- 背景（被不透明地面遮住，保留以支持后续做"地图外围"表现）
    spawn_sprite(bg.sky, map.w * 0.5, map.h * 0.5, map.w, map.h, -40)
    spawn_sprite(bg.far, map.w * 0.5, map.h * 0.62, map.w, map.h, -30)
    spawn_sprite(bg.ground, map.w * 0.5, map.h * 0.5, map.w, map.h, -20)
    -- 灯火光晕（在地面之上、角色之下）
    for _, l in ipairs(map.lights or {}) do
        local e = spawn_sprite(A.tex.glow_warm, l.x, l.y, l.size * 2, l.size * 2, -10,
                               l.r, l.g, l.b, 0.85)
        W.glows[#W.glows + 1] = { e = e, base = l.size * 2, phase = math.random() * 6.28 }
    end
    -- 前景遮挡层（纹理加载顺序保证在最上层）
    local fge = spawn_sprite(A.tex["fg_" .. map.id], map.w * 0.5, map.h * 0.5, map.w, map.h, 20)
    -- 相机边界
    if W.cam then
        ecs.camera_set_bounds(W.cam, 0, 0, map.w, map.h)
        ecs.set_transform_position(W.cam, map.spawn.x, map.spawn.y, 0.0)
    end
    return fge
end

--  碰撞 
function W.solid(wx, wy)
    local tx = math.floor(wx)
    local ty = math.floor(wy)
    if tx < 0 or ty < 0 or tx >= W.W or ty >= W.H then return true end
    local row = W.grid[W.H - ty]
    if not row then return true end
    return row:sub(tx + 1, tx + 1) == "#"
end

function W.rect_solid(x, y, hw, hh)
    local x0, x1 = math.floor(x - hw), math.floor(x + hw - 1e-4)
    local y0, y1 = math.floor(y - hh), math.floor(y + hh - 1e-4)
    for ty = y0, y1 do
        for tx = x0, x1 do
            if W.solid(tx + 0.5, ty + 0.5) then return true end
        end
    end
    return false
end

-- 轴分离 AABB 推挤，返回新坐标与碰撞标记
function W.move(x, y, hw, hh, dx, dy)
    local hit_x, hit_y = false, false
    if dx ~= 0 then
        local nx = x + dx
        if W.rect_solid(nx, y, hw, hh) then
            if dx > 0 then
                local tx = math.floor(nx + hw)
                nx = tx - hw - 0.001
            else
                local tx = math.floor(nx - hw)
                nx = tx + 1 + hw + 0.001
            end
            hit_x = true
        end
        x = nx
    end
    if dy ~= 0 then
        local ny = y + dy
        if W.rect_solid(x, ny, hw, hh) then
            if dy > 0 then
                local ty = math.floor(ny + hh)
                ny = ty - hh - 0.001
            else
                local ty = math.floor(ny - hh)
                ny = ty + 1 + hh + 0.001
            end
            hit_y = true
        end
        y = ny
    end
    return x, y, hit_x, hit_y
end

-- 视线判定（AI 用，简单采样）
function W.line_clear(x0, y0, x1, y1, step)
    step = step or 0.4
    local d = core.dist(x0, y0, x1, y1)
    local n = math.max(1, math.floor(d / step))
    for i = 1, n do
        local t = i / n
        if W.solid(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t) then return false end
    end
    return true
end

function W.update(dt)
    if B.enabled then B.update(dt) return end
    W.time = W.time + dt
    -- 灯火呼吸
    for _, g in ipairs(W.glows) do
        local k = 1.0 + 0.10 * math.sin(W.time * 2.1 + g.phase)
        local s = g.base * k
        ecs.set_transform_scale(g.e, s, s, 1.0)
    end
    local cx, cy = 0, 0
    if W.cam then cx, cy = ecs.get_transform_position(W.cam) end
    F.update_weather(dt, cx, cy)
end

return W