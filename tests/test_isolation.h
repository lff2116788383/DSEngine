/**
 * @file test_isolation.h
 * @brief 测试隔离工具集，提供全局状态重置的统一入口
 *
 * 使用方式：
 * 1. 在测试 Fixture 的 TearDown() 中调用 TestIsolation::ResetGlobalState()
 * 2. 或在需要精确控制重置范围时，单独调用各子函数
 *
 * 设计原则：
 * - ServiceLocator::Instance() 是 Meyers' singleton，进程内不可销毁重建，
 *   但 ResetAll() 可清空所有已注册的服务
 * - Input/Screen/Time 是纯静态全局类，必须显式调用 Reset()
 * - Debug 有 Init/ShutDown 对，通常不需要在测试间重置
 */

#ifndef DSE_TEST_ISOLATION_H
#define DSE_TEST_ISOLATION_H

#include "engine/core/service_locator.h"
#include "engine/input/input.h"
#include "engine/platform/screen.h"
#include "engine/base/time.h"

namespace dse::testing {

/// 全局状态重置工具
class TestIsolation {
public:
    /**
     * @brief 重置 ServiceLocator 中的所有已注册服务
     *
     * 等价于 ServiceLocator::Instance().ResetAll()，
     * 清除 EventBus/JobSystem/World/FramePipeline 等托管服务。
     */
    static void ResetServiceLocator() {
        dse::core::ServiceLocator::Instance().ResetAll();
    }

    /**
     * @brief 重置 Input 的所有静态状态
     *
     * 清除按键映射、鼠标位置、滚轮值、设备摇晃标记等。
     */
    static void ResetInput() {
        Input::Reset();
    }

    /**
     * @brief 重置 Screen 的所有静态状态
     *
     * 将宽高和宽高比恢复为 0。
     */
    static void ResetScreen() {
        Screen::Reset();
    }

    /**
     * @brief 重置 Time 的所有静态状态
     *
     * 将 delta_time、last_frame_time 恢复为 0，
     * fixed_update_time 恢复为 1/60，startup_time 重置为当前时刻。
     */
    static void ResetTime() {
        Time::Reset();
    }

    /**
     * @brief 重置所有全局可重置状态
     *
     * 这是最完整的重置，应作为测试安全网在 TearDown 中调用。
     * 执行顺序：先 Reset ServiceLocator（可能依赖 Input/Screen/Time），
     * 再 Reset 各静态类。
     */
    static void ResetGlobalState() {
        ResetServiceLocator();
        ResetInput();
        ResetScreen();
        ResetTime();
    }
};

} // namespace dse::testing

#endif // DSE_TEST_ISOLATION_H
