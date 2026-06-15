/**
 * @file mimalloc_allocator.h
 * @brief 可选通用堆后端：基于 mimalloc（微软高性能分配器）。
 *
 * 见 MEMORY_MANAGEMENT_DESIGN.md §3.4：仅当 CMake `DSE_MEM_BACKEND=mimalloc` 时启用
 * （宏 `DSE_MEM_USE_MIMALLOC`，默认 OFF，不动依赖树）。因所有分配都经门面 `Memory::Heap()`，
 * 翻开关即让全引擎走门面的分配整体切到 mimalloc，无需改其它代码。
 *
 * 本头文件不包含 `<mimalloc.h>`，可无条件被包含；实现仅在启用宏时编译（见 .cpp）。
 */

#ifndef DSE_CORE_MEMORY_MIMALLOC_ALLOCATOR_H
#define DSE_CORE_MEMORY_MIMALLOC_ALLOCATOR_H

#include <atomic>
#include "engine/core/memory/allocator.h"

namespace dse {
namespace core {

/**
 * @class MimallocAllocator
 * @brief mimalloc 后端，实现通用堆 `IAllocator`，线程安全。
 *
 * 字节统计以 `mi_usable_size` 为准（mimalloc 自管块大小，无需额外块头）。
 */
#if defined(DSE_MEM_USE_MIMALLOC)
class DSE_EXPORT MimallocAllocator : public IAllocator {
#else
// 未启用 mimalloc 后端时方法不编译（见 .cpp），故不可标 dllexport，
// 否则 DLL 构建会强制导出未定义的成员符号导致链接失败。
class MimallocAllocator : public IAllocator {
#endif
public:
    MimallocAllocator() = default;
    ~MimallocAllocator() override = default;

    void* Allocate(size_t size, size_t alignment, uint16_t tag) override;
    void Deallocate(void* ptr) override;
    void* Reallocate(void* ptr, size_t new_size, size_t alignment, uint16_t tag) override;
    const char* Name() const override { return "MimallocAllocator"; }

    /// 总量统计快照（聚合所有标签；以 mimalloc usable size 计）。
    MemoryStats TotalStats() const;

private:
    std::atomic<size_t> current_bytes_{0};
    std::atomic<size_t> peak_bytes_{0};
    std::atomic<size_t> alloc_count_{0};
    std::atomic<size_t> free_count_{0};
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_MIMALLOC_ALLOCATOR_H
