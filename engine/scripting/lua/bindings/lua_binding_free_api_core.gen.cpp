/**
 * @file lua_binding_free_api_core.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/function_defs.json
 *
 * api_core 组自由函数的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_api_version(lua_State* L) {
    int _ret = dse_api_version();
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFn_api_core(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"api_version", L_dse_api_version},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
