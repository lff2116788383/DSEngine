/**
 * @file memory_tracker.cpp
 * @brief MemoryTracker 实现。
 */

#include "engine/core/memory/memory_tracker.h"

#include "engine/base/debug.h"

namespace dse {
namespace core {

MemoryTracker& MemoryTracker::Instance() {
    static MemoryTracker instance;
    return instance;
}

void MemoryTracker::OnAlloc(uint16_t tag, size_t size, void* ptr) {
    TagCounters& c = tags_[Bucket(tag)];
    const size_t new_current = c.current.fetch_add(size, std::memory_order_relaxed) + size;
    size_t prev_peak = c.peak.load(std::memory_order_relaxed);
    while (new_current > prev_peak &&
           !c.peak.compare_exchange_weak(prev_peak, new_current, std::memory_order_relaxed)) {
    }
    c.alloc_count.fetch_add(1, std::memory_order_relaxed);

#if defined(DSE_MEM_TRACK_POINTERS)
    if (ptr != nullptr) {
        std::lock_guard<std::mutex> lock(pointers_mutex_);
        pointers_[ptr] = PointerInfo{tag, size};
    }
#else
    (void)ptr;
#endif
}

void MemoryTracker::OnFree(uint16_t tag, size_t size, void* ptr) {
    TagCounters& c = tags_[Bucket(tag)];
    c.current.fetch_sub(size, std::memory_order_relaxed);
    c.free_count.fetch_add(1, std::memory_order_relaxed);

#if defined(DSE_MEM_TRACK_POINTERS)
    if (ptr != nullptr) {
        std::lock_guard<std::mutex> lock(pointers_mutex_);
        pointers_.erase(ptr);
    }
#else
    (void)ptr;
#endif
}

MemoryStats MemoryTracker::Stats(uint16_t tag) const {
    const TagCounters& c = tags_[Bucket(tag)];
    MemoryStats s;
    s.current = c.current.load(std::memory_order_relaxed);
    s.peak = c.peak.load(std::memory_order_relaxed);
    s.alloc_count = c.alloc_count.load(std::memory_order_relaxed);
    s.free_count = c.free_count.load(std::memory_order_relaxed);
    return s;
}

MemoryStats MemoryTracker::TotalStats() const {
    MemoryStats total;
    for (uint16_t i = 0; i < kMaxTrackedTags; ++i) {
        const TagCounters& c = tags_[i];
        total.current += c.current.load(std::memory_order_relaxed);
        total.peak += c.peak.load(std::memory_order_relaxed);
        total.alloc_count += c.alloc_count.load(std::memory_order_relaxed);
        total.free_count += c.free_count.load(std::memory_order_relaxed);
    }
    return total;
}

bool MemoryTracker::HasLiveAllocations() const {
    for (uint16_t i = 0; i < kMaxTrackedTags; ++i) {
        const TagCounters& c = tags_[i];
        if (c.current.load(std::memory_order_relaxed) != 0 ||
            c.alloc_count.load(std::memory_order_relaxed) !=
                c.free_count.load(std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void MemoryTracker::Report(const char* scope_label) const {
    DEBUG_LOG_INFO("[MemoryTracker] report ({}); scope=engine-facade-only "
                   "(third-party/global allocations not included)",
                   scope_label ? scope_label : "");
    bool any = false;
    for (uint16_t i = 0; i < kMaxTrackedTags; ++i) {
        const TagCounters& c = tags_[i];
        const size_t current = c.current.load(std::memory_order_relaxed);
        const size_t allocs = c.alloc_count.load(std::memory_order_relaxed);
        const size_t frees = c.free_count.load(std::memory_order_relaxed);
        if (allocs == 0 && current == 0) {
            continue;
        }
        any = true;
        const char* name = MemoryTagName(i);
        DEBUG_LOG_INFO("  tag={} current={} bytes peak={} bytes allocs={} frees={} live={}",
                       name, current, c.peak.load(std::memory_order_relaxed), allocs, frees,
                       allocs - frees);
        if (current != 0 || allocs != frees) {
            DEBUG_LOG_WARN("  [LEAK] tag={} leaked {} bytes ({} live allocations)",
                           name, current, allocs - frees);
        }
    }
    if (!any) {
        DEBUG_LOG_INFO("  (no tracked allocations)");
    }
}

#if defined(DSE_MEM_TRACK_POINTERS)
size_t MemoryTracker::LivePointerCount() const {
    std::lock_guard<std::mutex> lock(pointers_mutex_);
    return pointers_.size();
}
#endif

} // namespace core
} // namespace dse
