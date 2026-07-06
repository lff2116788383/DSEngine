/**
 * @file lua_binding_open_world_p2p5.cpp
 * @brief Lua 绑定：P2-P5 大世界系统（Mesh Streaming / Physics LOD / Terrain Deform / Audio LOD）。
 *        薄包装委托至 C ABI。
 *
 * 注册到 dse 表下的子表：
 *   dse.mesh_streaming.*
 *   dse.physics_lod.*
 *   dse.terrain_deform.*
 *   dse.audio_lod.*
 */

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"

#include <cmath>
#include <limits>
#include <vector>

namespace dse::runtime::lua_binding {

namespace {

constexpr float kKeep = std::numeric_limits<float>::quiet_NaN();

// 从 table 字段读浮点，缺省返回 NaN（表示"使用默认值"）。
float OptFieldFloat(lua_State* L, int table_index, const char* field) {
    float v = kKeep;
    lua_getfield(L, table_index, field);
    if (lua_isnumber(L, -1)) v = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return v;
}

// 从 table 字段读整型，缺省返回 -1（表示"使用默认值"）。
int OptFieldInt(lua_State* L, int table_index, const char* field) {
    int v = -1;
    lua_getfield(L, table_index, field);
    if (lua_isnumber(L, -1)) v = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    return v;
}

// ============= P2: Mesh Streaming =============

int L_MS_Init(lua_State* L) {
    float hysteresis = kKeep;
    int budget = -1;
    if (lua_istable(L, 1)) {
        hysteresis = OptFieldFloat(L, 1, "hysteresis");
        budget = OptFieldInt(L, 1, "budget");
    }
    dse_mesh_streaming_init(hysteresis, budget);
    return 0;
}

int L_MS_RegisterMesh(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float r = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    lua_pushinteger(L, dse_mesh_streaming_register_mesh(name, x, y, z, r));
    return 1;
}

int L_MS_AddLOD(lua_State* L) {
    dse_mesh_streaming_add_lod(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                               static_cast<uint32_t>(luaL_checkinteger(L, 2)),
                               luaL_checkstring(L, 3),
                               static_cast<float>(luaL_checknumber(L, 4)),
                               static_cast<uint32_t>(luaL_optinteger(L, 5, 0)));
    return 0;
}

int L_MS_Tick(lua_State* L) {
    dse_mesh_streaming_tick(static_cast<float>(luaL_checknumber(L, 1)),
                            static_cast<float>(luaL_checknumber(L, 2)),
                            static_cast<float>(luaL_checknumber(L, 3)),
                            static_cast<float>(luaL_optnumber(L, 4, 0.016)));
    return 0;
}

int L_MS_GetCurrentLOD(lua_State* L) {
    lua_pushinteger(L, dse_mesh_streaming_get_current_lod(
                           static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    return 1;
}

int L_MS_GetMeshCount(lua_State* L) {
    lua_pushinteger(L, dse_mesh_streaming_get_mesh_count());
    return 1;
}

int L_MS_Shutdown(lua_State* L) {
    (void)L;
    dse_mesh_streaming_shutdown();
    return 0;
}

// ============= P3: Physics LOD =============

int L_PL_Init(lua_State* L) {
    float full = kKeep, reduced = kKeep, simplified = kKeep;
    if (lua_istable(L, 1)) {
        full = OptFieldFloat(L, 1, "full_distance");
        reduced = OptFieldFloat(L, 1, "reduced_distance");
        simplified = OptFieldFloat(L, 1, "simplified_distance");
    }
    dse_physics_lod_init(full, reduced, simplified);
    return 0;
}

int L_PL_RegisterBody(lua_State* L) {
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_physics_lod_register_body(eid,
                                  static_cast<float>(luaL_checknumber(L, 2)),
                                  static_cast<float>(luaL_checknumber(L, 3)),
                                  static_cast<float>(luaL_checknumber(L, 4)),
                                  static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    lua_pushinteger(L, eid);
    return 1;
}

int L_PL_Evaluate(lua_State* L) {
    float cx = static_cast<float>(luaL_checknumber(L, 1));
    float cy = static_cast<float>(luaL_checknumber(L, 2));
    float cz = static_cast<float>(luaL_checknumber(L, 3));
    uint32_t frame = static_cast<uint32_t>(luaL_optinteger(L, 4, 0));
    int total = dse_physics_lod_evaluate(cx, cy, cz, frame, nullptr, 0);
    lua_newtable(L);
    if (total <= 0) return 1;
    std::vector<uint32_t> ids(static_cast<size_t>(total));
    int n = dse_physics_lod_evaluate(cx, cy, cz, frame, ids.data(), total);
    if (n > total) n = total;
    for (int i = 0; i < n; ++i) {
        lua_pushinteger(L, ids[static_cast<size_t>(i)]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int L_PL_GetStats(lua_State* L) {
    int stats[4] = {0};
    if (!dse_physics_lod_get_stats(stats)) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushinteger(L, stats[0]); lua_setfield(L, -2, "full");
    lua_pushinteger(L, stats[1]); lua_setfield(L, -2, "reduced");
    lua_pushinteger(L, stats[2]); lua_setfield(L, -2, "simplified");
    lua_pushinteger(L, stats[3]); lua_setfield(L, -2, "sleeping");
    return 1;
}

int L_PL_Wake(lua_State* L) {
    dse_physics_lod_wake(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

int L_PL_Sleep(lua_State* L) {
    dse_physics_lod_sleep(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

int L_PL_Shutdown(lua_State* L) {
    (void)L;
    dse_physics_lod_shutdown();
    return 0;
}

// ============= P4: Terrain Deformation =============

int L_TD_Init(lua_State* L) {
    float max_depth = kKeep, max_height = kKeep;
    if (lua_istable(L, 1)) {
        max_depth = OptFieldFloat(L, 1, "max_depth");
        max_height = OptFieldFloat(L, 1, "max_height");
    }
    dse_terrain_deform_init(max_depth, max_height);
    return 0;
}

int L_TD_Deform(lua_State* L) {
    lua_pushinteger(L, dse_terrain_deform_apply(
                           static_cast<int>(luaL_checkinteger(L, 1)),
                           static_cast<float>(luaL_checknumber(L, 2)),
                           static_cast<float>(luaL_checknumber(L, 3)),
                           static_cast<float>(luaL_checknumber(L, 4)),
                           static_cast<float>(luaL_checknumber(L, 5)),
                           static_cast<float>(luaL_optnumber(L, 6, 1.0))));
    return 1;
}

int L_TD_Undo(lua_State* L) {
    lua_pushboolean(L, dse_terrain_deform_undo());
    return 1;
}

int L_TD_Redo(lua_State* L) {
    lua_pushboolean(L, dse_terrain_deform_redo());
    return 1;
}

int L_TD_SampleHeight(lua_State* L) {
    lua_pushnumber(L, dse_terrain_deform_sample_height(
                          static_cast<float>(luaL_checknumber(L, 1)),
                          static_cast<float>(luaL_checknumber(L, 2))));
    return 1;
}

int L_TD_Shutdown(lua_State* L) {
    (void)L;
    dse_terrain_deform_shutdown();
    return 0;
}

// ============= P5: Audio LOD =============

int L_AL_Init(lua_State* L) {
    float full_distance = kKeep;
    int max_active = -1;
    if (lua_istable(L, 1)) {
        full_distance = OptFieldFloat(L, 1, "full_distance");
        max_active = OptFieldInt(L, 1, "max_active");
    }
    dse_audio_lod_init(full_distance, max_active);
    return 0;
}

int L_AL_RegisterSource(lua_State* L) {
    lua_pushinteger(L, dse_audio_lod_register_source(
                           luaL_checkstring(L, 1),
                           static_cast<float>(luaL_checknumber(L, 2)),
                           static_cast<float>(luaL_checknumber(L, 3)),
                           static_cast<float>(luaL_checknumber(L, 4)),
                           static_cast<float>(luaL_optnumber(L, 5, 100.0)),
                           static_cast<float>(luaL_optnumber(L, 6, 0.0))));
    return 1;
}

int L_AL_Tick(lua_State* L) {
    dse_audio_lod_tick(static_cast<float>(luaL_checknumber(L, 1)),
                       static_cast<float>(luaL_checknumber(L, 2)),
                       static_cast<float>(luaL_checknumber(L, 3)),
                       static_cast<float>(luaL_optnumber(L, 4, 0.016)));
    return 0;
}

int L_AL_GetStats(lua_State* L) {
    int stats[4] = {0};
    if (!dse_audio_lod_get_stats(stats)) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushinteger(L, stats[0]); lua_setfield(L, -2, "full");
    lua_pushinteger(L, stats[1]); lua_setfield(L, -2, "reduced");
    lua_pushinteger(L, stats[2]); lua_setfield(L, -2, "virtual");
    lua_pushinteger(L, stats[3]); lua_setfield(L, -2, "culled");
    return 1;
}

int L_AL_IsAudible(lua_State* L) {
    lua_pushboolean(L, dse_audio_lod_is_audible(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    return 1;
}

int L_AL_Shutdown(lua_State* L) {
    (void)L;
    dse_audio_lod_shutdown();
    return 0;
}

// ============= Registration tables =============

const luaL_Reg mesh_streaming_funcs[] = {
    {"init", L_MS_Init}, {"register_mesh", L_MS_RegisterMesh}, {"add_lod", L_MS_AddLOD},
    {"tick", L_MS_Tick}, {"get_current_lod", L_MS_GetCurrentLOD},
    {"get_mesh_count", L_MS_GetMeshCount}, {"shutdown", L_MS_Shutdown},
    {nullptr, nullptr}
};

const luaL_Reg physics_lod_funcs[] = {
    {"init", L_PL_Init}, {"register_body", L_PL_RegisterBody},
    {"evaluate", L_PL_Evaluate}, {"get_stats", L_PL_GetStats},
    {"wake", L_PL_Wake}, {"sleep", L_PL_Sleep}, {"shutdown", L_PL_Shutdown},
    {nullptr, nullptr}
};

const luaL_Reg terrain_deform_funcs[] = {
    {"init", L_TD_Init}, {"deform", L_TD_Deform},
    {"undo", L_TD_Undo}, {"redo", L_TD_Redo},
    {"sample_height", L_TD_SampleHeight}, {"shutdown", L_TD_Shutdown},
    {nullptr, nullptr}
};

const luaL_Reg audio_lod_funcs[] = {
    {"init", L_AL_Init}, {"register_source", L_AL_RegisterSource},
    {"tick", L_AL_Tick}, {"get_stats", L_AL_GetStats},
    {"is_audible", L_AL_IsAudible}, {"shutdown", L_AL_Shutdown},
    {nullptr, nullptr}
};

} // anonymous namespace

void ShutdownOpenWorldP2P5Bindings() {
    dse_open_world_p2p5_shutdown();
}

void RegisterOpenWorldP2P5Bindings(lua_State* L) {
    static bool registered = false;
    if (!registered) {
        BindingCleanupRegistry::Instance().Register(ShutdownOpenWorldP2P5Bindings);
        registered = true;
    }
    // Expects dse table on top of stack
    lua_newtable(L);
    luaL_setfuncs(L, mesh_streaming_funcs, 0);
    lua_setfield(L, -2, "mesh_streaming");

    lua_newtable(L);
    luaL_setfuncs(L, physics_lod_funcs, 0);
    lua_setfield(L, -2, "physics_lod");

    lua_newtable(L);
    luaL_setfuncs(L, terrain_deform_funcs, 0);
    lua_setfield(L, -2, "terrain_deform");

    lua_newtable(L);
    luaL_setfuncs(L, audio_lod_funcs, 0);
    lua_setfield(L, -2, "audio_lod");
}

} // namespace dse::runtime::lua_binding
