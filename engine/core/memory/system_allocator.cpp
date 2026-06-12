/**
 * @file system_allocator.cpp
 * @brief SystemAllocator 实现。
 */

#include "engine/core/memory/system_allocator.h"

#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace dse {
namespace core {

namespace {

/// 块头：紧邻用户指针之前，记录释放所需信息与统计元数据。
struct BlockHeader {
    void* raw;          ///< std::malloc 返回的原始基址
    size_t size;        ///< 用户请求的字节数
    size_t alignment;   ///< 用户请求的对齐
    uint16_t tag;       ///< 内存标签 id
    uint16_t magic;     ///< 校验魔数，检测越界写/重复释放
};

constexpr uint16_t kHeaderMagic = 0xD5E1;

/// 块头自身需满足的对齐。
constexpr size_t kHeaderAlign = alignof(BlockHeader);

BlockHeader* HeaderOf(void* user_ptr) {
    return reinterpret_cast<BlockHeader*>(user_ptr) - 1;
}

} // namespace

void* SystemAllocator::Allocate(size_t size, size_t alignment, uint16_t tag) {
    if (alignment < kHeaderAlign) {
        alignment = kHeaderAlign;
    }
    if (!IsPowerOfTwo(alignment)) {
        return nullptr;
    }

    // 预留：块头 + 对齐冗余。用户指针前必须容纳一个 BlockHeader。
    const size_t header = sizeof(BlockHeader);
    const size_t total = size + alignment + header;
    void* raw = std::malloc(total);
    if (raw == nullptr) {
        return nullptr;
    }

    // 在 raw + header 之后向上对齐，确保用户指针前有完整块头空间。
    uintptr_t base = reinterpret_cast<uintptr_t>(raw) + header;
    uintptr_t user = AlignUp(static_cast<size_t>(base), alignment);
    void* user_ptr = reinterpret_cast<void*>(user);

    BlockHeader* h = HeaderOf(user_ptr);
    h->raw = raw;
    h->size = size;
    h->alignment = alignment;
    h->tag = tag;
    h->magic = kHeaderMagic;

    const size_t new_current = current_bytes_.fetch_add(size, std::memory_order_relaxed) + size;
    size_t prev_peak = peak_bytes_.load(std::memory_order_relaxed);
    while (new_current > prev_peak &&
           !peak_bytes_.compare_exchange_weak(prev_peak, new_current, std::memory_order_relaxed)) {
        // 重试直到峰值更新成功
    }
    alloc_count_.fetch_add(1, std::memory_order_relaxed);

    return user_ptr;
}

void SystemAllocator::Deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    BlockHeader* h = HeaderOf(ptr);
    // magic 校验：检测重复释放或越界破坏（断言留待追踪阶段强化，这里防御性提前返回）。
    if (h->magic != kHeaderMagic) {
        return;
    }
    const size_t size = h->size;
    void* raw = h->raw;
    h->magic = 0; // 防重复释放

    current_bytes_.fetch_sub(size, std::memory_order_relaxed);
    free_count_.fetch_add(1, std::memory_order_relaxed);

    std::free(raw);
}

void* SystemAllocator::Reallocate(void* ptr, size_t new_size, size_t alignment, uint16_t tag) {
    if (ptr == nullptr) {
        return Allocate(new_size, alignment, tag);
    }
    if (new_size == 0) {
        Deallocate(ptr);
        return nullptr;
    }
    BlockHeader* h = HeaderOf(ptr);
    const size_t old_size = (h->magic == kHeaderMagic) ? h->size : 0;
    void* dst = Allocate(new_size, alignment, tag);
    if (dst == nullptr) {
        return nullptr;
    }
    std::memcpy(dst, ptr, old_size < new_size ? old_size : new_size);
    Deallocate(ptr);
    return dst;
}

MemoryStats SystemAllocator::TotalStats() const {
    MemoryStats s;
    s.current = current_bytes_.load(std::memory_order_relaxed);
    s.peak = peak_bytes_.load(std::memory_order_relaxed);
    s.alloc_count = alloc_count_.load(std::memory_order_relaxed);
    s.free_count = free_count_.load(std::memory_order_relaxed);
    return s;
}

size_t SystemAllocator::AllocatedSize(void* ptr) {
    if (ptr == nullptr) {
        return 0;
    }
    BlockHeader* h = HeaderOf(ptr);
    return (h->magic == kHeaderMagic) ? h->size : 0;
}

uint16_t SystemAllocator::AllocatedTag(void* ptr) {
    if (ptr == nullptr) {
        return 0;
    }
    BlockHeader* h = HeaderOf(ptr);
    return (h->magic == kHeaderMagic) ? h->tag : 0;
}

} // namespace core
} // namespace dse
