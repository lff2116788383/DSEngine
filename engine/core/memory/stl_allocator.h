/**
 * @file stl_allocator.h
 * @brief 无状态 STL 容器适配器 StlAllocator<T, Tag> + Dse* 容器别名（仅提供，不推广）。
 *
 * 设计见 MEMORY_MANAGEMENT_DESIGN.md §3.9：
 *   - 无状态，`tag` 为模板非类型参数；分配/释放全部转发 `dse::core::Memory` 门面，
 *     从而纳入统一标签化追踪 / 预算视图。
 *   - **硬规矩**：`DseVector<T,Tag>` 与 `std::vector<T>` 是**不同类型**。跨接口/模块
 *     边界一律用 `std` 默认分配器容器；`Dse*` 容器只在实现内部局部使用，避免类型传染。
 *
 * 用法示例（仅实现内部）：
 * @code
 *   using namespace dse::core;
 *   DseVector<int, MemoryTag::Mesh> indices;   // 分配计入 MemoryTag::Mesh
 *   indices.reserve(1024);
 *   DseString<MemoryTag::Asset> name = "long string forcing heap allocation ...";
 * @endcode
 */

#ifndef DSE_CORE_MEMORY_STL_ALLOCATOR_H
#define DSE_CORE_MEMORY_STL_ALLOCATOR_H

#include <cstddef>
#include <functional>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/core/memory/allocator.h"
#include "engine/core/memory/memory.h"

namespace dse {
namespace core {

/**
 * @class StlAllocator
 * @brief 满足 C++ 具名要求 *Allocator* 的无状态分配器，转发到 `Memory` 门面。
 * @tparam T   元素类型。
 * @tparam Tag 计费标签（模板非类型参数，默认 `MemoryTag::Default`）。
 *
 * 无状态：所有同 `Tag` 的实例互相等价（`is_always_equal = true`）。释放不依赖标签，
 * 因此不同标签的实例之间也能安全互相释放（仅计费归属不同）。
 */
template <typename T, MemoryTag Tag = MemoryTag::Default>
class StlAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    /// 该分配器计费的标签。
    static constexpr MemoryTag kTag = Tag;

    template <typename U>
    struct rebind {
        using other = StlAllocator<U, Tag>;
    };

    constexpr StlAllocator() noexcept = default;

    template <typename U>
    constexpr StlAllocator(const StlAllocator<U, Tag>&) noexcept {}

    [[nodiscard]] T* allocate(size_type n) {
        if (n == 0) {
            return nullptr;
        }
        if (n > max_size()) {
            throw std::bad_array_new_length();
        }
        void* p = Memory::AllocAligned(n * sizeof(T), alignof(T), Tag);
        if (p == nullptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(p);
    }

    void deallocate(T* p, size_type /*n*/) noexcept {
        Memory::Free(p);
    }

    constexpr size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }
};

// 同 Tag 的任意元素类型恒等价；不同 Tag 视为不等（语义上归属不同视图）。
template <typename T1, MemoryTag Tag1, typename T2, MemoryTag Tag2>
constexpr bool operator==(const StlAllocator<T1, Tag1>&, const StlAllocator<T2, Tag2>&) noexcept {
    return Tag1 == Tag2;
}

template <typename T1, MemoryTag Tag1, typename T2, MemoryTag Tag2>
constexpr bool operator!=(const StlAllocator<T1, Tag1>& a, const StlAllocator<T2, Tag2>& b) noexcept {
    return !(a == b);
}

// ============================================================
// Dse* 容器别名（仅实现内部局部使用，勿跨接口/模块边界传递）
// ============================================================

template <typename T, MemoryTag Tag = MemoryTag::Default>
using DseVector = std::vector<T, StlAllocator<T, Tag>>;

template <typename T, MemoryTag Tag = MemoryTag::Default>
using DseDeque = std::deque<T, StlAllocator<T, Tag>>;

template <typename T, MemoryTag Tag = MemoryTag::Default>
using DseList = std::list<T, StlAllocator<T, Tag>>;

template <MemoryTag Tag = MemoryTag::Default>
using DseString = std::basic_string<char, std::char_traits<char>, StlAllocator<char, Tag>>;

template <typename Key, typename Value, MemoryTag Tag = MemoryTag::Default,
          typename Compare = std::less<Key>>
using DseMap = std::map<Key, Value, Compare, StlAllocator<std::pair<const Key, Value>, Tag>>;

template <typename Key, typename Value, MemoryTag Tag = MemoryTag::Default,
          typename Hash = std::hash<Key>, typename KeyEq = std::equal_to<Key>>
using DseUnorderedMap =
    std::unordered_map<Key, Value, Hash, KeyEq, StlAllocator<std::pair<const Key, Value>, Tag>>;

template <typename Key, MemoryTag Tag = MemoryTag::Default, typename Compare = std::less<Key>>
using DseSet = std::set<Key, Compare, StlAllocator<Key, Tag>>;

template <typename Key, MemoryTag Tag = MemoryTag::Default, typename Hash = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>>
using DseUnorderedSet = std::unordered_set<Key, Hash, KeyEq, StlAllocator<Key, Tag>>;

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_STL_ALLOCATOR_H
