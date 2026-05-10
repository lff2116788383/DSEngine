--------------------------------------------------------------------------------
-- KF_Framework — 敌人 Mutant AI (Phase 4)
-- AI 状态: idle → detect → chase → attack → (受击/死亡)
--------------------------------------------------------------------------------
local Config        = require("script.config")
local Audio         = require("script.audio")
local TerrainHeight = require("script.terrain_height")
local ASSET = Config.ASSET
local ENEMY = Config.ENEMY
local ui = dse.ui
local app = dse.app
local COND = {
    GREATER = Config.COND_GREATER,
    LESS    = Config.COND_LESS,
    IF      = Config.COND_IF,
    IFNOT   = Config.COND_IFNOT,
}

local ecs = dse.ecs

local Enemy = {}
Enemy.instances = {}

-- HP bar 参数 (KF: EnemyUiController)
local HP_BAR_W = 80       -- 血条宽度 (屏幕像素)
local HP_BAR_H = 8        -- 血条高度
local HP_OFFSET_Y = 400   -- 头顶偏移 (世界单位, KF: 4.0×100)
local HP_DISPLAY_DIST = 5000  -- 显示距离 (KF: 50×100)

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
        state = "idle",  -- idle / chase / attack / damaged / dead / return
        facing_yaw = 0,
        attack_cooldown = 0,
        damaged_timer = 0,
        dead_timer = 0,
        hit_player_this_attack = false,
        spawn_x = x,
        spawn_z = z,
        -- HP bar UI entities
        hp_bg = nil,
        hp_fill = nil,
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
    ecs.set_animator_3d_lock_root_motion(e, true)
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
    -- KF: follow/walk 都用 movement=0.1 → walk 动画; 高速追击用 run
    -- idle ↔ walk ↔ run 三级切换 (FSM 按注册顺序匹配, idle→run 优先于 idle→walk)
    tr("idle", "run",   0.167, false, 1.0, {{"speed", COND.GREATER, 0.5}})
    tr("idle", "walk",  0.167, false, 1.0, {{"speed", COND.GREATER, 0.05}})
    tr("walk", "idle",  0.167, false, 1.0, {{"speed", COND.LESS, 0.05}})
    tr("walk", "run",   0.167, false, 1.0, {{"speed", COND.GREATER, 0.5}})
    tr("run",  "walk",  0.167, false, 1.0, {{"speed", COND.LESS, 0.5}})
    tr("run",  "idle",  0.167, false, 1.0, {{"speed", COND.LESS, 0.05}})
    tr("idle", "punch", 0.083, false, 1.0, {{"attack", COND.IF, 0}})
    tr("walk", "punch", 0.083, false, 1.0, {{"attack", COND.IF, 0}})
    tr("run",  "punch", 0.083, false, 1.0, {{"attack", COND.IF, 0}})
    tr("punch", "idle", 0.167, true, 0.9, {})
    tr("idle", "roar",  0.167, false, 1.0, {{"roar", COND.IF, 0}})
    tr("roar", "idle",  0.167, true, 0.9, {})
    -- damage → idle handled by timer (no explicit "damaged" anim state for mutant)
    tr("idle", "dying", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    tr("walk", "dying", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    tr("run",  "dying", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    tr("punch","dying", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    tr("roar", "dying", 0.083, false, 1.0, {{"dead", COND.IF, 0}})

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
    -- KF: enemy_zombie_damaged_state.cpp:64 — kInvincibleTime=0.3s 内忽略伤害
    if data.state == "damaged" and data.damaged_timer > 0 then return end
    -- KF: actor_controller.cpp:119 — ReceiveDamage 无 defence 计算
    local dmg = amount
    data.hp = data.hp - dmg
    Audio.play_se("zombie_beat")
    if data.hp <= 0 then
        data.hp = 0
        data.state = "dead"
        ecs.set_animator_3d_param_trigger(data.entity, "dead")
        Audio.play_se("zombie_death")
    else
        data.state = "damaged"
        data.damaged_timer = 0.3  -- KF: kInvincibleTime = 0.3f
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
        if data.state == "dead" then
            -- KF: enemy_zombie_dying_state.cpp — kWaitTime=10.0s 后 SetAlive(false)
            data.dead_timer = (data.dead_timer or 0) + dt
            if data.dead_timer >= 10.0 and not data.hidden then
                -- DSE 无 SetAlive API, 移到地下隐藏
                ecs.set_transform_position(data.entity, 0, -9999, 0)
                data.hidden = true
            end
        else
            Enemy.update_one(data, dt, player_x, player_z)
        end
    end
end

function Enemy.update_one(data, dt, player_x, player_z)
    local ex, ey, ez = ecs.get_transform_position(data.entity)
    local dist = distance_xz(ex, ez, player_x, player_z)

    -- 受击硬直 (KF: ZombieImpactState — 恢复后根据距离决定下一状态)
    if data.state == "damaged" then
        data.damaged_timer = data.damaged_timer - dt
        ecs.set_animator_3d_param_float(data.entity, "speed", 0)
        if data.damaged_timer <= 0 then
            -- KF: damaged_state.cpp:47-53 — 有目标→follow, 无目标→walk
            if dist < data.warning_range then
                data.state = "chase"
            else
                data.state = "return"
                ecs.set_animator_3d_param_float(data.entity, "speed", 0.3)
            end
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
        -- KF: enemy_zombie_follow_state.cpp:42-49 — 脱离追击后进 walk (return)
        local lose_range = data.warning_range * 1.5
        if dist > lose_range then
            data.state = "return"
            ecs.set_animator_3d_param_float(data.entity, "speed", 0.3)  -- walk anim
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
    elseif data.state == "return" then
        -- KF: EnemyZombieWalkState — 向出生点移动, kMovementMultiplier=0.1, kArriveDistance=1.0
        local dx = data.spawn_x - ex
        local dz = data.spawn_z - ez
        local dist_to_spawn = math.sqrt(dx * dx + dz * dz)
        if dist_to_spawn < 100 then  -- KF: kArriveDistance=1.0 × 100
            data.state = "idle"
            ecs.set_animator_3d_param_float(data.entity, "speed", 0)
        else
            -- walk animation (speed=0.3 → walk 动画)
            ecs.set_animator_3d_param_float(data.entity, "speed", 0.3)
            dx, dz = dx / dist_to_spawn, dz / dist_to_spawn
            -- KF: walk/follow 都用 kMovementMultiplier=0.1, 即 move_speed 相同
            local spd = ENEMY.move_speed
            local new_x = ex + dx * spd * dt
            local new_z = ez + dz * spd * dt
            local new_y = TerrainHeight.get_height(new_x, new_z)
            ecs.set_transform_position(data.entity, new_x, new_y, new_z)
            local target_yaw = math.deg(math.atan(dx, dz))
            data.facing_yaw = target_yaw
            ecs.set_transform_rotation(data.entity, 0, target_yaw, 0)
        end
        -- KF: EnemyZombieWalkState::OnTrigger — return 途中发现玩家进 chase
        if dist < data.warning_range then
            data.state = "chase"
            Audio.play_se("zombie_warning")
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
            -- KF: Collider Awake/Sleep 确保每次攻击只命中一次
            if not data.hit_player_this_attack then
                local ex, _, ez = ecs.get_transform_position(data.entity)
                local dist = distance_xz(ex, ez, player_x, player_z)
                if dist < ENEMY.attack_range * 1.5 then
                    table.insert(hits, data.atk or ENEMY.attack)
                    data.hit_player_this_attack = true
                end
            end
        end
        -- 攻击结束时重置命中标记
        if data.state ~= "attack" then
            data.hit_player_this_attack = false
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

--------------------------------------------------------------------------------
-- 敌人 HP 条 UI (KF: EnemyUiController — 3D 世界空间血条)
-- 每帧根据敌人世界位置投影到屏幕空间, 更新 UI 位置/宽度/可见性
--------------------------------------------------------------------------------
function Enemy.setup_hp_bars()
    for _, data in ipairs(Enemy.instances) do
        if not data.hp_bg then
            -- 背景条 (深红)
            data.hp_bg = ecs.create_entity()
            ecs.add_transform(data.hp_bg, 0, 0, 0)
            ui.add_renderer(data.hp_bg, 0, 0.2, 0.05, 0.05, 0.7, 45, HP_BAR_W + 4, HP_BAR_H + 4)
            ui.set_anchor(data.hp_bg, 0.0, 0.0)  -- 左下角锚点 (手动定位)
            ui.set_visible(data.hp_bg, false)
        end
        if not data.hp_fill then
            -- 填充条 (红色, KF: enemy_life material = Color::kRed)
            data.hp_fill = ecs.create_entity()
            ecs.add_transform(data.hp_fill, 0, 0, 0)
            ui.add_renderer(data.hp_fill, 0, 1.0, 0.0, 0.0, 1.0, 46, HP_BAR_W, HP_BAR_H)
            ui.set_anchor(data.hp_fill, 0.0, 0.0)
            ui.set_visible(data.hp_fill, false)
        end
    end
end

function Enemy.update_hp_bars(cam_x, cam_y, cam_z)
    local screen_w = app.get_screen_width()
    local screen_h = app.get_screen_height()
    for _, data in ipairs(Enemy.instances) do
        if not data.hp_bg or not data.hp_fill then goto continue end

        -- 死亡: 隐藏 (KF: 不检查满血, 始终显示)
        if data.state == "dead" then
            ui.set_visible(data.hp_bg, false)
            ui.set_visible(data.hp_fill, false)
            goto continue
        end

        -- 距离检查: 使用摄像机位置 (KF: camera_position, kSquareDisplayDistance=50²)
        local ex, ey, ez = ecs.get_transform_position(data.entity)
        local dx = ex - cam_x
        local dy = ey - cam_y
        local dz = ez - cam_z
        local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
        if dist > HP_DISPLAY_DIST then
            ui.set_visible(data.hp_bg, false)
            ui.set_visible(data.hp_fill, false)
            goto continue
        end

        -- 投影到屏幕 (头顶位置)
        local sx, sy, vis = ecs.world_to_screen(ex, ey + HP_OFFSET_Y, ez)
        if not vis then
            ui.set_visible(data.hp_bg, false)
            ui.set_visible(data.hp_fill, false)
            goto continue
        end

        -- 透视缩放: 近处大, 远处小 (基准距离 1000)
        local scale = math.max(0.3, math.min(1.5, 1000.0 / (dist + 100)))
        local bar_w = HP_BAR_W * scale
        local bar_h = HP_BAR_H * scale

        -- 居中定位 (锚点=左下, 需偏移)
        local px_bg = sx - (bar_w + 4) * 0.5
        local py_bg = screen_h - sy - (bar_h + 4) * 0.5  -- UI Y 从底部算

        ui.set_position(data.hp_bg, px_bg, py_bg)
        ui.set_size(data.hp_bg, bar_w + 4, bar_h + 4)
        ui.set_visible(data.hp_bg, true)

        -- 填充条 (按 HP 比例缩放宽度)
        local ratio = math.max(0, data.hp / data.max_hp)
        ui.set_position(data.hp_fill, px_bg + 2, py_bg + 2)
        ui.set_size(data.hp_fill, bar_w * ratio, bar_h)
        ui.set_visible(data.hp_fill, true)

        ::continue::
    end
end

function Enemy.hide_hp_bars()
    for _, data in ipairs(Enemy.instances) do
        if data.hp_bg then ui.set_visible(data.hp_bg, false) end
        if data.hp_fill then ui.set_visible(data.hp_fill, false) end
    end
end

function Enemy.show_hp_bars()
    -- HP bars show/hide is controlled by update_hp_bars per-frame
end

return Enemy
