# DSEngine 3C System Design Document

> Character + Camera + Control 统一框架设计方案
> 
> **版本**: v2.0 (修订版 — 修正审视发现的架构问题)

## 1. 设计目标

在 DSEngine 现有 ECS 架构上，构建一套 **开箱即用的 3C 框架**，使开发者通过挂载组件 + 简单配置即可获得完整的角色控制、相机跟随、输入管线能力，无需从零在 Lua 中拼装分散的底层 API。

### 对标 UE 的映射关系

| UE 概念 | DSEngine 3C 对应 |
|:--------|:----------------|
| `ACharacter` + `UCharacterMovementComponent` | `CharacterMovementConfig` + `CharacterMovementState` + `CharacterMovementSystem` |
| `USpringArmComponent` + `UCameraComponent` | `SpringArm3DComponent` + `CameraArm3DSystem` |
| `APlayerController` + `Enhanced Input` | `PlayerControllerComponent` + `PlayerControllerSystem` |
| `UCharacterMovementComponent::MovementMode` | `MovementMode` 枚举（含 Crouching） |
| `ACharacter::Crouch()` | `MovementMode::Crouching` + 动态胶囊高度调整 |
| `UCharacterMovementComponent::bOrientRotationToMovement` | `RotationMode::OrientToMovement` |
| `ACharacter::bUseControllerRotationYaw` | `RotationMode::OrientToCamera` |
| Possess/Unpossess | `PlayerControllerComponent::camera_entity`（挂在角色实体上） |

### 设计原则

1. **纯 ECS 组件驱动** — 不引入继承层级；所有功能通过组件组合实现
2. **与现有系统零侵入** — 新增文件，不修改现有 `CharacterController3DComponent`、`Physics3DSystem`、`Input` 等
3. **分层解耦** — Movement 不依赖 Camera，Camera 不依赖 Input；PlayerController 是可选的粘合层
4. **Lua 一等公民** — 所有组件字段通过 codegen 暴露 Lua getter/setter
5. **可序列化** — 所有组件字段为 POD/简单类型，兼容现有 scene_json_codec 反射
6. **Config/State 分离** — 配置参数（少改动）与运行时状态（每帧读写）拆分为独立组件，优化 ECS 缓存局部性

---

## 2. 文件结构

```
modules/gameplay_3d/
├── character/                          ← Phase 1: Character Movement
│   ├── character_movement_system.h
│   └── character_movement_system.cpp
├── camera/
│   ├── free_camera_controller_system.h     (已有)
│   ├── free_camera_controller_system.cpp   (已有)
│   ├── camera_arm_3d_system.h          ← Phase 2: Spring Arm Camera
│   └── camera_arm_3d_system.cpp
├── player/                             ← Phase 3: Player Controller
│   ├── player_controller_system.h
│   └── player_controller_system.cpp
...

engine/ecs/
├── components_3d_character.h           ← 所有 3C 组件定义（新文件）
├── components_3d.h                     ← 新增 #include "components_3d_character.h"
...

engine/scripting/
├── native_api/
│   ├── dse_api_character_movement.gen.cpp   ← codegen C ABI
│   ├── dse_api_spring_arm.gen.cpp
│   └── dse_api_player_controller.gen.cpp
├── lua/bindings/
│   ├── lua_binding_ecs_character_movement.gen.cpp
│   ├── lua_binding_ecs_spring_arm.gen.cpp
│   └── lua_binding_ecs_player_controller.gen.cpp

tools/codegen/
├── binding_defs.json                   ← 追加 4 个组件定义（Config + State + SpringArm + PlayerCtrl）

tests/gtest/unit/engine/character/
├── character_movement_test.cpp         ← 单元测试
├── spring_arm_test.cpp
├── player_controller_test.cpp

tests/gtest/integration/engine/character/
├── character_3c_integration_test.cpp   ← 集成测试
```

---

## 3. Phase 1: CharacterMovementSystem

### 3.1 组件定义

