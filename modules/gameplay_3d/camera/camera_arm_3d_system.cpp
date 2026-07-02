#include "modules/gameplay_3d/camera/camera_arm_3d_system.h"
#include "engine/ecs/components_3d_character.h"
#include "engine/ecs/transform.h"
#include "engine/physics/physics3d/i_physics3d_system.h"
#include "engine/core/service_locator.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace dse::gameplay3d {

namespace {

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * std::min(t, 1.0f);
}

inline glm::vec3 LerpVec3(const glm::vec3& a, const glm::vec3& b, float t) {
    float clamped = std::min(t, 1.0f);
    return a + (b - a) * clamped;
}

} // namespace

void CameraArm3DSystem::Update(World& world, float dt) {
    if (dt <= 0.0f) return;

    auto* physics = dse::core::ServiceLocator::Instance()
                        .Get<dse::physics3d::IPhysics3DSystem>();

    auto view = world.registry().view<SpringArm3DComponent, TransformComponent>();

    for (auto entity : view) {
        auto& arm = view.get<SpringArm3DComponent>(entity);
        auto& cam_tf = view.get<TransformComponent>(entity);

        if (!arm.enabled) continue;

        // ── 获取目标位置 ──
        glm::vec3 target_pos(0.0f);
        if (arm.target_entity != entt::null) {
            auto* target_tf = world.registry().try_get<TransformComponent>(arm.target_entity);
            if (target_tf) {
                target_pos = target_tf->position;
            }
        }

        // 应用偏移
        glm::vec3 pivot = target_pos + arm.target_offset;

        // 平滑追踪
        arm.current_pivot_ = LerpVec3(arm.current_pivot_, pivot,
                                       arm.position_lag_speed * dt);

        // ── 计算旋转 ──
        arm.pitch = std::clamp(arm.pitch, arm.min_pitch, arm.max_pitch);

        float pitch_rad = glm::radians(arm.pitch);
        float yaw_rad = glm::radians(arm.yaw);

        // 球面坐标 → 方向向量
        glm::vec3 arm_dir;
        arm_dir.x = std::cos(pitch_rad) * std::sin(yaw_rad);
        arm_dir.y = std::sin(pitch_rad);
        arm_dir.z = std::cos(pitch_rad) * std::cos(yaw_rad);
        arm_dir = glm::normalize(arm_dir);

        // ── 目标臂长 ──
        float desired_length = arm.arm_length;
        if (arm.view_mode == SpringArm3DComponent::ViewMode::FirstPerson) {
            desired_length = 0.0f;
        }

        // ── 碰撞避让 ──
        float actual_length = desired_length;
        if (arm.collision_test && physics && desired_length > 0.0f) {
            glm::vec3 ray_origin = arm.current_pivot_;
            glm::vec3 ray_dir = arm_dir;
            auto result = physics->Raycast(ray_origin, ray_dir, desired_length + arm.probe_radius);
            if (result.hit) {
                float hit_dist = std::max(result.distance - arm.probe_radius, arm.min_arm_length);
                actual_length = std::min(actual_length, hit_dist);
            }
        }

        // 非对称插值：缩短快（碰撞即时响应）、恢复慢（避免抖动）
        if (actual_length < arm.current_arm_length_) {
            // 碰撞缩短 → 快速
            arm.current_arm_length_ = actual_length;
        } else {
            // 恢复 → 慢速
            float restore_speed = arm.position_lag_speed * 0.5f;
            arm.current_arm_length_ = Lerp(arm.current_arm_length_, actual_length,
                                            restore_speed * dt);
        }

        // ── 计算最终相机位置 ──
        glm::vec3 cam_pos = arm.current_pivot_ + arm_dir * arm.current_arm_length_;

        // ── 屏幕震动 ──
        if (arm.shake_trauma > 0.0f) {
            arm.shake_time_acc_ += dt;
            float trauma_sq = arm.shake_trauma * arm.shake_trauma;

            // Perlin-like shake via sin (cheap approximation)
            float freq = arm.shake_frequency;
            float t = arm.shake_time_acc_;
            float offset_x = std::sin(freq * t * 1.0f) * arm.shake_max_offset * trauma_sq;
            float offset_y = std::sin(freq * t * 1.3f + 1.7f) * arm.shake_max_offset * trauma_sq;
            float offset_z = std::sin(freq * t * 0.9f + 3.1f) * arm.shake_max_offset * trauma_sq;
            cam_pos += glm::vec3(offset_x, offset_y, offset_z);

            // 衰减
            arm.shake_trauma = std::max(0.0f, arm.shake_trauma - arm.shake_decay_rate * dt);
            if (arm.shake_trauma <= 0.0f) {
                arm.shake_time_acc_ = 0.0f;
            }
        }

        // ── 更新相机 Transform ──
        cam_tf.position = cam_pos;

        // 相机朝向 pivot
        glm::vec3 look_dir = glm::normalize(arm.current_pivot_ - cam_pos);
        if (glm::length(look_dir) > 0.001f) {
            // 从 look_dir 构造四元数
            // 默认 forward 是 -Z
            float cam_pitch = std::asin(look_dir.y);
            float cam_yaw = std::atan2(-look_dir.x, -look_dir.z);
            cam_tf.rotation = glm::quat(glm::vec3(cam_pitch, cam_yaw, 0.0f));
        }
        cam_tf.dirty = true;
    }
}

} // namespace dse::gameplay3d
