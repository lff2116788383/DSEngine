--------------------------------------------------------------------------------
-- KF_Framework — 敌人 Mutant AI (Phase 4)
-- AI 状态: idle → detect → chase → attack → (受击/死亡)
--------------------------------------------------------------------------------
local Config        = require("script.config")
local Audio         = require("script.audio")
local TerrainHeight = require("script.terrain_height")
local ASSET = Config.ASSET
local ENEMY = Config.ENEMY
local COND = {
    GREATER = Config.COND_GREATER,
    LESS    = Config.COND_LESS,
    IF      = Config.COND_IF,
    IFNOT   = Config.COND_IFNOT,
}

local ecs = dse.ecs

local Enemy = {}
Enemy.instances = {}

-- 单个敌人的数据
local function new_enemy_data(entity, x, z, params)
    params = params or {}
    return {
        entity = entity,
        hp = params.max_hp or ENEMY.max_hp,
        max_hp = params.max_hp or ENEMY.max_hp,
        atk = params.attack or ENEMY.attack,
        def = params.defence or ENEMY.defence,
        warning_range = params.warning_range or ENEMY.detect_range,  -- DSE units
        patrol_range = params.patrol_range or 2000,
        state = "idle",  -- idle / chase / attack / damaged / dead
        facing_yaw = 0,
        attack_cooldown = 0,
        damaged_timer = 0,
        spawn_x = x,
        spawn_z = z,
    }
end

