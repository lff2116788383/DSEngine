--------------------------------------------------------------------------------
-- IK 脚贴地形 + 动画层混合 Lua Demo
--
-- 演示内容:
-- 1. 基础行走动画 (Animator3D FSM)
-- 2. 上半身覆盖层 (AnimLayer bone mask + additive blend)
-- 3. 双脚 IK 贴地形 (FABRIK)
-- 4. 头部 LookAt IK (跟踪目标物体)
--
-- 用法: 在 DSEngine 中加载此项目目录即可运行
--------------------------------------------------------------------------------
local ecs = dse.ecs
local app = dse.app

--------------------------------------------------------------------------------
-- 配置
--------------------------------------------------------------------------------
local CONFIG = {
    -- 角色
    mesh_path    = "assets/character.dmesh",
    skel_path    = "assets/character.dskel",
    anim_idle    = "assets/idle.danim",
    anim_walk    = "assets/walk.danim",
    anim_wave    = "assets/wave.danim",      -- 上半身挥手
    -- IK
    ik_iterations = 12,
    ik_tolerance  = 0.005,
    foot_offset_y = 5.0,   -- 脚底偏移（避免脚陷入地面）
    -- LookAt
    look_target_height = 180.0,
}

--------------------------------------------------------------------------------
-- 实体
--------------------------------------------------------------------------------
local character = nil
local look_target = nil
local camera = nil

--------------------------------------------------------------------------------
-- 初始化
--------------------------------------------------------------------------------
function setup()
    -- 创建角色
    character = ecs.create_entity()
    ecs.add_transform(character, 0, 0, 0, 1, 1, 1)
    ecs.add_mesh_renderer(character, 1, 1, 1, 1)
    ecs.set_mesh_path(character, CONFIG.mesh_path)

    -- 基础动画 FSM
    ecs.add_animator_3d(character, CONFIG.anim_idle, CONFIG.skel_path)
    ecs.init_animator_3d_fsm(character)
    ecs.add_animator_3d_state(character, "idle", CONFIG.anim_idle, true, 1.0)
    ecs.add_animator_3d_state(character, "walk", CONFIG.anim_walk, true, 1.0)
    ecs.add_animator_3d_transition(character, "idle", "walk", 0.2, false, 1.0,
        {{"speed", 1, 0.01}})  -- COND_GREATER=1
    ecs.add_animator_3d_transition(character, "walk", "idle", 0.2, false, 1.0,
        {{"speed", 2, 0.01}})  -- COND_LESS=2
    ecs.set_animator_3d_state(character, "idle", 1.0, true)

    -- 动画层: 上半身覆盖 (挥手)
    ecs.add_anim_layer_component(character)
    local layer_idx = ecs.add_anim_layer(character, "upper_body_wave", 0.0, 0)
    -- Override mode=0, weight 初始=0 (通过输入激活)
    ecs.set_anim_layer_clip(character, layer_idx, CONFIG.anim_wave, 1.0, true)
    ecs.set_anim_layer_bone_mask(character, layer_idx, {
        "Spine", "Spine1", "Spine2",
        "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
        "RightShoulder", "RightArm", "RightForeArm", "RightHand",
        "Neck", "Head",
    })

    -- IK 组件
    ecs.add_ik_component(character)

    -- 左脚 FABRIK IK
    local left_foot = ecs.add_ik_chain(character,
        "left_foot",         -- name
        0,                   -- type: FABRIK=0
        "LeftUpLeg",         -- root bone
        "LeftFoot",          -- tip bone
        1.0                  -- weight
    )
    ecs.set_ik_iterations(character, left_foot, CONFIG.ik_iterations)

    -- 右脚 FABRIK IK
    local right_foot = ecs.add_ik_chain(character,
        "right_foot",
        0,                   -- FABRIK
        "RightUpLeg",
        "RightFoot",
        1.0
    )
    ecs.set_ik_iterations(character, right_foot, CONFIG.ik_iterations)

    -- 头部 LookAt IK
    local head_look = ecs.add_ik_chain(character,
        "head_lookat",
        1,                   -- LookAt=1
        "Neck",
        "Head",
        0.7                  -- 柔和权重
    )

    -- 创建 LookAt 目标实体
    look_target = ecs.create_entity()
    ecs.add_transform(look_target, 200, CONFIG.look_target_height, -300)
    ecs.set_ik_target_entity(character, head_look, look_target)

    -- 摄像机
    camera = ecs.create_entity()
    ecs.add_transform(camera, 0, 200, 500)
    ecs.set_transform_rotation(camera, -10, 180, 0)
    ecs.add_camera_3d(camera, 60, 0, 1, 10000)

    print("[IK Demo] Setup complete: character + 2 foot IK + head LookAt + upper body layer")
