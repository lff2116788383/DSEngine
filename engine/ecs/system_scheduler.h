/**
* @file system_scheduler.h
* @brief 并行 ECS 系统调度器 —— 基于组件读写集的依赖图调度
*
* 核心概念：
* - 每个 System 声明 ComponentAccess{reads[], writes[]}
* - 调度器构建依赖图：两 System 冲突 ⇔ 一方写了另一方读/写的组件
* - 无冲突的 System 作为带依赖的 Job 并行提交到 JobSystem
* - 结构性变更（建/删实体、加/删组件）走 CommandBuffer，帧内同步点 flush
*
* 使用方式：
*   SystemScheduler scheduler;
*   scheduler.Register("anim", {reads={Transform, Animator}, writes={BoneMatrix}}, anim_fn);
*   scheduler.Register("particle", {reads={Transform}, writes={Particle3D}}, particle_fn);
*   scheduler.BuildAndExecute(world, dt);  // 自动并行化
*
* 已知限制：
* - 当前使用 type_index 标识组件类型，运行期开销极小但非编译期
* - 不支持 System 内部动态发现的新依赖（需预先声明）
* - CommandBuffer 仅支持基本结构性操作（create/destroy/emplace/remove）
*/

#ifndef DSE_ECS_SYSTEM_SCHEDULER_H
#define DSE_ECS_SYSTEM_SCHEDULER_H

#include "engine/core/dse_export.h"
#include "engine/core/job_system.h"
#include "engine/ecs/world.h"
#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <typeindex>
#include <mutex>
#include <queue>

namespace dse {
namespace ecs {

/// 延迟执行的结构性变更命令（线程安全收集，帧同步点统一 flush）
struct DSE_EXPORT StructuralCommand {
    enum class Type {
        CreateEntity,
        DestroyEntity,
        EmplaceComponent,
        RemoveComponent,
    };

    Type type;
    ::Entity entity = entt::null;  ///< 全局 Entity 类型（定义于 world.h）
    std::type_index component_type = std::type_index(typeid(void));
    std::function<void(entt::registry&, ::Entity)> apply_fn;  ///< 类型擦除的组件操作
};

/// 命令缓冲区：收集结构性变更，在同步点统一应用
class DSE_EXPORT CommandBuffer {
public:
    /// 入队一条命令（线程安全）
    void Enqueue(StructuralCommand::Type type, ::Entity entity,
                 std::type_index comp_type,
                 std::function<void(entt::registry&, ::Entity)> apply_fn);

    /// flush 所有命令到 registry（主线程同步点调用）
    void Flush(entt::registry& registry);

    /// 清空命令缓冲（不执行）
    void Clear();

    /// 待执行命令数
    size_t PendingCount() const;

private:
    mutable std::mutex mutex_;
    std::queue<StructuralCommand> commands_;
};

/// 组件访问声明（用于依赖图构建）
/// @note 聚合体，支持 {reads, writes} 花括号初始化
struct ComponentAccess {
    std::vector<std::type_index> reads;   ///< 只读组件类型列表
    std::vector<std::type_index> writes;  ///< 读/写组件类型列表

    /// 创建只读访问声明
    template<typename... Ts>
    static ComponentAccess ReadOnly() {
        ComponentAccess a;
        a.reads = {std::type_index(typeid(Ts))...};
        return a;
    }

    /// 创建只写访问声明
    template<typename T>
    static ComponentAccess WriteOnly() {
        ComponentAccess a;
        a.writes = {std::type_index(typeid(T))};
        return a;
    }

    /// 创建读写访问声明（同时读和写同一组件）
    template<typename T>
    static ComponentAccess ReadWrite() {
        ComponentAccess a;
        a.reads = {std::type_index(typeid(T))};
        a.writes = {std::type_index(typeid(T))};
        return a;
    }

    /// 通用创建（等价于 ReadWrite<T>）
    template<typename T>
    static ComponentAccess Make() {
        return ReadWrite<T>();
    }
};

/// 系统更新函数类型
using SystemUpdateFn = std::function<void(::World&, float)>;

/// 系统注册条目
struct SystemEntry {
    std::string name;
    ComponentAccess access;
    SystemUpdateFn update_fn;
    bool enabled = true;
};

/// 系统调度器：基于组件读写集构建依赖图，并行执行无冲突系统
class DSE_EXPORT SystemScheduler {
public:
    SystemScheduler() = default;
    ~SystemScheduler() = default;

    /// 注册一个系统
    /// @param name 系统名称（用于 profiling/debug）
    /// @param access 组件读写声明
    /// @param update_fn 系统更新函数
    void Register(const std::string& name,
                  ComponentAccess access,
                  SystemUpdateFn update_fn);

    /// 注册一个无组件访问声明的系统（保守视为全写，不与其他系统并行）
    /// @param name 系统名称
    /// @param update_fn 系统更新函数
    void RegisterSerial(const std::string& name,
                        SystemUpdateFn update_fn);

    /// 构建依赖图并并行执行所有已注册系统
    /// @param world ECS World（全局命名空间 ::World）
    /// @param delta_time 帧间隔
    /// @note 阻塞直到所有系统完成。无 JobSystem 时退化为串行。
    void Execute(::World& world, float delta_time);

    /// 获取命令缓冲区（供系统内部收集结构性变更）
    CommandBuffer& GetCommandBuffer() { return command_buffer_; }

    /// flush 命令缓冲（通常在 Execute 后由调用方调用）
    void FlushCommands(entt::registry& registry);

    /// 启用/禁用指定系统
    void SetEnabled(const std::string& name, bool enabled);

    /// 获取系统数量
    size_t SystemCount() const { return entries_.size(); }

private:
    std::vector<SystemEntry> entries_;
    CommandBuffer command_buffer_;

    /// 检测两个系统是否有组件冲突
    /// 冲突条件：A 写了 B 读/写的组件，或 B 写了 A 读/写的组件
    static bool HasConflict(const ComponentAccess& a, const ComponentAccess& b);

    /// 构建依赖图：返回每个系统的依赖索引列表
    std::vector<std::vector<int>> BuildDependencyGraph() const;

    /// 拓扑排序 + 分层（同一层无冲突，可并行）
    std::vector<std::vector<int>> BuildParallelLayers(const std::vector<std::vector<int>>& deps) const;
};

} // namespace ecs
} // namespace dse

#endif // DSE_ECS_SYSTEM_SCHEDULER_H