--------------------------------------------------------------------------------
-- 创建一个 Mutant 敌人
--------------------------------------------------------------------------------
function Enemy.spawn(x, y, z, params)
    local e = ecs.create_entity()
    -- Scale=2.0: KF mutant.model RootNode scale=0.02 (same as knight)
    -- Y from terrain height (KF: enemy spawns with Y from demo.enemy, then Rigidbody falls onto terrain)
    local terrain_y = TerrainHeight.get_height(x, z)
    ecs.add_transform(e, x, terrain_y, z, 2.0, 2.0, 2.0)
    ecs.add_mesh_renderer(e, 1.0, 1.0, 1.0, 1.0)
    ecs.set_mesh_path(e, ASSET.mutant_mesh)
    ecs.set_mesh_shader_variant(e, "MESH_HALFLAMBERT")
    ecs.set_mesh_material(e, 0.0, 0.45, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    ecs.set_mesh_texture(e, "albedo", ASSET.mutant_tex_diff)
    ecs.set_mesh_texture(e, "normal", ASSET.mutant_tex_norm)

    -- Animator FSM
    ecs.add_animator_3d(e, ASSET.mutant_idle, ASSET.mutant_dskel)
    ecs.init_animator_3d_fsm(e)

    ecs.add_animator_3d_state(e, "idle",    ASSET.mutant_idle,   true,  1.0)
    ecs.add_animator_3d_state(e, "walk",    ASSET.mutant_walk,   true,  1.0)
    ecs.add_animator_3d_state(e, "run",     ASSET.mutant_run,    true,  1.0)
    ecs.add_animator_3d_state(e, "punch",   ASSET.mutant_punch,  false, 1.2)
    ecs.add_animator_3d_state(e, "swipe",   ASSET.mutant_swipe,  false, 1.0)
    ecs.add_animator_3d_state(e, "dying",   ASSET.mutant_dying,  false, 1.0)
    ecs.add_animator_3d_state(e, "roar",    ASSET.mutant_roar,   false, 1.0)

    -- Transitions
    local function tr(from, to, dur, exit, exit_t, conds)
        ecs.add_animator_3d_transition(e, from, to, dur, exit, exit_t, conds)
    end
    tr("idle", "run",   0.167, false, 1.0, {{"speed", COND.GREATER, 0.1}})
    tr("run",  "idle",  0.167, false, 1.0, {{"speed", COND.LESS, 0.1}})
    tr("idle", "punch", 0.083, false, 1.0, {{"attack", COND.IF, 0}})
    tr("run",  "punch", 0.083, false, 1.0, {{"attack", COND.IF, 0}})
    tr("punch", "idle", 0.167, true, 0.9, {})
    tr("idle", "roar",  0.167, false, 1.0, {{"roar", COND.IF, 0}})
    tr("roar", "idle",  0.167, true, 0.9, {})
    -- damage → idle handled by timer (no explicit "damaged" anim state for mutant)
    tr("idle", "dying", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    tr("run",  "dying", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    tr("punch","dying", 0.083, false, 1.0, {{"dead", COND.IF, 0}})

    ecs.set_animator_3d_state(e, "idle", 1.0, true)

    local data = new_enemy_data(e, x, z, params)
    table.insert(Enemy.instances, data)
    return data
end

--------------------------------------------------------------------------------
-- 软重置所有敌人 (回到出生点, 满血, idle 状态)
--------------------------------------------------------------------------------
function Enemy.reset_all()
    for _, data in ipairs(Enemy.instances) do
        data.hp = data.max_hp
        data.state = "idle"
        data.facing_yaw = 0
        data.attack_cooldown = 0
        data.damaged_timer = 0
        data.hit_this_attack = false
        -- 回到出生位置 (地形高度跟随)
        local sy = TerrainHeight.get_height(data.spawn_x, data.spawn_z)
        ecs.set_transform_position(data.entity, data.spawn_x, sy, data.spawn_z)
        ecs.set_transform_rotation(data.entity, 0, 0, 0)
        -- 重置动画到 idle
        ecs.set_animator_3d_state(data.entity, "idle", 1.0, true)
        ecs.set_animator_3d_param_float(data.entity, "speed", 0)
    end
end

--------------------------------------------------------------------------------
-- 对敌人造成伤害
--------------------------------------------------------------------------------
function Enemy.damage(data, amount)
    if data.state == "dead" then return end
    local dmg = math.max(1, amount - (data.def or ENEMY.defence))
    data.hp = data.hp - dmg
    Audio.play_se("zombie_beat")
    if data.hp <= 0 then
        data.hp = 0
        data.state = "dead"
        ecs.set_animator_3d_param_trigger(data.entity, "dead")
        Audio.play_se("zombie_death")
    else
        data.state = "damaged"
        data.damaged_timer = 0.5
    end
end

--------------------------------------------------------------------------------
-- 距离计算
--------------------------------------------------------------------------------
local function distance_xz(x1, z1, x2, z2)
    local dx = x1 - x2
    local dz = z1 - z2
    return math.sqrt(dx * dx + dz * dz)
end

--------------------------------------------------------------------------------
-- 全部敌人 AI 更新
--------------------------------------------------------------------------------
function Enemy.update_all(dt, player_x, player_y, player_z)
    for _, data in ipairs(Enemy.instances) do
        if data.state ~= "dead" then
            Enemy.update_one(data, dt, player_x, player_z)
        end
    end
end

function Enemy.update_one(data, dt, player_x, player_z)
    local ex, ey, ez = ecs.get_transform_position(data.entity)
    local dist = distance_xz(ex, ez, player_x, player_z)

    -- 受击硬直
    if data.state == "damaged" then
        data.damaged_timer = data.damaged_timer - dt
        ecs.set_animator_3d_param_float(data.entity, "speed", 0)
        if data.damaged_timer <= 0 then
            data.state = "idle"
        end
        return
    end

    -- 攻击冷却
    if data.attack_cooldown > 0 then
        data.attack_cooldown = data.attack_cooldown - dt
    end

    -- AI 状态切换
    if data.state == "idle" then
        ecs.set_animator_3d_param_float(data.entity, "speed", 0)
        -- KF: 检测范围为每敌人独立的 warning_range
        if dist < data.warning_range then
            data.state = "chase"
            ecs.set_animator_3d_param_trigger(data.entity, "roar")
            Audio.play_se("zombie_warning")
        end
    elseif data.state == "chase" then
        -- KF: lose_range = warning_range * 1.5
        local lose_range = data.warning_range * 1.5
        if dist > lose_range then
            data.state = "idle"
            ecs.set_animator_3d_param_float(data.entity, "speed", 0)
        elseif dist < ENEMY.attack_range and data.attack_cooldown <= 0 then
            data.state = "attack"
            ecs.set_animator_3d_param_trigger(data.entity, "attack")
            data.attack_cooldown = 1.5
        else
            -- 追踪玩家
            ecs.set_animator_3d_param_float(data.entity, "speed", 1.0)
            local dx = player_x - ex
            local dz = player_z - ez
            local len = math.sqrt(dx * dx + dz * dz)
            if len > 1 then
                dx = dx / len
                dz = dz / len
            end
            -- 移动 (地形高度跟随)
            local spd = ENEMY.move_speed
            local new_x = ex + dx * spd * dt
            local new_z = ez + dz * spd * dt
            local new_y = TerrainHeight.get_height(new_x, new_z)
            ecs.set_transform_position(data.entity, new_x, new_y, new_z)
            -- 朝向
            local target_yaw = math.deg(math.atan(dx, dz))
            data.facing_yaw = target_yaw
            ecs.set_transform_rotation(data.entity, 0, target_yaw, 0)
        end
    elseif data.state == "attack" then
        -- 攻击动画播放中，等待冷却结束回到 chase
        ecs.set_animator_3d_param_float(data.entity, "speed", 0)
        if data.attack_cooldown <= 0 then
            data.state = "chase"
        end
    end
end

--------------------------------------------------------------------------------
-- 检查敌人攻击是否命中玩家 (简单距离检测)
--------------------------------------------------------------------------------
function Enemy.check_attacks(player_x, player_z)
    local hits = {}
    for _, data in ipairs(Enemy.instances) do
        if data.state == "attack" and data.attack_cooldown > 1.2 then
            -- 攻击帧 (cooldown 刚开始的前0.3秒)
            local ex, _, ez = ecs.get_transform_position(data.entity)
            local dist = distance_xz(ex, ez, player_x, player_z)
            if dist < ENEMY.attack_range * 1.5 then
                table.insert(hits, data.atk or ENEMY.attack)
            end
        end
    end
    return hits
end

--------------------------------------------------------------------------------
-- 存活敌人数
--------------------------------------------------------------------------------
function Enemy.alive_count()
    local count = 0
    for _, data in ipairs(Enemy.instances) do
        if data.state ~= "dead" then
            count = count + 1
        end
    end
    return count
end

return Enemy
