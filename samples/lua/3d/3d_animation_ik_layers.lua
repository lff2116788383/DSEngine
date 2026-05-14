-- 3D sample: IK foot-planting + animation layer blending
-- 演示: FABRIK 双脚 IK 贴地形 / LookAt 头部跟踪 / 上半身动画层覆盖 / Root Motion API
--
-- 新 Lua API 覆盖:
--   AnimLayer: add_anim_layer_component, add_anim_layer, set_anim_layer_clip,
--              set_anim_layer_weight, set_anim_layer_bone_mask, set_anim_layer_enabled
--   IK:        add_ik_component, add_ik_chain, set_ik_target, set_ik_target_entity,
--              set_ik_weight, set_ik_pole_vector, set_ik_iterations, set_ik_enabled
--   Event:     add_animator_3d_event, pop_animator_3d_event
--   RootMotion: set_animator_3d_extract_root_motion, get_animator_3d_root_motion_delta

local AnimIKLayers3D = {}

local ecs = dse.ecs
local state = {
    character = nil,
    look_target = nil,
    camera = nil,
    time = 0.0,
    wave_weight = 0.0,
    left_foot_idx = 0,
    right_foot_idx = 1,
    head_look_idx = 2,
}

-- 资源路径 (使用 data/animation/minimal_rig 最小资源；无真实资源时用 cube rig 兜底)
local ANIM_DATA = "data/animation/minimal_rig/"
local MESH  = ANIM_DATA .. "character.dmesh"
local SKEL  = ANIM_DATA .. "character.dskel"
local IDLE  = ANIM_DATA .. "idle.danim"
local WALK  = ANIM_DATA .. "walk.danim"
local WAVE  = ANIM_DATA .. "wave.danim"

local IK_ITERS       = 12
local FOOT_OFFSET_Y  = 0.05
local LOOKAT_HEIGHT  = 1.8

--------------------------------------------------------------------------------
-- 地形高度模拟 (正弦波)
--------------------------------------------------------------------------------
local function terrain_height(x, z)
    return math.sin(x * 0.5) * 0.3 + math.cos(z * 0.7) * 0.2
end

