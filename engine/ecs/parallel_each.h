/**
 * @file parallel_each.h
 * @brief ECS 并行迭代工具 —— 将 entt view 按 batch 分片提交到 JobSystem
 *
 * 适用于无写冲突的 System 更新（每个实体只读写自己的组件数据）。
 * 对于有共享状态写入的 System，仍应使用串行迭代。
 */

#pragma once

#include <vector>
#include <cstddef>
#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include "engine/ecs/world.h"

namespace dse {
namespace ecs {

/**
 * @brief 并行遍历 entt view 中的所有实体
 * @tparam Components 要遍历的组件类型列表
 * @param world ECS World 引用
 * @param func 回调函数 func(Entity entity, Components&... comps)
 * @param batch_size 每个 job 处理的实体数（默认 64）
 *
 * @note 内部将 view 实体收集到 vector 后按 batch 提交 ParallelFor。
 *       若 JobSystem 未初始化，自动退化为串行执行。
 */
template <typename... Components, typename Func>
void ParallelEach(World& world, Func&& func, size_t batch_size = 64) {
    auto view = world.registry().view<Components...>();

    // Collect entities into a contiguous vector for indexed access
    std::vector<Entity> entities;
    for (auto e : view) {
        entities.push_back(e);
    }

    if (entities.empty()) return;

    auto& sl = dse::core::ServiceLocator::Instance();
    auto* job_sys = sl.Get<dse::core::JobSystem>();

    if (!job_sys) {
        // No job system: serial fallback
        for (auto e : entities) {
            func(e, view.template get<Components>(e)...);
        }
        return;
    }

    job_sys->ParallelFor(0, entities.size(), batch_size,
        [&entities, &view, &func](size_t idx) {
            auto e = entities[idx];
            func(e, view.template get<Components>(e)...);
        },
        dse::core::JobPriority::High);
}

} // namespace ecs
} // namespace dse
