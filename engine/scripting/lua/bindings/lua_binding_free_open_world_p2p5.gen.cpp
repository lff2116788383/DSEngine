/**
 * @file lua_binding_free_open_world_p2p5.gen.cpp
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

int L_dse_mesh_streaming_init(lua_State* L) {
    float hysteresis = static_cast<float>(luaL_checknumber(L, 1));
    int load_budget_per_frame = static_cast<int>(luaL_checkinteger(L, 2));
    dse_mesh_streaming_init(hysteresis, load_budget_per_frame);
    return 0;
}

int L_dse_mesh_streaming_register_mesh(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float radius = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    uint32_t _ret = dse_mesh_streaming_register_mesh(name, x, y, z, radius);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_mesh_streaming_add_lod(lua_State* L) {
    uint32_t mesh_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t level = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    const char* path = luaL_checkstring(L, 3);
    float distance = static_cast<float>(luaL_checknumber(L, 4));
    uint32_t triangle_count = static_cast<uint32_t>(luaL_checkinteger(L, 5));
    dse_mesh_streaming_add_lod(mesh_id, level, path, distance, triangle_count);
    return 0;
}

int L_dse_mesh_streaming_tick(lua_State* L) {
    float cam_x = static_cast<float>(luaL_checknumber(L, 1));
    float cam_y = static_cast<float>(luaL_checknumber(L, 2));
    float cam_z = static_cast<float>(luaL_checknumber(L, 3));
    float dt = static_cast<float>(luaL_optnumber(L, 4, 0.016));
    dse_mesh_streaming_tick(cam_x, cam_y, cam_z, dt);
    return 0;
}

int L_dse_mesh_streaming_get_current_lod(lua_State* L) {
    uint32_t mesh_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_mesh_streaming_get_current_lod(mesh_id);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_mesh_streaming_get_mesh_count(lua_State* L) {
    int _ret = dse_mesh_streaming_get_mesh_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_mesh_streaming_shutdown(lua_State* L) {
    dse_mesh_streaming_shutdown();
    return 0;
}

int L_dse_physics_lod_init(lua_State* L) {
    float full_distance = static_cast<float>(luaL_checknumber(L, 1));
    float reduced_distance = static_cast<float>(luaL_checknumber(L, 2));
    float simplified_distance = static_cast<float>(luaL_checknumber(L, 3));
    dse_physics_lod_init(full_distance, reduced_distance, simplified_distance);
    return 0;
}

int L_dse_physics_lod_register_body(lua_State* L) {
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float radius = static_cast<float>(luaL_checknumber(L, 5));
    dse_physics_lod_register_body(entity_id, x, y, z, radius);
    return 0;
}

int L_dse_physics_lod_wake(lua_State* L) {
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_physics_lod_wake(entity_id);
    return 0;
}

int L_dse_physics_lod_sleep(lua_State* L) {
    uint32_t entity_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_physics_lod_sleep(entity_id);
    return 0;
}

int L_dse_physics_lod_shutdown(lua_State* L) {
    dse_physics_lod_shutdown();
    return 0;
}

int L_dse_terrain_deform_init(lua_State* L) {
    float max_depth = static_cast<float>(luaL_checknumber(L, 1));
    float max_height = static_cast<float>(luaL_checknumber(L, 2));
    dse_terrain_deform_init(max_depth, max_height);
    return 0;
}

int L_dse_terrain_deform_apply(lua_State* L) {
    int type = static_cast<int>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float radius = static_cast<float>(luaL_checknumber(L, 5));
    float strength = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    int _ret = dse_terrain_deform_apply(type, x, y, z, radius, strength);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_terrain_deform_undo(lua_State* L) {
    int _ret = dse_terrain_deform_undo();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_terrain_deform_redo(lua_State* L) {
    int _ret = dse_terrain_deform_redo();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_terrain_deform_sample_height(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    float _ret = dse_terrain_deform_sample_height(x, z);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_terrain_deform_shutdown(lua_State* L) {
    dse_terrain_deform_shutdown();
    return 0;
}

int L_dse_audio_lod_init(lua_State* L) {
    float full_distance = static_cast<float>(luaL_checknumber(L, 1));
    int max_active_sources = static_cast<int>(luaL_checkinteger(L, 2));
    dse_audio_lod_init(full_distance, max_active_sources);
    return 0;
}

int L_dse_audio_lod_register_source(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float max_distance = static_cast<float>(luaL_optnumber(L, 5, 100.0));
    float priority = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    uint32_t _ret = dse_audio_lod_register_source(path, x, y, z, max_distance, priority);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_audio_lod_tick(lua_State* L) {
    float lx = static_cast<float>(luaL_checknumber(L, 1));
    float ly = static_cast<float>(luaL_checknumber(L, 2));
    float lz = static_cast<float>(luaL_checknumber(L, 3));
    float dt = static_cast<float>(luaL_optnumber(L, 4, 0.016));
    dse_audio_lod_tick(lx, ly, lz, dt);
    return 0;
}

int L_dse_audio_lod_is_audible(lua_State* L) {
    uint32_t source_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_audio_lod_is_audible(source_id);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_lod_shutdown(lua_State* L) {
    dse_audio_lod_shutdown();
    return 0;
}

int L_dse_physics_lod_evaluate(lua_State* L) {
    float cam_x = static_cast<float>(luaL_checknumber(L, 1));
    float cam_y = static_cast<float>(luaL_checknumber(L, 2));
    float cam_z = static_cast<float>(luaL_checknumber(L, 3));
    uint32_t frame = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    uint32_t _buf[512];
    int _count = dse_physics_lod_evaluate(cam_x, cam_y, cam_z, frame, _buf, 512);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_pushinteger(L, static_cast<lua_Integer>(_buf[_i]));
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

} // namespace

void RegisterOpenWorldP2P5Bindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"init", L_dse_mesh_streaming_init},
        {"register_mesh", L_dse_mesh_streaming_register_mesh},
        {"add_lod", L_dse_mesh_streaming_add_lod},
        {"tick", L_dse_mesh_streaming_tick},
        {"get_current_lod", L_dse_mesh_streaming_get_current_lod},
        {"get_mesh_count", L_dse_mesh_streaming_get_mesh_count},
        {"shutdown", L_dse_mesh_streaming_shutdown},
        {"init", L_dse_physics_lod_init},
        {"register_body", L_dse_physics_lod_register_body},
        {"wake", L_dse_physics_lod_wake},
        {"sleep", L_dse_physics_lod_sleep},
        {"shutdown", L_dse_physics_lod_shutdown},
        {"init", L_dse_terrain_deform_init},
        {"deform", L_dse_terrain_deform_apply},
        {"undo", L_dse_terrain_deform_undo},
        {"redo", L_dse_terrain_deform_redo},
        {"sample_height", L_dse_terrain_deform_sample_height},
        {"shutdown", L_dse_terrain_deform_shutdown},
        {"init", L_dse_audio_lod_init},
        {"register_source", L_dse_audio_lod_register_source},
        {"tick", L_dse_audio_lod_tick},
        {"is_audible", L_dse_audio_lod_is_audible},
        {"shutdown", L_dse_audio_lod_shutdown},
        {"physics_lod_evaluate", L_dse_physics_lod_evaluate},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
