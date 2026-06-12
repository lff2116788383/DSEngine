/**
 * @file event_id_cross_dll_test.cpp
 * @brief EventId 跨编译单元一致性验证
 *
 * 核心验证点：
 * - FNV-1a 哈希在相同字符串输入下产生相同结果（编译期和运行期）
 * - 不同字符串产生不同哈希值（无碰撞）
 * - 内置事件常量在多个编译单元中哈希一致
 * - 模拟跨 DLL 场景：不同作用域/匿名命名空间中的相同字符串产生相同 ID
 *
 * 注意：真正的跨 DLL 验证需要在 DLL 边界上测试，当前测试覆盖
 * 编译期哈希一致性和基本防碰撞，作为跨 DLL 安全的基线保障。
 */

#include <gtest/gtest.h>
#include "engine/core/event_id.h"
#include <cstdint>
#include <string>

// 模拟 DLL A 中的事件定义（匿名命名空间，模拟独立编译单元）
namespace {
    constexpr dse::core::EventId kDllA_MyEvent = dse::core::MakeEventId("MyCustomEvent");
    constexpr dse::core::EventId kDllA_WindowResize = dse::core::MakeEventId("WindowResize");
    constexpr dse::core::EventId kDllA_ResourceLoaded = dse::core::MakeEventId("ResourceLoaded");
}

// 模拟 DLL B 中的事件定义（另一个匿名命名空间，模拟另一个 DLL）
namespace dll_b {
    constexpr dse::core::EventId kMyEvent = dse::core::MakeEventId("MyCustomEvent");
    constexpr dse::core::EventId kWindowResize = dse::core::MakeEventId("WindowResize");
    constexpr dse::core::EventId kResourceLoaded = dse::core::MakeEventId("ResourceLoaded");
}

// ============================================================
// FNV-1a 哈希一致性测试
// ============================================================

// 测试 事件ID交叉DLL：情形1
TEST(EventIdCrossDllTest, TestCase1) {
    // 不同作用域中相同字符串应产生相同 ID
    EXPECT_EQ(kDllA_MyEvent, dll_b::kMyEvent);
    EXPECT_EQ(kDllA_WindowResize, dll_b::kWindowResize);
    EXPECT_EQ(kDllA_ResourceLoaded, dll_b::kResourceLoaded);
}

// 测试 事件ID交叉DLL：Insideevent且一致
TEST(EventIdCrossDllTest, InsideeventAndconsistent) {
    // 集中定义的常量应与直接计算一致
    EXPECT_EQ(dse::core::events::kUiClick, dse::core::MakeEventId("UiClick"));
    EXPECT_EQ(dse::core::events::kResourceLoaded, dse::core::MakeEventId("ResourceLoaded"));
    EXPECT_EQ(dse::core::events::kSceneLifecycle, dse::core::MakeEventId("SceneLifecycle"));
}

// 测试 事件ID交叉DLL：不同Strings产生不同Hashes
TEST(EventIdCrossDllTest, DifferentStringsProduceDifferentHashes) {
    // 不同的事件名应产生不同的 ID
    EXPECT_NE(dse::core::events::kUiClick, dse::core::events::kResourceLoaded);
    EXPECT_NE(dse::core::events::kUiClick, dse::core::events::kSceneLifecycle);
    EXPECT_NE(dse::core::events::kResourceLoaded, dse::core::events::kSceneLifecycle);

    // 自定义事件之间
    EXPECT_NE(kDllA_MyEvent, kDllA_WindowResize);
    EXPECT_NE(kDllA_MyEvent, kDllA_ResourceLoaded);
}

// 测试 事件ID交叉DLL：且一致
TEST(EventIdCrossDllTest, Andconsistent) {
    // constexpr 编译期求值
    constexpr dse::core::EventId compile_time = dse::core::MakeEventId("RuntimeTestEvent");
    // 运行期通过函数调用
    std::string event_name = "RuntimeTestEvent";
    dse::core::EventId runtime_time = dse::core::MakeEventId(event_name.c_str());
    EXPECT_EQ(compile_time, runtime_time);
}

// 测试 事件ID交叉DLL：空不零
TEST(EventIdCrossDllTest, EmptyNotZero) {
    // FNV-1a 对空字符串返回偏移基础值，不是 0
    constexpr dse::core::EventId empty_hash = dse::core::MakeEventId("");
    EXPECT_NE(empty_hash, 0ull);
}

// 测试 事件ID交叉DLL：尺寸
TEST(EventIdCrossDllTest, Size) {
    constexpr dse::core::EventId lower = dse::core::MakeEventId("myevent");
    constexpr dse::core::EventId upper = dse::core::MakeEventId("MyEvent");
    constexpr dse::core::EventId mixed = dse::core::MakeEventId("MYEVENT");
    EXPECT_NE(lower, upper);
    EXPECT_NE(lower, mixed);
    EXPECT_NE(upper, mixed);
}

