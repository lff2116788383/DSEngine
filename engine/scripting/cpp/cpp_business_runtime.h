/**
 * @file cpp_business_runtime.h
 * @brief C++ 原生业务逻辑运行时绑定，提供与 Lua 脚本类似的生命周期钩子
 */

#ifndef DSE_CPP_BUSINESS_RUNTIME_H
#define DSE_CPP_BUSINESS_RUNTIME_H

#include <functional>
#include "engine/core/dse_export.h"
#include "engine/ecs/world.h"

class AssetManager;

namespace dse::runtime {

/**
 * @struct CppBusinessHooks
 * @brief C++ 业务层注入的回调函数集
 */
struct CppBusinessHooks {
    std::function<void(World&, AssetManager&)> bootstrap; ///< 引擎初始化完毕时调用
    std::function<void(World&, float)> tick;              ///< 引擎每帧更新时调用
    std::function<void()> shutdown;                       ///< 引擎关停前调用
};

/**
 * @brief 配置 C++ 业务层的回调钩子
 * @param hooks 包含启动、更新和关闭回调的结构体
 */
DSE_EXPORT void ConfigureCppBusinessHooks(CppBusinessHooks hooks);

/**
 * @brief 触发 C++ 业务层的初始化逻辑
 * @param world 实体世界引用
 * @param asset_manager 资源管理器引用
 * @return 成功返回 true
 */
bool BootstrapCppBusiness(World& world, AssetManager& asset_manager);

/**
 * @brief 触发 C++ 业务层的每帧更新逻辑
 * @param world 实体世界引用
 * @param delta_time 距离上一帧的增量时间
 */
void TickCppBusiness(World& world, float delta_time);

/**
 * @brief 触发 C++ 业务层的清理逻辑
 */
void ShutdownCppBusiness();

}

#endif
