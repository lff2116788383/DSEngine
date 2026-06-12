/**
 * @file handle.h
 * @brief 句柄化资源表：`Handle<T>`（index + generation）+ `HandleTable<T, Tag>`。
 *
 * 设计见 MEMORY_MANAGEMENT_DESIGN.md §3.10：用 index+generation 句柄逐步替换 RHI/资产
 * 热路径的 `shared_ptr`，去掉原子引用计数；句柄是平凡可拷贝的小整数，便于存入 ECS 组件 /
 * 序列化 / 网络传输。资源销毁后槽位的 generation 自增，旧句柄因代号不匹配而**自动失效**
 * （把"使用已释放对象"变为可检测失败，而非静默指向被复用对象）。
 *
 * 用法示例：
 * @code
 *   HandleTable<Texture, MemoryTag::Texture> textures;
 *   Handle<Texture> h = textures.Create(desc);
 *   if (Texture* t = textures.Get(h)) { ...  }
 *   textures.Destroy(h);     // 之后 textures.Get(h) == nullptr
 * @endcode
 *
 * 线程安全：`HandleTable` 自身**不加锁**，由调用方按子系统约束并发访问（与 RHI/资产管理
 * 现有模型一致）。
 */

#ifndef DSE_CORE_MEMORY_HANDLE_H
#define DSE_CORE_MEMORY_HANDLE_H

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

#include "engine/core/memory/allocator.h"
#include "engine/core/memory/stl_allocator.h"

namespace dse {
namespace core {

/**
 * @struct Handle
 * @brief 指向 `HandleTable<T>` 中某资源的轻量句柄（index + generation）。
 * @tparam T 资源类型（用于类型安全，不同 T 的句柄互不可混用）。
 *
 * 平凡可拷贝；默认构造为无效句柄。仅 `HandleTable` 的 `IsValid/Get` 能判定其是否仍有效
 * （需 generation 匹配）。
 */
template <typename T>
struct Handle {
    static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    uint32_t index = kInvalidIndex;
    uint32_t generation = 0;

    /// 是否为「非空」句柄（仅检查哨兵 index，不校验表内代号）。
    constexpr bool IsValid() const noexcept { return index != kInvalidIndex; }

    friend constexpr bool operator==(const Handle& a, const Handle& b) noexcept {
        return a.index == b.index && a.generation == b.generation;
    }
    friend constexpr bool operator!=(const Handle& a, const Handle& b) noexcept {
        return !(a == b);
    }
};

/**
 * @class HandleTable
 * @brief 以句柄管理 T 实例的资源表（原地存储 + generation 失效 + free-list 复用）。
 * @tparam T   资源类型。
 * @tparam Tag 表内部容器的内存标签（计入统一视图）。
 *
 * 槽位存于 `DseDeque`：push_back 不移动已存在元素，元素地址稳定 —— 故 `Get` 返回的指针在
 * 该句柄被 `Destroy` 前始终有效。释放的槽位入 free-list 复用，复用时 generation 自增使旧句柄失效。
 */
template <typename T, MemoryTag Tag = MemoryTag::Default>
class HandleTable {
public:
    HandleTable() = default;
    HandleTable(const HandleTable&) = delete;
    HandleTable& operator=(const HandleTable&) = delete;
    ~HandleTable() { Clear(); }

    /// 构造一个 T 并返回其句柄。
    template <typename... Args>
    Handle<T> Create(Args&&... args) {
        uint32_t idx;
        if (!free_.empty()) {
            idx = free_.back();
            free_.pop_back();
        } else {
            idx = static_cast<uint32_t>(slots_.size());
            slots_.emplace_back();
        }
        Slot& s = slots_[idx];
        ::new (static_cast<void*>(s.storage)) T(std::forward<Args>(args)...);
        s.occupied = true;
        ++live_;
        return Handle<T>{idx, s.generation};
    }

    /// 句柄是否仍有效（index 在范围内、槽位占用中、且 generation 匹配）。
    bool IsValid(Handle<T> h) const noexcept {
        return h.index < slots_.size() && slots_[h.index].occupied &&
               slots_[h.index].generation == h.generation;
    }

    /// 返回句柄指向的对象指针；无效则返回 nullptr。
    T* Get(Handle<T> h) noexcept {
        return IsValid(h) ? slots_[h.index].Ptr() : nullptr;
    }
    const T* Get(Handle<T> h) const noexcept {
        return IsValid(h) ? slots_[h.index].Ptr() : nullptr;
    }

    /// 析构并回收句柄指向的对象；成功返回 true。之后该句柄永久失效。
    bool Destroy(Handle<T> h) {
        if (!IsValid(h)) {
            return false;
        }
        Slot& s = slots_[h.index];
        s.Ptr()->~T();
        s.occupied = false;
        ++s.generation; // 失效所有旧句柄
        free_.push_back(h.index);
        --live_;
        return true;
    }

    /// 当前存活对象数量。
    size_t Size() const noexcept { return live_; }
    bool Empty() const noexcept { return live_ == 0; }

    /// 已分配的槽位总数（含空闲槽）。
    size_t Capacity() const noexcept { return slots_.size(); }

    /// 析构所有存活对象并清空（generation 保留以维持已发句柄的失效语义）。
    void Clear() {
        for (size_t i = 0; i < slots_.size(); ++i) {
            Slot& s = slots_[i];
            if (s.occupied) {
                s.Ptr()->~T();
                s.occupied = false;
                ++s.generation;
                free_.push_back(static_cast<uint32_t>(i));
            }
        }
        live_ = 0;
    }

private:
    struct Slot {
        alignas(T) unsigned char storage[sizeof(T)];
        uint32_t generation = 0;
        bool occupied = false;

        T* Ptr() noexcept { return reinterpret_cast<T*>(storage); }
        const T* Ptr() const noexcept { return reinterpret_cast<const T*>(storage); }
    };

    DseDeque<Slot, Tag> slots_;
    DseVector<uint32_t, Tag> free_;
    size_t live_ = 0;
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_HANDLE_H
