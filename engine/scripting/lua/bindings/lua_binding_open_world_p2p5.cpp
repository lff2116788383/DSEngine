/**
 * @file lua_binding_open_world_p2p5.cpp
 * @brief Lua 绑定：P2-P5 大世界系统（Mesh Streaming / Physics LOD / Terrain Deform / Audio LOD）
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

#include "engine/render/mesh_streaming.h"
#include "engine/physics/physics3d/physics_lod.h"
#include "engine/terrain/terrain_deformation.h"
#include "engine/audio/audio_lod.h"
#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include <memory>

namespace dse::runtime::lua_binding {

namespace {

// ============= Singleton instances (for Lua access) =============
static std::unique_ptr<dse::render::MeshStreamingSystem> s_mesh_streaming;
static std::unique_ptr<dse::physics3d::PhysicsLODSystem> s_physics_lod;
static std::unique_ptr<dse::terrain::TerrainDeformationSystem> s_terrain_deform;
static std::unique_ptr<dse::audio::AudioLODSystem> s_audio_lod;

// ============= P2: Mesh Streaming =============

int L_MS_Init(lua_State* L) {
    if (!s_mesh_streaming) s_mesh_streaming = std::make_unique<dse::render::MeshStreamingSystem>();
    dse::render::MeshStreamingConfig cfg;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "hysteresis"); if (!lua_isnil(L, -1)) cfg.hysteresis_factor = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "budget"); if (!lua_isnil(L, -1)) cfg.load_budget_per_frame = static_cast<int>(lua_tointeger(L, -1)); lua_pop(L, 1);
    }
    s_mesh_streaming->Init(cfg);
    return 0;
}

int L_MS_RegisterMesh(lua_State* L) {
    if (!s_mesh_streaming) { lua_pushinteger(L, 0); return 1; }
    const char* name = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float r = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    uint32_t id = s_mesh_streaming->RegisterMesh(name, glm::vec3(x, y, z), r);
    lua_pushinteger(L, id);
    return 1;
}

int L_MS_AddLOD(lua_State* L) {
    if (!s_mesh_streaming) return 0;
    uint32_t mesh_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t level = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    const char* path = luaL_checkstring(L, 3);
    float dist = static_cast<float>(luaL_checknumber(L, 4));
    uint32_t tris = static_cast<uint32_t>(luaL_optinteger(L, 5, 0));
    s_mesh_streaming->AddLODLevel(mesh_id, level, path, dist, tris);
    return 0;
}

int L_MS_Tick(lua_State* L) {
    if (!s_mesh_streaming) return 0;
    float cx = static_cast<float>(luaL_checknumber(L, 1));
    float cy = static_cast<float>(luaL_checknumber(L, 2));
    float cz = static_cast<float>(luaL_checknumber(L, 3));
    float dt = static_cast<float>(luaL_optnumber(L, 4, 0.016));
    s_mesh_streaming->Tick(glm::vec3(cx, cy, cz), dt);
    return 0;
}

int L_MS_GetCurrentLOD(lua_State* L) {
    if (!s_mesh_streaming) { lua_pushinteger(L, 0); return 1; }
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, s_mesh_streaming->GetCurrentLOD(id));
    return 1;
}

int L_MS_GetMeshCount(lua_State* L) {
    lua_pushinteger(L, s_mesh_streaming ? s_mesh_streaming->GetMeshCount() : 0);
    return 1;
}

int L_MS_Shutdown(lua_State* L) {
    (void)L;
    if (s_mesh_streaming) s_mesh_streaming->Shutdown();
    return 0;
}

// ============= P3: Physics LOD =============

int L_PL_Init(lua_State* L) {
    if (!s_physics_lod) s_physics_lod = std::make_unique<dse::physics3d::PhysicsLODSystem>();
    dse::physics3d::PhysicsLODConfig cfg;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "full_distance"); if (!lua_isnil(L, -1)) cfg.full_distance = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "reduced_distance"); if (!lua_isnil(L, -1)) cfg.reduced_distance = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "simplified_distance"); if (!lua_isnil(L, -1)) cfg.simplified_distance = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
    }
    s_physics_lod->Init(cfg);
    return 0;
}

int L_PL_RegisterBody(lua_State* L) {
    if (!s_physics_lod) { lua_pushinteger(L, 0); return 1; }
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float r = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    s_physics_lod->RegisterBody(eid, glm::vec3(x, y, z), r);
    lua_pushinteger(L, eid);
    return 1;
}

int L_PL_Evaluate(lua_State* L) {
    if (!s_physics_lod) { lua_newtable(L); return 1; }
    float cx = static_cast<float>(luaL_checknumber(L, 1));
    float cy = static_cast<float>(luaL_checknumber(L, 2));
    float cz = static_cast<float>(luaL_checknumber(L, 3));
    uint32_t frame = static_cast<uint32_t>(luaL_optinteger(L, 4, 0));
    auto active = s_physics_lod->Evaluate(glm::vec3(cx, cy, cz), frame);
    lua_newtable(L);
    for (size_t i = 0; i < active.size(); ++i) {
        lua_pushinteger(L, active[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

int L_PL_GetStats(lua_State* L) {
    if (!s_physics_lod) { lua_pushnil(L); return 1; }
    auto stats = s_physics_lod->GetLevelStats();
    lua_newtable(L);
    lua_pushinteger(L, stats.full); lua_setfield(L, -2, "full");
    lua_pushinteger(L, stats.reduced); lua_setfield(L, -2, "reduced");
    lua_pushinteger(L, stats.simplified); lua_setfield(L, -2, "simplified");
    lua_pushinteger(L, stats.sleeping); lua_setfield(L, -2, "sleeping");
    return 1;
}

int L_PL_Wake(lua_State* L) {
    if (!s_physics_lod) return 0;
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    s_physics_lod->WakeBody(eid);
    return 0;
}

int L_PL_Sleep(lua_State* L) {
    if (!s_physics_lod) return 0;
    uint32_t eid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    s_physics_lod->SleepBody(eid);
    return 0;
}

int L_PL_Shutdown(lua_State* L) {
    (void)L;
    if (s_physics_lod) s_physics_lod->Shutdown();
    return 0;
}

// ============= P4: Terrain Deformation =============

int L_TD_Init(lua_State* L) {
    if (!s_terrain_deform) s_terrain_deform = std::make_unique<dse::terrain::TerrainDeformationSystem>();
    dse::terrain::TerrainDeformConfig cfg;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "max_depth"); if (!lua_isnil(L, -1)) cfg.max_deformation_depth = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "max_height"); if (!lua_isnil(L, -1)) cfg.max_deformation_height = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
    }
    s_terrain_deform->Init(cfg);
    return 0;
}

int L_TD_Deform(lua_State* L) {
    if (!s_terrain_deform) { lua_pushinteger(L, 0); return 1; }
    dse::terrain::DeformationOp op;
    op.type = static_cast<dse::terrain::DeformationType>(luaL_checkinteger(L, 1));
    op.center.x = static_cast<float>(luaL_checknumber(L, 2));
    op.center.y = static_cast<float>(luaL_checknumber(L, 3));
    op.center.z = static_cast<float>(luaL_checknumber(L, 4));
    op.radius = static_cast<float>(luaL_checknumber(L, 5));
    op.strength = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    lua_pushinteger(L, s_terrain_deform->ApplyDeformation(op));
    return 1;
}

int L_TD_Undo(lua_State* L) {
    lua_pushboolean(L, s_terrain_deform ? s_terrain_deform->Undo() : 0);
    return 1;
}

int L_TD_Redo(lua_State* L) {
    lua_pushboolean(L, s_terrain_deform ? s_terrain_deform->Redo() : 0);
    return 1;
}

int L_TD_SampleHeight(lua_State* L) {
    if (!s_terrain_deform) { lua_pushnumber(L, 0); return 1; }
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    lua_pushnumber(L, s_terrain_deform->SampleHeight(x, z));
    return 1;
}

int L_TD_Shutdown(lua_State* L) {
    (void)L;
    if (s_terrain_deform) s_terrain_deform->Shutdown();
    return 0;
}

// ============= P5: Audio LOD =============

int L_AL_Init(lua_State* L) {
    if (!s_audio_lod) s_audio_lod = std::make_unique<dse::audio::AudioLODSystem>();
    dse::audio::AudioLODConfig cfg;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "full_distance"); if (!lua_isnil(L, -1)) cfg.full_distance = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "max_active"); if (!lua_isnil(L, -1)) cfg.max_active_sources = static_cast<uint32_t>(lua_tointeger(L, -1)); lua_pop(L, 1);
    }
    s_audio_lod->Init(cfg);
    return 0;
}

int L_AL_RegisterSource(lua_State* L) {
    if (!s_audio_lod) { lua_pushinteger(L, 0); return 1; }
    const char* path = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float max_dist = static_cast<float>(luaL_optnumber(L, 5, 100.0));
    float prio = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    uint32_t id = s_audio_lod->RegisterSource(path, glm::vec3(x, y, z), max_dist, prio);
    lua_pushinteger(L, id);
    return 1;
}

int L_AL_Tick(lua_State* L) {
    if (!s_audio_lod) return 0;
    float lx = static_cast<float>(luaL_checknumber(L, 1));
    float ly = static_cast<float>(luaL_checknumber(L, 2));
    float lz = static_cast<float>(luaL_checknumber(L, 3));
    float dt = static_cast<float>(luaL_optnumber(L, 4, 0.016));
    s_audio_lod->Tick(glm::vec3(lx, ly, lz), glm::vec3(0, 0, -1), dt);
    return 0;
}

int L_AL_GetStats(lua_State* L) {
    if (!s_audio_lod) { lua_pushnil(L); return 1; }
    auto stats = s_audio_lod->GetStats();
    lua_newtable(L);
    lua_pushinteger(L, stats.full); lua_setfield(L, -2, "full");
    lua_pushinteger(L, stats.reduced); lua_setfield(L, -2, "reduced");
    lua_pushinteger(L, stats.virtual_count); lua_setfield(L, -2, "virtual");
    lua_pushinteger(L, stats.culled); lua_setfield(L, -2, "culled");
    return 1;
}

int L_AL_IsAudible(lua_State* L) {
    if (!s_audio_lod) { lua_pushboolean(L, 0); return 1; }
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, s_audio_lod->IsSourceAudible(id) ? 1 : 0);
    return 1;
}

int L_AL_Shutdown(lua_State* L) {
    (void)L;
    if (s_audio_lod) s_audio_lod->Shutdown();
    return 0;
}

// ============= Registration tables =============

static const luaL_Reg mesh_streaming_funcs[] = {
    {"init", L_MS_Init}, {"register_mesh", L_MS_RegisterMesh}, {"add_lod", L_MS_AddLOD},
    {"tick", L_MS_Tick}, {"get_current_lod", L_MS_GetCurrentLOD},
    {"get_mesh_count", L_MS_GetMeshCount}, {"shutdown", L_MS_Shutdown},
    {nullptr, nullptr}
};

static const luaL_Reg physics_lod_funcs[] = {
    {"init", L_PL_Init}, {"register_body", L_PL_RegisterBody},
    {"evaluate", L_PL_Evaluate}, {"get_stats", L_PL_GetStats},
    {"wake", L_PL_Wake}, {"sleep", L_PL_Sleep}, {"shutdown", L_PL_Shutdown},
    {nullptr, nullptr}
};

static const luaL_Reg terrain_deform_funcs[] = {
    {"init", L_TD_Init}, {"deform", L_TD_Deform},
    {"undo", L_TD_Undo}, {"redo", L_TD_Redo},
    {"sample_height", L_TD_SampleHeight}, {"shutdown", L_TD_Shutdown},
    {nullptr, nullptr}
};

static const luaL_Reg audio_lod_funcs[] = {
    {"init", L_AL_Init}, {"register_source", L_AL_RegisterSource},
    {"tick", L_AL_Tick}, {"get_stats", L_AL_GetStats},
    {"is_audible", L_AL_IsAudible}, {"shutdown", L_AL_Shutdown},
    {nullptr, nullptr}
};

} // anonymous namespace

void RegisterOpenWorldP2P5Bindings(lua_State* L) {
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
