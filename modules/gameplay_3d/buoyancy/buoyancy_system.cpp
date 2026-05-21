#include "modules/gameplay_3d/buoyancy/buoyancy_system.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_fluid.h"
#include "engine/ecs/transform.h"
#include "engine/physics/physics3d/i_physics3d_system.h"
#include "engine/base/debug.h"
#include <cmath>
#include <algorithm>

namespace dse {
namespace gameplay3d {

void BuoyancySystem::SetPhysics3D(physics3d::IPhysics3DSystem* physics3d) { physics3d_ = physics3d; }

void BuoyancySystem::FixedUpdate(World& world, float dt) {
    if (!physics3d_ || dt <= 0.0f) return;

    auto view = world.registry().view<BuoyancyComponent, RigidBody3DComponent, TransformComponent>();
    for (auto entity : view) {
        auto& buoyancy = view.get<BuoyancyComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        if (!buoyancy.enabled) continue;

        // 如果没有采样点，使用实体中心作为默认
        std::vector<BuoyancySamplePoint> samples;
        if (buoyancy.sample_points.empty()) {
            BuoyancySamplePoint center;
            center.offset = glm::vec3(0.0f);
            center.force_scale = 1.0f;
            samples.push_back(center);
        } else {
            samples = buoyancy.sample_points;
        }

        float total_submerge = 0.0f;
        glm::vec3 total_force(0.0f);
        glm::vec3 total_torque(0.0f);

        for (const auto& sp : samples) {
            // 采样点世界位置
            glm::vec3 world_pos = transform.position + sp.offset;
            float water_y = GetWaterLevel(world, world_pos, buoyancy);

            // 淹没深度
            float depth = water_y - world_pos.y;
            float submerge = std::clamp(depth / buoyancy.submerge_depth, 0.0f, 1.0f);
            total_submerge += submerge;

            if (submerge > 0.0f) {
                // 浮力（向上）
                float force_mag = buoyancy.buoyancy_force * submerge * sp.force_scale * 9.81f;
                glm::vec3 buoyancy_force(0.0f, force_mag, 0.0f);
                total_force += buoyancy_force;

                // 力矩
                glm::vec3 r = world_pos - transform.position;
                total_torque += glm::cross(r, buoyancy_force);
            }
        }

        buoyancy.submerge_ratio = total_submerge / static_cast<float>(samples.size());

        // 施加浮力
        if (glm::length(total_force) > 0.001f) {
            physics3d_->AddForce(entity, total_force);
        }

        // 水阻力
        if (buoyancy.submerge_ratio > 0.0f) {
            glm::vec3 velocity = physics3d_->GetVelocity(entity);
            float drag_factor = buoyancy.water_drag * buoyancy.submerge_ratio;
            glm::vec3 drag_force = -velocity * drag_factor;
            physics3d_->AddForce(entity, drag_force);

            // 角阻力（简化：通过减速线性速度的侧向分量）
            // 完整实现需要 AddTorque 接口，这里用线性阻力近似
            float angular_drag = buoyancy.water_angular_drag * buoyancy.submerge_ratio;
            (void)angular_drag; // TODO: 需要 Physics3DSystem::AddTorque 接口
        }
    }
}

float BuoyancySystem::GetWaterLevel(World& world, const glm::vec3& pos, const BuoyancyComponent& buoyancy) const {
    if (buoyancy.use_fluid_system) {
        // 尝试从 FluidEmitterComponent 获取水面高度
        // 简化策略：取最近的流体发射器的 floor_y + 粒子最大高度
        float best_water_y = -FLT_MAX;
        bool found = false;

        auto fluid_view = world.registry().view<FluidEmitterComponent, TransformComponent>();
        for (auto fe : fluid_view) {
            const auto& emitter = fluid_view.get<FluidEmitterComponent>(fe);
            const auto& ft = fluid_view.get<TransformComponent>(fe);
            if (!emitter.enabled) continue;

            // 简单范围检查：水平距离
            float dx = pos.x - ft.position.x;
            float dz = pos.z - ft.position.z;
            float h_dist = std::sqrt(dx * dx + dz * dz);

            // 流体发射器影响范围（基于发射器形状大小）
            float range = 10.0f; // 默认影响半径
            if (emitter.shape == FluidEmitterShape::Sphere) {
                range = emitter.sphere_radius * 5.0f;
            } else if (emitter.shape == FluidEmitterShape::Box) {
                range = std::max(emitter.box_half_extents.x, emitter.box_half_extents.z) * 5.0f;
            }

            if (h_dist < range) {
                // 水面高度 = 发射器位置Y + 粒子堆积高度估算
                float water_y = ft.position.y;
                // 扫描粒子找到此位置附近的最高水面
                for (const auto& p : emitter.particles) {
                    float pdx = pos.x - p.position.x;
                    float pdz = pos.z - p.position.z;
                    float pd = std::sqrt(pdx * pdx + pdz * pdz);
                    if (pd < emitter.particle_radius * 4.0f) {
                        water_y = std::max(water_y, p.position.y);
                    }
                }
                if (water_y > best_water_y) {
                    best_water_y = water_y;
                    found = true;
                }
            }
        }

        if (found) return best_water_y;
    }

    // 回退到全局水面高度
    return buoyancy.water_level;
}

} // namespace gameplay3d
} // namespace dse
