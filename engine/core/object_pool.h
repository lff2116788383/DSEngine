/**
 * @file object_pool.h
 * @brief 泛型对象池：原地构造/析构（placement-new），避免频繁堆分配与对象拷贝。
 *
 * 阶段4 重写：内部由固定大小的 PoolAllocator 支撑，`Acquire` 在池内存上原地
 * 构造对象并返回 T*，`Release` 显式析构后归还内存。相比旧版「按值拷贝存储」，
 * 适用于不可拷贝/重对象，且无额外拷贝开销。
 *
 * 单实例非线程安全：跨线程使用需调用方加锁。
 */

#ifndef DSE_OBJECT_POOL_H
#define DSE_OBJECT_POOL_H

#include <cstddef>
#include <new>
#include <utility>

#include "engine/core/memory/allocator.h"
#include "engine/core/memory/pool_allocator.h"

namespace dse::core {

/**
 * @class ObjectPool
 * @brief 原地存储的对象池，适用于频繁获取/归还同类型对象的场景。
 */
template <typename T>
class ObjectPool {
public:
    /**
     * @brief 构造对象池并预留容量。
     * @param initial_capacity 预留对象数（同时作为底层 chunk 的块数）。
     * @param tag 内存统计标签。
     */
    explicit ObjectPool(std::size_t initial_capacity = 0,
                        MemoryTag tag = MemoryTag::Default) {
        pool_.Init(sizeof(T), alignof(T),
                   initial_capacity == 0 ? kDefaultChunkBlocks : initial_capacity, tag);
    }

    ~ObjectPool() { pool_.Shutdown(); }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    /**
     * @brief 取一个对象：在池内存上原地构造并返回指针（容量不足自动扩容）。
     */
    template <class... Args>
    T* Acquire(Args&&... args) {
        void* mem = pool_.Allocate();
        if (mem == nullptr) {
            return nullptr;
        }
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    /**
     * @brief 归还对象：显式析构并将内存退回池。
     */
    void Release(T* obj) {
        if (obj == nullptr) {
            return;
        }
        obj->~T();
        pool_.Free(obj);
    }

    /// 当前立即可用（未占用）的块数。
    std::size_t AvailableCount() const { return pool_.FreeCount(); }
    /// 已占用的对象数。
    std::size_t UsedCount() const { return pool_.UsedCount(); }
    /// 池总容量（块数）。
    std::size_t Capacity() const { return pool_.Capacity(); }

private:
    static constexpr std::size_t kDefaultChunkBlocks = 32;
    PoolAllocator pool_;
};

} // namespace dse::core

#endif // DSE_OBJECT_POOL_H