```cpp
// engine/ecs/components_3d_character.h

#ifndef DSE_COMPONENTS_3D_CHARACTER_H
#define DSE_COMPONENTS_3D_CHARACTER_H

#include <glm/glm.hpp>
#include <string>
#include <cstdint>
#include <entt/entt.hpp>

namespace dse {

// ============================================================
// 枚举定义
// ============================================================

/// 角色移动模式状态机
enum class MovementMode : uint8_t {
    Walking    = 0,   // 地面行走（含跑步，由 speed 控制）
    Sprinting  = 1,   // 冲刺（独立模式，非 bool 标志）
    Crouching  = 2,   // 蹲伏（动态缩小碰撞胶囊）
    Falling    = 3,   // 空中下落（含跳跃上升阶段）
    Swimming   = 4,   // 水中游泳
    Flying     = 5,   // 自由飞行（无重力）
    Custom     = 6,   // 用户自定义（Lua 驱动）
};

/// 角色旋转模式
enum class RotationMode : uint8_t {
    OrientToMovement = 0,  // 角色面向移动方向（第三人称探索模式）
    OrientToCamera   = 1,  // 角色面向相机方向（战斗/瞄准/射击模式）
};

// ============================================================
// CharacterMovementConfig — 角色移动配置参数（少改动）
// ============================================================
//
// 存放移动相关的配置/调参数据。游戏运行中很少修改。
// 与 CharacterMovementState 拆分以优化 ECS 缓存局部性：
// System 主循环高频读写 State，仅按需读取 Config。
//
// 依赖：实体必须同时挂载 CharacterController3DComponent + TransformComponent
//       + CharacterMovementState
//
struct CharacterMovementConfig {
    bool enabled = true;

    // ── 地面移动参数 ──
    float max_walk_speed      = 6.0f;    // 行走最大速度 (m/s)
    float max_sprint_speed    = 10.0f;   // 冲刺最大速度 (m/s)
    float max_crouch_speed    = 3.0f;    // 蹲伏最大速度 (m/s)
    float ground_acceleration = 20.0f;   // 地面加速度 (m/s^2)
    float ground_deceleration = 12.0f;   // 地面减速度（无输入时）
    float ground_friction     = 8.0f;    // 地面摩擦系数

    // ── 蹲伏参数 ──
    float crouch_height       = 0.5f;    // 蹲伏时胶囊高度（覆盖 CC3D.height）
    float stand_height        = 1.0f;    // 站立胶囊高度（初始值与 CC3D.height 同步）

    // ── 空中参数 ──
    float gravity             = -19.62f; // 重力加速度（Y 轴，默认 2x 真实重力，手感更好）
    float max_fall_speed      = -30.0f;  // 最大下落速度（负值）
    float air_control         = 0.3f;    // 空中操控比例 [0,1]（1=完全操控，0=无操控）
    float jump_velocity       = 8.0f;    // 起跳速度 (m/s)
    int   max_jump_count      = 2;       // 最大跳跃次数（1=单跳，2=二段跳）
    float coyote_time         = 0.1f;    // 离地后仍可跳跃的宽限时间 (秒)
    float jump_buffer_time    = 0.15f;   // 落地前预输入跳跃的缓冲时间 (秒)

    // ── 游泳参数 ──
    float swim_speed          = 4.0f;    // 水中最大速度
    float swim_acceleration   = 8.0f;    // 水中加速度
    float buoyancy            = 1.0f;    // 浮力（抵消重力的比例，1.0=完全抵消）

    // ── 飞行参数 ──
    float fly_speed           = 8.0f;
    float fly_acceleration    = 10.0f;

    // ── 旋转 ──
    RotationMode rotation_mode = RotationMode::OrientToMovement;
    float rotation_rate       = 720.0f;  // 旋转速度 (度/秒)

    // ── 事件控制 ──
    bool publish_events       = true;    // 是否发布 Landed/Jumped/ModeChanged 事件
                                         // 玩家角色设为 true，大量 NPC 设为 false 以避免事件风暴
};

// ============================================================
// CharacterMovementState — 角色移动运行时状态（每帧读写）
// ============================================================
//
// 存放每帧需要高频读写的运行时数据。
// System 主循环遍历此组件 + CharacterController3DComponent + TransformComponent。
//
struct CharacterMovementState {
    // ── 移动模式 ──
    MovementMode movement_mode = MovementMode::Walking;

    // ── 输入向量（由 PlayerControllerSystem 或 Lua 每帧写入） ──
    glm::vec3 input_direction  = glm::vec3(0.0f); // 世界空间移动意图方向（归一化）
    bool      input_jump       = false;            // 跳跃请求（脉冲，单帧有效）
    bool      input_sprint     = false;            // 冲刺请求（持续）
    bool      input_crouch     = false;            // 蹲伏请求（切换式）

    // ── 运行时状态（由 System 写入，外部只读） ──
    glm::vec3 velocity         = glm::vec3(0.0f);  // 当前速度
    bool      is_grounded      = false;             // 是否着地
    bool      is_jumping       = false;             // 是否在跳跃上升阶段
    int       jump_count       = 0;                 // 当前已跳跃次数
    float     fall_time        = 0.0f;              // 持续下落时间

    // ── 内部计时器（System 内部使用） ──
    float coyote_timer_        = 0.0f;
    float jump_buffer_timer_   = 0.0f;
};

// ============================================================
// SpringArm3DComponent — 弹簧臂相机
// ============================================================
//
// 挂载在相机实体上。System 将根据 target 实体的位置 + arm 参数
// 计算相机的最终位置和旋转。
//
// 依赖：相机实体需有 TransformComponent + Camera3DComponent
//       target 实体需有 TransformComponent
//
struct SpringArm3DComponent {
    bool enabled = true;

    // ── 目标 ──
    entt::entity target_entity = entt::null; // 跟随的目标实体
    glm::vec3 target_offset    = glm::vec3(0.0f, 1.6f, 0.0f); // 目标点偏移（本地空间，默认头顶）

    // ── 臂参数 ──
    float arm_length           = 4.0f;   // 弹簧臂长度（米）
    float min_arm_length       = 0.5f;   // 碰撞避让最小臂长
    float max_arm_length       = 10.0f;  // 最大臂长（滚轮缩放用）

    // ── 碰撞避让 ──
    bool  collision_test       = true;   // 是否启用碰撞检测（无物理环境时自动降级为 false）
    float probe_radius         = 0.2f;   // 碰撞探针半径
    uint16_t collision_mask    = 0xFFFF; // 碰撞检测层掩码

    // ── 旋转控制 ──
    float pitch                = -20.0f; // 俯仰角 (度，负值=俯视)
    float yaw                  = 0.0f;   // 偏航角 (度)
    float min_pitch            = -80.0f; // 最小俯仰
    float max_pitch            = 80.0f;  // 最大俯仰
    float rotation_speed       = 0.15f;  // 鼠标旋转灵敏度

    // ── 平滑/延迟 ──
    float position_lag_speed   = 10.0f;  // 位置插值速度（0=无延迟，越大越快追上）
    float rotation_lag_speed   = 10.0f;  // 旋转插值速度

    // ── 视角模式 ──
    enum class ViewMode : uint8_t {
        ThirdPerson = 0,
        FirstPerson = 1,
    };
    ViewMode view_mode         = ViewMode::ThirdPerson;
    float first_person_fov     = 75.0f;  // 第一人称 FOV
    float third_person_fov     = 60.0f;  // 第三人称 FOV
    float fov_transition_speed = 8.0f;   // FOV 切换插值速度

    // ── 屏幕震动（3D 扩展） ──
    float shake_trauma         = 0.0f;   // 震动强度 [0,1]
    float shake_decay_rate     = 1.5f;   // 衰减率
    float shake_max_offset     = 0.3f;   // 最大位移偏移 (米)
    float shake_max_rotation   = 3.0f;   // 最大旋转偏移 (度)
    float shake_frequency      = 15.0f;  // 震动频率

    // ── 运行时状态 ──
    float current_arm_length_  = 4.0f;   // 碰撞避让后的实际臂长
    glm::vec3 current_pivot_   = glm::vec3(0.0f); // 当前平滑后的旋转中心
    float shake_time_acc_      = 0.0f;   // 震动时间累加器
};

// ============================================================
// PlayerControllerComponent — 玩家控制器
// ============================================================
//
// 粘合层：读取 Input → 转换为 CharacterMovement 输入向量 + Camera 旋转控制。
//
// 设计决策（v2 修订）：PlayerController 直接挂在角色实体上，
// 而非独立实体。减少一次实体查找，简化 Possess/Unpossess。
// camera_entity 字段指向关联的相机实体。
//
// 挂载位置：角色实体（与 CharacterMovementConfig/State 同一实体）
//
struct PlayerControllerComponent {
    bool enabled = true;

    // ── 关联相机 ──
    entt::entity camera_entity = entt::null; // 关联的相机实体（挂载 SpringArm3DComponent）

    // ── 输入映射 Action 名 ──
    //  与 ActionMapping 系统联动。若 action 名为空则使用默认键位。
    //  默认键位: WASD 移动, Space 跳跃, LShift 冲刺, LCtrl 蹲伏, 鼠标右键旋转相机
    std::string action_move_forward = "MoveForward";   // W/S 或左摇杆 Y
    std::string action_move_right   = "MoveRight";     // A/D 或左摇杆 X
    std::string action_jump         = "Jump";          // Space 或手柄 A
    std::string action_sprint       = "Sprint";        // LShift 或手柄 L3
    std::string action_crouch       = "Crouch";        // LCtrl 或手柄 B
    std::string action_look_x       = "LookX";         // 鼠标 X 或右摇杆 X
    std::string action_look_y       = "LookY";         // 鼠标 Y 或右摇杆 Y
    std::string action_toggle_view  = "ToggleView";    // V 键切换第一/第三人称

    // ── 输入参数 ──
    float mouse_sensitivity     = 0.15f;  // 鼠标旋转灵敏度
    float gamepad_sensitivity   = 2.0f;   // 手柄右摇杆灵敏度 (度/秒/单位)
    bool  invert_y              = false;  // Y 轴反转

    // ── 输入处理 ──
    // 死区（手柄摇杆）
    float stick_dead_zone       = 0.15f;  // 内死区
    float stick_outer_dead_zone = 0.95f;  // 外死区

    // 输入曲线 — 非线性响应
    // response_curve > 1.0: 指数响应（推得越多越快加速）
    // response_curve = 1.0: 线性
    // response_curve < 1.0: 对数响应（微调精确）
    float move_response_curve   = 1.5f;
    float look_response_curve   = 1.0f;

    // ── 运行时状态 ──
    glm::vec2 raw_move_input_   = glm::vec2(0.0f); // 原始移动输入 [-1,1]
    glm::vec2 raw_look_input_   = glm::vec2(0.0f); // 原始视角输入
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_CHARACTER_H
```

