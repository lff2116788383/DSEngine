--------------------------------------------------------------------------------
-- KF_Framework — 游戏流程管理 (Phase 6 + 8.7/8.8)
-- 状态: title → battle → result → title
-- 参考: KF mode_title.cpp, mode_demo.cpp, mode_result.cpp
--------------------------------------------------------------------------------
local Config = require("script.config")
local Audio  = require("script.audio")
local HUD    = require("script.hud")
local Fade   = require("script.fade")
local Player   = require("script.player")
local Enemy    = require("script.enemy")
local AutoPlay = require("script.autoplay")

local app    = dse.app
local ecs    = dse.ecs
local ui     = dse.ui
local assets = dse.assets

local GameFlow = {}
local font_tex = 0  -- bitmap_font.png handle

-- UI 纹理 handles
local title_tex = 0
local result_tex = 0
local play_game_tex = 0
local demo_play_tex = 0
local press_any_key_tex = 0

-- 状态枚举
GameFlow.STATE_TITLE  = "title"
GameFlow.STATE_BATTLE = "battle"
GameFlow.STATE_RESULT = "result"

-- 内部
local state = "title"
local result_timer = 0         -- 延迟进入 result 的计时器
local result_text = ""         -- "VICTORY" 或 "DEFEAT"
local result_ui = nil          -- Result 背景 UI
local result_hint_ui = nil     -- Result "PRESS ANY KEY" UI
local title_bg_ui = nil        -- Title 背景 UI
local title_btn_game_ui = nil  -- Title "PLAY GAME" 按钮
local title_btn_demo_ui = nil  -- Title "DEMO PLAY" 按钮
local title_selected = 0       -- 0=PLAY GAME, 1=DEMO PLAY
local flash_timer = 0          -- 闪烁计时器
local flash_speed = 1.0        -- 闪烁速度 (Result: 按键后加速到 15.0)
local flash_alpha = 1.0        -- FlashButton 当前 alpha (连续振荡)
local restart_cooldown = 0     -- 防止立即按键
local title_confirm_timer = 0  -- Title 确认后延迟 (KF: kWaitTime=0.25s)
local result_confirm_timer = 0 -- Result 确认后延迟 (KF: kWaitTime=1.0s)

-- Button color lerp 状态 (KF: ButtonController::ChangeColor 0.1s)
local btn_lerp_timer = 0
local btn_lerp_duration = 0.1
local btn_lerp_active = false
local btn_lerp_from_game = {1, 1, 1, 1}   -- PLAY GAME 当前颜色
local btn_lerp_from_demo = {0.5, 0.5, 0.5, 1} -- DEMO PLAY 当前颜色
local btn_lerp_to_game = {1, 1, 1, 1}
local btn_lerp_to_demo = {0.5, 0.5, 0.5, 1}

function GameFlow.get_state() return state end

