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
///
/// 命名规范：k + PascalCase 事件名
/// 新增事件必须在此注册，确保跨 DLL 一致性
namespace events {
    // --- UI 事件 ---
    constexpr EventId kUiClick         = MakeEventId("UiClick");
    constexpr EventId kUiHover         = MakeEventId("UiHover");
    constexpr EventId kUiFocus         = MakeEventId("UiFocus");
    constexpr EventId kUiBlur          = MakeEventId("UiBlur");
    constexpr EventId kUiDrag          = MakeEventId("UiDrag");
    constexpr EventId kUiSubmit        = MakeEventId("UiSubmit");

    // --- 资源事件 ---
    constexpr EventId kResourceLoaded  = MakeEventId("ResourceLoaded");
    constexpr EventId kResourceUnloaded = MakeEventId("ResourceUnloaded");
    constexpr EventId kResourceLoadFailed = MakeEventId("ResourceLoadFailed");
    constexpr EventId kTextureReady    = MakeEventId("TextureReady");
    constexpr EventId kShaderCompiled  = MakeEventId("ShaderCompiled");

    // --- 场景/实体事件 ---
    constexpr EventId kSceneLifecycle  = MakeEventId("SceneLifecycle");
    constexpr EventId kEntityCreated   = MakeEventId("EntityCreated");
    constexpr EventId kEntityDestroyed = MakeEventId("EntityDestroyed");
    constexpr EventId kParentChanged   = MakeEventId("ParentChanged");
    constexpr EventId kTransformChanged = MakeEventId("TransformChanged");

    // --- 窗口/输入事件 ---
    constexpr EventId kWindowResize    = MakeEventId("WindowResize");
    constexpr EventId kWindowClose     = MakeEventId("WindowClose");
    constexpr EventId kWindowFocus     = MakeEventId("WindowFocus");
    constexpr EventId kKeyDown         = MakeEventId("KeyDown");
    constexpr EventId kKeyUp           = MakeEventId("KeyUp");
    constexpr EventId kMouseDown       = MakeEventId("MouseDown");
    constexpr EventId kMouseUp         = MakeEventId("MouseUp");
    constexpr EventId kMouseMove       = MakeEventId("MouseMove");
    constexpr EventId kMouseScroll     = MakeEventId("MouseScroll");

    // --- 物理/碰撞事件 ---
    constexpr EventId kCollisionBegin  = MakeEventId("CollisionBegin");
    constexpr EventId kCollisionEnd    = MakeEventId("CollisionEnd");
    constexpr EventId kTriggerEnter    = MakeEventId("TriggerEnter");
    constexpr EventId kTriggerExit     = MakeEventId("TriggerExit");

    // --- 音频事件 ---
    constexpr EventId kAudioPlay       = MakeEventId("AudioPlay");
    constexpr EventId kAudioStop       = MakeEventId("AudioStop");
    constexpr EventId kAudioFinished   = MakeEventId("AudioFinished");

    // --- 动画事件 ---
    constexpr EventId kAnimationStart  = MakeEventId("AnimationStart");
    constexpr EventId kAnimationEnd    = MakeEventId("AnimationEnd");
    constexpr EventId kAnimationEvent  = MakeEventId("AnimationEvent");

    // --- 引擎生命周期事件 ---
    constexpr EventId kEngineInit      = MakeEventId("EngineInit");
    constexpr EventId kEngineShutdown  = MakeEventId("EngineShutdown");
    constexpr EventId kFrameBegin      = MakeEventId("FrameBegin");
    constexpr EventId kFrameEnd        = MakeEventId("FrameEnd");
    constexpr EventId kModuleLoaded    = MakeEventId("ModuleLoaded");
    constexpr EventId kModuleUnloaded  = MakeEventId("ModuleUnloaded");
} // namespace events

} // namespace core
} // namespace dse

#endif // DSE_CORE_EVENT_ID_H