### 3.2 CharacterMovementSystem

```cpp
// modules/gameplay_3d/character/character_movement_system.h

#ifndef DSE_CHARACTER_MOVEMENT_SYSTEM_H
#define DSE_CHARACTER_MOVEMENT_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse::gameplay3d {

class CharacterMovementSystem {
public:
    void Update(World& world, float delta_time);

private:
    void UpdateWalking(struct CharacterMovementConfig& cfg,
                       struct CharacterMovementState& state,
                       struct CharacterController3DComponent& cc,
                       struct TransformComponent& tf,
                       class IPhysics3DSystem* physics,
                       entt::entity entity,
                       float dt);

    void UpdateSprinting(CharacterMovementConfig& cfg,
                         CharacterMovementState& state,
                         CharacterController3DComponent& cc,
                         TransformComponent& tf,
                         IPhysics3DSystem* physics,
                         entt::entity entity,
                         float dt);

    void UpdateCrouching(CharacterMovementConfig& cfg,
                         CharacterMovementState& state,
                         CharacterController3DComponent& cc,
                         TransformComponent& tf,
                         IPhysics3DSystem* physics,
                         entt::entity entity,
                         float dt);

    void UpdateFalling(CharacterMovementConfig& cfg,
                       CharacterMovementState& state,
                       CharacterController3DComponent& cc,
                       TransformComponent& tf,
                       IPhysics3DSystem* physics,
                       entt::entity entity,
                       float dt);

    void UpdateSwimming(CharacterMovementConfig& cfg,
                        CharacterMovementState& state,
                        CharacterController3DComponent& cc,
                        TransformComponent& tf,
                        IPhysics3DSystem* physics,
                        entt::entity entity,
                        float dt);

    void UpdateFlying(CharacterMovementConfig& cfg,
                      CharacterMovementState& state,
                      CharacterController3DComponent& cc,
                      TransformComponent& tf,
                      IPhysics3DSystem* physics,
                      entt::entity entity,
                      float dt);

    void ApplyRotation(const CharacterMovementConfig& cfg,
                       const CharacterMovementState& state,
                       TransformComponent& tf,
                       const SpringArm3DComponent* arm,  // 可为 nullptr
                       float dt);

    void HandleJump(const CharacterMovementConfig& cfg,
                    CharacterMovementState& state,
                    CharacterController3DComponent& cc,
                    IPhysics3DSystem* physics,
                    entt::entity entity);

    void TransitionMode(const CharacterMovementConfig& cfg,
                        CharacterMovementState& state,
                        MovementMode new_mode,
                        entt::entity entity);
};

} // namespace dse::gameplay3d

#endif
```

### 3.3 核心算法伪代码

```
CharacterMovementSystem::Update(world, dt):
    physics = ServiceLocator::Get<IPhysics3DSystem>()  // 可为 nullptr
    event_bus = ServiceLocator::Get<EventBus>()

    for each entity with (CharacterMovementConfig, CharacterMovementState,
                          CharacterController3DComponent, TransformComponent):
        cfg   = CharacterMovementConfig
        state = CharacterMovementState
        cc    = CharacterController3DComponent
        tf    = TransformComponent
        if !cfg.enabled: continue

        // 0. 处理模式切换请求
        if state.input_sprint && state.movement_mode == Walking && state.is_grounded:
            TransitionMode(cfg, state, Sprinting, entity)
        if !state.input_sprint && state.movement_mode == Sprinting:
            TransitionMode(cfg, state, Walking, entity)
        if state.input_crouch && state.is_grounded &&
           (state.movement_mode == Walking || state.movement_mode == Sprinting):
            TransitionMode(cfg, state, Crouching, entity)
            cc.height = cfg.crouch_height  // 动态修改胶囊高度
        if !state.input_crouch && state.movement_mode == Crouching:
            // 检测头顶空间是否足够站起（向上射线检测）
            if CanStand(physics, entity, cfg.stand_height):
                cc.height = cfg.stand_height
                TransitionMode(cfg, state, Walking, entity)

        // 1. 更新计时器
        if state.is_grounded:
            state.coyote_timer_ = cfg.coyote_time
            state.fall_time = 0
            state.jump_count = 0
        else:
            state.coyote_timer_ -= dt
            state.fall_time += dt

        if state.input_jump:
            state.jump_buffer_timer_ = cfg.jump_buffer_time
        else:
            state.jump_buffer_timer_ -= dt

        // 2. 状态机分派
        switch state.movement_mode:
            Walking:   UpdateWalking(...)
            Sprinting: UpdateSprinting(...)
            Crouching: UpdateCrouching(...)
            Falling:   UpdateFalling(...)
            Swimming:  UpdateSwimming(...)
            Flying:    UpdateFlying(...)

        // 3. 朝向旋转
        arm = world.registry().try_get<SpringArm3DComponent>(entity)  // 同实体或通过 PlayerCtrl 关联
        ApplyRotation(cfg, state, tf, arm, dt)

        // 4. 同步状态
        state.is_grounded = cc.is_grounded
        state.velocity = cc.velocity

        // 5. 清除脉冲输入
        state.input_jump = false


UpdateWalking(cfg, state, cc, tf, physics, entity, dt):
    // 计算目标速度
    target_vel = state.input_direction * cfg.max_walk_speed

    // 加速/减速插值
    if length(state.input_direction) > 0.01:
        state.velocity.xz = MoveTowards(state.velocity.xz, target_vel.xz,
                                         cfg.ground_acceleration * dt)
    else:
        state.velocity.xz = MoveTowards(state.velocity.xz, 0,
                                         cfg.ground_deceleration * dt)

    // 地面摩擦
    state.velocity.xz *= max(0, 1.0 - cfg.ground_friction * dt)

    // 跳跃检测（含 coyote time + jump buffer）
    can_jump = (state.jump_count < cfg.max_jump_count) &&
               (state.coyote_timer_ > 0 || state.jump_count > 0)
    if state.jump_buffer_timer_ > 0 && can_jump:
        HandleJump(cfg, state, cc, physics, entity)
        TransitionMode(cfg, state, Falling, entity)
        return

    // 物理移动
    displacement = state.velocity * dt
    if physics:
        result = physics->MoveCharacter(entity, displacement, cc.min_move_distance, dt)
        state.is_grounded = result.is_grounded
    else:
        // 无物理后端回退：直接修改 Transform
        tf.position += displacement
        state.is_grounded = (tf.position.y <= 0.0)  // 简单地面检测

    // 离地 → 切换到 Falling
    if !state.is_grounded:
        TransitionMode(cfg, state, Falling, entity)


UpdateSprinting(cfg, state, cc, tf, physics, entity, dt):
    // 与 Walking 结构相同，使用 cfg.max_sprint_speed
    target_vel = state.input_direction * cfg.max_sprint_speed
    // ... 同 UpdateWalking，替换速度上限 ...


UpdateCrouching(cfg, state, cc, tf, physics, entity, dt):
    // 与 Walking 结构相同，使用 cfg.max_crouch_speed
    target_vel = state.input_direction * cfg.max_crouch_speed
    // ... 同 UpdateWalking，替换速度上限 ...
    // 蹲伏中不可跳跃（先站起再跳）


UpdateFalling(cfg, state, cc, tf, physics, entity, dt):
    // 空中水平操控
    if length(state.input_direction) > 0.01:
        max_speed = cfg.max_walk_speed
        target_vel_xz = state.input_direction.xz * max_speed
        state.velocity.xz = MoveTowards(state.velocity.xz, target_vel_xz,
                                         cfg.ground_acceleration * cfg.air_control * dt)

    // 重力
    state.velocity.y += cfg.gravity * dt
    state.velocity.y = max(state.velocity.y, cfg.max_fall_speed)

    // 多段跳
    can_multi_jump = state.jump_count < cfg.max_jump_count && state.jump_count > 0
    if state.input_jump && can_multi_jump:
        HandleJump(cfg, state, cc, physics, entity)

    // 物理移动
    displacement = state.velocity * dt
    if physics:
        result = physics->MoveCharacter(entity, displacement, cc.min_move_distance, dt)
        state.is_grounded = result.is_grounded
    else:
        tf.position += displacement
        state.is_grounded = (tf.position.y <= 0.0)

    // 着地
    if state.is_grounded:
        // 发布着地事件
        if cfg.publish_events && event_bus:
            event_bus->Publish<CharacterLandedEvent>(entity, state.fall_time, state.velocity.y)

        state.is_jumping = false
        TransitionMode(cfg, state, Walking, entity)
        // 检查 jump buffer
        if state.jump_buffer_timer_ > 0:
            HandleJump(cfg, state, cc, physics, entity)
            TransitionMode(cfg, state, Falling, entity)


HandleJump(cfg, state, cc, physics, entity):
    state.velocity.y = cfg.jump_velocity
    state.is_jumping = true
    state.jump_count += 1
    state.coyote_timer_ = 0
    state.jump_buffer_timer_ = 0
    if physics:
        physics->JumpCharacter(entity, cfg.jump_velocity)
    if cfg.publish_events && event_bus:
        event_bus->Publish<CharacterJumpedEvent>(entity, state.jump_count)


TransitionMode(cfg, state, new_mode, entity):
    old_mode = state.movement_mode
    if old_mode == new_mode: return
    state.movement_mode = new_mode
    if cfg.publish_events && event_bus:
        event_bus->Publish<MovementModeChangedEvent>(entity, old_mode, new_mode)


ApplyRotation(cfg, state, tf, arm, dt):
    if cfg.rotation_mode == OrientToMovement:
        // 角色朝向移动方向
        if length(state.velocity.xz) > 0.1:
            target_yaw = atan2(-state.velocity.x, -state.velocity.z)
            current_yaw = tf.rotation.y  // 欧拉角 Y
            tf.rotation.y = LerpAngle(current_yaw, target_yaw, cfg.rotation_rate * dt)
    elif cfg.rotation_mode == OrientToCamera:
        // 角色朝向相机方向（适用于射击/瞄准模式）
        if arm:
            tf.rotation.y = radians(arm->yaw + 180.0f)  // 面向相机前方
```

