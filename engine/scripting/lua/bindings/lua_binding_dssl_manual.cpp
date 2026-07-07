/**
 * @file lua_binding_dssl.cpp
 * @brief Lua 绑定：DSSL 材质系统 (dssl)
 *
 * 薄包装：仅做 Lua 参数读取与结果入栈，所有逻辑委托 C ABI（dse_api_extended.cpp）。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {

static int L_DsslLoadMaterial(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t id = dse_dssl_load_material(path);
    if (id == 0) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

static int L_DsslCreateInstance(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t id = dse_dssl_create_instance(path);
    if (id == 0) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

static int L_DsslSetFloat(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    dse_dssl_set_float(id, name, value);
    return 0;
}

static int L_DsslSetColor(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float r = static_cast<float>(luaL_checknumber(L, 3));
    float g = static_cast<float>(luaL_checknumber(L, 4));
    float b = static_cast<float>(luaL_checknumber(L, 5));
    float a = lua_gettop(L) >= 6 ? static_cast<float>(luaL_checknumber(L, 6)) : 1.0f;
    dse_dssl_set_color(id, name, r, g, b, a);
    return 0;
}

static int L_DsslSetVec3(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    dse_dssl_set_vec3(id, name, x, y, z);
    return 0;
}

static int L_DsslSetTexture(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    const char* tex_path = luaL_checkstring(L, 3);
    dse_dssl_set_texture(id, name, tex_path);
    return 0;
}

static int L_DsslSetTextureHandle(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    uint32_t handle = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    dse_dssl_set_texture_handle(id, name, handle);
    return 0;
}

static int L_DsslApplyMaterial(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(static_cast<entt::id_type>(helper::CheckEntity(L, 1)));
    uint32_t mat_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_dssl_apply_material(e, mat_id);
    return 0;
}

static int L_DsslGetFloat(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    lua_pushnumber(L, dse_dssl_get_float(id, name));
    return 1;
}

static int L_DsslGetColor(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float rgba[4] = {0, 0, 0, 0};
    dse_dssl_get_color(id, name, rgba);
    lua_pushnumber(L, rgba[0]);
    lua_pushnumber(L, rgba[1]);
    lua_pushnumber(L, rgba[2]);
    lua_pushnumber(L, rgba[3]);
    return 4;
}

void RegisterDSSLBindings(lua_State* L) {
    lua_newtable(L);

    static const luaL_Reg funcs[] = {
        {"load_material",      L_DsslLoadMaterial},
        {"create_instance",    L_DsslCreateInstance},
        {"set_float",          L_DsslSetFloat},
        {"set_color",          L_DsslSetColor},
        {"set_vec3",           L_DsslSetVec3},
        {"set_texture",        L_DsslSetTexture},
        {"set_texture_handle", L_DsslSetTextureHandle},
        {"apply_material",     L_DsslApplyMaterial},
        {"get_float",          L_DsslGetFloat},
        {"get_color",          L_DsslGetColor},
        {nullptr, nullptr}
    };

    luaL_setfuncs(L, funcs, 0);
    lua_setglobal(L, "dssl");
}

} // namespace dse::runtime::lua_binding
