#include "engine/ecs/floating_origin_system.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/world.h"
#include "engine/core/event_bus.h"
#include "engine/base/debug.h"
#ifdef DSE_ENABLE_3D
#include "engine/physics/physics3d/i_physics3d_system.h"
#endif
#include <glm/geometric.hpp>
#include <entt/entt.hpp>

namespace dse {

void FloatingOriginSystem::Tick(World& world, physics3d::IPhysics3DSystem* physics, core::EventBus* event_bus) {
    // 查找主相机位置
    glm::vec3 cam_pos(0.0f);
    bool found_camera = false;
    {
        auto view = world.registry().view<TransformComponent, dse::Camera3DComponent>();
        int best_priority = std::numeric_limits<int>::min();
        for (auto e : view) {
            auto& cam = view.get<dse::Camera3DComponent>(e);
            if (!cam.enabled) continue;
            if (cam.priority > best_priority) {
                best_priority = cam.priority;
                cam_pos = view.get<TransformComponent>(e).position;
                found_camera = true;
            }
        }
    }

    if (!found_camera) return;
    if (glm::length(cam_pos) < rebase_threshold_) return;

    const glm::vec3 offset = cam_pos;
    accumulated_origin_ += glm::dvec3(offset);

    DEBUG_LOG_INFO("[FloatingOrigin] Rebase triggered: offset=({:.1f}, {:.1f}, {:.1f}), accumulated=({:.1f}, {:.1f}, {:.1f})",
                   offset.x, offset.y, offset.z,
                   accumulated_origin_.x, accumulated_origin_.y, accumulated_origin_.z);

    // 1. ECS: 所有 TransformComponent 减去 offset
    auto transform_view = world.registry().view<TransformComponent>();
    for (auto entity : transform_view) {
        auto& t = transform_view.get<TransformComponent>(entity);
        t.position -= offset;
        t.dirty = true;
    }

    // 2. Physics: 平移所有 body
#ifdef DSE_ENABLE_3D
    if (physics) {
        physics->RebaseOrigin(offset);
    }
#endif

    // 3. 广播事件，其他子系统响应
    if (event_bus) {
        event_bus->Publish<core::OriginRebasedEvent>(offset);
    }
}

} // namespace dse
