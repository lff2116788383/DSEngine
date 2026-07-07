/**
 * @file lua_binding_free_audio.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/function_defs.json
 *
 * audio 组自由函数的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_audio_fade_out_all_sfx(lua_State* L) {
    float duration = static_cast<float>(luaL_checknumber(L, 1));
    dse_audio_fade_out_all_sfx(duration);
    return 0;
}

int L_dse_audio_source_is_playing(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_audio_source_is_playing(entity);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFn_audio(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "audio");
    helper::RegisterBindings(L, {
        {"fade_out_all_sfx", L_dse_audio_fade_out_all_sfx},
        {"source_is_playing", L_dse_audio_source_is_playing},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
