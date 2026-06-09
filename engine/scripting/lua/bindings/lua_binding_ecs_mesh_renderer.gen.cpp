/**
 * @file lua_binding_ecs_mesh_renderer.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * MeshRendererComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_mesh_renderer_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0, w = 0;
    dse_mesh_renderer_get_color(e, &x, &y, &z, &w);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z); lua_pushnumber(L, w);
    return 4;
}
int L_Set_mesh_renderer_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_mesh_renderer_set_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}
int L_Get_mesh_renderer_visible(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_mesh_renderer_get_visible(e));
    return 1;
}
int L_Set_mesh_renderer_visible(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_mesh_renderer_set_visible(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_mesh_renderer_metallic(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_mesh_renderer_get_metallic(e));
    return 1;
}
int L_Set_mesh_renderer_metallic(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_mesh_renderer_set_metallic(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_mesh_renderer_roughness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_mesh_renderer_get_roughness(e));
    return 1;
}
int L_Set_mesh_renderer_roughness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_mesh_renderer_set_roughness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_mesh_renderer_emissive(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_mesh_renderer_get_emissive(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_mesh_renderer_emissive(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_mesh_renderer_set_emissive(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_mesh_renderer_receive_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_mesh_renderer_get_receive_shadow(e));
    return 1;
}
int L_Set_mesh_renderer_receive_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_mesh_renderer_set_receive_shadow(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_mesh_renderer_mesh_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_mesh_renderer_get_mesh_path(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_mesh_renderer_mesh_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_mesh_renderer_set_mesh_path(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_mesh_renderer_shader_variant(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[128];
    if (dse_mesh_renderer_get_shader_variant(e, buf, 128) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_mesh_renderer_shader_variant(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_mesh_renderer_set_shader_variant(e, luaL_checkstring(L, 2));
    return 0;
}

} // namespace

void RegisterMeshRendererComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_mesh_color", L_Get_mesh_renderer_color},
        {"set_mesh_color", L_Set_mesh_renderer_color},
        {"get_mesh_visible", L_Get_mesh_renderer_visible},
        {"set_mesh_visible", L_Set_mesh_renderer_visible},
        {"get_mesh_metallic", L_Get_mesh_renderer_metallic},
        {"set_mesh_metallic", L_Set_mesh_renderer_metallic},
        {"get_mesh_roughness", L_Get_mesh_renderer_roughness},
        {"set_mesh_roughness", L_Set_mesh_renderer_roughness},
        {"get_mesh_emissive", L_Get_mesh_renderer_emissive},
        {"set_mesh_emissive", L_Set_mesh_renderer_emissive},
        {"get_mesh_receive_shadow", L_Get_mesh_renderer_receive_shadow},
        {"set_mesh_receive_shadow", L_Set_mesh_renderer_receive_shadow},
        {"get_mesh_path", L_Get_mesh_renderer_mesh_path},
        {"set_mesh_path", L_Set_mesh_renderer_mesh_path},
        {"get_mesh_shader_variant", L_Get_mesh_renderer_shader_variant},
        {"set_mesh_shader_variant", L_Set_mesh_renderer_shader_variant},
    });
}

} // namespace dse::runtime::lua_binding