// 测试 事件ID交叉DLL：FNV 1偏移基值为正确
TEST(EventIdCrossDllTest, FNV1aOffsetBaseValueIsCorrect) {
    // 验证 FNV-1a 偏移基础值
    // 对空字符串，哈希应等于初始偏移值 0xcbf29ce484222325
    constexpr dse::core::EventId empty_hash = dse::core::MakeEventId("");
    EXPECT_EQ(empty_hash, 0xcbf29ce484222325ull);
}

// 测试 事件ID交叉DLL：单一能够
TEST(EventIdCrossDllTest, SingleCan) {
    // 验证单字符 FNV-1a 计算过程可复现
    // 'a' 的 FNV-1a: offset_basis XOR 'a' (0x61) * FNV_prime
    constexpr dse::core::EventId hash_a = dse::core::MakeEventId("a");
    dse::core::EventId expected = 0xcbf29ce484222325ull;
    expected ^= static_cast<std::uint64_t>('a');
    expected *= 0x100000001b3ull;
    EXPECT_EQ(hash_a, expected);
}

// 测试 事件ID交叉DLL：事件无
TEST(EventIdCrossDllTest, EventWithout) {
    // 验证 events 命名空间中所有集中定义的事件 ID 之间无哈希碰撞
    constexpr dse::core::EventId ids[] = {
        // UI
        dse::core::events::kUiClick,
        dse::core::events::kUiHover,
        dse::core::events::kUiFocus,
        dse::core::events::kUiBlur,
        dse::core::events::kUiDrag,
        dse::core::events::kUiSubmit,
        // 资源
        dse::core::events::kResourceLoaded,
        dse::core::events::kResourceUnloaded,
        dse::core::events::kResourceLoadFailed,
        dse::core::events::kTextureReady,
        dse::core::events::kShaderCompiled,
        // 场景/实体
        dse::core::events::kSceneLifecycle,
        dse::core::events::kEntityCreated,
        dse::core::events::kEntityDestroyed,
        dse::core::events::kParentChanged,
        dse::core::events::kTransformChanged,
        // 窗口/输入
        dse::core::events::kWindowResize,
        dse::core::events::kWindowClose,
        dse::core::events::kWindowFocus,
        dse::core::events::kKeyDown,
        dse::core::events::kKeyUp,
        dse::core::events::kMouseDown,
        dse::core::events::kMouseUp,
        dse::core::events::kMouseMove,
        dse::core::events::kMouseScroll,
        // 物理/碰撞
        dse::core::events::kCollisionBegin,
        dse::core::events::kCollisionEnd,
        dse::core::events::kTriggerEnter,
        dse::core::events::kTriggerExit,
        // 音频
        dse::core::events::kAudioPlay,
        dse::core::events::kAudioStop,
        dse::core::events::kAudioFinished,
        // 动画
        dse::core::events::kAnimationStart,
        dse::core::events::kAnimationEnd,
        dse::core::events::kAnimationEvent,
        // 引擎生命周期
        dse::core::events::kEngineInit,
        dse::core::events::kEngineShutdown,
        dse::core::events::kFrameBegin,
        dse::core::events::kFrameEnd,
        dse::core::events::kModuleLoaded,
        dse::core::events::kModuleUnloaded,
    };

    constexpr int kCount = sizeof(ids) / sizeof(ids[0]);
    for (int i = 0; i < kCount; ++i) {
        for (int j = i + 1; j < kCount; ++j) {
            EXPECT_NE(ids[i], ids[j]) << "哈希碰撞: 事件索引 " << i << " 和 " << j;
        }
    }
}

// 测试 事件ID交叉DLL：设置于且一致
TEST(EventIdCrossDllTest, SetInAndconsistent) {
    // 验证 events 命名空间中的每个常量都等价于 MakeEventId 直接计算
    EXPECT_EQ(dse::core::events::kUiClick,         dse::core::MakeEventId("UiClick"));
    EXPECT_EQ(dse::core::events::kUiHover,         dse::core::MakeEventId("UiHover"));
    EXPECT_EQ(dse::core::events::kWindowResize,    dse::core::MakeEventId("WindowResize"));
    EXPECT_EQ(dse::core::events::kEntityCreated,   dse::core::MakeEventId("EntityCreated"));
    EXPECT_EQ(dse::core::events::kCollisionBegin,  dse::core::MakeEventId("CollisionBegin"));
    EXPECT_EQ(dse::core::events::kAudioPlay,       dse::core::MakeEventId("AudioPlay"));
    EXPECT_EQ(dse::core::events::kAnimationStart,  dse::core::MakeEventId("AnimationStart"));
    EXPECT_EQ(dse::core::events::kEngineInit,      dse::core::MakeEventId("EngineInit"));
    EXPECT_EQ(dse::core::events::kFrameBegin,      dse::core::MakeEventId("FrameBegin"));
    EXPECT_EQ(dse::core::events::kModuleLoaded,    dse::core::MakeEventId("ModuleLoaded"));
}
