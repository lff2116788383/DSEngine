/**
 * @file tracking_allocator.cpp
 * @brief TrackingAllocator 实现。
 */

#include "engine/core/memory/tracking_allocator.h"
#include "engine/core/memory/memory_tracker.h"

#include <cstring>
#include <cstdint>

namespace dse {
namespace core {

namespace {

/// 追踪块头：位于用户指针之前，记录释放与统计所需信息。
struct TrackHeader {
    void* inner;        ///< inner 分配器返回的基址
    size_t size;        ///< 用户请求字节数
    uint16_t tag;       ///< 内存标签
    uint16_t magic;     ///< 校验
};

constexpr uint16_t kTrackMagic = 0xD5E2;
constexpr size_t kTrackHeaderAlign = alignof(TrackHeader);

TrackHeader* HeaderOf(void* user_ptr) {
    return reinterpret_cast<TrackHeader*>(user_ptr) - 1;
}

} // namespace

TrackingAllocator::TrackingAllocator(IAllocator* inner, MemoryTracker* tracker)
    : inner_(inner), tracker_(tracker) {}

void* TrackingAllocator::Allocate(size_t size, size_t alignment, uint16_t tag) {
    if (alignment < kTrackHeaderAlign) {
        alignment = kTrackHeaderAlign;
    }
    if (!IsPowerOfTwo(alignment)) {
        return nullptr;
    }

    // 用户指针前预留对齐到 alignment 的整数倍空间，保证 user 仍对齐且块头可放下。
    const size_t prefix = AlignUp(sizeof(TrackHeader), alignment);
    void* base = inner_->Allocate(prefix + size, alignment, tag);
    if (base == nullptr) {
        return nullptr;
    }
    void* user_ptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base) + prefix);

    TrackHeader* h = HeaderOf(user_ptr);
    h->inner = base;
    h->size = size;
    h->tag = tag;
    h->magic = kTrackMagic;

    if (tracker_ != nullptr) {
        tracker_->OnAlloc(tag, size, user_ptr);
    }
    return user_ptr;
}

void TrackingAllocator::Deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    TrackHeader* h = HeaderOf(ptr);
    if (h->magic != kTrackMagic) {
        return;
    }
    if (tracker_ != nullptr) {
        tracker_->OnFree(h->tag, h->size, ptr);
    }
    void* inner = h->inner;
    h->magic = 0;
    inner_->Deallocate(inner);
}

void* TrackingAllocator::Reallocate(void* ptr, size_t new_size, size_t alignment, uint16_t tag) {
    if (ptr == nullptr) {
        return Allocate(new_size, alignment, tag);
    }
    if (new_size == 0) {
        Deallocate(ptr);
        return nullptr;
    }
    TrackHeader* h = HeaderOf(ptr);
    const size_t old_size = (h->magic == kTrackMagic) ? h->size : 0;
    void* dst = Allocate(new_size, alignment, tag);
    if (dst == nullptr) {
        return nullptr;
    }
    std::memcpy(dst, ptr, old_size < new_size ? old_size : new_size);
    Deallocate(ptr);
    return dst;
}

} // namespace core
} // namespace dse
