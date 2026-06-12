/**
 * @file allocator.h
 * @brief 内存子系统基础类型：内存标签、分配器接口、统计结构。
 *
 * 详见 docs/architecture/MEMORY_MANAGEMENT_DESIGN.md。
 * IAllocator 仅用于「通用堆」这一可替换后端层；线性/帧/池分配器是具体类型、无虚函数。
 */

#ifndef DSE_CORE_MEMORY_ALLOCATOR_H
#define DSE_CORE_MEMORY_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include "engine/core/dse_export.h"

namespace dse {
namespace core {

/**
 * @enum MemoryTag
 * @brief 内置内存标签，用于按子系统归类统计与预算。
 *
 * 插件/模块可在运行期通过 RegisterMemoryTag 注册新标签，得到 >= BuiltinCount 的 id。
 */
enum class MemoryTag : uint16_t {
    Default = 0,
    Render,
    RHI,
    Texture,
    Mesh,
    Material,
    Shader,
    Asset,
    Audio,
    Physics,
    ECS,
    Scene,
    Scripting,
    Net,
    Navigation,
    UI,
    Editor,
    Job,
    FrameTemp,
    BuiltinCount
};

/// 将 MemoryTag 转换为底层 id。
inline uint16_t TagId(MemoryTag tag) { return static_cast<uint16_t>(tag); }

/**
 * @brief 运行期注册一个新的内存标签（供 plugins/ 等使用）。
 * @param name 标签名（调用方需保证生命周期长于进程，或使用字面量）。
 * @return 新标签 id，保证 >= TagId(MemoryTag::BuiltinCount)。
 */
DSE_EXPORT uint16_t RegisterMemoryTag(const char* name);

/// 返回标签名（内置或运行期注册）；越界返回 "Unknown"。
DSE_EXPORT const char* MemoryTagName(uint16_t tag);
inline const char* MemoryTagName(MemoryTag tag) { return MemoryTagName(TagId(tag)); }

/// 当前已知标签数量（内置 + 运行期注册）。
DSE_EXPORT uint16_t MemoryTagCount();

/// 默认对齐（满足任意标准标量类型）。
constexpr size_t kDefaultAlignment = alignof(max_align_t);

/// 向上对齐到 alignment（必须为 2 的幂）。
inline size_t AlignUp(size_t value, size_t alignment) {
    return (value + (alignment - 1)) & ~(alignment - 1);
}

/// 判断是否为 2 的幂。
inline bool IsPowerOfTwo(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/**
 * @struct MemoryStats
 * @brief 某一标签（或总量）的内存统计快照。
 */
struct MemoryStats {
    size_t current = 0;      ///< 当前占用字节
    size_t peak = 0;         ///< 历史峰值字节
    size_t alloc_count = 0;  ///< 累计分配次数
    size_t free_count = 0;   ///< 累计释放次数
};

/**
 * @brief 预算超限回调：某标签用量越过其预算时调用一次（再次越过前不重复触发）。
 * @param tag 触发的标签 id。
 * @param usage 当前用量（字节，门面追踪量 + 子系统上报的外部用量）。
 * @param budget 该标签设定的预算（字节）。
 *
 * 未设置回调时默认仅输出告警日志。可由 AssetManager 等注册以触发 LRU 淘汰等策略。
 */
using BudgetExceededCallback = void (*)(uint16_t tag, size_t usage, size_t budget);

/**
 * @class IAllocator
 * @brief 通用堆后端接口（可替换：system / mimalloc / 追踪装饰器）。
 *
 * 仅此层使用虚函数；高频的线性/帧/池分配器为具体类型、不经此接口。
 */
class IAllocator {
public:
    virtual ~IAllocator() = default;

    /**
     * @brief 分配内存。
     * @param size 字节数。
     * @param alignment 对齐（2 的幂）。
     * @param tag 内存标签 id。
     * @return 对齐后的指针；失败返回 nullptr。
     */
    virtual void* Allocate(size_t size, size_t alignment, uint16_t tag) = 0;

    /**
     * @brief 释放由本分配器分配的内存（size/标签由块头记录，无需调用方提供）。
     */
    virtual void Deallocate(void* ptr) = 0;

    /**
     * @brief 重新分配，保留原数据 min(old, new) 字节。
     */
    virtual void* Reallocate(void* ptr, size_t new_size, size_t alignment, uint16_t tag) = 0;

    /// 后端名称（用于日志/诊断）。
    virtual const char* Name() const = 0;
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_ALLOCATOR_H
