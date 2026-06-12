/**
 * @file mimalloc_allocator.cpp
 * @brief MimallocAllocator 实现（仅在 DSE_MEM_USE_MIMALLOC 启用时编译，否则为空翻译单元）。
 */

#include "engine/core/memory/mimalloc_allocator.h"

#if defined(DSE_MEM_USE_MIMALLOC)

#include <mimalloc.h>

namespace dse {
namespace core {

namespace {

/// 以 new_current 更新峰值（CAS 重试）。
void UpdatePeak(std::atomic<size_t>& peak, size_t new_current) {
    size_t prev = peak.load(std::memory_order_relaxed);
    while (new_current > prev &&
           !peak.compare_exchange_weak(prev, new_current, std::memory_order_relaxed)) {
        // 重试直到峰值更新成功
    }
}

} // namespace

void* MimallocAllocator::Allocate(size_t size, size_t alignment, uint16_t tag) {
    (void)tag; // mimalloc 自身不分标签；标签统计由上层 TrackingAllocator 负责。
    if (alignment < alignof(max_align_t)) {
        alignment = alignof(max_align_t);
    }
    if (!IsPowerOfTwo(alignment)) {
        return nullptr;
    }
    void* p = mi_malloc_aligned(size, alignment);
    if (p == nullptr) {
        return nullptr;
    }
    const size_t actual = mi_usable_size(p);
    const size_t new_current =
        current_bytes_.fetch_add(actual, std::memory_order_relaxed) + actual;
    UpdatePeak(peak_bytes_, new_current);
    alloc_count_.fetch_add(1, std::memory_order_relaxed);
    return p;
}

void MimallocAllocator::Deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    const size_t actual = mi_usable_size(ptr);
    current_bytes_.fetch_sub(actual, std::memory_order_relaxed);
    free_count_.fetch_add(1, std::memory_order_relaxed);
    mi_free(ptr);
}

void* MimallocAllocator::Reallocate(void* ptr, size_t new_size, size_t alignment, uint16_t tag) {
    (void)tag;
    if (ptr == nullptr) {
        return Allocate(new_size, alignment, tag);
    }
    if (new_size == 0) {
        Deallocate(ptr);
        return nullptr;
    }
    if (alignment < alignof(max_align_t)) {
        alignment = alignof(max_align_t);
    }
    if (!IsPowerOfTwo(alignment)) {
        return nullptr;
    }
    const size_t old_actual = mi_usable_size(ptr);
    void* np = mi_realloc_aligned(ptr, new_size, alignment);
    if (np == nullptr) {
        return nullptr; // 原块未释放，调用方仍持有 ptr
    }
    const size_t new_actual = mi_usable_size(np);
    if (new_actual >= old_actual) {
        const size_t new_current =
            current_bytes_.fetch_add(new_actual - old_actual, std::memory_order_relaxed) +
            (new_actual - old_actual);
        UpdatePeak(peak_bytes_, new_current);
    } else {
        current_bytes_.fetch_sub(old_actual - new_actual, std::memory_order_relaxed);
    }
    alloc_count_.fetch_add(1, std::memory_order_relaxed);
    free_count_.fetch_add(1, std::memory_order_relaxed);
    return np;
}

MemoryStats MimallocAllocator::TotalStats() const {
    MemoryStats s;
    s.current = current_bytes_.load(std::memory_order_relaxed);
    s.peak = peak_bytes_.load(std::memory_order_relaxed);
    s.alloc_count = alloc_count_.load(std::memory_order_relaxed);
    s.free_count = free_count_.load(std::memory_order_relaxed);
    return s;
}

} // namespace core
} // namespace dse

#endif // DSE_MEM_USE_MIMALLOC
