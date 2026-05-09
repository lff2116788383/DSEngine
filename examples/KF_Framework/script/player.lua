--------------------------------------------------------------------------------
-- KF_Framework — 玩家 Knight (Phase 2+3)
-- 角色创建、FSM配置、输入处理、第三人称摄像机
--------------------------------------------------------------------------------
local Config   = require("script.config")
local Audio    = require("script.audio")
local AutoPlay = require("script.autoplay")
local ASSET = Config.ASSET
local CAM = Config.CAMERA
local COND = {
    GREATER = Config.COND_GREATER,
    LESS    = Config.COND_LESS,
    IF      = Config.COND_IF,
    IFNOT   = Config.COND_IFNOT,
}

local app = dse.app
local ecs = dse.ecs

local Player = {}

-- 内部状态
local knight = nil
local camera = nil
local state = {
    speed = 0.0,
    facing_yaw = 180.0,  -- face -Z (= KF's +Z forward after Z-flip)
    velocity_y = 0.0,
    grounded = true,
    hp = Config.PLAYER.max_hp,
    dead = false,
    invincible_timer = 0.0,  -- KF: kInvincibleTime = 0.5f
}

--------------------------------------------------------------------------------
-- 软重置 (用于 Result → Title → Battle 重新开始)
--------------------------------------------------------------------------------
function Player.reset()
    if not knight then return end
    state.hp = Config.PLAYER.max_hp
    state.dead = false
    state.speed = 0.0
    state.facing_yaw = 180.0
    state.velocity_y = 0.0
    state.grounded = true
    state.invincible_timer = 0.0
    -- 重置位置到 KF demo.player 初始位置
    ecs.set_transform_position(knight, -8258, 0, 9542)
    ecs.set_transform_rotation(knight, 0, 180, 0)  -- face -Z (scene forward)
    -- 重置动画到 idle
    ecs.set_animator_3d_state(knight, "idle", 1.0, true)
    ecs.set_animator_3d_param_float(knight, "speed", 0)
end

function Player.get_entity() return knight end
function Player.get_position()
    if not knight then return 0, 0, 0 end
    return ecs.get_transform_position(knight)
end
function Player.get_hp() return state.hp end
function Player.is_dead() return state.dead end
function Player.get_facing_yaw() return state.facing_yaw end

-- 攻击状态查询 (用于 Phase 5 战斗判定)
-- 返回: is_attacking, attack_range
-- KF 原始: attack collider 在 RightHand 骨骼, frame 70-80 激活
-- DSE 简化: 使用 normalized_time 窗口 [0.3, 0.6] 作为命中帧
function Player.is_attacking()
    if not knight or state.dead then return false end
    local ok, state_name, norm_time = ecs.get_animator_3d_state(knight)
    if not ok then return false end
    if state_name == "attack1" or state_name == "attack2" or state_name == "attack3"
       or state_name == "kick" then
        if norm_time >= 0.3 and norm_time <= 0.6 then
            return true
        end
    end
    return false
end

-- KF 源码: block_state.OnDamaged 不调用 ReceiveDamage, 格挡=100%免疫
-- KF 源码: impact_state.OnDamaged 有 kInvincibleTime=0.5s
-- KF 源码: ReceiveDamage 直接 life-damage, defence_未参与计算
function Player.damage(amount)
    if state.dead then return end
    if state.invincible_timer > 0 then return end

    -- 格挡检测: KF block_state.OnDamaged → 不扣血, 播放 block+guard_voice
    local ok, anim_state = ecs.get_animator_3d_state(knight)
    if ok and (anim_state == "block" or anim_state == "block_idle") then
        Audio.play_se("block")
        Audio.play_se("guard_voice")
        state.invincible_timer = Config.PLAYER.invincible_time
        return
    end

    -- 非格挡: 直接扣血 (KF: ReceiveDamage 无 defence 计算)
    local dmg = amount
    state.hp = state.hp - dmg
    state.invincible_timer = Config.PLAYER.invincible_time  -- 受击无敌 0.5s
    if state.hp <= 0 then
        state.hp = 0
        state.dead = true
        ecs.set_animator_3d_param_trigger(knight, "dead")
        Audio.play_se("death_voice")
    else
        ecs.set_animator_3d_param_trigger(knight, "damaged")
        -- KF: impact_state.Init → Play(kZombieBeatSe) + Play(kDamageVoice1Se + rand(0,2))
        Audio.play_se("zombie_beat")
        Audio.play_damage_voice()
    end
end

--------------------------------------------------------------------------------
-- 创建 Knight + Camera
--------------------------------------------------------------------------------
function Player.setup()
    -- Knight entity
    knight = ecs.create_entity()
    -- KF demo.player: (-82.5767, 0, -95.4176) → DSE: ×100, z取反
    ecs.add_transform(knight, -8258, 0, 9542)
    ecs.set_transform_rotation(knight, 0, 180, 0)  -- face -Z (scene forward after Z-flip)
    ecs.add_mesh_renderer(knight, 1.0, 1.0, 1.0, 1.0)
    ecs.set_mesh_path(knight, ASSET.knight_mesh)
    ecs.set_mesh_shader_variant(knight, "MESH_HALFLAMBERT")
    ecs.set_mesh_material(knight, 0.0, 0.45, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    ecs.set_mesh_texture(knight, "albedo", ASSET.knight_tex_diff)
    ecs.set_mesh_texture(knight, "normal", ASSET.knight_tex_norm)
    ecs.set_mesh_texture(knight, "metallic_roughness", ASSET.knight_tex_spec)

    -- Animator FSM
    ecs.add_animator_3d(knight, ASSET.anim_idle, ASSET.knight_dskel)
    ecs.init_animator_3d_fsm(knight)

    -- States
    ecs.add_animator_3d_state(knight, "idle",       ASSET.anim_idle,       true,  1.0)
    ecs.add_animator_3d_state(knight, "walk",       ASSET.anim_walk,       true,  1.0)
    ecs.add_animator_3d_state(knight, "run",        ASSET.anim_run,        true,  1.2)
    ecs.add_animator_3d_state(knight, "attack1",    ASSET.anim_attack1,    false, 1.3)
    ecs.add_animator_3d_state(knight, "attack2",    ASSET.anim_attack2,    false, 1.3)
    ecs.add_animator_3d_state(knight, "attack3",    ASSET.anim_attack3,    false, 1.4)
    ecs.add_animator_3d_state(knight, "block",      ASSET.anim_block,      false, 1.0)
    ecs.add_animator_3d_state(knight, "block_idle", ASSET.anim_block_idle, true,  1.0)
    ecs.add_animator_3d_state(knight, "jump",       ASSET.anim_jump,       false, 1.0)
    ecs.add_animator_3d_state(knight, "impact",     ASSET.anim_impact,     false, 1.0)
    ecs.add_animator_3d_state(knight, "death",      ASSET.anim_death,      false, 1.0)
    ecs.add_animator_3d_state(knight, "cast",       ASSET.anim_cast,       false, 1.0)
    ecs.add_animator_3d_state(knight, "kick",       ASSET.anim_kick,       false, 1.2)

    -- Transitions
    local function tr(from, to, dur, exit, exit_t, conds)
        ecs.add_animator_3d_transition(knight, from, to, dur, exit, exit_t, conds)
    end
    -- idle ↔ walk ↔ run
    tr("idle", "walk", 0.167, false, 1.0, {{"speed", COND.GREATER, 0.1}})
    tr("walk", "idle", 0.167, false, 1.0, {{"speed", COND.LESS, 0.1}})
    tr("walk", "run",  0.167, false, 1.0, {{"speed", COND.GREATER, 0.7}})
    tr("run",  "walk", 0.167, false, 1.0, {{"speed", COND.LESS, 0.7}})
    -- attack
    tr("idle", "attack1", 0.083, false, 1.0, {{"attack", COND.IF, 0}})
    tr("walk", "attack1", 0.083, false, 1.0, {{"attack", COND.IF, 0}})
    tr("run",  "attack1", 0.083, false, 1.0, {{"attack", COND.IF, 0}})
    tr("attack1", "attack2", 0.02, true, 0.85, {{"attack", COND.IF, 0}})
    tr("attack2", "attack3", 0.02, true, 0.85, {{"attack", COND.IF, 0}})
    tr("attack1", "idle", 0.167, true, 0.95, {})
    tr("attack2", "idle", 0.167, true, 0.95, {})
    tr("attack3", "idle", 0.167, true, 0.95, {})
    -- block
    tr("idle", "block", 0.083, false, 1.0, {{"block", COND.IF, 0}})
    tr("walk", "block", 0.083, false, 1.0, {{"block", COND.IF, 0}})
    tr("block", "block_idle", 0.083, true, 0.9, {})
    tr("block_idle", "idle", 0.167, false, 1.0, {{"blocking", COND.LESS, 0.5}})
    -- jump
    tr("idle", "jump", 0.167, false, 1.0, {{"jump", COND.IF, 0}})
    tr("walk", "jump", 0.167, false, 1.0, {{"jump", COND.IF, 0}})
    tr("run",  "jump", 0.167, false, 1.0, {{"jump", COND.IF, 0}})
    tr("jump", "idle", 0.167, true, 0.9, {})
    -- impact / death
    tr("idle", "impact", 0.083, false, 1.0, {{"damaged", COND.IF, 0}})
    tr("walk", "impact", 0.083, false, 1.0, {{"damaged", COND.IF, 0}})
    tr("run",  "impact", 0.083, false, 1.0, {{"damaged", COND.IF, 0}})
    tr("impact", "idle", 0.167, true, 0.9, {})
    tr("idle",   "death", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    tr("walk",   "death", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    tr("run",    "death", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    tr("impact", "death", 0.083, false, 1.0, {{"dead", COND.IF, 0}})
    -- kick / cast
    tr("idle", "kick", 0.083, false, 1.0, {{"kick", COND.IF, 0}})
    tr("kick", "idle", 0.167, true, 0.9, {})
    tr("idle", "cast", 0.083, false, 1.0, {{"cast", COND.IF, 0}})
    tr("cast", "idle", 0.167, true, 0.95, {})

    ecs.set_animator_3d_state(knight, "idle", 1.0, true)

    -- Camera (第三人称跟随 — KF rig/pivot 初始位置)
    -- KF: eye = pivot + RotX(pitch) * (0,0,-dist)
    -- DSE: behind_dist = dist*cos(15°) ≈ 483, up_extra = dist*sin(15°) ≈ 129
    camera = ecs.create_entity()
    local pitch_rad = math.rad(-CAM.pitch)
    local init_behind = CAM.distance * math.cos(pitch_rad)
    local init_up = CAM.distance * math.sin(pitch_rad)
    ecs.add_transform(camera, -8258, CAM.offset_y + init_up, 9542 + init_behind)
    ecs.set_transform_rotation(camera, CAM.pitch, 0, 0)  -- 面朝-Z (DSE默认, 场景前方)
    ecs.add_camera_3d(camera, CAM.fov, 0, CAM.near_clip, CAM.far_clip)

    print("[KF_Framework] Knight + Camera ready. 13 states configured.")
end

--------------------------------------------------------------------------------
-- 每帧更新: 输入 → 移动 → FSM → 摄像机跟随
--------------------------------------------------------------------------------
function Player.update(dt)
    if not knight or state.dead then return end
    if dt > 0.1 then dt = 0.1 end

    -- 受击无敌计时器
    if state.invincible_timer > 0 then
        state.invincible_timer = state.invincible_timer - dt
    end

    -- 输入 (手动 or AutoPlay AI)
    local move_x, move_z = 0, 0
    local running = false
    local ai_attack, ai_block = false, false
    if AutoPlay.is_enabled() then
        move_x, move_z, running, ai_attack, ai_block = AutoPlay.get_input(dt, knight)
    else
        if app.get_key(87)  then move_z = -1 end  -- W (forward = -Z in DSE = +Z in KF)
        if app.get_key(83)  then move_z =  1 end  -- S (backward = +Z in DSE)
        if app.get_key(65)  then move_x = -1 end  -- A
        if app.get_key(68)  then move_x =  1 end  -- D
        running = app.get_key(340) or app.get_key(344)
    end
    local raw_speed = math.sqrt(move_x * move_x + move_z * move_z)

    -- 归一化速度
    local norm_speed = 0.0
    if raw_speed > 0.01 then
        norm_speed = running and 1.0 or 0.5
    end
    state.speed = norm_speed
    ecs.set_animator_3d_param_float(knight, "speed", norm_speed)

    -- 移动 + 朝向
    if raw_speed > 0.01 then
        local nx = move_x / raw_speed
        local nz = move_z / raw_speed
        -- 朝向插值
        local target_yaw = math.deg(math.atan(nx, nz))
        local yaw_diff = target_yaw - state.facing_yaw
        while yaw_diff > 180 do yaw_diff = yaw_diff - 360 end
        while yaw_diff < -180 do yaw_diff = yaw_diff + 360 end
        local max_turn = Config.PLAYER.turn_speed * dt
        if math.abs(yaw_diff) < max_turn then
            state.facing_yaw = target_yaw
        else
            state.facing_yaw = state.facing_yaw + max_turn * (yaw_diff > 0 and 1 or -1)
        end
        -- 位移
        local spd = running and Config.PLAYER.run_speed or Config.PLAYER.move_speed
        local px, py, pz = ecs.get_transform_position(knight)
        ecs.set_transform_position(knight, px + nx * spd * dt, py, pz + nz * spd * dt)
    end
    ecs.set_transform_rotation(knight, 0, state.facing_yaw, 0)

    -- 简易跳跃物理
    if app.get_key_down(32) and state.grounded then
        state.velocity_y = Config.PLAYER.jump_speed
        state.grounded = false
        ecs.set_animator_3d_param_trigger(knight, "jump")
    end
    if not state.grounded then
        state.velocity_y = state.velocity_y + Config.PLAYER.gravity * dt
        local px, py, pz = ecs.get_transform_position(knight)
        local new_y = py + state.velocity_y * dt
        if new_y <= 0 then
            new_y = 0
            state.velocity_y = 0
            state.grounded = true
        end
        ecs.set_transform_position(knight, px, new_y, pz)
    end

    -- 攻击/格挡/技能触发器
    local do_attack = ai_attack or app.get_mouse_left_down()
    local do_block_down = ai_block or app.get_mouse_right_down()
    local do_block_hold = ai_block or app.get_mouse_right()
    if do_attack then
        ecs.set_animator_3d_param_trigger(knight, "attack")
        -- KF 源码: 攻击语音在各 step state 的 kBeginAttackFrame 触发
        -- 此处简化: 触发时根据当前动画状态推断 step
        local ok2, cur_anim = ecs.get_animator_3d_state(knight)
        local step = 1
        if ok2 then
            if cur_anim == "attack1" then step = 2
            elseif cur_anim == "attack2" then step = 3
            end
        end
        Audio.play_se("sord_attack")
        Audio.play_attack_voice(step)
    end
    if do_block_down then
        ecs.set_animator_3d_param_trigger(knight, "block")
        Audio.play_se("block")
    end
    local blocking = do_block_hold and 1.0 or 0.0
    ecs.set_animator_3d_param_float(knight, "blocking", blocking)
    if app.get_key_down(81) then ecs.set_animator_3d_param_trigger(knight, "kick") end
    if app.get_key_down(69) then ecs.set_animator_3d_param_trigger(knight, "cast") end

    -- 第三人称摄像机跟随
    Player.update_camera(dt)
end

--------------------------------------------------------------------------------
-- 第三人称摄像机 (KF: ThirdPersonCamera — rig/pivot 层级)
-- KF 原始结构:
--   rig: position = player_pos(lerped), rotation.y = yaw
--   pivot: position.y = kOffsetY(3.5), rotation.x = kPitchDefault(15°)
--   local_eye = (0, 0, -distance)  →  eye orbits around pivot
-- DSE 转换: ×100, Z取反, RH
--------------------------------------------------------------------------------
function Player.update_camera(dt)
    if not camera or not knight then return end
    local px, py, pz = ecs.get_transform_position(knight)

    -- KF rig/pivot 计算 (转换到 DSE)
    -- pitch 将 distance 分解为水平和垂直分量
    local pitch_rad = math.rad(-CAM.pitch)  -- CAM.pitch=-15, 取绝对值15°
    local behind_dist = CAM.distance * math.cos(pitch_rad)  -- 水平距离 (后方)
    local up_extra    = CAM.distance * math.sin(pitch_rad)  -- 额外高度 (pitch 抬升)

    -- 目标位置: 角色后方 + 上方 (绕角色朝向旋转)
    -- 我们的游戏: yaw=0 → 面朝+Z, "后方" = -Z
    local yaw_rad = math.rad(state.facing_yaw)
    local cam_x = px - math.sin(yaw_rad) * behind_dist
    local cam_y = py + CAM.offset_y + up_extra
    local cam_z = pz - math.cos(yaw_rad) * behind_dist

    -- 平滑插值 (KF: kMoveLerpTime = 0.075)
    local cx, cy, cz = ecs.get_transform_position(camera)
    local t = math.min(1.0, CAM.lerp_speed * dt)
    local new_x = cx + (cam_x - cx) * t
    local new_y = cy + (cam_y - cy) * t
    local new_z = cz + (cam_z - cz) * t

    ecs.set_transform_position(camera, new_x, new_y, new_z)

    -- look-at: 朝向 pivot 点 (player + offset_y)
    -- DSE 摄像机默认朝向 -Z, 所以 yaw 需要用 atan(-dx, -dz) 使其转180°
    local look_y = py + CAM.offset_y
    local dx = px - new_x
    local dy = look_y - new_y
    local dz = pz - new_z
    local dist_xz = math.sqrt(dx * dx + dz * dz)
    local cam_yaw = math.deg(math.atan(-dx, -dz))
    local cam_pitch = math.deg(math.atan(dy, dist_xz))

    ecs.set_transform_rotation(camera, cam_pitch, cam_yaw, 0)
end

return Player
