/**
 * @file system_allocator.h
 * @brief 默认通用堆后端：基于 std::malloc/free，带块头以记录 size/对齐/标签。
 *
 * 块头紧邻用户指针之前，因此 Deallocate 无需调用方再传 size，统计与校验可靠。
 * 对齐通过过量分配 + 向上对齐实现，跨平台一致（不依赖 _aligned_malloc/posix_memalign）。
 */

#ifndef DSE_CORE_MEMORY_SYSTEM_ALLOCATOR_H
#define DSE_CORE_MEMORY_SYSTEM_ALLOCATOR_H

#include <atomic>
#include "engine/core/memory/allocator.h"

namespace dse {
namespace core {

/**
 * @class SystemAllocator
 * @brief 进程默认堆分配器，线程安全（malloc/free 本身线程安全；统计用原子）。
 */
class DSE_EXPORT SystemAllocator : public IAllocator {
public:
    SystemAllocator() = default;
    ~SystemAllocator() override = default;

    void* Allocate(size_t size, size_t alignment, uint16_t tag) override;
    void Deallocate(void* ptr) override;
    void* Reallocate(void* ptr, size_t new_size, size_t alignment, uint16_t tag) override;
    const char* Name() const override { return "SystemAllocator"; }

    /// 总量统计快照（聚合所有标签）。
    MemoryStats TotalStats() const;

    /// 返回某次分配的载荷大小（从块头读取，供 Realloc/统计使用）。
    static size_t AllocatedSize(void* ptr);
    /// 返回某次分配的标签（从块头读取）。
    static uint16_t AllocatedTag(void* ptr);

private:
    std::atomic<size_t> current_bytes_{0};
    std::atomic<size_t> peak_bytes_{0};
    std::atomic<size_t> alloc_count_{0};
    std::atomic<size_t> free_count_{0};
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_SYSTEM_ALLOCATOR_H
