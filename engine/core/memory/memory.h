/**
 * @file memory.h
 * @brief 内存子系统统一门面 dse::core::Memory。
 *
 * 所有引擎自管的通用堆分配应经此门面，以便集中追踪、限额与替换后端。
 * 详见 docs/architecture/MEMORY_MANAGEMENT_DESIGN.md。
 *
 * 阶段1（当前）：门面 + SystemAllocator 后端，行为等价于 malloc/free。
 * 追踪/预算/帧分配器/池在后续阶段接入。
 */

#ifndef DSE_CORE_MEMORY_MEMORY_H
#define DSE_CORE_MEMORY_MEMORY_H

#include <new>
#include <utility>
#include "engine/core/memory/allocator.h"

namespace dse {
namespace core {

/**
 * @struct MemoryConfig
 * @brief 内存子系统初始化配置（后续阶段扩展：后端选择、帧缓冲大小等）。
 */
struct MemoryConfig {
    // 阶段1暂无可配项；预留以稳定 Init 接口。
};

/**
 * @class Memory
 * @brief 进程级内存门面。生命周期由 EngineInstance 在最早期 Init、最末期 Shutdown。
 *
 * 设计边界：门面只负责「分配 + 统计采集」，不持有业务状态/策略；
 * 因其早于 ServiceLocator，故为进程级而非注册服务（见设计文档 §4.1）。
 * 未显式 Init 时首次使用会惰性初始化，便于测试与早期分配。
 */
class DSE_EXPORT Memory {
public:
    /// 初始化（幂等）。EngineInstance 构造最早期调用。
    static void Init(const MemoryConfig& config = MemoryConfig{});

    /// 关闭：输出泄漏报告（后续阶段）。后端为进程级，不在此销毁以容忍晚期释放。
    static void Shutdown();

    /// 通用堆分配（默认对齐）。
    static void* Alloc(size_t size, MemoryTag tag = MemoryTag::Default);

    /// 通用堆分配（指定对齐，必须为 2 的幂）。
    static void* AllocAligned(size_t size, size_t alignment, MemoryTag tag = MemoryTag::Default);

    /// 重新分配，保留原数据。
    static void* Realloc(void* ptr, size_t new_size, MemoryTag tag = MemoryTag::Default);

    /// 释放（size/标签由块头记录）。
    static void Free(void* ptr);

    /// 总量统计快照（聚合所有标签）。
    static MemoryStats TotalStats();

    /// 某标签统计快照（需启用 DSE_ENABLE_MEM_TRACKING；否则返回零值）。
    static MemoryStats Stats(MemoryTag tag);

    /// 是否已启用按标签追踪（编译期开关 DSE_ENABLE_MEM_TRACKING）。
    static bool TrackingEnabled();

    /// 输出泄漏/占用报告到日志（启用追踪时按标签，否则仅总量）。
    static void ReportLeaks();

    /// 访问通用堆后端（高级用法）。
    static IAllocator& Heap();

private:
    static IAllocator* heap_;
};

/**
 * @brief 经门面分配并构造对象（带标签），返回 T*。
 */
template <class T, class... Args>
T* New(MemoryTag tag, Args&&... args) {
    void* mem = Memory::AllocAligned(sizeof(T), alignof(T), tag);
    if (mem == nullptr) {
        return nullptr;
    }
    return ::new (mem) T(std::forward<Args>(args)...);
}

/**
 * @brief 析构并经门面释放由 New 创建的对象。
 */
template <class T>
void Delete(T* ptr) {
    if (ptr == nullptr) {
        return;
    }
    ptr->~T();
    Memory::Free(ptr);
}

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_MEMORY_H