--------------------------------------------------------------------------------
-- 初始化（进入 Title 状态）
--------------------------------------------------------------------------------
function GameFlow.setup()
    state = "title"
    result_timer = 0
    flash_timer = 0
    restart_cooldown = 1.0
    flash_speed = 1.0
    title_selected = 0
    -- 加载位图字体 (HUD 仍用 bitmap font)
    font_tex = assets.load_texture("font/bitmap_font.png")
    if font_tex == 0 then
        font_tex = assets.load_texture("data/font/bitmap_font.png")
    end
    -- 加载 UI 纹理
    title_tex = assets.load_texture("assets/textures/ui/title.jpg")
    result_tex = assets.load_texture("assets/textures/ui/result.jpg")
    play_game_tex = assets.load_texture("assets/textures/ui/play_game.png")
    demo_play_tex = assets.load_texture("assets/textures/ui/demo_play.png")
    press_any_key_tex = assets.load_texture("assets/textures/ui/press_any_key.png")
    GameFlow.create_title_ui()
    Audio.play_bgm("title")  -- KF: ModeTitle::OnCompleteLoading → kTitleBgm
    Fade.start_with_loading()  -- KF: 启动时从 Loading 动画开始

    -- 自动截图模式: 跳过 Title 直接进入 Battle
    if os.getenv("DSE_AUTO_BATTLE") then
        -- DSE_AUTO_BATTLE=2 → DemoPlay (AI自动战斗, 匹配KF DEMO PLAY模式)
        -- DSE_AUTO_BATTLE=1 → PlayGame (无AI, 匹配KF PLAY GAME模式)
        if os.getenv("DSE_AUTO_BATTLE") == "2" then
            AutoPlay.set_enabled(true)
        else
            AutoPlay.set_enabled(false)
        end
        title_confirm_timer = 0.5  -- 等半秒让场景加载完
    end
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
        -- Button color lerp 更新 (KF: ButtonController::Update)
        if btn_lerp_active then
            btn_lerp_timer = btn_lerp_timer + dt
            local t = math.min(1.0, btn_lerp_timer / btn_lerp_duration)
            if title_btn_game_ui then
                local r = btn_lerp_from_game[1] + (btn_lerp_to_game[1] - btn_lerp_from_game[1]) * t
                local g = btn_lerp_from_game[2] + (btn_lerp_to_game[2] - btn_lerp_from_game[2]) * t
                local b = btn_lerp_from_game[3] + (btn_lerp_to_game[3] - btn_lerp_from_game[3]) * t
                ui.set_color(title_btn_game_ui, r, g, b, 1)
            end
            if title_btn_demo_ui then
                local r = btn_lerp_from_demo[1] + (btn_lerp_to_demo[1] - btn_lerp_from_demo[1]) * t
                local g = btn_lerp_from_demo[2] + (btn_lerp_to_demo[2] - btn_lerp_from_demo[2]) * t
                local b = btn_lerp_from_demo[3] + (btn_lerp_to_demo[3] - btn_lerp_from_demo[3]) * t
                ui.set_color(title_btn_demo_ui, r, g, b, 1)
            end
            if t >= 1.0 then btn_lerp_active = false end
        end

        -- Title 确认延迟 (KF: kWaitTime=0.25s)
        if title_confirm_timer > 0 then
            title_confirm_timer = title_confirm_timer - dt
            if title_confirm_timer <= 0 then
                GameFlow.enter_battle()
            end
            -- 延迟中不接受输入
        else
            -- 左右切换选中按钮 (KF: mode_title.cpp)
            restart_cooldown = restart_cooldown - dt
            if restart_cooldown <= 0 then
                if app.get_key_down(263) then  -- Left
                    if title_selected ~= 0 then
                        title_selected = 0
                        Audio.play_se("cursor")
                        GameFlow.start_button_lerp()
                    end
                elseif app.get_key_down(262) then  -- Right
                    if title_selected ~= 1 then
                        title_selected = 1
                        Audio.play_se("cursor")
                        GameFlow.start_button_lerp()
                    end
                end
                -- 确认 (Space / Enter / Mouse)
                if app.get_key_down(32) or app.get_key_down(257)
                   or app.get_mouse_left_down() then
                    Audio.play_se("submit")  -- KF: mode_title.cpp → Play(kSubmitSe)
                    if title_selected == 0 then
                        AutoPlay.set_enabled(false)
                    else
                        AutoPlay.set_enabled(true)
                    end
                    title_confirm_timer = 0.25  -- KF: kWaitTime = 0.25f
                end
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
        -- "PRESS ANY KEY" alpha 振荡 (KF: FlashButtonController)
        -- alpha += flash_speed * dt, 到达 0/1 反弹
        if result_hint_ui then
            flash_alpha = flash_alpha + flash_speed * dt
            if flash_alpha >= 1.0 then
                flash_alpha = 1.0
                flash_speed = -math.abs(flash_speed)
            elseif flash_alpha <= 0.0 then
                flash_alpha = 0.0
                flash_speed = math.abs(flash_speed)
            end
            ui.set_color(result_hint_ui, 1, 1, 1, flash_alpha)
        end

        -- Result 确认延迟 (KF: kWaitTime=1.0s)
        if result_confirm_timer > 0 then
            result_confirm_timer = result_confirm_timer - dt
            if result_confirm_timer <= 0 then
                GameFlow.enter_title()
            end
        else
            -- 等待按键
            restart_cooldown = restart_cooldown - dt
            if restart_cooldown <= 0 then
                if app.get_key_down(32) or app.get_key_down(257)  -- Space / Enter
                   or app.get_mouse_left_down() then
                    Audio.play_se("submit")  -- KF: mode_result.cpp → Play(kSubmitSe)
                    flash_speed = 15.0       -- KF: 按键后加速闪烁
                    result_confirm_timer = 1.0  -- KF: kWaitTime = 1.0f
                end
            end
        end
    end