end

--------------------------------------------------------------------------------
-- 地形高度模拟 (简单正弦波)
--------------------------------------------------------------------------------
local function get_terrain_height(x, z)
    return math.sin(x * 0.01) * 20.0 + math.cos(z * 0.015) * 15.0
end

--------------------------------------------------------------------------------
-- 每帧更新
--------------------------------------------------------------------------------
local time_acc = 0.0
local wave_active = false
local left_foot_idx = 0
local right_foot_idx = 1
local head_look_idx = 2

function update(dt)
    if not character then return end
    time_acc = time_acc + dt

    -- 简单自动行走 (来回)
    local walk_speed = 100.0
    local px, py, pz = ecs.get_transform_position(character)
    local moving = math.sin(time_acc * 0.5) > -0.3  -- 大部分时间在走
    local speed_val = moving and 1.0 or 0.0
    ecs.set_animator_3d_param_float(character, "speed", speed_val)

    if moving then
        local dir_z = math.cos(time_acc * 0.2) * walk_speed * dt
        local dir_x = math.sin(time_acc * 0.2) * walk_speed * dt
        px = px + dir_x
        pz = pz + dir_z
        local ground_y = get_terrain_height(px, pz)
        ecs.set_transform_position(character, px, ground_y, pz)
    end

    -- IK 脚贴地形
    local px2, py2, pz2 = ecs.get_transform_position(character)

    -- 左脚目标: 角色位置偏左 + 地形高度
    local lx = px2 - 20
    local lz = pz2
    local ly = get_terrain_height(lx, lz) + CONFIG.foot_offset_y
    ecs.set_ik_target(character, left_foot_idx, lx, ly, lz)

    -- 右脚目标: 角色位置偏右 + 地形高度
    local rx = px2 + 20
    local rz = pz2
    local ry = get_terrain_height(rx, rz) + CONFIG.foot_offset_y
    ecs.set_ik_target(character, right_foot_idx, rx, ry, rz)

    -- LookAt 目标绕角色旋转
    local target_x = px2 + math.cos(time_acc) * 300
    local target_z = pz2 + math.sin(time_acc) * 300
    ecs.set_transform_position(look_target, target_x, CONFIG.look_target_height, target_z)

    -- 动画层: 每隔 5 秒激活上半身挥手
    local cycle = time_acc % 10.0
    local target_weight = (cycle > 5.0 and cycle < 8.0) and 1.0 or 0.0
    -- 平滑过渡权重
    local current_w = wave_active and 1.0 or 0.0
    local blend_speed = 3.0
    if target_weight > current_w then
        current_w = math.min(current_w + blend_speed * dt, target_weight)
    else
        current_w = math.max(current_w - blend_speed * dt, target_weight)
    end
    wave_active = current_w > 0.01
    ecs.set_anim_layer_weight(character, 0, current_w)

    -- Root Motion 读取 (仅演示 API)
    local rmx, rmy, rmz = ecs.get_animator_3d_root_motion_delta(character)
    if rmx and (math.abs(rmx) > 0.001 or math.abs(rmz) > 0.001) then
        -- 如果有 root motion 可以应用到角色位置
        -- ecs.set_transform_position(character, px2 + rmx, py2 + rmy, pz2 + rmz)
    end
end
