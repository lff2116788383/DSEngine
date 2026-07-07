/**
 * @file lua_binding_free_functions.gen.h
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/function_defs.json
 *
 * 声明所有自由函数 Lua 绑定的注册入口。
 */

#pragma once

struct lua_State;

namespace dse::runtime::lua_binding {

void RegisterFreeFn_api_core(lua_State* L);
void RegisterFreeFn_app(lua_State* L);
void RegisterFreeFn_audio(lua_State* L);
void RegisterFreeFn_ecs_gap(lua_State* L);
void RegisterFreeFn_ui(lua_State* L);
void RegisterFreeFn_localization(lua_State* L);

/// 注册所有自由函数绑定（在 lua_binding_modules.cpp 中调用）
inline void RegisterAllFreeFunctionBindings(lua_State* L) {
    RegisterFreeFn_api_core(L);
    RegisterFreeFn_app(L);
    RegisterFreeFn_audio(L);
    RegisterFreeFn_ecs_gap(L);
    RegisterFreeFn_ui(L);
    RegisterFreeFn_localization(L);
}

} // namespace dse::runtime::lua_binding
