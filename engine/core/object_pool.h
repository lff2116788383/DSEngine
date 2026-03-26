/**
 * @file object_pool.h
 * @brief 泛型对象池实现，提供重用现有对象的机制，避免频繁的分配和析构开销
 */

#ifndef DSE_OBJECT_POOL_H
#define DSE_OBJECT_POOL_H

#include <vector>
#include <functional>
#include <cstddef>

namespace dse::core {

template <typename T>
/**
 * @class ObjectPool
 * @brief 轻量级的对象池，适用于需要频繁获取和归还相同类型对象的场景
 */
class ObjectPool {
public:
    using Factory = std::function<T()>;

    /**
     * @brief 构造函数，初始化对象池
     * @param initial_capacity 初始容量，池会在创建时预分配这些对象
     * @param factory 自定义的对象构造工厂函数（可选）
     */
    explicit ObjectPool(std::size_t initial_capacity = 0, Factory factory = Factory())
        : factory_(factory) {
        Reserve(initial_capacity);
    }

    /**
     * @brief 预分配指定数量的对象到池中
     * @param capacity 目标容量
     */
    void Reserve(std::size_t capacity) {
        for (std::size_t i = free_list_.size(); i < capacity; ++i) {
            free_list_.push_back(CreateObject());
        }
    }

    /**
     * @brief 从对象池中获取一个对象，如果池为空则动态创建新对象
     * @return 获取到的对象实例
     */
    T Acquire() {
        if (free_list_.empty()) {
            return CreateObject();
        }
        T value = free_list_.back();
        free_list_.pop_back();
        return value;
    }

    /**
     * @brief 将使用完毕的对象归还到池中
     * @param value 要归还的对象
     */
    void Release(T value) {
        free_list_.push_back(value);
    }

    /**
     * @brief 获取当前池中立即可用的空闲对象数量
     * @return 空闲对象的个数
     */
    std::size_t AvailableCount() const {
        return free_list_.size();
    }

private:
    /**
     * @brief 内部方法：通过工厂函数或默认构造函数创建新对象
     * @return 新创建的对象
     */
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