end

--------------------------------------------------------------------------------
-- Title UI 创建
-- KF: mode_title.cpp — 背景图 title.jpg + PLAY GAME / DEMO PLAY 按钮
-- DSE: 纹理渲染 + 左右切换选中
--------------------------------------------------------------------------------
function GameFlow.create_title_ui()
    -- 全屏背景 title.jpg (KF: 1280×720)
    if not title_bg_ui then
        title_bg_ui = ecs.create_entity()
        ecs.add_transform(title_bg_ui, 0, 0, 0)
    end
    ui.add_renderer(title_bg_ui, title_tex, 1, 1, 1, 1, 100, app.get_screen_width(), app.get_screen_height())
    ui.set_anchor(title_bg_ui, 0.5, 0.5)
    ui.set_position(title_bg_ui, 0, 0)

    -- "PLAY GAME" 按钮 — 居中偏左 200px, 偏下 180px (KF: size 320×56)
    if not title_btn_game_ui then
        title_btn_game_ui = ecs.create_entity()
        ecs.add_transform(title_btn_game_ui, 0, 0, 0)
    end
    ui.add_renderer(title_btn_game_ui, play_game_tex, 1, 1, 1, 1, 101, 320, 56)
    ui.set_anchor(title_btn_game_ui, 0.5, 0.5)
    ui.set_position(title_btn_game_ui, -200, -180)

    -- "DEMO PLAY" 按钮 — 居中偏右 200px, 偏下 180px (KF: size 320×56)
    if not title_btn_demo_ui then
        title_btn_demo_ui = ecs.create_entity()
        ecs.add_transform(title_btn_demo_ui, 0, 0, 0)
    end
    ui.add_renderer(title_btn_demo_ui, demo_play_tex, 0.5, 0.5, 0.5, 1, 101, 320, 56)
    ui.set_anchor(title_btn_demo_ui, 0.5, 0.5)
    ui.set_position(title_btn_demo_ui, 200, -180)

    -- 初始选中状态
    GameFlow.update_title_buttons()
end

--------------------------------------------------------------------------------
-- Title 按钮选中状态更新 (即时设置, 用于初始化)
--------------------------------------------------------------------------------
function GameFlow.update_title_buttons()
    if title_btn_game_ui then
        if title_selected == 0 then
            ui.set_color(title_btn_game_ui, 1, 1, 1, 1)       -- 选中: 白色
        else
            ui.set_color(title_btn_game_ui, 0.5, 0.5, 0.5, 1) -- 未选中: 灰色
        end
    end
    if title_btn_demo_ui then
        if title_selected == 1 then
            ui.set_color(title_btn_demo_ui, 1, 1, 1, 1)       -- 选中: 白色
        else
            ui.set_color(title_btn_demo_ui, 0.5, 0.5, 0.5, 1) -- 未选中: 灰色
        end
    end
    -- 同步 lerp 状态
    btn_lerp_from_game = title_selected == 0 and {1,1,1,1} or {0.5,0.5,0.5,1}
    btn_lerp_from_demo = title_selected == 1 and {1,1,1,1} or {0.5,0.5,0.5,1}
    btn_lerp_active = false
end