### 3.4 EventBus 集成

```cpp
// 新增事件（engine/core/event_id.h 中追加 ID）

namespace dse::core {

/// 角色着地事件（仅 publish_events=true 的实体发布）
struct CharacterLandedEvent {
    entt::entity entity = entt::null;
    float fall_duration = 0.0f;    // 下落持续时间
    float impact_velocity = 0.0f;  // 着地瞬间的 Y 速度（负值）
};

/// 角色跳跃事件
struct CharacterJumpedEvent {
    entt::entity entity = entt::null;
    int jump_count = 0;            // 第几段跳（1=首跳，2=二段跳）
};

/// 移动模式切换事件
struct MovementModeChangedEvent {
    entt::entity entity = entt::null;
    MovementMode old_mode;
    MovementMode new_mode;
};

} // namespace dse::core
```

---

## 4. Phase 2: CameraArm3DSystem (Spring Arm)

### 4.1 核心算法伪代码

```
CameraArm3DSystem::Update(world, dt):
    physics = ServiceLocator::Get<IPhysics3DSystem>()  // 可为 nullptr

    for each camera_entity with (SpringArm3DComponent, TransformComponent):
        arm = SpringArm3DComponent
        cam_tf = TransformComponent
        if !arm.enabled || arm.target_entity == entt::null: continue

        target_tf = world.registry().try_get<TransformComponent>(arm.target_entity)
        if !target_tf: continue

        // 1. 计算旋转中心（目标位置 + 偏移）
        pivot = target_tf.position + target_tf.rotation * arm.target_offset

        // 2. 位置平滑
        t = 1.0 - exp(-arm.position_lag_speed * dt)
        arm.current_pivot_ = lerp(arm.current_pivot_, pivot, t)

        // 3. 计算臂方向（基于 pitch/yaw）
        arm_direction = SphericalToCartesian(arm.pitch, arm.yaw)

        // 4. 碰撞检测（优雅降级：无物理时跳过）
        actual_length = arm.arm_length
        if arm.collision_test && physics:
            ray_result = physics->Raycast(arm.current_pivot_,
                                          arm_direction,
                                          arm.arm_length + arm.probe_radius,
                                          arm.collision_mask)
            if ray_result.hit:
                actual_length = max(arm.min_arm_length,
                                    ray_result.distance - arm.probe_radius)

        // 5. 平滑臂长变化（避免突跳）
        //    缩短时快速响应（12x），恢复时慢速回弹（4x），避免抖动
        blend_speed = (actual_length < arm.current_arm_length_) ? 12.0 : 4.0
        arm.current_arm_length_ = lerp(arm.current_arm_length_, actual_length,
                                        1.0 - exp(-blend_speed * dt))

        // 6. 第一人称 / 第三人称
        if arm.view_mode == FirstPerson:
            cam_tf.position = arm.current_pivot_
        else:
            cam_tf.position = arm.current_pivot_ + arm_direction * arm.current_arm_length_

        // 7. 相机朝向 — 始终看向 pivot
        cam_tf.rotation = LookAt(cam_tf.position, arm.current_pivot_)

        // 8. FOV 插值
        if camera3d = registry.try_get<Camera3DComponent>(camera_entity):
            target_fov = (arm.view_mode == FirstPerson) ?
                          arm.first_person_fov : arm.third_person_fov
            camera3d.fov = lerp(camera3d.fov, target_fov,
                                1.0 - exp(-arm.fov_transition_speed * dt))

        // 9. 屏幕震动（3D）
        if arm.shake_trauma > 0:
            arm.shake_time_acc_ += dt
            arm.shake_trauma = max(0, arm.shake_trauma - arm.shake_decay_rate * dt)
            intensity = arm.shake_trauma * arm.shake_trauma  // 二次方衰减
            offset = PerlinShake3D(arm.shake_time_acc_, arm.shake_frequency) *
                     arm.shake_max_offset * intensity
            cam_tf.position += offset

        cam_tf.dirty = true
```

### 4.2 碰撞避让示意图

```
                 [Camera]
                   /
                  / arm_length (after collision test)
                 /
    -----[Wall]--/------
                /
    [Pivot] ← target_offset from Character
       |
    [Character]
```

当射线碰到墙壁时，`actual_length` 被缩短到碰撞点前方 `probe_radius` 处，
保证相机永远不会穿入几何体。碰撞检测通过 `collision_mask` 过滤，
可以设定专用的相机碰撞层（例如薄墙对游戏物理不碰撞但仍然挡住相机）。

### 4.3 无物理环境降级

