/**
 * @file lua_binding_free_ecs_core.gen.cpp
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

int L_dse_entity_create(lua_State* L) {
    uint32_t _ret = dse_entity_create();
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_scene_load(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_scene_load(path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_entity_destroy(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_entity_destroy(e);
    return 0;
}

int L_dse_ecs_add_transform(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float sx = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float sy = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    float sz = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    dse_ecs_add_transform(e, x, y, z, sx, sy, sz);
    return 0;
}

int L_dse_ecs_add_parent(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t parent = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_ecs_add_parent(e, parent);
    return 0;
}

int L_dse_ecs_set_parent(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t parent = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_ecs_set_parent(e, parent);
    return 0;
}

int L_dse_ecs_get_parent(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t _ret = dse_ecs_get_parent(e);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_ecs_clear_parent(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ecs_clear_parent(e);
    return 0;
}

int L_dse_ecs_add_script(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    dse_ecs_add_script(e, path);
    return 0;
}

int L_dse_ecs_set_script_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    dse_ecs_set_script_path(e, path);
    return 0;
}

int L_dse_ecs_set_script_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ecs_set_script_enabled(e, enabled);
    return 0;
}

int L_dse_ecs_get_script_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ecs_get_script_enabled(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_scene_load_sub_async(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_scene_load_sub_async(path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_scene_unload_sub(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    dse_scene_unload_sub(path);
    return 0;
}

int L_dse_scene_unload_all_subs(lua_State* L) {
    dse_scene_unload_all_subs();
    return 0;
}

int L_dse_scene_is_sub_loaded(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_scene_is_sub_loaded(path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_scene_get_sub_count(lua_State* L) {
    int _ret = dse_scene_get_sub_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_scene_get_pending_count(lua_State* L) {
    int _ret = dse_scene_get_pending_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_scene_transition_to(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int mode = static_cast<int>(luaL_checkinteger(L, 2));
    float fade_duration = static_cast<float>(luaL_checknumber(L, 3));
    dse_scene_transition_to(path, mode, fade_duration);
    return 0;
}

int L_dse_scene_get_transition_state(lua_State* L) {
    int _ret = dse_scene_get_transition_state();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_scene_get_fade_progress(lua_State* L) {
    float _ret = dse_scene_get_fade_progress();
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_uuid_resolve(lua_State* L) {
    const char* uuid_str = luaL_checkstring(L, 1);
    uint32_t _ret = dse_uuid_resolve(uuid_str);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_ecs_count_entities_with(lua_State* L) {
    const char* component = luaL_checkstring(L, 1);
    int _ret = dse_ecs_count_entities_with(component);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ecs_has_component(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* component = luaL_checkstring(L, 2);
    int _ret = dse_ecs_has_component(e, component);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_scene_save(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_scene_save(path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_scene_save_prefab(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    int _ret = dse_scene_save_prefab(e, path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ecs_set_time_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float scale = static_cast<float>(luaL_checknumber(L, 2));
    dse_ecs_set_time_scale(e, scale);
    return 0;
}

int L_dse_ecs_get_time_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_ecs_get_time_scale(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_ecs_find_entities_by_mesh_path(lua_State* L) {
    const char* mesh_path = luaL_checkstring(L, 1);
    uint32_t _buf[512];
    int _count = dse_ecs_find_entities_by_mesh_path(mesh_path, _buf, 512);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_pushinteger(L, static_cast<lua_Integer>(_buf[_i]));
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

int L_dse_ecs_find_entities_with(lua_State* L) {
    const char* component = luaL_checkstring(L, 1);
    uint32_t _buf[1024];
    int _count = dse_ecs_find_entities_with(component, _buf, 1024);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_pushinteger(L, static_cast<lua_Integer>(_buf[_i]));
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

} // namespace

void RegisterEcsCoreBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"create_entity", L_dse_entity_create},
        {"load_scene", L_dse_scene_load},
        {"destroy_entity", L_dse_entity_destroy},
        {"add_transform", L_dse_ecs_add_transform},
        {"add_parent", L_dse_ecs_add_parent},
        {"set_parent", L_dse_ecs_set_parent},
        {"get_parent", L_dse_ecs_get_parent},
        {"clear_parent", L_dse_ecs_clear_parent},
        {"add_script", L_dse_ecs_add_script},
        {"set_script_path", L_dse_ecs_set_script_path},
        {"set_script_enabled", L_dse_ecs_set_script_enabled},
        {"get_script_enabled", L_dse_ecs_get_script_enabled},
        {"load_sub_scene_async", L_dse_scene_load_sub_async},
        {"unload_sub_scene", L_dse_scene_unload_sub},
        {"unload_all_sub_scenes", L_dse_scene_unload_all_subs},
        {"is_sub_scene_loaded", L_dse_scene_is_sub_loaded},
        {"get_sub_scene_count", L_dse_scene_get_sub_count},
        {"get_pending_scene_count", L_dse_scene_get_pending_count},
        {"transition_to", L_dse_scene_transition_to},
        {"get_transition_state", L_dse_scene_get_transition_state},
        {"get_fade_progress", L_dse_scene_get_fade_progress},
        {"resolve_uuid", L_dse_uuid_resolve},
        {"count_entities_with", L_dse_ecs_count_entities_with},
        {"has_component", L_dse_ecs_has_component},
        {"save_scene", L_dse_scene_save},
        {"save_prefab", L_dse_scene_save_prefab},
        {"set_time_scale", L_dse_ecs_set_time_scale},
        {"get_time_scale", L_dse_ecs_get_time_scale},
        {"find_entities_by_mesh_path", L_dse_ecs_find_entities_by_mesh_path},
        {"find_entities_with", L_dse_ecs_find_entities_with},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
