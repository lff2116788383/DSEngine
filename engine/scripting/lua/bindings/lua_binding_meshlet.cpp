/**
 * @file lua_binding_meshlet.cpp
 * @brief Lua 绑定：Meshlet/Cluster 渲染系统
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

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include "engine/render/meshlet/meshlet_builder.h"
#include "engine/render/meshlet/meshlet_cull_pass.h"
#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include <unordered_map>
#include <memory>
#include <cstring>

namespace dse::runtime::lua_binding {

namespace {

struct LuaMeshletMesh {
    dse::render::MeshletMesh data;
};

static uint32_t s_next_mesh_id = 1;
static std::unordered_map<uint32_t, std::unique_ptr<LuaMeshletMesh>> s_meshes;

static uint32_t s_next_cull_id = 1;
static std::unordered_map<uint32_t, std::unique_ptr<dse::render::MeshletCullPass>> s_culls;

glm::mat4 ReadMat4FromTable(lua_State* L, int idx) {
    glm::mat4 m(1.0f);
    float* ptr = &m[0][0];
    for (int i = 1; i <= 16; ++i) {
        lua_rawgeti(L, idx, i);
        ptr[i - 1] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    return m;
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
    std::vector<glm::vec3> positions(pos_len / 3);
    for (int i = 0; i < pos_len; i += 3) {
        lua_rawgeti(L, 1, i + 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, 1, i + 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, 1, i + 3); float z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        positions[i / 3] = glm::vec3(x, y, z);
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
    dse::render::MeshletBuildConfig config;
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "max_vertices");
        if (!lua_isnil(L, -1)) config.max_vertices = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 3, "max_triangles");
        if (!lua_isnil(L, -1)) config.max_triangles = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    dse::render::MeshletBuilder builder;
    auto result = builder.Build(positions, indices, config);

    uint32_t id = s_next_mesh_id++;
    auto entry = std::make_unique<LuaMeshletMesh>();
    entry->data = std::move(result);
    s_meshes[id] = std::move(entry);

    lua_pushinteger(L, id);
    return 1;
}

// meshlet.serialize(mesh_id, path)
int L_Serialize(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);

    auto it = s_meshes.find(id);
    if (it == s_meshes.end()) { lua_pushboolean(L, 0); return 1; }

    bool ok = dse::render::MeshletBuilder::Serialize(it->second->data, path);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// meshlet.deserialize(path)
int L_Deserialize(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    auto entry = std::make_unique<LuaMeshletMesh>();
    if (!dse::render::MeshletBuilder::Deserialize(path, entry->data)) {
        lua_pushnil(L);
        return 1;
    }

    uint32_t id = s_next_mesh_id++;
    s_meshes[id] = std::move(entry);
    lua_pushinteger(L, id);
    return 1;
}

// meshlet.destroy(mesh_id)
int L_Destroy(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    s_meshes.erase(id);
    return 0;
}

// meshlet.get_info(mesh_id)
int L_GetInfo(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    auto it = s_meshes.find(id);
    if (it == s_meshes.end()) { lua_pushnil(L); return 1; }

    const auto& m = it->second->data;
    lua_newtable(L);
    lua_pushinteger(L, static_cast<int>(m.meshlets.size()));
    lua_setfield(L, -2, "meshlet_count");
    lua_pushinteger(L, static_cast<int>(m.positions.size()));
    lua_setfield(L, -2, "vertex_count");
    lua_pushinteger(L, static_cast<int>(m.global_indices.size()));
    lua_setfield(L, -2, "index_count");
    lua_pushinteger(L, static_cast<int>(m.meshlet_vertices.size()));
    lua_setfield(L, -2, "meshlet_vertex_count");
    return 1;
}

// meshlet.cull_create()
int L_CullCreate(lua_State* L) {
    uint32_t id = s_next_cull_id++;
    s_culls[id] = std::make_unique<dse::render::MeshletCullPass>();
    lua_pushinteger(L, id);
    return 1;
}

// meshlet.cull_destroy(cull_id)
int L_CullDestroy(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    s_culls.erase(id);
    return 0;
}

// meshlet.cull_register(cull_id, mesh_id)
int L_CullRegister(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t mesh_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));

    auto cit = s_culls.find(cull_id);
    if (cit == s_culls.end()) { lua_pushnil(L); return 1; }
    auto mit = s_meshes.find(mesh_id);
    if (mit == s_meshes.end()) { lua_pushnil(L); return 1; }

    uint32_t reg_id = cit->second->RegisterMesh(mit->second->data);
    lua_pushinteger(L, reg_id);
    return 1;
}

// meshlet.cull_unregister(cull_id, registered_mesh_id)
int L_CullUnregister(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t reg_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));

    auto cit = s_culls.find(cull_id);
    if (cit != s_culls.end()) {
        cit->second->UnregisterMesh(reg_id);
    }
    return 0;
}

// meshlet.cull_begin_frame(cull_id)
int L_CullBeginFrame(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    auto cit = s_culls.find(cull_id);
    if (cit != s_culls.end()) {
        cit->second->BeginFrame();
    }
    return 0;
}

// meshlet.cull_add_instance(cull_id, reg_mesh_id, model_table_16)
int L_CullAddInstance(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t reg_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TTABLE);

    auto cit = s_culls.find(cull_id);
    if (cit == s_culls.end()) return 0;

    glm::mat4 model = ReadMat4FromTable(L, 3);
    cit->second->AddInstance(reg_id, model);
    return 0;
}

// meshlet.cull_prepare(cull_id, vp_table_16, cam_x, cam_y, cam_z)
int L_CullPrepare(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    float cx = static_cast<float>(luaL_checknumber(L, 3));
    float cy = static_cast<float>(luaL_checknumber(L, 4));
    float cz = static_cast<float>(luaL_checknumber(L, 5));

    auto cit = s_culls.find(cull_id);
    if (cit == s_culls.end()) { lua_pushinteger(L, 0); return 1; }

    glm::mat4 vp = ReadMat4FromTable(L, 2);
    // We pass identity view/proj for PrepareGPUData (it doesn't use them directly)
    uint32_t count = cit->second->PrepareGPUData(glm::mat4(1.0f), vp, glm::vec3(cx, cy, cz));
    lua_pushinteger(L, count);
    return 1;
}

// meshlet.cull_execute_cpu(cull_id, vp_table_16, cam_x, cam_y, cam_z, flags?)
int L_CullExecuteCPU(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    float cx = static_cast<float>(luaL_checknumber(L, 3));
    float cy = static_cast<float>(luaL_checknumber(L, 4));
    float cz = static_cast<float>(luaL_checknumber(L, 5));

    auto cit = s_culls.find(cull_id);
    if (cit == s_culls.end()) { lua_pushinteger(L, 0); return 1; }

    glm::mat4 vp = ReadMat4FromTable(L, 2);

    dse::render::MeshletCullConfig config;
    if (lua_isnumber(L, 6)) {
        uint32_t flags = static_cast<uint32_t>(lua_tointeger(L, 6));
        config.enable_frustum_cull = (flags & 1) != 0;
        config.enable_occlusion_cull = (flags & 2) != 0;
        config.enable_cone_cull = (flags & 4) != 0;
    }

    cit->second->CullCPU(vp, glm::vec3(cx, cy, cz), config);
    lua_pushinteger(L, cit->second->GetVisibleMeshletCount());
    return 1;
}

// meshlet.cull_stats(cull_id)
int L_CullStats(lua_State* L) {
    uint32_t cull_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    auto cit = s_culls.find(cull_id);
    if (cit == s_culls.end()) { lua_pushnil(L); return 1; }

    lua_newtable(L);
    lua_pushinteger(L, cit->second->GetTotalMeshletCount());
    lua_setfield(L, -2, "total");
    lua_pushinteger(L, cit->second->GetVisibleMeshletCount());
    lua_setfield(L, -2, "visible");
    lua_pushinteger(L, cit->second->GetRegisteredMeshCount());
    lua_setfield(L, -2, "meshes");
    lua_pushinteger(L, cit->second->GetInstanceCount());
    lua_setfield(L, -2, "instances");
    return 1;
}

} // anonymous namespace

void ShutdownMeshletBindings() {
    s_meshes.clear();
    s_next_mesh_id = 1;
    s_culls.clear();
    s_next_cull_id = 1;
}

namespace {

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

void RegisterMeshletBindings(lua_State* L) {
    lua_newtable(L);
    luaL_setfuncs(L, meshlet_funcs, 0);
    lua_setglobal(L, "meshlet");
}

} // namespace dse::runtime::lua_binding
