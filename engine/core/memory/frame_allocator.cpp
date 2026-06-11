/**
 * @file frame_allocator.cpp
 * @brief FrameAllocator 实现。
 */

#include "engine/core/memory/frame_allocator.h"
#include "engine/base/debug.h"

namespace dse {
namespace core {

void FrameAllocator::Init(size_t per_buffer_capacity, uint32_t buffer_count, MemoryTag tag) {
    Shutdown();
    if (buffer_count < 2) {
        buffer_count = 2;
    }
    if (buffer_count > kMaxFrameBuffers) {
        buffer_count = kMaxFrameBuffers;
    }
    buffer_count_ = buffer_count;
    current_ = 0;
    frame_number_ = 0;
    for (uint32_t i = 0; i < buffer_count_; ++i) {
        buffers_[i].Init(per_buffer_capacity, tag);
    }
}

void FrameAllocator::Shutdown() {
    for (uint32_t i = 0; i < buffer_count_; ++i) {
        buffers_[i].Shutdown();
    }
    buffer_count_ = 0;
    current_ = 0;
}

void FrameAllocator::BeginFrame() {
    if (buffer_count_ == 0) {
        return;
    }
    current_ = (current_ + 1) % buffer_count_;
    buffers_[current_].Reset();
    ++frame_number_;
}

void* FrameAllocator::Alloc(size_t size, size_t alignment) {
    if (buffer_count_ == 0) {
        return nullptr;
    }
    return buffers_[current_].Allocate(size, alignment);
}

} // namespace core
} // namespace dse