当 `IPhysics3DSystem` 不可用时（编辑器预览、最小化测试场景）：
- `collision_test` 自动降级为 false
- 弹簧臂仍然工作（位置跟随 + 旋转），只是没有碰撞避让
- 这确保了 3C 系统在任何环境下都能运行

---

## 5. Phase 3: PlayerControllerSystem

### 5.1 架构变更（v2 修订）

**v1 设计**: PlayerController 是独立实体 → 3 个实体（角色 + 相机 + 控制器）
**v2 修订**: PlayerController 直接挂在角色实体上 → 2 个实体（角色 + 相机）

好处：
- 减少一次实体查找（不需要 `possessed_entity` 间接引用）
- Possess/Unpossess 简化为 添加/移除 `PlayerControllerComponent`
- 与 UE 的 `APlayerController::Possess()` 语义一致

### 5.2 核心算法伪代码

```
PlayerControllerSystem::Update(world, unscaled_dt):
    // 注意：PlayerController 使用 unscaled_dt，暂停时 UI 和输入仍响应

    for each entity with (PlayerControllerComponent, CharacterMovementState):
        pc    = PlayerControllerComponent
        state = CharacterMovementState
        if !pc.enabled: continue

        // 同实体上获取 Config（按需读取）
        cfg = world.registry().try_get<CharacterMovementConfig>(entity)

        // 1. 读取原始输入
        raw_move = vec2(0)
        raw_look = vec2(0)

        // 键盘/手柄移动（优先 ActionMapping，回退到默认键位）
        if ActionMapping::HasAction(pc.action_move_forward):
            raw_move.y = GetActionAxis(pc.action_move_forward)
            raw_move.x = GetActionAxis(pc.action_move_right)
        else:
            if Input::GetKey(KEY_CODE_W): raw_move.y += 1
            if Input::GetKey(KEY_CODE_S): raw_move.y -= 1
            if Input::GetKey(KEY_CODE_D): raw_move.x += 1
            if Input::GetKey(KEY_CODE_A): raw_move.x -= 1

        // 手柄摇杆
        if Input::IsGamepadConnected(0):
            stick = vec2(Input::GetGamepadAxis(0, AXIS_LEFT_X),
                         Input::GetGamepadAxis(0, AXIS_LEFT_Y))
            stick = ApplyRadialDeadZone(stick, pc.stick_dead_zone, pc.stick_outer_dead_zone)
            raw_move += stick

        // 归一化（防止对角线 >1）
        if length(raw_move) > 1: raw_move = normalize(raw_move)

        // 2. 应用输入曲线（非线性响应）
        move_magnitude = pow(length(raw_move), pc.move_response_curve)
        processed_move = (raw_move != 0) ? normalize(raw_move) * move_magnitude : vec2(0)

        // 3. 视角输入
        if Input::GetMouseButton(MOUSE_BUTTON_RIGHT):
            mouse_delta = Input::GetSwipeDelta()
            raw_look.x += mouse_delta.x * pc.mouse_sensitivity
            raw_look.y += mouse_delta.y * pc.mouse_sensitivity * (pc.invert_y ? 1 : -1)

        // 手柄右摇杆
        if Input::IsGamepadConnected(0):
            r_stick = vec2(Input::GetGamepadAxis(0, AXIS_RIGHT_X),
                           Input::GetGamepadAxis(0, AXIS_RIGHT_Y))
            r_stick = ApplyRadialDeadZone(r_stick, pc.stick_dead_zone, pc.stick_outer_dead_zone)
            raw_look += r_stick * pc.gamepad_sensitivity * unscaled_dt

        // 4. 转换移动输入到世界空间（相对于相机 yaw）
        if pc.camera_entity != entt::null:
            arm = world.registry().try_get<SpringArm3DComponent>(pc.camera_entity)
            if arm:
                cam_yaw_rad = radians(arm->yaw)
                cam_forward = vec3(-sin(cam_yaw_rad), 0, -cos(cam_yaw_rad))
                cam_right   = vec3(cos(cam_yaw_rad),  0, -sin(cam_yaw_rad))
                world_move  = cam_forward * processed_move.y + cam_right * processed_move.x
            else:
                world_move = vec3(processed_move.x, 0, -processed_move.y)
        else:
            world_move = vec3(processed_move.x, 0, -processed_move.y)

        // 5. 写入 CharacterMovementState（同实体）
        state.input_direction = world_move
        state.input_jump = Input::GetKeyDown(KEY_CODE_SPACE) ||
                           ActionMapping::GetActionDown(pc.action_jump)
        state.input_sprint = Input::GetKey(KEY_CODE_LEFT_SHIFT) ||
                             ActionMapping::GetAction(pc.action_sprint)
        state.input_crouch = Input::GetKeyDown(KEY_CODE_LEFT_CONTROL) ||
                             ActionMapping::GetActionDown(pc.action_crouch)

        // 6. 写入 SpringArm3DComponent（相机旋转）
        if pc.camera_entity != entt::null:
            arm = world.registry().try_get<SpringArm3DComponent>(pc.camera_entity)
            if arm:
                arm->yaw   += raw_look.x
                arm->pitch += raw_look.y
                arm->pitch = clamp(arm->pitch, arm->min_pitch, arm->max_pitch)

        // 7. 视角切换（第一/第三人称）
        if ActionMapping::GetActionDown(pc.action_toggle_view) ||
           Input::GetKeyDown(KEY_CODE_V):
            if arm:
                arm->view_mode = (arm->view_mode == ThirdPerson) ?
                                  FirstPerson : ThirdPerson
```

### 5.3 Possess / Unpossess API（v2 修订）

```cpp
/// PlayerController 挂在角色实体上，Possess/Unpossess 通过添加/移除组件实现

namespace dse::gameplay3d {

/// 让角色实体获得玩家控制能力
inline void Possess(World& world, entt::entity character_entity, entt::entity camera_entity) {
    auto& pc = world.registry().emplace_or_replace<PlayerControllerComponent>(character_entity);
    pc.camera_entity = camera_entity;

    // 确保 SpringArm 指向该角色
    if (auto* arm = world.registry().try_get<SpringArm3DComponent>(camera_entity)) {
        arm->target_entity = character_entity;
    }
}

/// 解除玩家控制
inline void Unpossess(World& world, entt::entity character_entity) {
    // 清空输入
    if (auto* state = world.registry().try_get<CharacterMovementState>(character_entity)) {
        state->input_direction = glm::vec3(0.0f);
        state->input_jump = false;
        state->input_sprint = false;
        state->input_crouch = false;
    }
    world.registry().remove<PlayerControllerComponent>(character_entity);
}

/// 切换控制到另一个角色
inline void SwitchPossession(World& world, entt::entity old_char, entt::entity new_char) {
    entt::entity cam = entt::null;
    if (auto* pc = world.registry().try_get<PlayerControllerComponent>(old_char)) {
        cam = pc->camera_entity;
    }
    Unpossess(world, old_char);
    if (cam != entt::null) {
        Possess(world, new_char, cam);
    }
}

} // namespace dse::gameplay3d
```

### 5.4 死区处理算法

```
/// 径向死区（圆形，适用于摇杆）— 比轴向死区更自然
ApplyRadialDeadZone(input, inner_dead, outer_dead) -> vec2:
    magnitude = length(input)
    if magnitude < inner_dead:
        return vec2(0)
    // 重映射 [inner_dead, outer_dead] → [0, 1]
    normalized_mag = clamp((magnitude - inner_dead) / (outer_dead - inner_dead), 0, 1)
    return normalize(input) * normalized_mag
```

