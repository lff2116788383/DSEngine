#ifndef DSE_CORE_MEMORY_POOL_H
#define DSE_CORE_MEMORY_POOL_H

#include <vector>
#include <memory>
#include <stdexcept>

namespace core {

template <typename T>
class MemoryPool {
public:
    MemoryPool(size_t pool_size = 1024) {
        pool_.reserve(pool_size);
        for (size_t i = 0; i < pool_size; ++i) {
            pool_.push_back(std::make_unique<T>());
            free_list_.push_back(pool_.back().get());
        }
    }

    T* Allocate() {
        if (free_list_.empty()) {
            // Expand pool
            pool_.push_back(std::make_unique<T>());
            return pool_.back().get();
        }
        T* obj = free_list_.back();
        free_list_.pop_back();
        return obj;
    }

    void Free(T* obj) {
        free_list_.push_back(obj);
    }

private:
    std::vector<std::unique_ptr<T>> pool_;
    std::vector<T*> free_list_;
};

} // namespace core

#endif // DSE_CORE_MEMORY_POOL_H
