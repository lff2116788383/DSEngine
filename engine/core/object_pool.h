#ifndef DSE_OBJECT_POOL_H
#define DSE_OBJECT_POOL_H

#include <vector>
#include <functional>
#include <cstddef>

namespace dse::core {

template <typename T>
class ObjectPool {
public:
    using Factory = std::function<T()>;

    explicit ObjectPool(std::size_t initial_capacity = 0, Factory factory = Factory())
        : factory_(factory) {
        Reserve(initial_capacity);
    }

    void Reserve(std::size_t capacity) {
        for (std::size_t i = free_list_.size(); i < capacity; ++i) {
            free_list_.push_back(CreateObject());
        }
    }

    T Acquire() {
        if (free_list_.empty()) {
            return CreateObject();
        }
        T value = free_list_.back();
        free_list_.pop_back();
        return value;
    }

    void Release(T value) {
        free_list_.push_back(value);
    }

    std::size_t AvailableCount() const {
        return free_list_.size();
    }

private:
    T CreateObject() {
        if (factory_) {
            return factory_();
        }
        return T{};
    }

    Factory factory_;
    std::vector<T> free_list_;
};

}

#endif