--------------------------------------------------------------------------------
-- Title 按钮颜色 lerp 启动 (KF: ButtonController::ChangeColor 0.1s)
--------------------------------------------------------------------------------
function GameFlow.start_button_lerp()
    -- 记录当前颜色作为 lerp 起点
    btn_lerp_from_game = {btn_lerp_to_game[1], btn_lerp_to_game[2], btn_lerp_to_game[3], 1}
    btn_lerp_from_demo = {btn_lerp_to_demo[1], btn_lerp_to_demo[2], btn_lerp_to_demo[3], 1}
    -- 设置目标颜色
    if title_selected == 0 then
        btn_lerp_to_game = {1, 1, 1, 1}
        btn_lerp_to_demo = {0.5, 0.5, 0.5, 1}
    else
        btn_lerp_to_game = {0.5, 0.5, 0.5, 1}
        btn_lerp_to_demo = {1, 1, 1, 1}
    end
    btn_lerp_timer = 0
    btn_lerp_active = true
end

function GameFlow.hide_title_ui()
    if title_bg_ui       then ui.set_visible(title_bg_ui, false) end
    if title_btn_game_ui then ui.set_visible(title_btn_game_ui, false) end
    if title_btn_demo_ui then ui.set_visible(title_btn_demo_ui, false) end
end

function GameFlow.show_title_ui()
    if title_bg_ui       then ui.set_visible(title_bg_ui, true) end
    if title_btn_game_ui then ui.set_visible(title_btn_game_ui, true) end
    if title_btn_demo_ui then ui.set_visible(title_btn_demo_ui, true) end
end

--------------------------------------------------------------------------------
-- Title → Battle
-- KF: mode_title.cpp → FadeTo(ModeDemo)
--------------------------------------------------------------------------------
function GameFlow.enter_battle()
    Fade.fade_out(1.0, function()
        state = "battle"
        result_timer = 0
        -- 软重置玩家和敌人 (KF: ModeDemo 重建场景)
        Player.reset()
        Enemy.reset_all()
        AutoPlay.reset()
        GameFlow.hide_title_ui()
        HUD.show()
        Audio.play_bgm("game")  -- KF: ModeDemo::OnCompleteLoading → kGameBgm
        print("[KF_Framework] Battle start!")
        -- Fade.fade_in 由状态机自动触发 (WaitIn → FadeIn)
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
        flash_speed = 1.0
        flash_alpha = 1.0
        result_confirm_timer = 0
        title_confirm_timer = 0
        Audio.play_bgm("result")  -- KF: ModeResult::OnCompleteLoading → kResultBgm

        -- 全屏背景 result.jpg "THANKS FOR PLAYING" (KF: 1280×720)
        if not result_ui then
            result_ui = ecs.create_entity()
            ecs.add_transform(result_ui, 0, 0, 0)
        end
        ui.add_renderer(result_ui, result_tex, 1, 1, 1, 1, 100, app.get_screen_width(), app.get_screen_height())
        ui.set_anchor(result_ui, 0.5, 0.5)
        ui.set_position(result_ui, 0, 0)
        ui.set_visible(result_ui, true)

        -- "PRESS ANY KEY" 闪烁按钮 (KF: FlashButtonController, size 560×73)
        if not result_hint_ui then
            result_hint_ui = ecs.create_entity()
            ecs.add_transform(result_hint_ui, 0, 0, 0)
        end
        ui.add_renderer(result_hint_ui, press_any_key_tex, 1, 1, 1, 1, 101, 560, 73)
        ui.set_anchor(result_hint_ui, 0.5, 0.5)
        ui.set_position(result_hint_ui, 0, -180)
        ui.set_visible(result_hint_ui, true)

        print("[KF_Framework] " .. result_text .. "! Press any key to continue.")
        -- Fade.fade_in 由状态机自动触发 (WaitIn → FadeIn)
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
        flash_speed = 1.0
        flash_alpha = 1.0
        title_selected = 0
        title_confirm_timer = 0
        result_confirm_timer = 0
        -- 隐藏 Result UI
        if result_ui      then ui.set_visible(result_ui, false) end
        if result_hint_ui  then ui.set_visible(result_hint_ui, false) end
        -- 显示 Title UI
        GameFlow.show_title_ui()
        GameFlow.update_title_buttons()
        Audio.play_bgm("title")  -- KF: kTitleBgm
        print("[KF_Framework] Back to Title.")
        -- Fade.fade_in 由状态机自动触发 (WaitIn → FadeIn)
    end)
end

return GameFlow
