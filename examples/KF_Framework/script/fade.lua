--------------------------------------------------------------------------------
-- KF_Framework — Fade 过渡系统 (Phase 8.6)
-- 参考: KF source_code/fade_system.cpp
-- FadeOut: alpha 0→1 (黑幕覆盖), FadeIn: alpha 1→0 (黑幕消失)
-- 默认 fade_time = 1.0s (KF: FadeTo 默认参数)
--------------------------------------------------------------------------------
local ecs = dse.ecs
local ui  = dse.ui

local Fade = {}

-- 内部状态
local fade_entity = nil
local fade_state = "none"      -- "none" | "out" | "in"
local fade_timer = 0
local fade_duration = 1.0
local fade_callback = nil

--------------------------------------------------------------------------------
-- 初始化: 创建全屏黑色 UIRenderer (z_order=200, 最顶层)
--------------------------------------------------------------------------------
function Fade.setup()
    fade_entity = ecs.create_entity()
    ecs.add_transform(fade_entity, 0, 0, 0)
    -- 全屏黑色, 初始 alpha=0 (透明)
    ui.add_renderer(fade_entity, 0, 0, 0, 0, 0, 200, 1920, 1080)
    ui.set_anchor(fade_entity, 0.5, 0.5)
    ui.set_visible(fade_entity, false)
end

--------------------------------------------------------------------------------
-- FadeOut: 黑幕从透明到不透明 (alpha 0→1)
-- KF: FadeSystem::FadeOut → time_counter 0→fade_time
--------------------------------------------------------------------------------
function Fade.fade_out(duration, callback)
    if not fade_entity then return end
    fade_state = "out"
    fade_duration = duration or 1.0
    fade_timer = 0
    fade_callback = callback
    ui.set_visible(fade_entity, true)
    ui.set_color(fade_entity, 0, 0, 0, 0)
end

--------------------------------------------------------------------------------
-- FadeIn: 黑幕从不透明到透明 (alpha 1→0)
-- KF: FadeSystem::FadeIn → time_counter fade_time→0
--------------------------------------------------------------------------------
function Fade.fade_in(duration, callback)
    if not fade_entity then return end
    fade_state = "in"
    fade_duration = duration or 1.0
    fade_timer = fade_duration
    fade_callback = callback
    ui.set_visible(fade_entity, true)
    ui.set_color(fade_entity, 0, 0, 0, 1)
end

--------------------------------------------------------------------------------
-- 每帧更新
--------------------------------------------------------------------------------
function Fade.update(dt)
    if fade_state == "none" or not fade_entity then return end

    if fade_state == "out" then
        fade_timer = fade_timer + dt
        local alpha = math.min(1.0, fade_timer / fade_duration)
        ui.set_color(fade_entity, 0, 0, 0, alpha)
        if fade_timer >= fade_duration then
            fade_state = "none"
            if fade_callback then
                local cb = fade_callback
                fade_callback = nil
                cb()
            end
        end
    elseif fade_state == "in" then
        fade_timer = fade_timer - dt
        local alpha = math.max(0.0, fade_timer / fade_duration)
        ui.set_color(fade_entity, 0, 0, 0, alpha)
        if fade_timer <= 0 then
            fade_state = "none"
            ui.set_visible(fade_entity, false)
            if fade_callback then
                local cb = fade_callback
                fade_callback = nil
                cb()
            end
        end
    end
end

--------------------------------------------------------------------------------
-- 查询
--------------------------------------------------------------------------------
function Fade.is_fading()
    return fade_state ~= "none"
end

return Fade
