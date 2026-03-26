/**
 * @file memory_pool.h
 * @brief 核心内存池实现，提供对象的高效分配与回收，减少内存碎片
 */

#ifndef DSE_CORE_MEMORY_POOL_H
#define DSE_CORE_MEMORY_POOL_H

#include <vector>
#include <memory>
#include <mutex>

namespace dse {
namespace core {

template<typename T>
/**
 * @class MemoryPool
 * @brief 线程安全的对象内存池，使用 Placement New 避免频繁的堆内存分配
 */
class MemoryPool {
public:
    explicit MemoryPool(size_t initial_capacity = 100) {
        Expand(initial_capacity);
    }

    ~MemoryPool() {
        for (auto* ptr : all_blocks_) {
            ::operator delete(ptr);
        }
    }

    /**
     * @brief 从内存池中分配并初始化一个对象
     * @return 指向已初始化对象的指针
     */
    T* Allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_list_.empty()) {
            Expand(all_blocks_.size() > 0 ? all_blocks_.size() * 2 : 100);
        }
        T* ptr = free_list_.back();
        free_list_.pop_back();
        return new(ptr) T(); // Placement new to initialize
    }

    /**
     * @brief 销毁对象并将其内存归还给内存池
     * @param ptr 要回收的对象指针
     */
    void Free(T* ptr) {
        if (!ptr) return;
        ptr->~T(); // Explicit destructor call
        std::lock_guard<std::mutex> lock(mutex_);
        free_list_.push_back(ptr);
    }

private:
    /**
     * @brief 扩充内存池容量
     * @param count 需要新增的块数量
     */
    void Expand(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            T* ptr = static_cast<T*>(::operator new(sizeof(T)));
            all_blocks_.push_back(ptr);
            free_list_.push_back(ptr);
        }
    }

    std::vector<T*> all_blocks_;
    std::vector<T*> free_list_;
    std::mutex mutex_;
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_POOL_H
