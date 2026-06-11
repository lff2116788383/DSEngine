/**
 * @file memory.cpp
 * @brief Memory 门面实现（阶段1：转发到 SystemAllocator）。
 */

#include "engine/core/memory/memory.h"
#include "engine/core/memory/system_allocator.h"
#include "engine/base/debug.h"

namespace dse {
namespace core {

namespace {

/// 进程默认堆。Meyers 单例规避静态初始化顺序问题。
SystemAllocator& DefaultHeap() {
    static SystemAllocator instance;
    return instance;
}

} // namespace

IAllocator* Memory::heap_ = nullptr;

void Memory::Init(const MemoryConfig& /*config*/) {
    if (heap_ == nullptr) {
        heap_ = &DefaultHeap();
    }
}

void Memory::Shutdown() {
    ReportLeaks();
    // 后端为进程级，刻意不在此销毁：进程退出期仍可能有晚期释放。
}

IAllocator& Memory::Heap() {
    if (heap_ == nullptr) {
        heap_ = &DefaultHeap();
    }
    return *heap_;
}

void* Memory::Alloc(size_t size, MemoryTag tag) {
    return Heap().Allocate(size, kDefaultAlignment, TagId(tag));
}

void* Memory::AllocAligned(size_t size, size_t alignment, MemoryTag tag) {
    return Heap().Allocate(size, alignment, TagId(tag));
}

void* Memory::Realloc(void* ptr, size_t new_size, MemoryTag tag) {
    return Heap().Reallocate(ptr, new_size, kDefaultAlignment, TagId(tag));
}

void Memory::Free(void* ptr) {
    Heap().Deallocate(ptr);
}

MemoryStats Memory::TotalStats() {
    return DefaultHeap().TotalStats();
}

void Memory::ReportLeaks() {
    const MemoryStats s = DefaultHeap().TotalStats();
    DEBUG_LOG_INFO("[Memory] heap={} current={} bytes peak={} bytes allocs={} frees={}",
                   DefaultHeap().Name(), s.current, s.peak, s.alloc_count, s.free_count);
    if (s.current != 0) {
        DEBUG_LOG_WARN("[Memory] {} bytes still live at report (live allocations={})",
                       s.current, s.alloc_count - s.free_count);
    }
}

} // namespace core
} // namespace dse
