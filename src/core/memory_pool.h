#ifndef DSE_CORE_MEMORY_POOL_H
#define DSE_CORE_MEMORY_POOL_H

#include <vector>
#include <memory>
#include <mutex>

namespace dse {
namespace core {

template<typename T>
class MemoryPool {
public:
    explicit MemoryPool(size_t initial_capacity = 100) {
        Expand(initial_capacity);
    }

    ~MemoryPool() {
        for (auto* ptr : all_blocks_) {
            delete ptr;
        }
    }

    T* Allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_list_.empty()) {
            Expand(all_blocks_.size() > 0 ? all_blocks_.size() * 2 : 100);
        }
        T* ptr = free_list_.back();
        free_list_.pop_back();
        return new(ptr) T(); // Placement new to initialize
    }

    void Free(T* ptr) {
        if (!ptr) return;
        ptr->~T(); // Explicit destructor call
        std::lock_guard<std::mutex> lock(mutex_);
        free_list_.push_back(ptr);
    }

private:
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
