/**
 * @file floating_origin_system.h
 * @brief Floating Origin System — 当相机远离原点超过阈值时，
 *        整体平移所有 ECS 实体和物理 body，保持坐标精度。
 */

#ifndef DSE_ECS_FLOATING_ORIGIN_SYSTEM_H
#define DSE_ECS_FLOATING_ORIGIN_SYSTEM_H

#include <glm/glm.hpp>

class World;

namespace dse {
namespace physics3d { class IPhysics3DSystem; }
namespace core { class EventBus; }

class FloatingOriginSystem {
public:
    /// 每帧调用，检查是否需要 rebase
    void Tick(World& world, physics3d::IPhysics3DSystem* physics, core::EventBus* event_bus);

    /// 累积的绝对原点偏移（双精度）
    const glm::dvec3& accumulated_origin() const { return accumulated_origin_; }

    /// 本地坐标 → 绝对坐标
    glm::dvec3 ToAbsolute(const glm::vec3& local) const {
        return accumulated_origin_ + glm::dvec3(local);
    }

    /// 绝对坐标 → 本地坐标
    glm::vec3 ToLocal(const glm::dvec3& absolute) const {
        return glm::vec3(absolute - accumulated_origin_);
    }

    void set_rebase_threshold(float threshold) { rebase_threshold_ = threshold; }
    float rebase_threshold() const { return rebase_threshold_; }

private:
    glm::dvec3 accumulated_origin_{0.0};
    float rebase_threshold_ = 5000.0f;
};

} // namespace dse

#endif // DSE_ECS_FLOATING_ORIGIN_SYSTEM_H
