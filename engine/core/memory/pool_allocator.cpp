/**
 * @file pool_allocator.cpp
 * @brief PoolAllocator 实现。
 */

#include "engine/core/memory/pool_allocator.h"
#include "engine/core/memory/memory.h"
#include "engine/base/debug.h"

#include <cstdint>

namespace dse {
namespace core {

PoolAllocator::~PoolAllocator() {
    Shutdown();
}

void PoolAllocator::Init(size_t block_size, size_t block_alignment,
                         size_t blocks_per_chunk, MemoryTag tag) {
    Shutdown();
    if (!IsPowerOfTwo(block_alignment)) {
        block_alignment = kDefaultAlignment;
    }
    // 块需能容纳侵入式链表节点，且步长按对齐取整。
    size_t stride = block_size < sizeof(FreeNode) ? sizeof(FreeNode) : block_size;
    stride = AlignUp(stride, block_alignment);
    block_stride_ = stride;
    block_align_ = block_alignment;
    blocks_per_chunk_ = blocks_per_chunk == 0 ? 32 : blocks_per_chunk;
    tag_ = TagId(tag);
    free_head_ = nullptr;
    chunks_ = nullptr;
    total_blocks_ = 0;
    free_count_ = 0;
    chunk_count_ = 0;
    AddChunk(blocks_per_chunk_);
}

void PoolAllocator::Shutdown() {
    Chunk* c = chunks_;
    while (c != nullptr) {
        Chunk* next = c->next;
        Memory::Free(c->memory);
        Memory::Free(c);
        c = next;
    }
    chunks_ = nullptr;
    free_head_ = nullptr;
    block_stride_ = 0;
    total_blocks_ = 0;
    free_count_ = 0;
    chunk_count_ = 0;
}

bool PoolAllocator::AddChunk(size_t block_count) {
    if (block_count == 0) {
        block_count = blocks_per_chunk_;
    }
    void* mem = Memory::AllocAligned(block_stride_ * block_count, block_align_,
                                     static_cast<MemoryTag>(tag_));
    if (mem == nullptr) {
        DEBUG_LOG_WARN("[PoolAllocator] chunk allocation failed: {} blocks x {} bytes",
                       block_count, block_stride_);
        return false;
    }
    Chunk* node = static_cast<Chunk*>(Memory::Alloc(sizeof(Chunk), static_cast<MemoryTag>(tag_)));
    if (node == nullptr) {
        Memory::Free(mem);
        return false;
    }
    node->memory = mem;
    node->next = chunks_;
    chunks_ = node;
    ++chunk_count_;

    // 将整块切分为定长块并串入空闲链表。
    uint8_t* base = static_cast<uint8_t*>(mem);
    for (size_t i = 0; i < block_count; ++i) {
        auto* fn = reinterpret_cast<FreeNode*>(base + i * block_stride_);
        fn->next = free_head_;
        free_head_ = fn;
    }
    total_blocks_ += block_count;
    free_count_ += block_count;
    return true;
}

void* PoolAllocator::Allocate() {
    if (block_stride_ == 0) {
        return nullptr;
    }
    if (free_head_ == nullptr) {
        // 按当前容量翻倍增长（首 chunk 已在 Init 建立）。
        if (!AddChunk(blocks_per_chunk_)) {
            return nullptr;
        }
    }
    FreeNode* node = free_head_;
    free_head_ = node->next;
    --free_count_;
    return node;
}

void PoolAllocator::Free(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    auto* node = static_cast<FreeNode*>(ptr);
    node->next = free_head_;
    free_head_ = node;
    ++free_count_;
}

} // namespace core
} // namespace dse
