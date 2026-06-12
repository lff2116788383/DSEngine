/**
 * @file pool_allocator.h
 * @brief 固定大小块池分配器：具体类型、无虚函数，O(1) 分配/回收，无碎片。
 *
 * 块以侵入式空闲链表管理（空闲块内存复用为 next 指针）。容量不足时按 chunk
 * 成倍/固定增长，底层内存经 Memory 门面申请；Shutdown 释放全部 chunk。
 * 单实例非线程安全：跨线程使用需调用方自行加锁（见 JobSystem 完成信号池示范）。
 */

#ifndef DSE_CORE_MEMORY_POOL_ALLOCATOR_H
#define DSE_CORE_MEMORY_POOL_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include "engine/core/memory/allocator.h"

namespace dse {
namespace core {

/**
 * @class PoolAllocator
 * @brief 固定大小块的池分配器。
 */
class DSE_EXPORT PoolAllocator {
public:
    PoolAllocator() = default;
    ~PoolAllocator();

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&) = delete;
    PoolAllocator& operator=(PoolAllocator&&) = delete;

    /**
     * @brief 初始化（幂等：重复 Init 先释放旧 chunk）。
     * @param block_size 单块字节数（自动抬升至 >= sizeof(void*)）。
     * @param block_alignment 块对齐（2 的幂）。
     * @param blocks_per_chunk 每个 chunk 的块数（同时作为首块预留容量）。
     * @param tag 统计标签。
     */
    void Init(size_t block_size, size_t block_alignment, size_t blocks_per_chunk,
              MemoryTag tag = MemoryTag::Default);
    void Shutdown();

    /// 取一块（空闲耗尽时自动扩容；扩容失败返回 nullptr）。
    void* Allocate();
    /// 归还一块（nullptr 安全忽略）。
    void Free(void* ptr);

    size_t BlockSize() const { return block_stride_; }
    size_t Capacity() const { return total_blocks_; }
    size_t FreeCount() const { return free_count_; }
    size_t UsedCount() const { return total_blocks_ - free_count_; }
    size_t ChunkCount() const { return chunk_count_; }
    bool IsInitialized() const { return block_stride_ > 0; }

private:
    struct FreeNode { FreeNode* next; };
    struct Chunk { void* memory; Chunk* next; };

    bool AddChunk(size_t block_count);

    FreeNode* free_head_ = nullptr;
    Chunk* chunks_ = nullptr;
    size_t block_stride_ = 0;
    size_t block_align_ = 0;
    size_t blocks_per_chunk_ = 0;
    size_t total_blocks_ = 0;
    size_t free_count_ = 0;
    size_t chunk_count_ = 0;
    uint16_t tag_ = TagId(MemoryTag::Default);
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_POOL_ALLOCATOR_H
