#ifndef DSE_COMPONENTS_3D_CHARACTER_H
#define DSE_COMPONENTS_3D_CHARACTER_H

/**
 * @file components_3d_character.h
 * @brief 3C (Character-Camera-Control) 框架组件定义
 *
 * 包含：
 * - CharacterMovementConfig  — 角色移动配置参数（少改动）
 * - CharacterMovementState   — 角色移动运行时状态（每帧读写）
 * - SpringArm3DComponent     — 弹簧臂相机（碰撞避让 + 跟随）
 * - PlayerControllerComponent — 玩家输入管线（Input → Movement + Camera）
 */

#include <glm/glm.hpp>
#include <string>
#include <cstdint>
#include <entt/entity/entity.hpp>

namespace dse {

// ============================================================
// 枚举定义
// ============================================================

/// 角色移动模式状态机
enum class MovementMode : uint8_t {
    Walking    = 0,
    Sprinting  = 1,
    Crouching  = 2,
    Falling    = 3,
    Swimming   = 4,
    Flying     = 5,
    Custom     = 6,
};

/// 角色旋转模式
enum class RotationMode : uint8_t {
    OrientToMovement = 0,   ///< 角色面向移动方向（第三人称探索模式）
    OrientToCamera   = 1,   ///< 角色面向相机方向（战斗/瞄准模式）
};

// ============================================================
// CharacterMovementConfig — 角色移动配置参数
// ============================================================

struct CharacterMovementConfig {
    bool enabled = true;

    // 地面移动
    float max_walk_speed      = 6.0f;
    float max_sprint_speed    = 10.0f;
    float max_crouch_speed    = 3.0f;
    float ground_acceleration = 20.0f;
    float ground_deceleration = 12.0f;
    float ground_friction     = 8.0f;

    // 蹲伏
    float crouch_height       = 0.5f;
    float stand_height        = 1.0f;

    // 空中
    float gravity             = -19.62f;
    float max_fall_speed      = -30.0f;
    float air_control         = 0.3f;
    float jump_velocity       = 8.0f;
    int   max_jump_count      = 2;
    float coyote_time         = 0.1f;
    float jump_buffer_time    = 0.15f;

    // 游泳
    float swim_speed          = 4.0f;
    float swim_acceleration   = 8.0f;
    float buoyancy            = 1.0f;

    // 飞行
    float fly_speed           = 8.0f;
    float fly_acceleration    = 10.0f;

    // 旋转
    RotationMode rotation_mode = RotationMode::OrientToMovement;
    float rotation_rate       = 720.0f;

    // 事件控制
    bool publish_events       = true;
};

// ============================================================
// CharacterMovementState — 角色移动运行时状态
// ============================================================

struct CharacterMovementState {
    MovementMode movement_mode = MovementMode::Walking;

    // 输入（由 PlayerControllerSystem 或 Lua 每帧写入）
    glm::vec3 input_direction  = glm::vec3(0.0f);
    bool      input_jump       = false;
    bool      input_sprint     = false;
    bool      input_crouch     = false;

    // 运行时状态（由 System 写入）
    glm::vec3 velocity         = glm::vec3(0.0f);
    bool      is_grounded      = false;
    bool      is_jumping       = false;
    int       jump_count       = 0;
    float     fall_time        = 0.0f;

    // 内部计时器
    float coyote_timer_        = 0.0f;
    float jump_buffer_timer_   = 0.0f;
};

// ============================================================
// SpringArm3DComponent — 弹簧臂相机
// ============================================================

struct SpringArm3DComponent {
    bool enabled = true;

    // 目标
    entt::entity target_entity = entt::null;
    glm::vec3 target_offset    = glm::vec3(0.0f, 1.6f, 0.0f);

    // 臂参数
    float arm_length           = 4.0f;
    float min_arm_length       = 0.5f;
    float max_arm_length       = 10.0f;

    // 碰撞避让
    bool  collision_test       = true;
    float probe_radius         = 0.2f;
    uint16_t collision_mask    = 0xFFFF;

    // 旋转控制
    float pitch                = -20.0f;
    float yaw                  = 0.0f;
    float min_pitch            = -80.0f;
    float max_pitch            = 80.0f;
    float rotation_speed       = 0.15f;

    // 平滑
    float position_lag_speed   = 10.0f;
    float rotation_lag_speed   = 10.0f;

    // 视角模式
    enum class ViewMode : uint8_t {
        ThirdPerson = 0,
        FirstPerson = 1,
    };
    ViewMode view_mode         = ViewMode::ThirdPerson;
    float first_person_fov     = 75.0f;
    float third_person_fov     = 60.0f;
    float fov_transition_speed = 8.0f;

    // 屏幕震动
    float shake_trauma         = 0.0f;
    float shake_decay_rate     = 1.5f;
    float shake_max_offset     = 0.3f;
    float shake_max_rotation   = 3.0f;
    float shake_frequency      = 15.0f;

    // 运行时状态
    float current_arm_length_  = 4.0f;
    glm::vec3 current_pivot_   = glm::vec3(0.0f);
    float shake_time_acc_      = 0.0f;
};

// ============================================================
// PlayerControllerComponent — 玩家控制器
// ============================================================

struct PlayerControllerComponent {
    bool enabled = true;

    // 关联相机
    entt::entity camera_entity = entt::null;

    // 输入映射 Action 名
    std::string action_move_forward = "MoveForward";
    std::string action_move_right   = "MoveRight";
    std::string action_jump         = "Jump";
    std::string action_sprint       = "Sprint";
    std::string action_crouch       = "Crouch";
    std::string action_look_x       = "LookX";
    std::string action_look_y       = "LookY";
    std::string action_toggle_view  = "ToggleView";

    // 输入参数
    float mouse_sensitivity     = 0.15f;
    float gamepad_sensitivity   = 2.0f;
    bool  invert_y              = false;

    // 死区
    float stick_dead_zone       = 0.15f;
    float stick_outer_dead_zone = 0.95f;

    // 输入曲线
    float move_response_curve   = 1.5f;
    float look_response_curve   = 1.0f;

    // 运行时状态
    glm::vec2 raw_move_input_   = glm::vec2(0.0f);
    glm::vec2 raw_look_input_   = glm::vec2(0.0f);
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_CHARACTER_H
