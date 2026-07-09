/**
* @file jolt_job_bridge.cpp
* @brief Jolt→引擎 JobSystem 桥接函数实现
*
* 此文件独立编译，不包含任何 Jolt 头文件，避免 Jolt 宏与
* MSVC 标准库 <queue>（被 job_system.h 引入）冲突。
* 提供两个桥接函数：
* - BridgeExecute: 将 Jolt job 提交到引擎 JobSystem
* - GetEngineJobSystem: 从 ServiceLocator 获取 JobSystem 指针
*/

#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include <functional>

namespace dse {
namespace physics3d {

/// 桥接函数：将 Jolt job 提交到引擎 JobSystem
void BridgeExecute(void* job_sys_ptr, const std::function<void()>& func) {
    auto* js = static_cast<dse::core::JobSystem*>(job_sys_ptr);
    js->Execute(func);
}

/// 从 ServiceLocator 获取引擎 JobSystem 指针（void* 形式）
/// 返回 nullptr 表示 JobSystem 未注册
void* GetEngineJobSystem() {
    return dse::core::ServiceLocator::Instance().Get<dse::core::JobSystem>();
}

} // namespace physics3d
} // namespace dse
