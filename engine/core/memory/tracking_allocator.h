/**
 * @file tracking_allocator.h
 * @brief 通用堆追踪装饰器：包裹任意 IAllocator，向 MemoryTracker 上报每标签统计。
 *
 * 自带紧凑块头（记录 inner 基址/size/tag），因此与具体后端无关
 * （可叠加在 SystemAllocator 或未来 mimalloc 之上）。仅 Debug 启用。
 */

#ifndef DSE_CORE_MEMORY_TRACKING_ALLOCATOR_H
#define DSE_CORE_MEMORY_TRACKING_ALLOCATOR_H

#include "engine/core/memory/allocator.h"

namespace dse {
namespace core {

class MemoryTracker;

/**
 * @class TrackingAllocator
 * @brief 包裹内层堆分配器，分配/释放时更新 MemoryTracker。
 */
class DSE_EXPORT TrackingAllocator : public IAllocator {
public:
    TrackingAllocator(IAllocator* inner, MemoryTracker* tracker);
    ~TrackingAllocator() override = default;

    void* Allocate(size_t size, size_t alignment, uint16_t tag) override;
    void Deallocate(void* ptr) override;
    void* Reallocate(void* ptr, size_t new_size, size_t alignment, uint16_t tag) override;
    const char* Name() const override { return "TrackingAllocator"; }

    IAllocator* Inner() const { return inner_; }

private:
    IAllocator* inner_;
    MemoryTracker* tracker_;
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_TRACKING_ALLOCATOR_H
