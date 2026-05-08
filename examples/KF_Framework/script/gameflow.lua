--------------------------------------------------------------------------------
-- KF_Framework — 游戏流程管理 (Phase 6)
-- 状态: battle → result (→ 按键重启)
-- 参考: KF source_code/mode/mode_demo.cpp, mode_result.cpp
--------------------------------------------------------------------------------
local Config = require("script.config")
local Audio  = require("script.audio")
local HUD    = require("script.hud")
local Fade   = require("script.fade")

local app = dse.app
local ecs = dse.ecs
local ui  = dse.ui

local GameFlow = {}

-- 状态枚举
GameFlow.STATE_BATTLE = "battle"
GameFlow.STATE_RESULT = "result"

-- 内部
local state = "battle"
local result_timer = 0         -- 延迟进入 result 的计时器
local result_text = ""         -- "VICTORY" 或 "DEFEAT"
local result_ui = nil          -- UI 实体 (lazy create)
local result_sub_ui = nil      -- 副标题 UI
local fade_alpha = 0           -- 用于简单淡入效果
local restart_cooldown = 0     -- 防止立即重启

function GameFlow.get_state() return state end

--------------------------------------------------------------------------------
-- 初始化（battle 开始时调用）
--------------------------------------------------------------------------------
function GameFlow.setup()
    state = "battle"
    result_timer = 0
    fade_alpha = 0
    Audio.play_bgm("game")  -- KF: ModeDemo::OnCompleteLoading → kGameBgm
end

--------------------------------------------------------------------------------
-- 战斗结束判定
--------------------------------------------------------------------------------
function GameFlow.check_battle_end(player_dead, enemies_alive)
    if state ~= "battle" then return end

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
    if state == "battle" then
        -- 等待结局延迟
        if result_timer > 0 then
            result_timer = result_timer - dt
            if result_timer <= 0 then
                GameFlow.enter_result()
            end
        end
    elseif state == "result" then
        -- 淡入效果
        if fade_alpha < 1.0 then
            fade_alpha = math.min(1.0, fade_alpha + dt * 2.0)
        end
        -- 等待按键重启
        restart_cooldown = restart_cooldown - dt
        if restart_cooldown <= 0 then
            if app.get_key_down(32) or app.get_key_down(257)  -- Space / Enter
               or app.get_mouse_left_down() then
                Audio.play_se("submit")  -- KF: mode_result.cpp → Play(kSubmitSe)
                GameFlow.restart()
            end
        end
    end
end

--------------------------------------------------------------------------------
-- 进入 Result 画面
--------------------------------------------------------------------------------
function GameFlow.enter_result()
    -- KF: FadeTo(ModeResult) — 先淡出, 再显示 Result
    HUD.hide()
    Fade.fade_out(1.0, function()
        state = "result"
        fade_alpha = 0
        restart_cooldown = 1.0
        Audio.play_bgm("result")  -- KF: ModeResult::OnCompleteLoading → kResultBgm

        -- 创建 Result UI
        if not result_ui then
            result_ui = ecs.create_entity()
            ecs.add_transform(result_ui, 0, 0, 0)
        end
        ui.add_renderer(result_ui, 0, 0, 0, 0, 0.7, 100, 1920, 1080)
        ui.set_anchor(result_ui, 0.5, 0.5)

        if not result_sub_ui then
            result_sub_ui = ecs.create_entity()
            ecs.add_transform(result_sub_ui, 0, 0, 0)
        end
        local color_tag = result_text == "VICTORY" and "#00ff88" or "#ff4444"
        local display = "<color=" .. color_tag .. ">" .. result_text .. "</color>"
        ui.add_rich_text(result_sub_ui, display, 1, 1, 1, 1, true, false)

        print("[KF_Framework] " .. result_text .. "! Press Space/Enter to restart.")

        -- 淡入显示 Result 画面
        Fade.fade_in(1.0, nil)
    end)
end

--------------------------------------------------------------------------------
-- 重启游戏 (简易实现: 打印提示，实际需要引擎支持场景重载)
--------------------------------------------------------------------------------
function GameFlow.restart()
    print("[KF_Framework] Restarting...")
    -- KF: ModeResult → FadeTo(ModeTitle)
    Fade.fade_out(1.0, function()
        state = "battle"
        result_timer = 0
        -- 隐藏 Result UI
        if result_ui then
            ui.set_visible(result_ui, false)
        end
        if result_sub_ui then
            ui.set_visible(result_sub_ui, false)
        end
        -- 淡入回到游戏
        Fade.fade_in(1.0, nil)
    end)
end

return GameFlow
