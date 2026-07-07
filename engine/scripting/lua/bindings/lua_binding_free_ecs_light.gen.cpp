/**
 * @file lua_binding_free_ecs_light.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_rendering_add_skybox(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* cubemap_path = luaL_checkstring(L, 2);
    dse_rendering_add_skybox(e, cubemap_path);
    return 0;
}

int L_dse_rendering_set_gi_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_rendering_set_gi_probe_enabled(e, enabled);
    return 0;
}

int L_dse_rendering_set_light_probe_ex(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float influence_radius = static_cast<float>(luaL_checknumber(L, 2));
    int needs_rebake = static_cast<int>(luaL_checkinteger(L, 3));
    dse_rendering_set_light_probe_ex(e, influence_radius, needs_rebake);
    return 0;
}

int L_dse_rendering_set_light_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_rendering_set_light_probe_enabled(e, enabled);
    return 0;
}

int L_dse_rendering_set_reflection_probe_ex(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float influence_radius = static_cast<float>(luaL_checknumber(L, 2));
    float box_x = static_cast<float>(luaL_checknumber(L, 3));
    float box_y = static_cast<float>(luaL_checknumber(L, 4));
    float box_z = static_cast<float>(luaL_checknumber(L, 5));
    int resolution = static_cast<int>(luaL_checkinteger(L, 6));
    dse_rendering_set_reflection_probe_ex(e, influence_radius, box_x, box_y, box_z, resolution);
    return 0;
}

int L_dse_rendering_set_reflection_probe_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_rendering_set_reflection_probe_enabled(e, enabled);
    return 0;
}

} // namespace

void RegisterEcsRenderingLightBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    helper::RegisterBindings(L, {
        {"add_skybox", L_dse_rendering_add_skybox},
        {"set_gi_probe_enabled", L_dse_rendering_set_gi_probe_enabled},
        {"set_light_probe", L_dse_rendering_set_light_probe_ex},
        {"set_light_probe_enabled", L_dse_rendering_set_light_probe_enabled},
        {"set_reflection_probe", L_dse_rendering_set_reflection_probe_ex},
        {"set_reflection_probe_enabled", L_dse_rendering_set_reflection_probe_enabled},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