--------------------------------------------------------------------------------
-- setup
--------------------------------------------------------------------------------
function AnimIKLayers3D.setup(config)
    -- 灯光
    local light = ecs.create_entity()
    ecs.add_directional_light_3d(light, -0.35, -1.0, -0.32, 1.0, 0.94, 0.86, 1.2, 0.18, 0.35)

    -- 地面
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, -0.1, 0, 10, 0.2, 10)
    local gv = { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
                 -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
    local gi = { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
    ecs.add_mesh_renderer(ground, 0.35, 0.40, 0.35, 1.0, gv, gi)
    ecs.set_mesh_shader_variant(ground, "MESH_LIT")

    -- 角色
    local ch = ecs.create_entity()
    ecs.add_transform(ch, 0, 0, 0, 1, 1, 1)
    ecs.add_mesh_renderer(ch, 1, 1, 1, 1)
    ecs.set_mesh_path(ch, MESH)

    -- 基础动画 FSM: idle <-> walk
    ecs.add_animator_3d(ch, IDLE, SKEL)
    ecs.init_animator_3d_fsm(ch)
    ecs.add_animator_3d_state(ch, "idle", IDLE, true, 1.0)
    ecs.add_animator_3d_state(ch, "walk", WALK, true, 1.0)
    local COND_GREATER, COND_LESS = 1, 2
    ecs.add_animator_3d_transition(ch, "idle", "walk", 0.2, false, 1.0,
        {{"speed", COND_GREATER, 0.01}})
    ecs.add_animator_3d_transition(ch, "walk", "idle", 0.2, false, 1.0,
        {{"speed", COND_LESS, 0.01}})
    ecs.set_animator_3d_state(ch, "idle", 1.0, true)

    -- 动画事件: 脚步声
    ecs.add_animator_3d_event(ch, "footstep_left",  0.25)
    ecs.add_animator_3d_event(ch, "footstep_right", 0.75)

    -- Root Motion
    ecs.set_animator_3d_extract_root_motion(ch, true)

    -- 动画层: 上半身覆盖 (挥手动画)
    ecs.add_anim_layer_component(ch)
    local layer = ecs.add_anim_layer(ch, "upper_body_wave", 0.0, 0)  -- Override, weight=0
    ecs.set_anim_layer_clip(ch, layer, WAVE, 1.0, true)
    ecs.set_anim_layer_bone_mask(ch, layer, {
        "Spine", "Spine1", "Spine2",
        "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
        "RightShoulder", "RightArm", "RightForeArm", "RightHand",
        "Neck", "Head",
    })

    -- IK 组件 + 3 条链
    ecs.add_ik_component(ch)

    state.left_foot_idx = ecs.add_ik_chain(ch, "left_foot", 0, "LeftUpLeg", "LeftFoot", 1.0)
    ecs.set_ik_iterations(ch, state.left_foot_idx, IK_ITERS)

    state.right_foot_idx = ecs.add_ik_chain(ch, "right_foot", 0, "RightUpLeg", "RightFoot", 1.0)
    ecs.set_ik_iterations(ch, state.right_foot_idx, IK_ITERS)

    state.head_look_idx = ecs.add_ik_chain(ch, "head_lookat", 1, "Neck", "Head", 0.7)

    -- LookAt 目标实体
    local target = ecs.create_entity()
    ecs.add_transform(target, 2, LOOKAT_HEIGHT, -3)
    ecs.set_ik_target_entity(ch, state.head_look_idx, target)
    state.look_target = target

    -- 摄像机
    local cam = ecs.create_entity()
    local dist = (type(config) == "table" and type(config.camera_distance) == "number")
        and config.camera_distance or 6.0
    ecs.add_transform(cam, 0, 2.5, dist, 1, 1, 1)
    ecs.set_transform_rotation(cam, -12, 0, 0)
    ecs.add_camera_3d(cam, 60, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(cam, 5.5, 0.12)
    end
    state.camera = cam

    state.character = ch
    print("[3d_animation_ik_layers] setup done: FSM + 1 anim layer + 2 foot IK + head LookAt")
end

--------------------------------------------------------------------------------
-- update
--------------------------------------------------------------------------------
function AnimIKLayers3D.update(dt)
    local ch = state.character
    if not ch then return end
    state.time = state.time + dt

    -- 自动行走 (来回切换)
    local moving = math.sin(state.time * 0.5) > -0.3
    ecs.set_animator_3d_param_float(ch, "speed", moving and 1.0 or 0.0)

    local px, py, pz = ecs.get_transform_position(ch)
    if moving then
        local spd = 1.5
        px = px + math.sin(state.time * 0.2) * spd * dt
        pz = pz + math.cos(state.time * 0.2) * spd * dt
        py = terrain_height(px, pz)
        ecs.set_transform_position(ch, px, py, pz)
    end

    -- IK 脚贴地形
    local lx, lz = px - 0.15, pz
    ecs.set_ik_target(ch, state.left_foot_idx, lx, terrain_height(lx, lz) + FOOT_OFFSET_Y, lz)
    local rx, rz = px + 0.15, pz
    ecs.set_ik_target(ch, state.right_foot_idx, rx, terrain_height(rx, rz) + FOOT_OFFSET_Y, rz)

    -- LookAt 目标绕角色旋转
    ecs.set_transform_position(state.look_target,
        px + math.cos(state.time) * 3,
        LOOKAT_HEIGHT,
        pz + math.sin(state.time) * 3)

    -- 上半身动画层: 周期性激活
    local cycle = state.time % 10.0
    local tw = (cycle > 5.0 and cycle < 8.0) and 1.0 or 0.0
    local blend_spd = 3.0
    if tw > state.wave_weight then
        state.wave_weight = math.min(state.wave_weight + blend_spd * dt, tw)
    else
        state.wave_weight = math.max(state.wave_weight - blend_spd * dt, tw)
    end
    ecs.set_anim_layer_weight(ch, 0, state.wave_weight)

    -- 动画事件轮询
    local evt = ecs.pop_animator_3d_event(ch)
    while evt and evt ~= "" do
        -- print("[event] " .. evt)
        evt = ecs.pop_animator_3d_event(ch)
    end

    -- Root Motion 读取
    local rmx, rmy, rmz = ecs.get_animator_3d_root_motion_delta(ch)
    if rmx and (math.abs(rmx) > 0.0001 or math.abs(rmz) > 0.0001) then
        -- 可选: 应用 root motion
    end
end

-- 模块入口 (与其他 3d sample 一致)
function setup(config) AnimIKLayers3D.setup(config) end
function update(dt) AnimIKLayers3D.update(dt) end

return AnimIKLayers3D
