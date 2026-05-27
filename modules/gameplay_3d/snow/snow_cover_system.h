#pragma once

#include "engine/ecs/world.h"
#include <entt/entt.hpp>

namespace dse {
namespace gameplay3d {

/// 雪地覆盖系统
/// - 根据天气状态驱动 SnowCoverComponent::coverage 的积雪/融雪
/// - 在 TerrainSystem::Render 之前将 snow 参数注入 TerrainComponent 相关实体
class SnowCoverSystem {
public:
    void Update(World& world, float delta_time);
    void Shutdown(World& world);
};

} // namespace gameplay3d
} // namespace dse
