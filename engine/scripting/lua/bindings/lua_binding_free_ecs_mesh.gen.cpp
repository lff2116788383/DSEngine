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
#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

namespace dse::runtime::lua_binding {
namespace {

int L_dse_mesh_renderer_set_depth_state(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int depth_test = helper::CheckBool(L, 2) ? 1 : 0;
    int depth_write = helper::CheckBool(L, 3) ? 1 : 0;
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

int L_dse_morph_add_target(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    std::vector<float> deltas;
    if (lua_istable(L, 3)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 3));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 3, _i);
            if (lua_isnumber(L, -1)) deltas.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    dse_morph_add_target(e, name, deltas.data(), static_cast<int>(deltas.size()));
    return 0;
}

int L_dse_morph_simple_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_morph_simple_set_enabled(e, enabled);
    return 0;
}

int L_add_mesh_renderer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    dse_mesh_renderer_add(e, "");
    dse_mesh_renderer_set_color(e, r, g, b, a);
    return 0;
}

int L_dse_mesh_renderer_set_material_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float metallic = static_cast<float>(luaL_checknumber(L, 2));
    float roughness = static_cast<float>(luaL_checknumber(L, 3));
    float ao = static_cast<float>(luaL_checknumber(L, 4));
    float er = static_cast<float>(luaL_checknumber(L, 5));
    float eg = static_cast<float>(luaL_checknumber(L, 6));
    float eb = static_cast<float>(luaL_checknumber(L, 7));
    float normal_strength = static_cast<float>(luaL_checknumber(L, 8));
    int receive_shadow = helper::CheckBool(L, 9) ? 1 : 0;
    int double_sided = helper::CheckBool(L, 10) ? 1 : 0;
    float cr = static_cast<float>(luaL_checknumber(L, 11));
    float cg = static_cast<float>(luaL_checknumber(L, 12));
    float cb = static_cast<float>(luaL_checknumber(L, 13));
    float ca = static_cast<float>(luaL_checknumber(L, 14));
    dse_mesh_renderer_set_material_params(e, metallic, roughness, ao, er, eg, eb, normal_strength, receive_shadow, double_sided, cr, cg, cb, ca);
    return 0;
}

} // namespace

void RegisterEcsRenderingMeshBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"set_mesh_depth_state", L_dse_mesh_renderer_set_depth_state},
        {"set_mesh_material_scalar", L_dse_mesh_renderer_set_material_scalar},
        {"set_mesh_advanced_material", L_dse_mesh_renderer_set_advanced_material},
        {"set_mesh_emissive", L_dse_mesh_renderer_set_emissive_authoring},
        {"add_morph", L_dse_morph_simple_add},
        {"morph_add_target", L_dse_morph_add_target},
        {"set_morph_enabled", L_dse_morph_simple_set_enabled},
        {"add_mesh_renderer", L_add_mesh_renderer},
        {"set_mesh_material", L_dse_mesh_renderer_set_material_params},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
