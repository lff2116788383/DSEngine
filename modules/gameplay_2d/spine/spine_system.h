/**
 * @file spine_system.h
 * @brief Spine 2D 系统，处理骨骼动画更新及渲染数据的生成
 */

#ifndef DSE_SPINE_SYSTEM_H
#define DSE_SPINE_SYSTEM_H

#include <entt/entt.hpp>
#include "engine/render/rhi/rhi_device.h"
#include "engine/ecs/world.h"

namespace dse {
namespace gameplay2d {

/**
 * @class SpineSystem
 * @brief Spine骨骼动画更新系统
 */
class SpineSystem {
public:
    SpineSystem() = default;
    ~SpineSystem();

    /**
     * @brief 每帧更新 Spine 动画状态和骨骼矩阵
     * @param registry ECS 注册表
     * @param dt 增量时间
     */
    void Update(entt::registry& registry, float dt);

    void Render(World& world, CommandBuffer& cmd_buffer);

    void Shutdown(entt::registry& registry);

private:
    void CleanupComponent(SpineRendererComponent& comp);
};

} // namespace gameplay2d
} // namespace dse

#endif
