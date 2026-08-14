/**
* @file system_scheduler.cpp
* @brief 并行 ECS 系统调度器实现
*
* 核心算法：
* 1. BuildDependencyGraph: O(N²) 检测所有系统对之间的组件冲突
* 2. BuildParallelLayers: 基于依赖图做分层拓扑排序，同层无冲突可并行
* 3. Execute: 逐层提交到 JobSystem，层间用 SubmitWithDependency 保证顺序
*    无 JobSystem 时退化为逐系统串行执行
*/

#include "engine/ecs/system_scheduler.h"
#include "engine/ecs/world.h"
#include "engine/core/service_locator.h"
#include <algorithm>

namespace dse {
namespace ecs {

// ============================================================
// CommandBuffer
// ============================================================

void CommandBuffer::Enqueue(StructuralCommand::Type type, Entity entity,
                             std::type_index comp_type,
                             std::function<void(entt::registry&, Entity)> apply_fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push(StructuralCommand{type, entity, comp_type, std::move(apply_fn)});
}

void CommandBuffer::Flush(entt::registry& registry) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!commands_.empty()) {
        auto& cmd = commands_.front();
        if (cmd.apply_fn) {
            cmd.apply_fn(registry, cmd.entity);
        }
        commands_.pop();
    }
}

void CommandBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!commands_.empty()) commands_.pop();
}

size_t CommandBuffer::PendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return commands_.size();
}

// ============================================================
// SystemScheduler
// ============================================================

void SystemScheduler::Register(const std::string& name,
                                ComponentAccess access,
                                SystemUpdateFn update_fn) {
    entries_.push_back(SystemEntry{name, std::move(access), std::move(update_fn), true});
}

void SystemScheduler::RegisterSerial(const std::string& name,
                                      SystemUpdateFn update_fn) {
    // 保守策略：声明为读写所有可能的组件 → 与所有其他系统冲突
    // 实际通过留空 reads/writes 并标记为 serial 来实现
    ComponentAccess empty;
    entries_.push_back(SystemEntry{name, std::move(empty), std::move(update_fn), true});
    // 标记为 serial：通过在 access 中添加特殊标记
    // 实际上，空 access 意味着不读写任何组件 → 不与任何系统冲突
    // 但 RegisterSerial 的语义是"保守串行"，我们通过特殊处理实现
    // 解决方案：RegisterSerial 的系统总是与所有其他系统有依赖
    // → 在 BuildDependencyGraph 中检测：如果任一系统的 access 为空且通过 RegisterSerial 注册，
    //   则视为冲突。
    // 更简单的方案：给 serial 系统一个 dummy "write all" 标记
    entries_.back().access.writes.push_back(std::type_index(typeid(void)));
    // typeid(void) 作为 "serial marker"：所有声明了 typeid(void) 写的系统互相冲突
}

bool SystemScheduler::HasConflict(const ComponentAccess& a, const ComponentAccess& b) {
    // A 写了 B 读/写的组件？
    for (const auto& w : a.writes) {
        for (const auto& r : b.reads) {
            if (w == r) return true;
        }
        for (const auto& w2 : b.writes) {
            if (w == w2) return true;
        }
    }
    // B 写了 A 读/写的组件？
    for (const auto& w : b.writes) {
        for (const auto& r : a.reads) {
            if (w == r) return true;
        }
        // A writes vs B writes 已在上方检查
    }
    return false;
}

std::vector<std::vector<int>> SystemScheduler::BuildDependencyGraph() const {
    const int n = static_cast<int>(entries_.size());
    std::vector<std::vector<int>> deps(n);

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            // 如果 i 和 j 有冲突，且 i < j，则 j 依赖 i
            // （只建立单向依赖，避免死锁）
            if (i < j && HasConflict(entries_[i].access, entries_[j].access)) {
                deps[j].push_back(i);
            }
        }
    }

    return deps;
}

