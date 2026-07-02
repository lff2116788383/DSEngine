#include "modules/gameplay_3d/character/character_movement_system.h"
#include "engine/ecs/components_3d_character.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/transform.h"
#include "engine/physics/physics3d/i_physics3d_system.h"
#include "engine/core/service_locator.h"
#include "engine/core/event_bus.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace dse::gameplay3d {

namespace {

/// 向目标值线性移动，不超过 max_delta
inline float MoveTowards(float current, float target, float max_delta) {
    float diff = target - current;
    if (std::abs(diff) <= max_delta) return target;
    return current + std::copysign(max_delta, diff);
}

inline glm::vec2 MoveTowardsVec2(glm::vec2 current, glm::vec2 target, float max_delta) {
    glm::vec2 diff = target - current;
    float dist = glm::length(diff);
    if (dist <= max_delta || dist < 1e-6f) return target;
    return current + diff / dist * max_delta;
}

/// 短弧插值角度 (弧度)
inline float LerpAngle(float current, float target, float t) {
    float diff = target - current;
    // 归一化到 [-PI, PI]
    while (diff > glm::pi<float>()) diff -= glm::two_pi<float>();
    while (diff < -glm::pi<float>()) diff += glm::two_pi<float>();
    return current + diff * std::min(t, 1.0f);
}

// 3C 事件结构
struct CharacterLandedEvent : public dse::core::Event {
    CharacterLandedEvent(entt::entity e, float duration, float velocity)
        : entity(e), fall_duration(duration), impact_velocity(velocity) {}
    entt::entity entity = entt::null;
    float fall_duration = 0.0f;
    float impact_velocity = 0.0f;
    static constexpr dse::core::EventId kEventId = dse::core::events::kCharacterLanded;
};

struct CharacterJumpedEvent : public dse::core::Event {
    CharacterJumpedEvent(entt::entity e, int count)
        : entity(e), jump_count(count) {}
    entt::entity entity = entt::null;
    int jump_count = 0;
    static constexpr dse::core::EventId kEventId = dse::core::events::kCharacterJumped;
};

struct MovementModeChangedEvent : public dse::core::Event {
    MovementModeChangedEvent(entt::entity e, MovementMode old_m, MovementMode new_m)
        : entity(e), old_mode(old_m), new_mode(new_m) {}
    entt::entity entity = entt::null;
    MovementMode old_mode = MovementMode::Walking;
    MovementMode new_mode = MovementMode::Walking;
    static constexpr dse::core::EventId kEventId = dse::core::events::kMovementModeChanged;
};

} // namespace

