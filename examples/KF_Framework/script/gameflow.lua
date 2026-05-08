--------------------------------------------------------------------------------
-- KF_Framework — 游戏流程管理 (Phase 6 + 8.7/8.8)
-- 状态: title → battle → result → title
-- 参考: KF mode_title.cpp, mode_demo.cpp, mode_result.cpp
--------------------------------------------------------------------------------
local Config = require("script.config")
local Audio  = require("script.audio")
local HUD    = require("script.hud")
local Fade   = require("script.fade")

local app    = dse.app
local ecs    = dse.ecs
local ui     = dse.ui
local assets = dse.assets

local GameFlow = {}
local font_tex = 0  -- bitmap_font.png handle

-- 状态枚举
GameFlow.STATE_TITLE  = "title"
GameFlow.STATE_BATTLE = "battle"
GameFlow.STATE_RESULT = "result"

-- 内部
local state = "title"
local result_timer = 0         -- 延迟进入 result 的计时器
local result_text = ""         -- "VICTORY" 或 "DEFEAT"
local result_ui = nil          -- Result 背景 UI
local result_sub_ui = nil      -- Result 文字 UI
local result_hint_ui = nil     -- Result "Press Any Key" UI
local title_bg_ui = nil        -- Title 背景 UI
local title_text_ui = nil      -- Title 标题 UI
local title_hint_ui = nil      -- Title "Press Enter" UI
local flash_timer = 0          -- 闪烁计时器
local restart_cooldown = 0     -- 防止立即按键

function GameFlow.get_state() return state end

--------------------------------------------------------------------------------
-- 初始化（进入 Title 状态）
--------------------------------------------------------------------------------
function GameFlow.setup()
    state = "title"
    result_timer = 0
    flash_timer = 0
    restart_cooldown = 1.0
    -- 加载位图字体 (DSE 内置: 16col×6row, ASCII start=32)
    font_tex = assets.load_texture("font/bitmap_font.png")
    if font_tex == 0 then
        font_tex = assets.load_texture("data/font/bitmap_font.png")
    end
    GameFlow.create_title_ui()
    Audio.play_bgm("title")  -- KF: ModeTitle::OnCompleteLoading → kTitleBgm
    Fade.fade_in(1.0, nil)   -- 初始淡入
end

--------------------------------------------------------------------------------
-- 战斗结束判定
--------------------------------------------------------------------------------
function GameFlow.check_battle_end(player_dead, enemies_alive)
    if state ~= "battle" then return end
    if result_timer > 0 then return end  -- 已触发

    if player_dead then
        result_text = "DEFEAT"
        result_timer = 8.0  -- KF: mode_demo.h kWaitTime = 8.0f
    elseif enemies_alive <= 0 then
        result_text = "VICTORY"
        result_timer = 0.1  -- KF: GameTime::kTimeInterval ≈ 1帧
    end
end

--------------------------------------------------------------------------------
-- 每帧更新
--------------------------------------------------------------------------------
function GameFlow.update(dt)
    flash_timer = flash_timer + dt

    if state == "title" then
        -- "Press Enter to Start" 闪烁 (KF: FlashButtonController)
        -- 使用可见性切换 (label 子元素不响应 set_color)
        if title_hint_ui then
            local visible = math.sin(flash_timer * 3.0) > -0.3
            ui.set_visible(title_hint_ui, visible)
        end
        -- 等待按键
        restart_cooldown = restart_cooldown - dt
        if restart_cooldown <= 0 then
            if app.get_key_down(32) or app.get_key_down(257)  -- Space / Enter
               or app.get_mouse_left_down() then
                Audio.play_se("submit")  -- KF: mode_title.cpp → Play(kSubmitSe)
                GameFlow.enter_battle()
            end
        end

    elseif state == "battle" then
        -- 等待结局延迟
        if result_timer > 0 then
            result_timer = result_timer - dt
            if result_timer <= 0 then
                GameFlow.enter_result()
            end
        end

    elseif state == "result" then
        -- "Press Any Key" 闪烁 (KF: FlashButtonController)
        if result_hint_ui then
            local visible = math.sin(flash_timer * 3.0) > -0.3
            ui.set_visible(result_hint_ui, visible)
        end
        -- 等待按键
        restart_cooldown = restart_cooldown - dt
        if restart_cooldown <= 0 then
            if app.get_key_down(32) or app.get_key_down(257)  -- Space / Enter
               or app.get_mouse_left_down() then
                Audio.play_se("submit")  -- KF: mode_result.cpp → Play(kSubmitSe)
                GameFlow.enter_title()
            end
        end
    end
end

