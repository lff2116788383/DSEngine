#include "modules/gameplay_3d/vehicle/vehicle_system.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/transform.h"
#include "engine/physics/physics3d/i_physics3d_system.h"
#include "engine/base/debug.h"
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <algorithm>

namespace { constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f; }

namespace dse {
namespace gameplay3d {

void VehicleSystem::SetPhysics3D(physics3d::IPhysics3DSystem* physics3d) { physics3d_ = physics3d; }

void VehicleSystem::FixedUpdate(World& world, float dt) {
    auto view = world.registry().view<VehicleComponent, RigidBody3DComponent, TransformComponent>();
    for (auto entity : view) {
        auto& vehicle = view.get<VehicleComponent>(entity);
        if (!vehicle.enabled) continue;

        if (!vehicle.initialized) {
            InitializeVehicle(world, entity, vehicle);
        }
        if (vehicle.initialized) {
            SimulateVehicle(world, entity, vehicle, dt);
        }
    }
}

void VehicleSystem::InitializeVehicle(World& world, entt::entity entity, VehicleComponent& vehicle) {
    (void)world; (void)entity;
    if (vehicle.wheels.empty()) {
        // 默认四轮配置
        vehicle.wheels.resize(4);
        // 前左
        vehicle.wheels[0].position = glm::vec3(-0.8f, 0.0f, 1.2f);
        vehicle.wheels[0].is_steer_wheel = true;
        // 前右
        vehicle.wheels[1].position = glm::vec3(0.8f, 0.0f, 1.2f);
        vehicle.wheels[1].is_steer_wheel = true;
        // 后左
        vehicle.wheels[2].position = glm::vec3(-0.8f, 0.0f, -1.2f);
        vehicle.wheels[2].is_drive_wheel = true;
        // 后右
        vehicle.wheels[3].position = glm::vec3(0.8f, 0.0f, -1.2f);
        vehicle.wheels[3].is_drive_wheel = true;
    }
    vehicle.wheel_states.resize(vehicle.wheels.size());
    vehicle.initialized = true;
    DEBUG_LOG_INFO("[Vehicle] Initialized with {} wheels for entity {}", vehicle.wheels.size(), static_cast<uint32_t>(entity));
}

void VehicleSystem::SimulateVehicle(World& world, entt::entity entity, VehicleComponent& vehicle, float dt) {
    if (!physics3d_ || dt <= 0.0f) return;

    auto& transform = world.registry().get<TransformComponent>(entity);

    glm::mat4 world_mat = glm::translate(glm::mat4(1.0f), transform.position)
                         * glm::mat4_cast(transform.rotation);
    glm::vec3 forward = -glm::vec3(world_mat[2]); // -Z = forward
    glm::vec3 right = glm::vec3(world_mat[0]);
    glm::vec3 up = glm::vec3(world_mat[1]);

    glm::vec3 velocity = physics3d_->GetVelocity(entity);
    vehicle.current_speed = glm::dot(velocity, forward);

    glm::vec3 total_force(0.0f);
    glm::vec3 total_torque(0.0f);

    for (size_t i = 0; i < vehicle.wheels.size(); ++i) {
        const auto& wheel = vehicle.wheels[i];
        auto& state = vehicle.wheel_states[i];

        // 车轮世界位置
        glm::vec3 wheel_world = transform.position
            + right * wheel.position.x
            + up * wheel.position.y
            + forward * (-wheel.position.z); // 注意：车辆模型通常Z朝前

        // Raycast 向下检测地面
        glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);
        float ray_len = wheel.suspension_rest_length + wheel.radius;

        auto hit = physics3d_->Raycast(wheel_world, ray_dir, ray_len);
        // 跳过自身实体的碰撞
        state.grounded = hit.hit && hit.entity != entity;

        if (state.grounded) {
            float hit_dist = hit.distance;
            state.compression = ray_len - hit_dist;
            state.contact_point = hit.hit_point;
            state.contact_normal = hit.hit_normal;

            // 1. 悬挂力（弹簧-阻尼器）
            float spring_force = state.compression * wheel.suspension_stiffness;
            // 悬挂速度估算
            float prev_compression = state.compression; // 简化：使用当前值
            float compression_velocity = (state.compression - prev_compression) / dt;
            float damper_force = compression_velocity * wheel.suspension_damping;
            float suspension_force = std::max(0.0f, spring_force + damper_force);

            glm::vec3 force_up = state.contact_normal * suspension_force;
            total_force += force_up;

            // 力矩（悬挂力对重心的力矩）
            glm::vec3 r = wheel_world - transform.position;
            total_torque += glm::cross(r, force_up);

            // 2. 驱动力
            if (wheel.is_drive_wheel) {
                float engine_force = vehicle.throttle * vehicle.max_engine_force;
                glm::vec3 drive_dir = forward;
                if (wheel.is_steer_wheel) {
                    float steer_rad = vehicle.steering * vehicle.max_steer_angle * kDeg2Rad;
                    drive_dir = glm::normalize(forward * std::cos(steer_rad) + right * std::sin(steer_rad));
                }
                total_force += drive_dir * engine_force;
            }

            // 3. 刹车力
            if (vehicle.brake > 0.0f) {
                float brake_force = vehicle.brake * vehicle.max_brake_force;
                if (glm::length(velocity) > 0.1f) {
                    glm::vec3 brake_dir = -glm::normalize(velocity);
                    total_force += brake_dir * brake_force;
                }
            }

            // 4. 侧向摩擦力（防止侧滑）
            float side_speed = glm::dot(velocity, right);
            float lateral_friction = -side_speed * wheel.friction * suspension_force * 0.01f;
            total_force += right * lateral_friction;

            // 5. 转向力矩
            if (wheel.is_steer_wheel) {
                float steer_angle = vehicle.steering * vehicle.max_steer_angle * kDeg2Rad;
                state.steer_angle = steer_angle;
            }

            // 更新车轮旋转
            float wheel_speed = vehicle.current_speed;
            state.angular_velocity = (wheel.radius > 0.0f) ? wheel_speed / wheel.radius : 0.0f;
            state.rotation += state.angular_velocity * dt;

        } else {
            state.compression = 0.0f;
            state.angular_velocity *= 0.98f; // 空中缓慢减速
            state.rotation += state.angular_velocity * dt;
        }
    }

    // 施加合力
    if (glm::length(total_force) > 0.01f) {
        physics3d_->AddForce(entity, total_force);
    }

    // 转向力矩
    float total_steer = 0.0f;
    int steer_count = 0;
    for (const auto& ws : vehicle.wheel_states) {
        if (ws.steer_angle != 0.0f) {
            total_steer += ws.steer_angle;
            steer_count++;
        }
    }
    if (steer_count > 0 && std::abs(vehicle.current_speed) > 0.1f) {
        float avg_steer = total_steer / static_cast<float>(steer_count);
        float steer_torque = avg_steer * vehicle.current_speed * 50.0f;
        glm::vec3 yaw_torque = up * steer_torque;
        physics3d_->AddTorque(entity, yaw_torque);
    }
}

} // namespace gameplay3d
} // namespace dse