void CharacterMovementSystem::Update(World& world, float dt) {
    if (dt <= 0.0f) return;

    auto* physics = dse::core::ServiceLocator::Instance()
                        .Get<dse::physics3d::IPhysics3DSystem>();
    auto* event_bus = dse::core::ServiceLocator::Instance()
                          .Get<dse::core::EventBus>();

    auto view = world.registry().view<CharacterMovementConfig,
                                       CharacterMovementState,
                                       CharacterController3DComponent,
                                       TransformComponent>();

    for (auto entity : view) {
        auto& cfg   = view.get<CharacterMovementConfig>(entity);
        auto& state = view.get<CharacterMovementState>(entity);
        auto& cc    = view.get<CharacterController3DComponent>(entity);
        auto& tf    = view.get<TransformComponent>(entity);

        if (!cfg.enabled) continue;

        MovementMode prev_mode = state.movement_mode;

        // ── 处理冲刺/蹲伏模式切换请求 ──
        if (state.input_sprint && state.movement_mode == MovementMode::Walking && state.is_grounded) {
            state.movement_mode = MovementMode::Sprinting;
        }
        if (!state.input_sprint && state.movement_mode == MovementMode::Sprinting) {
            state.movement_mode = MovementMode::Walking;
        }
        if (state.input_crouch && state.is_grounded &&
            (state.movement_mode == MovementMode::Walking || state.movement_mode == MovementMode::Sprinting)) {
            state.movement_mode = MovementMode::Crouching;
            cc.height = cfg.crouch_height;
        }
        if (!state.input_crouch && state.movement_mode == MovementMode::Crouching) {
            cc.height = cfg.stand_height;
            state.movement_mode = MovementMode::Walking;
        }

        // 发布模式切换事件
        if (prev_mode != state.movement_mode && cfg.publish_events && event_bus) {
            event_bus->Publish<MovementModeChangedEvent>(entity, prev_mode, state.movement_mode);
        }

        // ── 更新计时器 ──
        if (state.is_grounded) {
            state.coyote_timer_ = cfg.coyote_time;
            state.fall_time = 0.0f;
            state.jump_count = 0;
        } else {
            state.coyote_timer_ -= dt;
            state.fall_time += dt;
        }

        if (state.input_jump) {
            state.jump_buffer_timer_ = cfg.jump_buffer_time;
        } else {
            state.jump_buffer_timer_ -= dt;
        }

        // ── 获取当前模式最大速度 ──
        float max_speed = cfg.max_walk_speed;
        switch (state.movement_mode) {
            case MovementMode::Sprinting: max_speed = cfg.max_sprint_speed; break;
            case MovementMode::Crouching: max_speed = cfg.max_crouch_speed; break;
            case MovementMode::Swimming:  max_speed = cfg.swim_speed; break;
            case MovementMode::Flying:    max_speed = cfg.fly_speed; break;
            default: break;
        }

        // ── 状态机分派 ──
        bool was_grounded = state.is_grounded;

        switch (state.movement_mode) {
            case MovementMode::Walking:
            case MovementMode::Sprinting:
            case MovementMode::Crouching: {
                // 地面移动
                glm::vec2 vel_xz(state.velocity.x, state.velocity.z);
                glm::vec2 input_xz(state.input_direction.x, state.input_direction.z);
                float input_len = glm::length(input_xz);

                if (input_len > 0.01f) {
                    glm::vec2 target_xz = input_xz / input_len * max_speed;
                    vel_xz = MoveTowardsVec2(vel_xz, target_xz, cfg.ground_acceleration * dt);
                } else {
                    vel_xz = MoveTowardsVec2(vel_xz, glm::vec2(0.0f), cfg.ground_deceleration * dt);
                }

                // 地面摩擦
                float friction_factor = std::max(0.0f, 1.0f - cfg.ground_friction * dt);
                vel_xz *= friction_factor;

                state.velocity.x = vel_xz.x;
                state.velocity.z = vel_xz.y;
                state.velocity.y = 0.0f;  // 地面不累积 Y 速度

                // 跳跃检测
                bool can_jump = (state.jump_count < cfg.max_jump_count) &&
                                (state.coyote_timer_ > 0.0f || state.jump_count > 0);
                if (state.jump_buffer_timer_ > 0.0f && can_jump &&
                    state.movement_mode != MovementMode::Crouching) {
                    state.velocity.y = cfg.jump_velocity;
                    state.is_jumping = true;
                    state.jump_count++;
                    state.coyote_timer_ = 0.0f;
                    state.jump_buffer_timer_ = 0.0f;
                    if (physics) physics->JumpCharacter(entity, cfg.jump_velocity);
                    if (cfg.publish_events && event_bus) {
                        event_bus->Publish<CharacterJumpedEvent>(entity, state.jump_count);
                    }
                    state.movement_mode = MovementMode::Falling;
                    if (prev_mode != MovementMode::Falling && cfg.publish_events && event_bus) {
                        event_bus->Publish<MovementModeChangedEvent>(entity, prev_mode, MovementMode::Falling);
                    }
                    break;  // 跳跃后不做物理移动，下帧处理
                }

                // 物理移动
                glm::vec3 displacement = state.velocity * dt;
                if (physics) {
                    auto result = physics->MoveCharacter(entity, displacement, cc.min_move_distance, dt);
                    state.is_grounded = result.is_grounded;
                    state.velocity = result.velocity;
                } else {
                    tf.position += displacement;
                    tf.dirty = true;
                    state.is_grounded = (tf.position.y <= 0.0f);
                }

                // 离地 → Falling
                if (!state.is_grounded) {
                    MovementMode old = state.movement_mode;
                    state.movement_mode = MovementMode::Falling;
                    if (cfg.publish_events && event_bus) {
                        event_bus->Publish<MovementModeChangedEvent>(entity, old, MovementMode::Falling);
                    }
                }
                break;
            }

            case MovementMode::Falling: {
                // 空中水平操控
                glm::vec2 vel_xz(state.velocity.x, state.velocity.z);
                glm::vec2 input_xz(state.input_direction.x, state.input_direction.z);
                float input_len = glm::length(input_xz);

                if (input_len > 0.01f) {
                    glm::vec2 target_xz = input_xz / input_len * max_speed;
                    vel_xz = MoveTowardsVec2(vel_xz, target_xz,
                                              cfg.ground_acceleration * cfg.air_control * dt);
                }

                state.velocity.x = vel_xz.x;
                state.velocity.z = vel_xz.y;

                // 重力
                state.velocity.y += cfg.gravity * dt;
                state.velocity.y = std::max(state.velocity.y, cfg.max_fall_speed);

                // 多段跳
                bool can_multi_jump = state.jump_count < cfg.max_jump_count && state.jump_count > 0;
                if (state.input_jump && can_multi_jump) {
                    state.velocity.y = cfg.jump_velocity;
                    state.is_jumping = true;
                    state.jump_count++;
                    if (physics) physics->JumpCharacter(entity, cfg.jump_velocity);
                    if (cfg.publish_events && event_bus) {
                        event_bus->Publish<CharacterJumpedEvent>(entity, state.jump_count);
                    }
                }

                // 物理移动
                glm::vec3 displacement = state.velocity * dt;
                if (physics) {
                    auto result = physics->MoveCharacter(entity, displacement, cc.min_move_distance, dt);
                    state.is_grounded = result.is_grounded;
                    state.velocity = result.velocity;
                } else {
                    tf.position += displacement;
                    tf.dirty = true;
                    state.is_grounded = (tf.position.y <= 0.0f);
                    if (state.is_grounded) {
                        tf.position.y = 0.0f;
                        state.velocity.y = 0.0f;
                    }
                }

                // 着地
                if (state.is_grounded) {
                    if (cfg.publish_events && event_bus) {
                        event_bus->Publish<CharacterLandedEvent>(entity, state.fall_time, state.velocity.y);
                    }
                    state.is_jumping = false;
                    state.movement_mode = MovementMode::Walking;
                    if (cfg.publish_events && event_bus) {
                        event_bus->Publish<MovementModeChangedEvent>(entity, MovementMode::Falling, MovementMode::Walking);
                    }

                    // 检查 jump buffer
                    if (state.jump_buffer_timer_ > 0.0f && state.jump_count < cfg.max_jump_count) {
                        state.velocity.y = cfg.jump_velocity;
                        state.is_jumping = true;
                        state.jump_count++;
                        state.jump_buffer_timer_ = 0.0f;
                        if (physics) physics->JumpCharacter(entity, cfg.jump_velocity);
                        if (cfg.publish_events && event_bus) {
                            event_bus->Publish<CharacterJumpedEvent>(entity, state.jump_count);
                        }
                        state.movement_mode = MovementMode::Falling;
                        if (cfg.publish_events && event_bus) {
                            event_bus->Publish<MovementModeChangedEvent>(entity, MovementMode::Walking, MovementMode::Falling);
                        }
                    }
                }
                break;
            }

            case MovementMode::Swimming: {
                glm::vec3 target = state.input_direction * cfg.swim_speed;
                // 浮力抵消重力
                float effective_gravity = cfg.gravity * (1.0f - cfg.buoyancy);
                target.y += effective_gravity;

                state.velocity.x = MoveTowards(state.velocity.x, target.x, cfg.swim_acceleration * dt);
                state.velocity.y = MoveTowards(state.velocity.y, target.y, cfg.swim_acceleration * dt);
                state.velocity.z = MoveTowards(state.velocity.z, target.z, cfg.swim_acceleration * dt);

                glm::vec3 displacement = state.velocity * dt;
                if (physics) {
                    auto result = physics->MoveCharacter(entity, displacement, cc.min_move_distance, dt);
                    state.velocity = result.velocity;
                } else {
                    tf.position += displacement;
                    tf.dirty = true;
                }
                break;
            }

            case MovementMode::Flying: {
                glm::vec3 target = state.input_direction * cfg.fly_speed;
                state.velocity.x = MoveTowards(state.velocity.x, target.x, cfg.fly_acceleration * dt);
                state.velocity.y = MoveTowards(state.velocity.y, target.y, cfg.fly_acceleration * dt);
                state.velocity.z = MoveTowards(state.velocity.z, target.z, cfg.fly_acceleration * dt);

                glm::vec3 displacement = state.velocity * dt;
                if (physics) {
                    auto result = physics->MoveCharacter(entity, displacement, cc.min_move_distance, dt);
                    state.velocity = result.velocity;
                } else {
                    tf.position += displacement;
                    tf.dirty = true;
                }
                break;
            }

            case MovementMode::Custom:
                // 由 Lua 脚本驱动
                break;
        }

        // ── 朝向旋转 ──
        float vel_xz_len = std::sqrt(state.velocity.x * state.velocity.x +
                                      state.velocity.z * state.velocity.z);
        if (vel_xz_len > 0.1f) {
            if (cfg.rotation_mode == RotationMode::OrientToMovement) {
                float target_yaw = std::atan2(-state.velocity.x, -state.velocity.z);
                // 从当前四元数提取 yaw
                glm::vec3 fwd = tf.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                float current_yaw = std::atan2(-fwd.x, -fwd.z);
                float new_yaw = LerpAngle(current_yaw, target_yaw,
                                           glm::radians(cfg.rotation_rate) * dt);
                tf.rotation = glm::quat(glm::vec3(0.0f, new_yaw, 0.0f));
                tf.dirty = true;
            }
        }

        if (cfg.rotation_mode == RotationMode::OrientToCamera) {
            auto* arm = world.registry().try_get<SpringArm3DComponent>(entity);
            if (!arm) {
                // 尝试从 PlayerController 获取 camera entity
                auto* pc = world.registry().try_get<PlayerControllerComponent>(entity);
                if (pc && pc->camera_entity != entt::null) {
                    arm = world.registry().try_get<SpringArm3DComponent>(pc->camera_entity);
                }
            }
            if (arm) {
                float cam_yaw = glm::radians(arm->yaw + 180.0f);
                tf.rotation = glm::quat(glm::vec3(0.0f, cam_yaw, 0.0f));
                tf.dirty = true;
            }
        }

        // ── 同步 CC3D 状态 ──
        cc.is_grounded = state.is_grounded;
        cc.velocity = state.velocity;

        // ── 清除脉冲输入 ──
        state.input_jump = false;
    }
}

} // namespace dse::gameplay3d