--------------------------------------------------------------------------------
-- Title UI 创建
-- KF: mode_title.cpp — 背景图 + 按钮 + kTitleBgm
-- DSE: 全屏半透明覆盖层 + rich_text
--------------------------------------------------------------------------------
function GameFlow.create_title_ui()
    -- 背景覆盖 (深蓝半透明)
    if not title_bg_ui then
        title_bg_ui = ecs.create_entity()
        ecs.add_transform(title_bg_ui, 0, 0, 0)
    end
    ui.add_renderer(title_bg_ui, 0, 0.05, 0.05, 0.15, 0.85, 100, 1920, 1080)
    ui.set_anchor(title_bg_ui, 0.5, 0.5)

    -- 标题文字 "KF_Framework Demo" (bitmap font, 居中偏上)
    if not title_text_ui then
        title_text_ui = ecs.create_entity()
        ecs.add_transform(title_text_ui, 0, 0, 0)
    end
    ui.add_label(title_text_ui, "KF_Framework Demo", font_tex,
        1.0, 0.95, 0.70, 1.0,   -- RGBA
        28.0, 40.0, 4.0,        -- glyph_w, glyph_h, spacing
        16, 6, 32,               -- atlas_cols, atlas_rows, ascii_start
        -270.0, 120.0)           -- offset_x, offset_y (Y+向上, 标题在上)

    -- "Press Enter to Start" 闪烁提示 (居中偏下)
    if not title_hint_ui then
        title_hint_ui = ecs.create_entity()
        ecs.add_transform(title_hint_ui, 0, 0, 0)
    end
    ui.add_label(title_hint_ui, "Press Enter to Start", font_tex,
        0.8, 0.8, 0.8, 1.0,
        14.0, 20.0, 2.0,
        16, 6, 32,
        -150.0, 50.0)            -- offset_x, offset_y (标题下方)
end

function GameFlow.hide_title_ui()
    if title_bg_ui   then ui.set_visible(title_bg_ui, false) end
    if title_text_ui then ui.set_visible(title_text_ui, false) end
    if title_hint_ui then ui.set_visible(title_hint_ui, false) end
end

function GameFlow.show_title_ui()
    if title_bg_ui   then ui.set_visible(title_bg_ui, true) end
    if title_text_ui then ui.set_visible(title_text_ui, true) end
    if title_hint_ui then ui.set_visible(title_hint_ui, true) end
end

--------------------------------------------------------------------------------
-- Title → Battle
-- KF: mode_title.cpp → FadeTo(ModeDemo)
--------------------------------------------------------------------------------
function GameFlow.enter_battle()
    Fade.fade_out(1.0, function()
        state = "battle"
        result_timer = 0
        GameFlow.hide_title_ui()
        HUD.show()
        Audio.play_bgm("game")  -- KF: ModeDemo::OnCompleteLoading → kGameBgm
        print("[KF_Framework] Battle start!")
        Fade.fade_in(1.0, nil)
    end)
end

--------------------------------------------------------------------------------
-- Battle → Result
-- KF: mode_demo.cpp → FadeTo(ModeResult)
--------------------------------------------------------------------------------
function GameFlow.enter_result()
    HUD.hide()
    Fade.fade_out(1.0, function()
        state = "result"
        restart_cooldown = 1.0
        flash_timer = 0
        Audio.play_bgm("result")  -- KF: ModeResult::OnCompleteLoading → kResultBgm

        -- Result 背景
        if not result_ui then
            result_ui = ecs.create_entity()
            ecs.add_transform(result_ui, 0, 0, 0)
        end
        ui.add_renderer(result_ui, 0, 0, 0, 0, 0.7, 100, 1920, 1080)
        ui.set_anchor(result_ui, 0.5, 0.5)
        ui.set_visible(result_ui, true)

        -- 结果文字 (VICTORY=绿 / DEFEAT=红)
        if not result_sub_ui then
            result_sub_ui = ecs.create_entity()
            ecs.add_transform(result_sub_ui, 0, 0, 0)
        end
        local r, g, b = 1.0, 0.3, 0.3
        if result_text == "VICTORY" then r, g, b = 0.0, 1.0, 0.53 end
        ui.add_label(result_sub_ui, result_text, font_tex,
            r, g, b, 1.0,
            28.0, 40.0, 4.0,
            16, 6, 32,
            -110.0, 60.0)

        -- "Press Any Key" 闪烁提示 (KF: FlashButtonController)
        if not result_hint_ui then
            result_hint_ui = ecs.create_entity()
            ecs.add_transform(result_hint_ui, 0, 0, 0)
        end
        ui.add_label(result_hint_ui, "Press Any Key", font_tex,
            0.7, 0.7, 0.7, 1.0,
            14.0, 20.0, 2.0,
            16, 6, 32,
            -110.0, -20.0)

        print("[KF_Framework] " .. result_text .. "! Press any key to continue.")
        Fade.fade_in(1.0, nil)
    end)
end

--------------------------------------------------------------------------------
-- Result → Title
-- KF: mode_result.cpp → FadeTo(ModeTitle)
--------------------------------------------------------------------------------
function GameFlow.enter_title()
    Fade.fade_out(1.0, function()
        state = "title"
        restart_cooldown = 1.0
        flash_timer = 0
        -- 隐藏 Result UI
        if result_ui      then ui.set_visible(result_ui, false) end
        if result_sub_ui   then ui.set_visible(result_sub_ui, false) end
        if result_hint_ui  then ui.set_visible(result_hint_ui, false) end
        -- 显示 Title UI
        GameFlow.show_title_ui()
        Audio.play_bgm("title")  -- KF: kTitleBgm
        print("[KF_Framework] Back to Title.")
        Fade.fade_in(1.0, nil)
    end)
end

return GameFlow
