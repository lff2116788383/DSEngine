/**
 * @file lua_binding_free_ecs_mesh.gen.cpp
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

int L_dse_mesh_renderer_set_depth_state(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int depth_test = static_cast<int>(luaL_checkinteger(L, 2));
    int depth_write = static_cast<int>(luaL_checkinteger(L, 3));
    dse_mesh_renderer_set_depth_state(e, depth_test, depth_write);
    return 0;
}

int L_dse_mesh_renderer_set_material_scalar(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    dse_mesh_renderer_set_material_scalar(e, name, value);
    return 0;
}

int L_dse_mesh_renderer_set_advanced_material(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float clear_coat = static_cast<float>(luaL_checknumber(L, 2));
    float clear_coat_roughness = static_cast<float>(luaL_checknumber(L, 3));
    float anisotropy = static_cast<float>(luaL_checknumber(L, 4));
    float pom_height_scale = static_cast<float>(luaL_checknumber(L, 5));
    float sss_strength = static_cast<float>(luaL_checknumber(L, 6));
    float sss_r = static_cast<float>(luaL_checknumber(L, 7));
    float sss_g = static_cast<float>(luaL_checknumber(L, 8));
    float sss_b = static_cast<float>(luaL_checknumber(L, 9));
    dse_mesh_renderer_set_advanced_material(e, clear_coat, clear_coat_roughness, anisotropy, pom_height_scale, sss_strength, sss_r, sss_g, sss_b);
    return 0;
}

int L_dse_mesh_renderer_set_emissive_authoring(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    dse_mesh_renderer_set_emissive_authoring(e, r, g, b);
    return 0;
}

int L_dse_morph_simple_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_morph_simple_add(e);
    return 0;
}

int L_dse_morph_simple_add_target(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float weight = static_cast<float>(luaL_checknumber(L, 3));
    dse_morph_simple_add_target(e, name, weight);
    return 0;
}

int L_dse_morph_simple_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_morph_simple_set_enabled(e, enabled);
    return 0;
}

} // namespace

void RegisterEcsRenderingMeshBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    helper::RegisterBindings(L, {
        {"set_mesh_depth_state", L_dse_mesh_renderer_set_depth_state},
        {"set_mesh_material_scalar", L_dse_mesh_renderer_set_material_scalar},
        {"set_mesh_advanced_material", L_dse_mesh_renderer_set_advanced_material},
        {"set_mesh_emissive", L_dse_mesh_renderer_set_emissive_authoring},
        {"add_morph", L_dse_morph_simple_add},
        {"morph_add_target", L_dse_morph_simple_add_target},
        {"set_morph_enabled", L_dse_morph_simple_set_enabled},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
