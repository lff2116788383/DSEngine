/**
 * @file lua_binding_meshlet.cpp
 * @brief Lua 薄包装：Meshlet/Cluster 渲染系统
 *
 * 仅做 Lua 栈 ↔ C ABI 参数转换，所有逻辑委托 dse_meshlet_* 函数。
 *
 * 全局表 "meshlet"：
 *   meshlet.build(positions_flat, indices, config?) → meshlet_mesh_id
 *   meshlet.serialize(mesh_id, path) → bool
 *   meshlet.deserialize(path) → meshlet_mesh_id | nil
 *   meshlet.destroy(mesh_id)
 *   meshlet.get_info(mesh_id) → {meshlet_count, vertex_count, ...}
 *
 *   -- Cull Pass
 *   meshlet.cull_create() → cull_id
 *   meshlet.cull_destroy(cull_id)
 *   meshlet.cull_register(cull_id, mesh_id) → registered_mesh_id
 *   meshlet.cull_unregister(cull_id, registered_mesh_id)
 *   meshlet.cull_begin_frame(cull_id)
 *   meshlet.cull_add_instance(cull_id, reg_mesh_id, model_table_16)
 *   meshlet.cull_prepare(cull_id, vp_table_16, cam_x, cam_y, cam_z) → count
 *   meshlet.cull_execute_cpu(cull_id, vp_table_16, cam_x, cam_y, cam_z, flags?) → visible_count
 *   meshlet.cull_stats(cull_id) → {total, visible, meshes, instances}
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include <vector>

namespace dse::runtime::lua_binding {
namespace {

// 从 Lua 栈读取 16 元素表到 float[16]
void ReadMat4FromTable(lua_State* L, int idx, float* out16) {
    for (int i = 0; i < 16; ++i) {
        lua_rawgeti(L, idx, i + 1);
        out16[i] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
}

// meshlet.build(positions_flat, indices, config?)
int L_Build(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    // Read positions as flat array {x1,y1,z1, x2,y2,z2, ...}
    int pos_len = static_cast<int>(lua_rawlen(L, 1));
    if (pos_len % 3 != 0) {
        return luaL_error(L, "positions must be flat array of xyz triplets");
    }
    int pos_count = pos_len / 3;
    std::vector<float> positions(pos_len);
    for (int i = 0; i < pos_len; ++i) {
        lua_rawgeti(L, 1, i + 1);
        positions[i] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }

    // Read indices
    int idx_len = static_cast<int>(lua_rawlen(L, 2));
    std::vector<uint32_t> indices(idx_len);
    for (int i = 0; i < idx_len; ++i) {
        lua_rawgeti(L, 2, i + 1);
        indices[i] = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    // Optional config
    uint32_t max_vertices = 64;
    uint32_t max_triangles = 124;
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "max_vertices");
        if (!lua_isnil(L, -1)) max_vertices = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 3, "max_triangles");
        if (!lua_isnil(L, -1)) max_triangles = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    uint32_t id = dse_meshlet_build(positions.data(), pos_count,
                                    indices.data(), idx_len,
                                    max_vertices, max_triangles);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

// meshlet.serialize(mesh_id, path)
int L_Serialize(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    int ok = dse_meshlet_serialize(id, path);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// meshlet.deserialize(path)
int L_Deserialize(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t id = dse_meshlet_deserialize(path);
    if (id == 0) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

// meshlet.destroy(mesh_id)
int L_Destroy(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_destroy(id);
    return 0;
}

// meshlet.get_info(mesh_id)
int L_GetInfo(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int meshlets = 0, vertices = 0, indices = 0, meshlet_vertices = 0;
    dse_meshlet_get_info(id, &meshlets, &vertices, &indices, &meshlet_vertices);
    lua_newtable(L);
    lua_pushinteger(L, meshlets);          lua_setfield(L, -2, "meshlet_count");
    lua_pushinteger(L, vertices);          lua_setfield(L, -2, "vertex_count");
    lua_pushinteger(L, indices);           lua_setfield(L, -2, "index_count");
    lua_pushinteger(L, meshlet_vertices);  lua_setfield(L, -2, "meshlet_vertex_count");
    return 1;
}

// meshlet.cull_create()
int L_CullCreate(lua_State* L) {
    uint32_t id = dse_meshlet_cull_create();
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

// meshlet.cull_destroy(cull_id)
int L_CullDestroy(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_cull_destroy(id);
    return 0;
}

// meshlet.cull_register(cull_id, mesh_id)
int L_CullRegister(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t mesh_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t reg_id = dse_meshlet_cull_register(cull_id, mesh_id);
    lua_pushinteger(L, static_cast<lua_Integer>(reg_id));
    return 1;
}

// meshlet.cull_unregister(cull_id, registered_mesh_id)
int L_CullUnregister(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t reg_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_meshlet_cull_unregister(cull_id, reg_id);
    return 0;
}

// meshlet.cull_begin_frame(cull_id)
int L_CullBeginFrame(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_cull_begin_frame(cull_id);
    return 0;
}

// meshlet.cull_add_instance(cull_id, reg_mesh_id, model_table_16)
int L_CullAddInstance(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t reg_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TTABLE);
    float matrix[16];
    ReadMat4FromTable(L, 3, matrix);
    dse_meshlet_cull_add_instance(cull_id, reg_id, matrix);
    return 0;
}

// meshlet.cull_prepare(cull_id, vp_table_16, cam_x, cam_y, cam_z)
int L_CullPrepare(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    float cx = static_cast<float>(luaL_checknumber(L, 3));
    float cy = static_cast<float>(luaL_checknumber(L, 4));
    float cz = static_cast<float>(luaL_checknumber(L, 5));
    float vp[16];
    ReadMat4FromTable(L, 2, vp);
    uint32_t count = dse_meshlet_cull_prepare(cull_id, vp, cx, cy, cz);
    lua_pushinteger(L, static_cast<lua_Integer>(count));
    return 1;
}

// meshlet.cull_execute_cpu(cull_id, vp_table_16, cam_x, cam_y, cam_z, flags?)
int L_CullExecuteCPU(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    float cx = static_cast<float>(luaL_checknumber(L, 3));
    float cy = static_cast<float>(luaL_checknumber(L, 4));
    float cz = static_cast<float>(luaL_checknumber(L, 5));
    uint32_t flags = 0;
    if (lua_isnumber(L, 6)) {
        flags = static_cast<uint32_t>(lua_tointeger(L, 6));
    }
    float vp[16];
    ReadMat4FromTable(L, 2, vp);
    uint32_t visible = dse_meshlet_cull_execute_cpu(cull_id, vp, cx, cy, cz, flags);
    lua_pushinteger(L, static_cast<lua_Integer>(visible));
    return 1;
}

// meshlet.cull_stats(cull_id)
int L_CullStats(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int total = 0, visible = 0, meshes = 0, instances = 0;
    dse_meshlet_cull_stats(cull_id, &total, &visible, &meshes, &instances);
    lua_newtable(L);
    lua_pushinteger(L, total);       lua_setfield(L, -2, "total");
    lua_pushinteger(L, visible);     lua_setfield(L, -2, "visible");
    lua_pushinteger(L, meshes);      lua_setfield(L, -2, "meshes");
    lua_pushinteger(L, instances);   lua_setfield(L, -2, "instances");
    return 1;
}

static const luaL_Reg meshlet_funcs[] = {
    {"build",              L_Build},
    {"serialize",          L_Serialize},
    {"deserialize",        L_Deserialize},
    {"destroy",            L_Destroy},
    {"get_info",           L_GetInfo},
    {"cull_create",        L_CullCreate},
    {"cull_destroy",       L_CullDestroy},
    {"cull_register",      L_CullRegister},
    {"cull_unregister",    L_CullUnregister},
    {"cull_begin_frame",   L_CullBeginFrame},
    {"cull_add_instance",  L_CullAddInstance},
    {"cull_prepare",       L_CullPrepare},
    {"cull_execute_cpu",   L_CullExecuteCPU},
    {"cull_stats",         L_CullStats},
    {nullptr, nullptr}
};

} // anonymous namespace

void ShutdownMeshletBindings() {
    // C ABI 管理所有状态，Lua 侧无需清理
}

void RegisterMeshletBindings(lua_State* L) {
    static bool registered = false;
    if (!registered) {
        BindingCleanupRegistry::Instance().Register(ShutdownMeshletBindings);
        registered = true;
    }
    lua_newtable(L);
    luaL_setfuncs(L, meshlet_funcs, 0);
    lua_setglobal(L, "meshlet");
}

} // namespace dse::runtime::lua_binding
