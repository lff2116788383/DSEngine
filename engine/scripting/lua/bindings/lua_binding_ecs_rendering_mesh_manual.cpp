/**
 * @file lua_binding_ecs_rendering_mesh.cpp
 * @brief MeshRenderer / Morph Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）。薄包装委托至 C ABI。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <cmath>
#include <vector>

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

/// 可选浮点参数：缺省时返回 NaN（C ABI 哨兵 = 保持当前值）。
inline float OptNan(lua_State* L, int i) {
    return lua_isnoneornil(L, i) ? NAN : helper::CheckFloat(L, i);
}

/// 读取 Lua 数组（float 表）到 vector。
std::vector<float> ReadFloatArray(lua_State* L, int idx) {
    std::vector<float> out;
    if (!lua_istable(L, idx)) return out;
    const int len = static_cast<int>(lua_rawlen(L, idx));
    out.reserve(static_cast<size_t>(len));
    for (int i = 1; i <= len; ++i) {
        lua_rawgeti(L, idx, i);
        out.push_back(static_cast<float>(luaL_checknumber(L, -1)));
        lua_pop(L, 1);
    }
    return out;
}

// ============================================================
// MeshRenderer
// ============================================================

int L_EcsAddMeshRenderer(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float r = helper::OptFloat(L, 2, 1.0f);
    float g = helper::OptFloat(L, 3, 1.0f);
    float b = helper::OptFloat(L, 4, 1.0f);
    float a = helper::OptFloat(L, 5, 1.0f);

    std::vector<float> vertices = ReadFloatArray(L, 6);
    std::vector<int> indices;
    if (lua_istable(L, 7)) {
        const int i_len = static_cast<int>(lua_rawlen(L, 7));
        indices.reserve(static_cast<size_t>(i_len));
        for (int i = 1; i <= i_len; ++i) {
            lua_rawgeti(L, 7, i);
            indices.push_back(static_cast<int>(luaL_checkinteger(L, -1)));
            lua_pop(L, 1);
        }
    }
    dse_mesh_renderer_add_procedural(EID(e), r, g, b, a,
                                     vertices.empty() ? nullptr : vertices.data(),
                                     static_cast<int>(vertices.size()),
                                     indices.empty() ? nullptr : indices.data(),
                                     static_cast<int>(indices.size()));
    return 0;
}

// set_mesh_path / get_mesh_path 已迁至 codegen（binding_defs.json，capi_setter:manual）→
// dse_mesh_renderer_set_mesh_path（含 temp_* 清理）

int L_EcsSetMeshMaterial(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);

    // 检查第二个参数是否为字符串（dmat 路径）— 委托 dse_mesh_renderer_set_material_from_dmat
    if (lua_type(L, 2) == LUA_TSTRING) {
        const char* dmat_path = lua_tostring(L, 2);
        const uint32_t material_index = lua_gettop(L) >= 3 && lua_isinteger(L, 3)
            ? static_cast<uint32_t>(lua_tointeger(L, 3))
            : 0u;
        dse_mesh_renderer_set_material_from_dmat(EID(e), dmat_path, material_index);
        return 0;
    }

    // 标量参数路径（NaN=保持当前值；-1=保持当前布尔值）
    int receive_shadow = lua_gettop(L) >= 9 ? (helper::CheckBool(L, 9) ? 1 : 0) : -1;
    int double_sided = lua_gettop(L) >= 10 ? (helper::CheckBool(L, 10) ? 1 : 0) : -1;
    float cr = NAN, cg = NAN, cb = NAN, ca = NAN;
    // 可选 base_color 覆盖：用于 .dmat 之后的 Lua 材质创作恢复贴图的白色乘色，
    // 避免 .dmat base_color 与 material_albedo 双重相乘把 PBR 贴图压成灰色。
    if (lua_gettop(L) >= 13) {
        cr = helper::CheckFloat(L, 11);
        cg = helper::CheckFloat(L, 12);
        cb = helper::CheckFloat(L, 13);
        if (lua_gettop(L) >= 14) {
            ca = helper::CheckFloat(L, 14);
        }
    }
    dse_mesh_renderer_set_material_params(EID(e),
        OptNan(L, 2), OptNan(L, 3), OptNan(L, 4),
        OptNan(L, 5), OptNan(L, 6), OptNan(L, 7),
        OptNan(L, 8), receive_shadow, double_sided,
        cr, cg, cb, ca);
    return 0;
}

int L_EcsSetMeshDepthState(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int depth_test = helper::CheckBool(L, 2) ? 1 : 0;
    int depth_write = lua_gettop(L) >= 3 ? (helper::CheckBool(L, 3) ? 1 : 0) : -1;
    dse_mesh_renderer_set_depth_state(EID(e), depth_test, depth_write);
    return 0;
}

// set_mesh_shader_variant / get_mesh_shader_variant 已迁至 codegen（binding_defs.json）

int L_EcsSetMeshMaterialScalar(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* param_name = luaL_checkstring(L, 2);
    float value = helper::CheckFloat(L, 3);
    dse_mesh_renderer_set_material_scalar(EID(e), param_name, value);
    return 0;
}

int L_EcsSetMeshAdvancedMaterial(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_mesh_renderer_set_advanced_material(EID(e),
        helper::OptFloat(L, 2, 0.0f),
        helper::OptFloat(L, 3, 0.1f),
        helper::OptFloat(L, 4, 0.0f),
        helper::OptFloat(L, 5, 0.0f),
        helper::OptFloat(L, 6, 0.0f),
        helper::OptFloat(L, 7, 0.0f),
        helper::OptFloat(L, 8, 0.0f),
        helper::OptFloat(L, 9, 0.0f));
    return 0;
}

int L_EcsSetMeshTexture(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* slot = luaL_checkstring(L, 2);
    const char* texture_path = luaL_checkstring(L, 3);

    // 委托 dse_mesh_renderer_set_texture（slot 别名 / handle 绑定 / 贴图尺寸 逐值等价）
    uint32_t handle = 0;
    int width = 0, height = 0;
    if (!dse_mesh_renderer_set_texture(EID(e), slot, texture_path, &handle, &width, &height)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    helper::PushInt(L, static_cast<int>(handle));
    helper::PushInt(L, width);
    helper::PushInt(L, height);
    return 4;
}

int L_EcsSetMeshUv(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    if (!lua_istable(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    std::vector<float> uvs = ReadFloatArray(L, 2);
    int attr_count = 0, vertex_count = 0;
    int ok = dse_mesh_renderer_set_uvs(EID(e), uvs.empty() ? nullptr : uvs.data(),
                                       static_cast<int>(uvs.size()), &attr_count, &vertex_count);
    lua_pushboolean(L, ok ? 1 : 0);
    helper::PushInt(L, attr_count);
    helper::PushInt(L, vertex_count);
    return 3;
}

int L_EcsSetMeshNormals(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    if (!lua_istable(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    std::vector<float> normals = ReadFloatArray(L, 2);
    int attr_count = 0, vertex_count = 0;
    int ok = dse_mesh_renderer_set_normals(EID(e), normals.empty() ? nullptr : normals.data(),
                                           static_cast<int>(normals.size()), &attr_count, &vertex_count);
    lua_pushboolean(L, ok ? 1 : 0);
    helper::PushInt(L, attr_count);
    helper::PushInt(L, vertex_count);
    return 3;
}

int L_EcsSetMeshTangents(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    if (!lua_istable(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    std::vector<float> tangents = ReadFloatArray(L, 2);
    int attr_count = 0, vertex_count = 0;
    int ok = dse_mesh_renderer_set_tangents(EID(e), tangents.empty() ? nullptr : tangents.data(),
                                            static_cast<int>(tangents.size()), &attr_count, &vertex_count);
    lua_pushboolean(L, ok ? 1 : 0);
    helper::PushInt(L, attr_count);
    helper::PushInt(L, vertex_count);
    return 3;
}

// MeshRenderer emissive setter — 设置后标记数据源为 ComponentFallback
int L_EcsSetMeshEmissive(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_mesh_renderer_set_emissive_authoring(EID(e),
        helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));
    return 0;
}


// ============================================================
// MorphComponent 绑定
// ============================================================

// add_morph(entity)
int L_EcsAddMorph(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_morph_simple_add(EID(e));
    return 0;
}

// morph_add_target(entity, name, weight)
int L_EcsMorphAddTarget(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* name = helper::CheckString(L, 2);
    float weight = helper::OptFloat(L, 3, 0.0f);
    dse_morph_simple_add_target(EID(e), name, weight);
    return 0;
}

// morph_set_weight(entity, name_or_index, weight)
int L_EcsMorphSetWeight(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float weight = helper::CheckFloat(L, 3);
    if (lua_isinteger(L, 2)) {
        dse_morph_simple_set_weight_index(EID(e), static_cast<int>(lua_tointeger(L, 2)), weight);
    } else {
        dse_morph_simple_set_weight(EID(e), helper::CheckString(L, 2), weight);
    }
    return 0;
}

// morph_get_weight(entity, name_or_index) → weight
int L_EcsMorphGetWeight(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float weight = 0.0f;
    if (lua_isinteger(L, 2)) {
        weight = dse_morph_simple_get_weight_index(EID(e), static_cast<int>(lua_tointeger(L, 2)));
    } else {
        weight = dse_morph_simple_get_weight(EID(e), helper::CheckString(L, 2));
    }
    lua_pushnumber(L, weight);
    return 1;
}

int L_EcsSetMorphEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_morph_simple_set_enabled(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}


} // namespace

void RegisterEcsRenderingMeshBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_mesh_renderer",         L_EcsAddMeshRenderer},
        {"set_mesh_material",         L_EcsSetMeshMaterial},
        {"set_mesh_depth_state",      L_EcsSetMeshDepthState},
        {"set_mesh_material_scalar",  L_EcsSetMeshMaterialScalar},
        {"set_mesh_texture",          L_EcsSetMeshTexture},
        {"set_mesh_uvs",              L_EcsSetMeshUv},
        {"set_mesh_normals",          L_EcsSetMeshNormals},
        {"set_mesh_tangents",         L_EcsSetMeshTangents},
        {"set_mesh_emissive",         L_EcsSetMeshEmissive},
        {"set_mesh_advanced_material", L_EcsSetMeshAdvancedMaterial},
        {"add_morph",                 L_EcsAddMorph},
        {"morph_add_target",          L_EcsMorphAddTarget},
        {"morph_set_weight",          L_EcsMorphSetWeight},
        {"morph_get_weight",          L_EcsMorphGetWeight},
        {"set_morph_enabled",         L_EcsSetMorphEnabled},
    });
}

} // namespace dse::runtime::lua_binding
