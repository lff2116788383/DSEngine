/**
 * @file lua_binding_ecs_tree_ext.cpp
 * @brief TreeComponent 字符串路径 Lua 绑定（委托 dse_api）
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

constexpr int kPathBufferSize = 512;

int PushTreePath(lua_State* L, uint32_t e,
                 int (*getter)(uint32_t, char*, int)) {
    char buf[kPathBufferSize];
    if (getter(e, buf, kPathBufferSize) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}

int L_Get_tree_mesh_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    return PushTreePath(L, e, dse_tree_get_mesh_path);
}

int L_Set_tree_mesh_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    dse_tree_set_mesh_path(e, path);
    return 0;
}

int L_Get_tree_lod1_mesh_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    return PushTreePath(L, e, dse_tree_get_lod1_mesh_path);
}

int L_Set_tree_lod1_mesh_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    dse_tree_set_lod1_mesh_path(e, path);
    return 0;
}

int L_Get_tree_billboard_texture_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    return PushTreePath(L, e, dse_tree_get_billboard_texture_path);
}

int L_Set_tree_billboard_texture_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    dse_tree_set_billboard_texture_path(e, path);
    return 0;
}

} // namespace

void RegisterTreeComponentExtBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_tree_mesh_path", L_Get_tree_mesh_path},
        {"set_tree_mesh_path", L_Set_tree_mesh_path},
        {"get_tree_lod1_mesh_path", L_Get_tree_lod1_mesh_path},
        {"set_tree_lod1_mesh_path", L_Set_tree_lod1_mesh_path},
        {"get_tree_billboard_texture_path", L_Get_tree_billboard_texture_path},
        {"set_tree_billboard_texture_path", L_Set_tree_billboard_texture_path},
    });
}

} // namespace dse::runtime::lua_binding