---

## 6. 模块集成

### 6.1 Gameplay3DModule 更新管线变化

```
OnUpdate(world, frame):
    const float scaled_dt = time.scaled_dt;
    const float unscaled_dt = time.unscaled_dt;

    // ── 已有系统 ──
    free_camera_controller_system_.Update(world, unscaled_dt);
    day_night_cycle_system_.Update(world, scaled_dt);
    // ... animation pipeline ...
    steering_system_.Update(world, scaled_dt);

    // ── 3C 系统更新 ──（插入位置：steering_system 之后，behavior_tree 之前）
    // 顺序重要：Input 先驱动 → Movement 计算新位置 → Camera 基于新位置跟随
    player_controller_system_.Update(world, unscaled_dt);   // 输入不受 time_scale 影响
    character_movement_system_.Update(world, scaled_dt);     // 移动受 time_scale 影响
    camera_arm_3d_system_.Update(world, unscaled_dt);        // 相机跟随用真实时间

    // ── 已有系统（不变） ──
    // behavior_tree, cutscene, nav_agent, ...
```

### 6.2 Gameplay3DModule 成员变量新增

```cpp
// 在 gameplay_3d_module.h private 区域新增：
#include "modules/gameplay_3d/character/character_movement_system.h"
#include "modules/gameplay_3d/camera/camera_arm_3d_system.h"
#include "modules/gameplay_3d/player/player_controller_system.h"

CharacterMovementSystem character_movement_system_;
CameraArm3DSystem camera_arm_3d_system_;
PlayerControllerSystem player_controller_system_;
```

### 6.3 Floating Origin 集成

```cpp
// 在 Gameplay3DModule::OnInit 的 OriginRebasedEvent 订阅中追加：

// SpringArm: 偏移缓存的旋转中心
for (auto [e, arm] : reg.view<dse::SpringArm3DComponent>().each()) {
    arm.current_pivot_ -= evt.offset;
}
```

---

## 7. Lua 绑定

### 7.1 binding_defs.json 追加

```json
{
    "name": "CharacterMovementConfig",
    "prefix": "character_movement_cfg",
    "lua_table": "ecs",
    "include": "engine/ecs/components_3d_character.h",
    "namespace": "dse",
    "fields": [
        {"name": "enabled",              "type": "bool"},
        {"name": "max_walk_speed",       "type": "float", "range": [0, 100]},
        {"name": "max_sprint_speed",     "type": "float", "range": [0, 100]},
        {"name": "max_crouch_speed",     "type": "float", "range": [0, 100]},
        {"name": "ground_acceleration",  "type": "float"},
        {"name": "ground_deceleration",  "type": "float"},
        {"name": "ground_friction",      "type": "float"},
        {"name": "crouch_height",        "type": "float"},
        {"name": "stand_height",         "type": "float"},
        {"name": "gravity",             "type": "float"},
        {"name": "max_fall_speed",      "type": "float"},
        {"name": "air_control",         "type": "float", "range": [0, 1]},
        {"name": "jump_velocity",       "type": "float"},
        {"name": "max_jump_count",      "type": "int"},
        {"name": "coyote_time",         "type": "float"},
        {"name": "jump_buffer_time",    "type": "float"},
        {"name": "rotation_mode",       "type": "int",   "enum": "RotationMode"},
        {"name": "rotation_rate",       "type": "float"},
        {"name": "publish_events",      "type": "bool"}
    ]
},
{
    "name": "CharacterMovementState",
    "prefix": "character_movement",
    "lua_table": "ecs",
    "include": "engine/ecs/components_3d_character.h",
    "namespace": "dse",
    "fields": [
        {"name": "movement_mode",      "type": "int",   "enum": "MovementMode"},
        {"name": "input_direction",    "type": "vec3"},
        {"name": "input_jump",         "type": "bool"},
        {"name": "input_sprint",       "type": "bool"},
        {"name": "input_crouch",       "type": "bool"},
        {"name": "velocity",           "type": "vec3",  "readonly": true},
        {"name": "is_grounded",        "type": "bool",  "readonly": true},
        {"name": "is_jumping",         "type": "bool",  "readonly": true},
        {"name": "jump_count",         "type": "int",   "readonly": true}
    ]
},
{
    "name": "SpringArm3DComponent",
    "prefix": "spring_arm",
    "lua_table": "ecs",
    "include": "engine/ecs/components_3d_character.h",
    "namespace": "dse",
    "fields": [
        {"name": "enabled",            "type": "bool"},
        {"name": "target_entity",      "type": "entity"},
        {"name": "target_offset",      "type": "vec3"},
        {"name": "arm_length",         "type": "float", "range": [0, 100]},
        {"name": "min_arm_length",     "type": "float"},
        {"name": "max_arm_length",     "type": "float"},
        {"name": "collision_test",     "type": "bool"},
        {"name": "pitch",             "type": "float"},
        {"name": "yaw",               "type": "float"},
        {"name": "min_pitch",         "type": "float"},
        {"name": "max_pitch",         "type": "float"},
        {"name": "rotation_speed",    "type": "float"},
        {"name": "position_lag_speed","type": "float"},
        {"name": "rotation_lag_speed","type": "float"},
        {"name": "view_mode",         "type": "int",   "enum": "SpringArm3DComponent::ViewMode"},
        {"name": "shake_trauma",      "type": "float", "range": [0, 1]},
        {"name": "current_arm_length_","type": "float", "readonly": true}
    ]
},
{
    "name": "PlayerControllerComponent",
    "prefix": "player_controller",
    "lua_table": "ecs",
    "include": "engine/ecs/components_3d_character.h",
    "namespace": "dse",
    "fields": [
        {"name": "enabled",             "type": "bool"},
        {"name": "camera_entity",       "type": "entity"},
        {"name": "mouse_sensitivity",   "type": "float"},
        {"name": "gamepad_sensitivity", "type": "float"},
        {"name": "invert_y",            "type": "bool"},
        {"name": "stick_dead_zone",     "type": "float"},
        {"name": "move_response_curve", "type": "float"},
        {"name": "look_response_curve", "type": "float"}
    ]
}
```

### 7.2 完整 Lua 使用示例

