--------------------------------------------------------------------------------
-- KF_Framework — 游戏流程管理 (Phase 6)
-- 状态: battle → result (→ 按键重启)
-- 参考: KF source_code/mode/mode_demo.cpp, mode_result.cpp
--------------------------------------------------------------------------------
local Config = require("script.config")
local Audio  = require("script.audio")

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
        result_timer = 2.0  -- KF: kWaitTime 延迟后切换
    elseif enemies_alive <= 0 then
        result_text = "VICTORY"
        result_timer = 1.0
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
                GameFlow.restart()
            end
        end
    end
end

--------------------------------------------------------------------------------
-- 进入 Result 画面
--------------------------------------------------------------------------------
function GameFlow.enter_result()
    state = "result"
    fade_alpha = 0
    restart_cooldown = 1.0  -- 1 秒后才允许重启
    Audio.play_bgm("result")  -- KF: ModeResult::OnCompleteLoading → kResultBgm

    -- 创建 Result UI
    if not result_ui then
        result_ui = ecs.create_entity()
        ecs.add_transform(result_ui, 0, 0, 0)
    end
    -- 半透明黑色背景
    ui.add_renderer(result_ui, 0, 0, 0, 0, 0.7, 100, 1280, 720)

    -- 结果文字
    if not result_sub_ui then
        result_sub_ui = ecs.create_entity()
        ecs.add_transform(result_sub_ui, 0, 0, 0)
    end
    -- 使用 rich_text 显示结果
    local color_tag = result_text == "VICTORY" and "#00ff88" or "#ff4444"
    local display = "<color=" .. color_tag .. ">" .. result_text .. "</color>"
    ui.add_rich_text(result_sub_ui, display, 1, 1, 1, 1, true, false)

    print("[KF_Framework] " .. result_text .. "! Press Space/Enter to restart.")
end

--------------------------------------------------------------------------------
-- 重启游戏 (简易实现: 打印提示，实际需要引擎支持场景重载)
--------------------------------------------------------------------------------
function GameFlow.restart()
    print("[KF_Framework] Restarting...")
    -- DSEngine 目前没有 Lua 层面的场景重载 API
    -- 简单处理: 隐藏 result UI，重置状态
    -- 实际游戏需要 app.reload_script() 或类似功能
    state = "battle"
    result_timer = 0
    -- 将 UI 移到屏幕外（隐藏）
    if result_ui then
        ui.add_renderer(result_ui, 0, 0, 0, 0, 0, 100, 0, 0)
    end
    if result_sub_ui then
        ui.add_rich_text(result_sub_ui, "", 0, 0, 0, 0, false, false)
    end
end

return GameFlow
