/**
 * @file script.h
 * @brief 脚本挂载组件（Lua/C++ 双轨）
 */

#ifndef DSE_ECS_COMPONENTS_2D_SCRIPT_H
#define DSE_ECS_COMPONENTS_2D_SCRIPT_H

#include <string>

/**
 * @struct ScriptComponent
 * @brief Lua 脚本挂载组件，指向实体绑定的 Lua 业务逻辑
 */
struct ScriptComponent {
    std::string script_path;  ///< Lua 脚本的资源路径
    bool enabled = true;      ///< 是否执行脚本的生命周期函数
};

/**
 * @struct LuaScriptComponent
 * @brief Sol2 绑定的 Lua 脚本实例组件，持有运行时脚本环境
 */
struct LuaScriptComponent {
    std::string script_path;
    bool is_initialized = false;
    
    // Sol2 table instance representing the script environment for this entity
    // We use a void pointer or forward declaration here if sol::table is not included
    // to avoid polluting the ECS header with Lua/Sol2 dependencies.
    void* script_instance = nullptr; 
};

#endif // DSE_ECS_COMPONENTS_2D_SCRIPT_H
