/**
 * @file lua_binding_ecs_rendering_mesh.cpp
 * @brief MeshRenderer / Morph Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/sprite.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/ecs/components_3d_terrain_tile.h"
#include "engine/ecs/components_3d_navmesh.h"
#include "engine/ecs/components_3d_foliage.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/lut_loader.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/platform/screen.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <limits>


namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

// ============================================================
// MeshRenderer
// ============================================================

int L_EcsAddMeshRenderer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float r = helper::OptFloat(L, 2, 1.0f);
    float g = helper::OptFloat(L, 3, 1.0f);
    float b = helper::OptFloat(L, 4, 1.0f);
    float a = helper::OptFloat(L, 5, 1.0f);

    auto& mesh = world->registry().emplace_or_replace<MeshRendererComponent>(e);
    mesh.mesh_path.clear();
    mesh.color = glm::vec4(r, g, b, a);
    mesh.temp_vertices.clear();
    mesh.temp_indices.clear();
    mesh.temp_uvs.clear();
    mesh.temp_normals.clear();
    mesh.temp_tangents.clear();

    if (lua_istable(L, 6)) {
        int v_len = lua_rawlen(L, 6);
        for (int i = 1; i <= v_len; ++i) {
            lua_rawgeti(L, 6, i);
            mesh.temp_vertices.push_back(static_cast<float>(luaL_checknumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    const std::size_t vertex_count = mesh.temp_vertices.size() / 3;
    if (lua_istable(L, 7)) {
        int i_len = lua_rawlen(L, 7);
        for (int i = 1; i <= i_len; ++i) {
            lua_rawgeti(L, 7, i);
            const lua_Integer raw_index = luaL_checkinteger(L, -1);
            if (raw_index >= 0 &&
                static_cast<std::size_t>(raw_index) < vertex_count) {
                mesh.temp_indices.push_back(static_cast<uint32_t>(raw_index));
            }
            lua_pop(L, 1);
        }
    }
    return 0;
}

// set_mesh_path / get_mesh_path 已迁至 codegen（binding_defs.json，capi_setter:manual）→
// dse_mesh_renderer_set_mesh_path（含 temp_* 清理）

int L_EcsSetMeshMaterial(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh) return 0;

    // 检查第二个参数是否为字符串（dmat 路径）— 委托 dse_mesh_renderer_set_material_from_dmat
    if (lua_type(L, 2) == LUA_TSTRING) {
        const char* dmat_path = lua_tostring(L, 2);
        const uint32_t material_index = lua_gettop(L) >= 3 && lua_isinteger(L, 3)
            ? static_cast<uint32_t>(lua_tointeger(L, 3))
            : 0u;
        dse_mesh_renderer_set_material_from_dmat(EID(e), dmat_path, material_index);
        return 0;
    }

    // 标量参数路径
    mesh->metallic = helper::OptFloat(L, 2, mesh->metallic);
    mesh->roughness = helper::OptFloat(L, 3, mesh->roughness);
    mesh->ao = helper::OptFloat(L, 4, mesh->ao);
    float er = helper::OptFloat(L, 5, mesh->emissive.r);
    float eg = helper::OptFloat(L, 6, mesh->emissive.g);
    float eb = helper::OptFloat(L, 7, mesh->emissive.b);
    mesh->emissive = glm::vec3(er, eg, eb);
    mesh->normal_strength = helper::OptFloat(L, 8, mesh->normal_strength);
    if (lua_gettop(L) >= 9) {
        mesh->receive_shadow = helper::CheckBool(L, 9);
    }
    if (lua_gettop(L) >= 10) {
        mesh->material_double_sided = helper::CheckBool(L, 10);
    }
    // 可选 base_color 覆盖：用于 .dmat 之后的 Lua 材质创作恢复贴图的白色乘色，
    // 避免 .dmat base_color 与 material_albedo 双重相乘把 PBR 贴图压成灰色。
    if (lua_gettop(L) >= 13) {
        mesh->color.r = helper::CheckFloat(L, 11);
        mesh->color.g = helper::CheckFloat(L, 12);
        mesh->color.b = helper::CheckFloat(L, 13);
        if (lua_gettop(L) >= 14) {
            mesh->color.a = helper::CheckFloat(L, 14);
        }
    }
    return 0;
}

int L_EcsSetMeshDepthState(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh) return 0;
    mesh->depth_test_enabled = helper::CheckBool(L, 2);
    mesh->depth_write_enabled = lua_gettop(L) >= 3 ? helper::CheckBool(L, 3) : mesh->depth_write_enabled;
    return 0;
}

// set_mesh_shader_variant / get_mesh_shader_variant 已迁至 codegen（binding_defs.json）

int L_EcsSetMeshMaterialScalar(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* param_name = luaL_checkstring(L, 2);
    float value = helper::CheckFloat(L, 3);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh) return 0;
    std::string name(param_name);
    if (name == "metallic") mesh->metallic = value;
    else if (name == "roughness") mesh->roughness = value;
    else if (name == "ao") mesh->ao = value;
    else if (name == "normal_strength") mesh->normal_strength = value;
    else if (name == "material_alpha_cutoff") mesh->material_alpha_cutoff = value;
    else if (name == "sss_strength") mesh->sss_strength = value;
    else if (name == "clear_coat") mesh->clear_coat = value;
    else if (name == "clear_coat_roughness") mesh->clear_coat_roughness = value;
    else if (name == "anisotropy") mesh->anisotropy = value;
    else if (name == "pom_height_scale") mesh->pom_height_scale = value;

    // 直接 Lua 材质创作应覆盖复制的 .dmat/MaterialInstance 值
    mesh->material_data_source = MeshRendererComponent::MaterialDataSource::ComponentFallback;
    return 0;
}

int L_EcsSetMeshAdvancedMaterial(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh) return 0;
    mesh->clear_coat             = helper::OptFloat(L, 2, 0.0f);
    mesh->clear_coat_roughness   = helper::OptFloat(L, 3, 0.1f);
    mesh->anisotropy             = helper::OptFloat(L, 4, 0.0f);
    mesh->pom_height_scale       = helper::OptFloat(L, 5, 0.0f);
    mesh->sss_strength           = helper::OptFloat(L, 6, 0.0f);
    mesh->sss_tint               = glm::vec3(
        helper::OptFloat(L, 7, 0.0f),
        helper::OptFloat(L, 8, 0.0f),
        helper::OptFloat(L, 9, 0.0f));
    mesh->material_data_source = MeshRendererComponent::MaterialDataSource::ComponentFallback;
    return 0;
}

int L_EcsSetMeshTexture(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }

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
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh || !lua_istable(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    mesh->temp_uvs.clear();
    const int uv_len = lua_rawlen(L, 2);
    for (int i = 1; i <= uv_len; ++i) {
        lua_rawgeti(L, 2, i);
        mesh->temp_uvs.push_back(static_cast<float>(luaL_checknumber(L, -1)));
        lua_pop(L, 1);
    }

    const std::size_t vertex_count = mesh->temp_vertices.size() / 3;
    const bool ok = vertex_count > 0 && mesh->temp_uvs.size() == vertex_count * 2;
    lua_pushboolean(L, ok ? 1 : 0);
    helper::PushInt(L, static_cast<int>(mesh->temp_uvs.size() / 2));
    helper::PushInt(L, static_cast<int>(vertex_count));
    return 3;
}

int L_EcsSetMeshNormals(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh || !lua_istable(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    mesh->temp_normals.clear();
    const int normal_len = lua_rawlen(L, 2);
    for (int i = 1; i <= normal_len; ++i) {
        lua_rawgeti(L, 2, i);
        mesh->temp_normals.push_back(static_cast<float>(luaL_checknumber(L, -1)));
        lua_pop(L, 1);
    }

    const std::size_t vertex_count = mesh->temp_vertices.size() / 3;
    const bool ok = vertex_count > 0 && mesh->temp_normals.size() == vertex_count * 3;
    lua_pushboolean(L, ok ? 1 : 0);
    helper::PushInt(L, static_cast<int>(mesh->temp_normals.size() / 3));
    helper::PushInt(L, static_cast<int>(vertex_count));
    return 3;
}

int L_EcsSetMeshTangents(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh || !lua_istable(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    mesh->temp_tangents.clear();
    const int tangent_len = lua_rawlen(L, 2);
    for (int i = 1; i <= tangent_len; ++i) {
        lua_rawgeti(L, 2, i);
        mesh->temp_tangents.push_back(static_cast<float>(luaL_checknumber(L, -1)));
        lua_pop(L, 1);
    }

    const std::size_t vertex_count = mesh->temp_vertices.size() / 3;
    const bool ok = vertex_count > 0 && mesh->temp_tangents.size() == vertex_count * 3;
    lua_pushboolean(L, ok ? 1 : 0);
    helper::PushInt(L, static_cast<int>(mesh->temp_tangents.size() / 3));
    helper::PushInt(L, static_cast<int>(vertex_count));
    return 3;
}

// MeshRenderer emissive setter — 设置后标记数据源为 ComponentFallback
DSE_LUA_COMPONENT_SETTER_POST(MeshEmissive, MeshRendererComponent, emissive,
    glm::vec3(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4)),
    comp->material_data_source = MeshRendererComponent::MaterialDataSource::ComponentFallback)


// ============================================================
// MorphComponent 绑定
// ============================================================

// add_morph(entity)
int L_EcsAddMorph(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& morph = world->registry().emplace_or_replace<MorphComponent>(e);
    morph.enabled = true;
    return 0;
}

// morph_add_target(entity, name, weight)
int L_EcsMorphAddTarget(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* morph = helper::TryGetComponent<MorphComponent>(*world, e);
    if (!morph) return 0;
    MorphTarget target;
    target.name = helper::CheckString(L, 2);
    target.weight = helper::OptFloat(L, 3, 0.0f);
    morph->targets.push_back(target);
    return 0;
}

// morph_set_weight(entity, name_or_index, weight)
int L_EcsMorphSetWeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* morph = helper::TryGetComponent<MorphComponent>(*world, e);
    if (!morph) return 0;
    float weight = helper::CheckFloat(L, 3);
    if (lua_isinteger(L, 2)) {
        int idx = static_cast<int>(lua_tointeger(L, 2));
        if (idx >= 0 && idx < static_cast<int>(morph->targets.size())) {
            morph->targets[idx].weight = weight;
        }
    } else {
        const char* name = helper::CheckString(L, 2);
        for (auto& t : morph->targets) {
            if (t.name == name) { t.weight = weight; break; }
        }
    }
    return 0;
}

// morph_get_weight(entity, name_or_index) → weight
int L_EcsMorphGetWeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnumber(L, 0.0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* morph = helper::TryGetComponentConst<MorphComponent>(*world, e);
    if (!morph) { lua_pushnumber(L, 0.0); return 1; }
    if (lua_isinteger(L, 2)) {
        int idx = static_cast<int>(lua_tointeger(L, 2));
        if (idx >= 0 && idx < static_cast<int>(morph->targets.size())) {
            lua_pushnumber(L, morph->targets[idx].weight);
            return 1;
        }
    } else {
        const char* name = helper::CheckString(L, 2);
        for (const auto& t : morph->targets) {
            if (t.name == name) { lua_pushnumber(L, t.weight); return 1; }
        }
    }
    lua_pushnumber(L, 0.0);
    return 1;
}

DSE_LUA_COMPONENT_SETTER(MorphEnabled, MorphComponent, enabled, bool, helper::CheckBool(L, 2))


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
