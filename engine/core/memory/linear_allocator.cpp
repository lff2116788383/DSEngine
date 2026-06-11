/**
 * @file linear_allocator.cpp
 * @brief LinearAllocator 实现。
 */

#include "engine/core/memory/linear_allocator.h"
#include "engine/core/memory/memory.h"
#include "engine/base/debug.h"

#include <cstdint>

namespace dse {
namespace core {

LinearAllocator::~LinearAllocator() {
    Shutdown();
}

void LinearAllocator::Init(size_t capacity, MemoryTag tag) {
    Shutdown();
    tag_ = TagId(tag);
    capacity_ = capacity;
    offset_ = 0;
    high_water_ = 0;
    overflow_count_ = 0;
    if (capacity_ > 0) {
        buffer_ = static_cast<uint8_t*>(Memory::AllocAligned(capacity_, kDefaultAlignment, tag));
    }
}

void LinearAllocator::Shutdown() {
    if (buffer_ != nullptr) {
        Memory::Free(buffer_);
        buffer_ = nullptr;
    }
    capacity_ = 0;
    offset_ = 0;
}

void* LinearAllocator::Allocate(size_t size, size_t alignment) {
    if (buffer_ == nullptr || size == 0 || !IsPowerOfTwo(alignment)) {
        return nullptr;
    }
    const uintptr_t base = reinterpret_cast<uintptr_t>(buffer_);
    const uintptr_t cur = base + offset_;
    const uintptr_t aligned = AlignUp(cur, alignment);
    const size_t new_offset = (aligned - base) + size;
    if (new_offset > capacity_) {
        ++overflow_count_;
        DEBUG_LOG_WARN("[LinearAllocator] overflow: request {} bytes (align {}), "
                       "used {}/{} bytes — returning nullptr (caller should fall back to heap)",
                       size, alignment, offset_, capacity_);
        return nullptr;
    }
    offset_ = new_offset;
    if (offset_ > high_water_) {
        high_water_ = offset_;
    }
    return reinterpret_cast<void*>(aligned);
}

void LinearAllocator::Reset() {
    offset_ = 0;
}

void LinearAllocator::Rewind(size_t mark) {
    if (mark <= offset_) {
        offset_ = mark;
    }
}

} // namespace core
} // namespace dse
