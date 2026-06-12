/**
 * @file linear_allocator.h
 * @brief 线性（bump-pointer）分配器：具体类型、无虚函数，适合每帧/每任务瞬时数据。
 *
 * 仅向前推进指针，整体 Reset 一次性回收，不支持单块释放。
 * 溢出（超过容量）不崩溃：返回 nullptr 并计数 + 告警，调用方需处理（通常退回堆分配）。
 * 单实例非线程安全：每帧分配器供主线程用，每线程另有独立 ThreadScratch。
 */

#ifndef DSE_CORE_MEMORY_LINEAR_ALLOCATOR_H
#define DSE_CORE_MEMORY_LINEAR_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include "engine/core/memory/allocator.h"

namespace dse {
namespace core {

/**
 * @class LinearAllocator
 * @brief bump-pointer 线性分配器，底层缓冲经 Memory 门面一次性申请。
 */
class DSE_EXPORT LinearAllocator {
public:
    LinearAllocator() = default;
    ~LinearAllocator();

    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&) = delete;
    LinearAllocator& operator=(LinearAllocator&&) = delete;

    /// 申请底层缓冲（幂等：重复 Init 先释放旧缓冲）。
    void Init(size_t capacity, MemoryTag tag = MemoryTag::FrameTemp);
    /// 释放底层缓冲。
    void Shutdown();

    /// 分配 size 字节，按 alignment 对齐；容量不足返回 nullptr（不崩溃）。
    void* Allocate(size_t size, size_t alignment = kDefaultAlignment);

    /// 一次性回收：复位偏移到 0（不释放底层缓冲）。
    void Reset();

    /// 当前偏移标记，可配合 Rewind 做局部回退。
    size_t Mark() const { return offset_; }
    /// 回退到先前的 Mark（mark 必须 <= 当前偏移）。
    void Rewind(size_t mark);

    size_t Capacity() const { return capacity_; }
    size_t Used() const { return offset_; }
    size_t HighWater() const { return high_water_; }
    size_t OverflowCount() const { return overflow_count_; }
    bool IsInitialized() const { return buffer_ != nullptr; }

private:
    uint8_t* buffer_ = nullptr;
    size_t capacity_ = 0;
    size_t offset_ = 0;
    size_t high_water_ = 0;
    size_t overflow_count_ = 0;
    uint16_t tag_ = TagId(MemoryTag::FrameTemp);
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_LINEAR_ALLOCATOR_H