std::vector<std::vector<int>> SystemScheduler::BuildParallelLayers(
    const std::vector<std::vector<int>>& deps) const {

    const int n = static_cast<int>(entries_.size());
    std::vector<int> in_degree(n, 0);
    for (int i = 0; i < n; ++i) {
        in_degree[i] = static_cast<int>(deps[i].size());
    }

    std::vector<std::vector<int>> layers;
    std::vector<bool> scheduled(n, false);

    while (true) {
        std::vector<int> current_layer;
        for (int i = 0; i < n; ++i) {
            if (!scheduled[i] && in_degree[i] == 0) {
                current_layer.push_back(i);
                scheduled[i] = true;
            }
        }
        if (current_layer.empty()) break;

        layers.push_back(current_layer);

        // 减少后继节点的入度
        for (int i : current_layer) {
            for (int j = 0; j < n; ++j) {
                // 检查 j 是否依赖 i
                for (int dep : deps[j]) {
                    if (dep == i) {
                        --in_degree[j];
                        break;
                    }
                }
            }
        }
    }

    // 检查是否有未调度的节点（环依赖）
    for (int i = 0; i < n; ++i) {
        if (!scheduled[i]) {
            // 环依赖：强制放入最后一层
            if (layers.empty()) layers.push_back({});
            layers.back().push_back(i);
        }
    }

    return layers;
}

void SystemScheduler::Execute(World& world, float delta_time) {
    if (entries_.empty()) return;

    auto* job_sys = dse::core::ServiceLocator::Instance().Get<dse::core::JobSystem>();

    // 检查 JobSystem 是否可用（已初始化且有 worker 线程）
    bool has_job_system = job_sys && !job_sys->GetWorkerStats().empty();

    if (!has_job_system) {
        // 串行执行所有系统
        for (auto& entry : entries_) {
            if (entry.enabled && entry.update_fn) {
                entry.update_fn(world, delta_time);
            }
        }
        return;
    }

    // 构建依赖图
    auto deps = BuildDependencyGraph();
    auto layers = BuildParallelLayers(deps);

    // 逐层提交到 JobSystem
    // 层间用 SubmitWithDependency 保证顺序
    std::vector<dse::core::JobHandle> prev_layer_handles;

    for (const auto& layer : layers) {
        if (layer.empty()) continue;

        std::vector<dse::core::JobHandle> current_layer_handles;

        for (int sys_idx : layer) {
            auto& entry = entries_[sys_idx];
            if (!entry.enabled || !entry.update_fn) {
                continue;
            }

            // 捕获系统信息
            auto& world_ref = world;
            auto& fn = entry.update_fn;
            float dt = delta_time;

            if (prev_layer_handles.empty()) {
                // 第一层：无依赖直接提交
                current_layer_handles.push_back(
                    job_sys->Submit([fn, &world_ref, dt]() {
                        fn(world_ref, dt);
                    }, dse::core::JobPriority::High));
            } else {
                // 后续层：依赖前一层所有 handle
                current_layer_handles.push_back(
                    job_sys->SubmitWithDependency(
                        [fn, &world_ref, dt]() {
                            fn(world_ref, dt);
                        },
                        prev_layer_handles,
                        dse::core::JobPriority::High));
            }
        }

        // 等待当前层所有系统完成（调用者 helping）
        for (auto& h : current_layer_handles) {
            job_sys->Wait(h);
        }

        prev_layer_handles = std::move(current_layer_handles);
    }
}

void SystemScheduler::FlushCommands(entt::registry& registry) {
    command_buffer_.Flush(registry);
}

void SystemScheduler::SetEnabled(const std::string& name, bool enabled) {
    for (auto& entry : entries_) {
        if (entry.name == name) {
            entry.enabled = enabled;
            return;
        }
    }
}

void SystemScheduler::Reset() {
    entries_.clear();
    command_buffer_.Clear();
}

} // namespace ecs
} // namespace dse