```lua
-- ============================================================
-- 第三人称角色控制 — 完整示例
-- ============================================================

-- 1. 创建角色实体
local player = ecs.create_entity()
ecs.add_transform(player, 0, 5, 0)

-- 物理胶囊碰撞体（底层）
ecs.add_character_controller3d(player, 0.3, 1.0, 45, 0.3)

-- 高级移动系统（角色配置 + 运行时状态自动创建）
ecs.add_character_movement_cfg(player)
ecs.add_character_movement(player)

-- 调参
ecs.set_character_movement_cfg_max_walk_speed(player, 5.0)
ecs.set_character_movement_cfg_max_sprint_speed(player, 9.0)
ecs.set_character_movement_cfg_jump_velocity(player, 10.0)
ecs.set_character_movement_cfg_max_jump_count(player, 2)   -- 二段跳
ecs.set_character_movement_cfg_air_control(player, 0.35)
ecs.set_character_movement_cfg_gravity(player, -20.0)

-- 2. 创建弹簧臂相机
local cam = ecs.create_entity()
ecs.add_transform(cam, 0, 0, 0)
ecs.add_camera3d(cam, 60, 0.1, 1000)
ecs.add_spring_arm(cam)
ecs.set_spring_arm_target_entity(cam, player)
ecs.set_spring_arm_arm_length(cam, 5.0)
ecs.set_spring_arm_collision_test(cam, true)
ecs.set_spring_arm_target_offset(cam, 0, 1.8, 0)  -- 肩部偏移

-- 3. 挂载玩家控制器（直接在角色实体上）
ecs.add_player_controller(player)
ecs.set_player_controller_camera_entity(player, cam)
ecs.set_player_controller_mouse_sensitivity(player, 0.2)

-- 4. 运行时：切换控制到 NPC
function on_possess_npc(npc_entity)
    -- 使用 C++ API 或直接操作组件
    ecs.remove_player_controller(player)    -- 释放旧角色
    ecs.add_player_controller(npc_entity)   -- 控制新角色
    ecs.set_player_controller_camera_entity(npc_entity, cam)
    ecs.set_spring_arm_target_entity(cam, npc_entity)
end

-- 5. 触发屏幕震动
function on_explosion(pos)
    ecs.set_spring_arm_shake_trauma(cam, 0.8)
end

-- 6. 切换第一/第三人称
function toggle_view()
    local mode = ecs.get_spring_arm_view_mode(cam)
    ecs.set_spring_arm_view_mode(cam, mode == 0 and 1 or 0)
end

-- 7. 切换战斗模式（角色朝向相机方向）
function enter_combat_mode()
    ecs.set_character_movement_cfg_rotation_mode(player, 1)  -- OrientToCamera
end
function exit_combat_mode()
    ecs.set_character_movement_cfg_rotation_mode(player, 0)  -- OrientToMovement
end

-- 8. 查询运行时状态
function on_update()
    local grounded = ecs.get_character_movement_is_grounded(player)
    local vel_x, vel_y, vel_z = ecs.get_character_movement_velocity(player)
    local mode = ecs.get_character_movement_movement_mode(player)
    -- mode: 0=Walking, 1=Sprinting, 2=Crouching, 3=Falling, 4=Swimming, 5=Flying
end
```

---

## 8. 测试策略

### 8.1 单元测试

| 测试文件 | 覆盖内容 |
|:---------|:---------|
| `character_movement_test.cpp` | 状态机切换（Walking↔Falling↔Sprinting↔Crouching）、重力加速、jump buffer、coyote time、二段跳、各模式速度上限、orient_to_movement 旋转、orient_to_camera 旋转、蹲伏胶囊高度变化、无物理后端回退 |
| `spring_arm_test.cpp` | 臂长碰撞缩短、pitch/yaw 限制、位置平滑追踪、第一/第三人称切换、FOV 插值、震动衰减、碰撞缩短快速/恢复慢速非对称插值、无物理降级 |
| `player_controller_test.cpp` | Possess/Unpossess/SwitchPossession、输入方向转世界空间（基于相机 yaw）、径向死区过滤、response curve 非线性、默认键位回退 |

### 8.2 集成测试

| 测试文件 | 覆盖内容 |
|:---------|:---------|
| `character_3c_integration_test.cpp` | 完整链路：MockInput → PlayerController → CharacterMovement → 验证位置变化；相机跟随验证；EventBus 事件接收验证 |

### 8.3 测试可行性

所有 3C 测试均可在**无 GPU 环境**下运行：
- CharacterMovement：依赖 `IPhysics3DSystem` 可通过 mock（返回预设 `CharacterMoveResult`）；无物理时有 Transform 回退路径
- SpringArm：碰撞避让依赖 `Raycast` 可通过 mock；无物理时自动降级
- PlayerController：依赖 `Input` 静态方法可通过 `Input::RecordKey()` 模拟输入
- EventBus：直接使用真实 `EventBus` 实例验证事件发布

---

## 9. 反射/序列化集成

所有新组件的字段均使用 POD 类型（float/bool/int/vec3/enum/entity/string），
与现有 `reflect/component_reflection.gen.cpp` + `scene/scene_json_codec.gen.h` 的
codegen 流程兼容：

1. 在 `binding_defs.json` 追加 4 个组件定义
2. 运行 `tools/codegen/generate.py`
3. 自动生成：
   - C ABI getter/setter（`dse_api_*.gen.cpp`）
   - Lua 绑定（`lua_binding_ecs_*.gen.cpp`）
   - JSON 序列化/反序列化（`scene_json_codec.gen.h` 追加对应 to/from_json）
   - 反射注册（`component_reflection.gen.cpp`）

### 实施前验证点
- 确认 codegen 支持 `entity` 类型字段（`entt::entity` ↔ `uint32_t` 序列化）
- 确认 codegen 支持 `enum` 类型字段的整型映射
- 确认 codegen 支持 `std::string` 类型的 Lua 绑定（get/set string）

---

## 10. 实施计划

### Phase 1: CharacterMovementSystem (核心，最高优先级)

| 步骤 | 工作内容 | 产出文件 |
|:----:|:---------|:---------|
| 1.1 | 定义 `components_3d_character.h`（Config + State + 枚举） | `engine/ecs/components_3d_character.h` |
| 1.2 | 实现 `CharacterMovementSystem` | `modules/gameplay_3d/character/character_movement_system.h/.cpp` |
| 1.3 | EventBus 事件定义 | `engine/core/event_id.h`（追加 3 个事件结构） |
| 1.4 | 集成到 Gameplay3DModule | `gameplay_3d_module.h/.cpp`（新增成员 + Update 调用） |
| 1.5 | 单元测试 | `tests/gtest/unit/engine/character/character_movement_test.cpp` |
| 1.6 | binding_defs.json + codegen | `tools/codegen/binding_defs.json` + 生成文件 |
| 1.7 | CMakeLists 更新 | 新增源文件编译 |

### Phase 2: CameraArm3DSystem (弹簧臂)

| 步骤 | 工作内容 | 产出文件 |
|:----:|:---------|:---------|
| 2.1 | 实现 `CameraArm3DSystem` | `modules/gameplay_3d/camera/camera_arm_3d_system.h/.cpp` |
| 2.2 | 集成到 Gameplay3DModule（含 Floating Origin） | `gameplay_3d_module.h/.cpp` |
| 2.3 | 单元测试 | `tests/gtest/unit/engine/character/spring_arm_test.cpp` |
| 2.4 | binding_defs.json + codegen | 追加 SpringArm3DComponent |

### Phase 3: PlayerControllerSystem (输入管线)

| 步骤 | 工作内容 | 产出文件 |
|:----:|:---------|:---------|
| 3.1 | 实现 `PlayerControllerSystem` + Possess/Unpossess | `modules/gameplay_3d/player/player_controller_system.h/.cpp` |
| 3.2 | 集成到 Gameplay3DModule | `gameplay_3d_module.h/.cpp` |
| 3.3 | 单元测试 + 集成测试 | `tests/gtest/unit/...` + `tests/gtest/integration/...` |
| 3.4 | binding_defs.json + codegen | 追加 PlayerControllerComponent |

### Phase 4: 测试 & 打磨

| 步骤 | 工作内容 |
|:----:|:---------|
| 4.1 | 全量构建 + 测试验证 |
| 4.2 | Lua 示例脚本（第三人称角色控制 demo） |
| 4.3 | 编辑器 Inspector 集成（组件字段自动 UI） |

