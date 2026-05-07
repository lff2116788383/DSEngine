/**
 * @file lua_runtime.h
 * @brief Lua 脚本运行时系统，处理 Lua 虚拟机的生命周期和 C++ 接口的绑定暴露
 */

#ifndef DSE_LUA_RUNTIME_H
#define DSE_LUA_RUNTIME_H

#include <functional>
#include <string>
#include "engine/ecs/world.h"
class AssetManager;

namespace dse::runtime {

/**
 * @struct LuaApiContext
 * @brief 暴露给 Lua 环境的全局上下文状态和系统回调注入
 */
struct LuaApiContext {
    World* world = nullptr;
    std::function<void(const std::string&)> set_window_title;
    std::function<int()> get_draw_calls;
    std::function<int()> get_max_batch_sprites;
    std::function<int()> get_sprite_count;
    AssetManager* asset_manager = nullptr;
};

/**
 * @brief 配置 Lua 运行时所需的上下文环境
 * @param context 填充了具体系统指针的上下文结构体
 */
void ConfigureLuaApiContext(const LuaApiContext& context);

/**
 * @brief 设置引擎启动时加载的入口 Lua 脚本路径
 * @param script_path 脚本路径（如 "script/main.lua"）
 */
void SetStartupLuaScriptPath(const std::string& script_path);

/**
 * @brief 初始化 Lua 虚拟机并加载所有绑定的 C++ API
 * @return 启动成功返回 true，否则返回 false
 */
bool BootstrapLuaRuntime();

/**
 * @brief 每帧触发 Lua 层的全局 Update 回调
 * @param delta_time 距离上一帧的时间间隔
 */
void TickLuaRuntime(float delta_time);

/**
 * @brief 关闭 Lua 虚拟机，释放所有脚本资源
 */
void ShutdownLuaRuntime();

size_t GetLuaMemoryUsage();

/**
 * @brief 在当前 Lua 虚拟机中执行一段字符串代码（用于编辑器 REPL）
 * @param code 要执行的 Lua 代码字符串
 * @param out_result 执行结果或错误信息（可为 nullptr）
 * @return 执行成功返回 true，语法/运行错误返回 false
 */
bool ExecuteLuaString(const char* code, std::string* out_result = nullptr);

}

#endif
