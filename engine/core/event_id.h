/**
 * @file event_id.h
 * @brief 跨 DLL 安全的事件类型标识，使用编译期 FNV-1a 哈希替代 std::type_index
 *
 * 核心设计：
 * 1. 使用 FNV-1a 64 位哈希确保事件 ID 在所有编译单元/DLL 中一致
 * 2. 集中定义已知事件 ID 常量，避免哈希冲突
 * 3. 编译期求值，零运行时开销
 */

#ifndef DSE_CORE_EVENT_ID_H
#define DSE_CORE_EVENT_ID_H

#include <cstdint>

namespace dse {
namespace core {

/// 事件类型标识符（64 位，跨 DLL 安全）
using EventId = std::uint64_t;

/**
 * @brief 编译期 FNV-1a 64 位字符串哈希
 * @param str 以空字符结尾的字符串字面量
 * @return 稳定的 64 位哈希值，跨所有编译单元一致
 *
 * @example
 * constexpr EventId my_event = MakeEventId("MyCustomEvent");
 */
constexpr EventId MakeEventId(const char* str) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    while (*str) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(*str));
        hash *= 0x100000001b3ull;
        ++str;
    }
    return hash;
}

/// 已注册事件 ID 常量（集中定义，避免冲突）
namespace events {
    constexpr EventId kUiClick         = MakeEventId("UiClick");
    constexpr EventId kResourceLoaded  = MakeEventId("ResourceLoaded");
    constexpr EventId kSceneLifecycle  = MakeEventId("SceneLifecycle");
} // namespace events

} // namespace core
} // namespace dse

#endif // DSE_CORE_EVENT_ID_H
