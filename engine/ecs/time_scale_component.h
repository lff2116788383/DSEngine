/**
 * @file time_scale_component.h
 * @brief 逐实体（分层）时间缩放组件。
 *
 * 实体最终生效缩放 = 全局 time-scale × 该组件 scale（对标 Unreal CustomTimeDilation）。
 * 作用于运动学 / 动画 / 粒子 / 脚本驱动的推进；自由动态刚体只跟随全局缩放（见
 * docs/design/TIME_SCALE.md Option A）。未挂该组件的实体 scale 视为 1。
 */

#ifndef DSE_ENGINE_ECS_TIME_SCALE_COMPONENT_H
#define DSE_ENGINE_ECS_TIME_SCALE_COMPONENT_H

#include <entt/entt.hpp>

namespace dse {

struct TimeScaleComponent {
    float scale = 1.0f;  ///< 实体局部时间缩放，>=0
};

/**
 * @brief 计算实体的逐实体生效增量时间 = global_scaled_dt × local_scale。
 *        未挂 TimeScaleComponent 的实体按 local_scale=1 处理。
 */
inline float ResolveEntityDt(float global_scaled_dt,
                             const entt::registry& registry,
                             entt::entity e) {
    if (const auto* ts = registry.try_get<TimeScaleComponent>(e)) {
        return global_scaled_dt * ts->scale;
    }
    return global_scaled_dt;
}

} // namespace dse

#endif // DSE_ENGINE_ECS_TIME_SCALE_COMPONENT_H
