--------------------------------------------------------------------------------
-- KF_Framework — DemoPlay 自动战斗 AI (KF: ModeDemoPlay)
-- 控制玩家自动寻敌、攻击、格挡
-- 参考: KF mode_demo_play.cpp — AI 自动操控 PlayerController
--------------------------------------------------------------------------------
local Config = require("script.config")
local Audio  = require("script.audio")
local Enemy  = require("script.enemy")

local ecs = dse.ecs
local PLAYER = Config.PLAYER

local AutoPlay = {}

local enabled = false
local attack_timer = 0      -- 攻击间隔计时
local block_timer = 0        -- 格挡持续计时
local action_timer = 0       -- 行动决策计时
local current_action = "seek" -- seek / attack / block / idle

function AutoPlay.is_enabled() return enabled end
function AutoPlay.set_enabled(v) enabled = v end
function AutoPlay.toggle() enabled = not enabled end

--------------------------------------------------------------------------------
-- 找到最近的存活敌人
--------------------------------------------------------------------------------
local function find_nearest_enemy(px, pz)
    local best = nil
    local best_dist = math.huge
    for _, data in ipairs(Enemy.instances) do
        if data.state ~= "dead" then
            local ex, _, ez = ecs.get_transform_position(data.entity)
            local dx = ex - px
            local dz = ez - pz
            local dist = math.sqrt(dx * dx + dz * dz)
            if dist < best_dist then
                best_dist = dist
                best = data
            end
        end
    end
    return best, best_dist
end

--------------------------------------------------------------------------------
-- 生成虚拟输入 (替代键盘/鼠标)
-- 返回: move_x, move_z, running, attack, block
--------------------------------------------------------------------------------
function AutoPlay.get_input(dt, knight_entity)
    if not enabled or not knight_entity then
        return 0, 0, false, false, false
    end

    attack_timer = attack_timer - dt
    action_timer = action_timer - dt

    local px, _, pz = ecs.get_transform_position(knight_entity)
    local target, dist = find_nearest_enemy(px, pz)

    -- 无敌人 → idle
    if not target then
        return 0, 0, false, false, false
    end

    local ex, _, ez = ecs.get_transform_position(target.entity)
    local dx = ex - px
    local dz = ez - pz
    local len = math.sqrt(dx * dx + dz * dz)
    local nx, nz = 0, 0
    if len > 1 then
        nx = dx / len
        nz = dz / len
    end

    local move_x, move_z = 0, 0
    local running = false
    local attack = false
    local block = false

    local attack_range = 280.0   -- 略大于战斗判定距离
    local close_range = 400.0

    if dist < attack_range then
        -- 近距离: 攻击或格挡
        if block_timer > 0 then
            block_timer = block_timer - dt
            block = true
        elseif attack_timer <= 0 then
            attack = true
            attack_timer = 0.6  -- 连击间隔
            -- 偶尔格挡 (20% 概率, 敌人在攻击时)
            if target.state == "attack" and math.random() < 0.3 then
                block_timer = 0.8
                block = true
                attack = false
            end
        end
    elseif dist < close_range then
        -- 中距离: 走向敌人
        move_x = nx
        move_z = nz
    else
        -- 远距离: 跑向敌人
        move_x = nx
        move_z = nz
        running = true
    end

    return move_x, move_z, running, attack, block
end

function AutoPlay.reset()
    attack_timer = 0
    block_timer = 0
    action_timer = 0
    current_action = "seek"
end

return AutoPlay