---

## 11. 与现有系统的关系图

```
                 ┌─────────────────────────────────────┐
                 │      PlayerControllerSystem          │
                 │   (读 Input/ActionMapping/Gamepad)    │
                 │   挂在角色实体上，直接写 State       │
                 └──────┬───────────────┬──────────────┘
                        │               │
           input_direction/jump    yaw/pitch
           input_sprint/crouch     (写入 SpringArm)
                        │               │
                        ▼               ▼
     ┌────────────────────────┐  ┌──────────────────┐
     │ CharacterMovement      │  │ CameraArm3D      │
     │ System                 │  │ System            │
     │ (Config + State +      │  │ (碰撞避让 + 跟随  │
     │  状态机 + 物理移动)    │  │  + FOV + 震动)    │
     └──────┬─────────────────┘  └──────┬───────────┘
            │                           │
            ▼                           ▼
  ┌──────────────────┐       ┌──────────────────────┐
  │ IPhysics3DSystem │       │ IPhysics3DSystem     │
  │ .MoveCharacter() │       │ .Raycast()           │
  │ .JumpCharacter() │       │ (碰撞避让，可选)     │
  │ (可选，有回退)    │       │ (无物理时降级)       │
  └──────────────────┘       └──────────────────────┘

  ┌───────────────────────────────────────────────────────┐
  │               Gameplay3DModule::OnUpdate()             │
  │                                                        │
  │  1. free_camera_controller (已有，unscaled_dt)         │
  │  2. day_night_cycle / animation / steering (已有)      │
  │  3. player_controller_system_  (NEW，unscaled_dt) ──┐ │
  │  4. character_movement_system_ (NEW，scaled_dt)   ──┤ │
  │  5. camera_arm_3d_system_      (NEW，unscaled_dt) ──┘ │
  │  6. behavior_tree / cutscene / nav_agent (已有)        │
  │  7. frustum_culling / lod / hlod (已有)                │
  └───────────────────────────────────────────────────────┘
```

---

## 12. 特殊考量

### 12.1 与现有 FreeCameraController 的关系
- **不冲突**。FreeCameraController 是编辑器/调试用的自由飞行相机
- 运行时场景使用 3C 系统（PlayerController + SpringArm）
- 两者通过 `enabled` 字段互斥：进入 play mode 时禁用 FreeCamera，启用 3C

### 12.2 与 SteeringSystem 的关系
- SteeringSystem 用于 AI NPC 移动（Seek/Flee/Arrive）
- CharacterMovementSystem 用于 **玩家** 角色移动（也可用于需要高级移动的 NPC）
- 两者不冲突：纯 AI NPC 挂 SteeringComponent，玩家挂 CharacterMovement*

### 12.3 与现有 CharacterController3DComponent 的关系
- **不修改** 现有组件。CharacterMovementConfig/State 是更高层的封装
- Movement 组件通过物理系统的 `MoveCharacter()` API 间接使用 CharacterController3DComponent
- 角色实体组件栈：TransformComponent + CC3D + CMConfig + CMState + (可选) PlayerController

### 12.4 时间缩放
- CharacterMovement 使用 `scaled_dt`（暂停时角色冻结）
- PlayerController 使用 `unscaled_dt`（暂停时仍可操作菜单/旋转相机）
- CameraArm 使用 `unscaled_dt`（相机跟随不受暂停影响）

### 12.5 Swimming 模式切换
Swimming 模式需要水域检测机制。推荐两种方式（可并存）：
1. **Trigger Volume**: 通过 BoxCollider3D（is_trigger=true）标记水域，碰撞回调切换模式
2. **Lua 脚本驱动**: `ecs.set_character_movement_movement_mode(player, 4)` 手动切换
3. **未来**: 与现有 FluidEmitterComponent 联动，检测实体是否在流体区域内

### 12.6 网络同步（未来扩展）
- CharacterMovementState 的 `velocity`/`movement_mode`/`is_grounded` 是可同步字段
- 未来可通过 replication 系统标记这些字段为 replicated
- Config/State 分离天然适合网络：Config 只在初始化时同步，State 每帧同步

### 12.7 Floating Origin
- 在 Gameplay3DModule 的 OriginRebasedEvent 订阅中追加 SpringArm pivot 偏移
- CharacterMovement 不需要特殊处理（Transform 已由物理系统偏移）

### 12.8 并行化考量
- 当前实现为单线程顺序更新
- 未来可对 CharacterMovementSystem 使用 `ParallelEach`（每实体的移动计算独立）
- 注意：ParallelEach 要求 System 无共享可变状态 — 当前设计满足此约束

---

## 13. 已知限制与未来扩展

| 限制 | 说明 | 未来扩展方向 |
|:-----|:-----|:------------|
| 无根运动 (Root Motion) | 引擎有 AnimatorSystem 但 CMSystem 不消费 root motion 数据 | 增加 `root_motion_mode` 字段，从 Animator 提取位移注入 velocity |
| 无移动平台支持 | 站在电梯/载具上不会继承平台速度 | 增加 `movement_base` entity 引用，每帧叠加 base 速度 |
| Swimming 需要手动切换 | 没有自动水域检测 | 集成 Trigger Volume 或 FluidEmitter 碰撞回调 |
| 无网络同步 | 本地单机专用 | Config/State 分离已为网络预留架构 |
| 无攀爬/梯子 | Custom mode 预留了扩展口 | 通过 Custom mode + Lua 脚本实现 |

---

## 14. 总结

| 阶段 | 新增文件 | 新增代码行 | 核心能力 |
|:----:|:--------:|:---------:|:---------|
| Phase 1 | 3 (+2 gen) | ~600 | 角色移动状态机（走/跑/蹲/跳/落/飞/游）+ 二段跳 + coyote time + 旋转模式 |
| Phase 2 | 2 (+2 gen) | ~350 | 弹簧臂相机 + 碰撞避让 + 第一/第三人称切换 + 3D 震动 + 无物理降级 |
| Phase 3 | 2 (+2 gen) | ~450 | 输入管线 + Possess/Unpossess + 径向死区 + 响应曲线 + 手柄 + 蹲伏 |
| Phase 4 | 4 (tests) | ~700 | 单元/集成测试 + Lua 示例 |
| **合计** | **~17** | **~2,100** | **完整 3C 框架** |

### v2 相对 v1 的修订要点

| # | 修订内容 | 理由 |
|:-:|:---------|:-----|
| 1 | `uint32_t` → `entt::entity` | 类型安全，与代码库一致 |
| 2 | `char[32]` → `std::string` | 消除截断风险，与代码库一致 |
| 3 | 增加 `RotationMode` 枚举 | 支持战斗/瞄准模式（OrientToCamera） |
| 4 | PlayerController 挂在角色实体上 | 减少间接层，简化 Possess |
| 5 | Config/State 拆分 | 优化 ECS 缓存局部性 |
| 6 | 增加 `publish_events` 开关 | 避免大量 NPC 的事件风暴 |
| 7 | 增加 `Crouching` + `Sprinting` 移动模式 | Sprint 从 bool → 状态机一致 |
| 8 | 碰撞避让非对称插值 | 缩短快速响应，恢复慢速回弹 |
| 9 | 无物理环境降级方案 | 编辑器/测试场景可用 |
| 10 | 径向死区替代轴向死区 | 对角线方向更自然 |
