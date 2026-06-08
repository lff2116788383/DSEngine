/**
 * @file lua_binding_ecs_rendering.cpp
 * @brief ECS Lua 绑定 — 渲染相关（Camera、Sprite、MeshRenderer、Light、Skybox、
 *        Terrain、PostProcess、Steering、FreeCameraController）
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
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

// ============================================================
// Camera（2D + 3D）
// ============================================================

int L_EcsAddCamera(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float ortho_size = helper::OptFloat(L, 2, 10.0f);
    int priority = helper::OptInt(L, 3, 0);
    auto& camera = world->registry().emplace_or_replace<CameraComponent>(e);
    camera.enabled = true;
    camera.priority = priority;
    camera.orthographic = true;
    camera.orthographic_size = ortho_size;
    return 0;
}

int L_EcsAddCamera3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float fov = helper::OptFloat(L, 2, 60.0f);
    int priority = helper::OptInt(L, 3, 0);
    auto& camera = world->registry().emplace_or_replace<Camera3DComponent>(e);
    camera.enabled = true;
    camera.priority = priority;
    camera.fov = fov;
    camera.near_clip = helper::OptFloat(L, 4, 0.1f);
    camera.far_clip = helper::OptFloat(L, 5, 1000.0f);
    if (camera.near_clip <= 0.0f) camera.near_clip = 0.1f;
    if (camera.far_clip <= camera.near_clip) camera.far_clip = camera.near_clip + 1000.0f;
    return 0;
}

int L_EcsSetCameraPriority(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int priority = helper::CheckInt(L, 2);
    if (auto* cam = helper::TryGetComponent<Camera3DComponent>(*world, e)) {
        cam->priority = priority;
    }
    if (auto* cam = helper::TryGetComponent<CameraComponent>(*world, e)) {
        cam->priority = priority;
    }
    return 0;
}

int L_EcsSetCameraEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool enabled = helper::CheckBool(L, 2);
    if (auto* cam = helper::TryGetComponent<Camera3DComponent>(*world, e)) {
        cam->enabled = enabled;
    }
    if (auto* cam = helper::TryGetComponent<CameraComponent>(*world, e)) {
        cam->enabled = enabled;
    }
    return 0;
}

int L_EcsSetCameraFollow(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity camera_entity = helper::CheckEntity(L, 1);
    Entity target_entity = helper::CheckEntity(L, 2);
    float damping = helper::OptFloat(L, 3, 0.12f);
    float dead_zone_x = helper::OptFloat(L, 4, 0.0f);
    float dead_zone_y = helper::OptFloat(L, 5, 0.0f);
    float offset_x = helper::OptFloat(L, 6, 0.0f);
    float offset_y = helper::OptFloat(L, 7, 0.0f);
    if (world->registry().valid(camera_entity)) {
        auto& follow = world->registry().emplace_or_replace<CameraFollowComponent>(camera_entity);
        follow.target = target_entity;
        follow.damping = damping;
        follow.dead_zone = glm::vec2(dead_zone_x, dead_zone_y);
        follow.offset = glm::vec3(offset_x, offset_y, 0.0f);
        follow.enabled = true;
    }
    return 0;
}

int L_EcsAddFreeCameraController(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& controller = world->registry().emplace_or_replace<FreeCameraControllerComponent>(e);
    controller.enabled = true;
    controller.move_speed = helper::OptFloat(L, 2, 5.0f);
    controller.mouse_sensitivity = helper::OptFloat(L, 3, 0.1f);
    return 0;
}

// ============================================================
// Sprite
// ============================================================

int L_EcsAddSprite(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float r = helper::OptFloat(L, 2, 1.0f);
    float g = helper::OptFloat(L, 3, 1.0f);
    float b = helper::OptFloat(L, 4, 1.0f);
    float a = helper::OptFloat(L, 5, 1.0f);
    int order = helper::OptInt(L, 6, 0);
    unsigned int texture_handle = static_cast<unsigned int>(helper::OptInt(L, 7, 0));
    auto& sprite = world->registry().emplace_or_replace<SpriteRendererComponent>(e);
    sprite.color = glm::vec4(r, g, b, a);
    sprite.order_in_layer = order;
    sprite.texture_handle = texture_handle;
    sprite.visible = true;
    return 0;
}

// Sprite vec2 字段 setter — 使用宏替代手写样板
DSE_LUA_COMPONENT_SETTER(SpriteUvScroll, SpriteRendererComponent, uv_scroll_speed, glm::vec2, helper::CheckVec2(L, 2))
DSE_LUA_COMPONENT_SETTER(SpriteUvOffset, SpriteRendererComponent, uv_offset, glm::vec2, helper::CheckVec2(L, 2))

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

int L_EcsSetMeshPath(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* mesh_path = luaL_checkstring(L, 2);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh) return 0;
    mesh->mesh_path = mesh_path;
    mesh->temp_vertices.clear();
    mesh->temp_indices.clear();
    mesh->temp_uvs.clear();
    mesh->temp_normals.clear();
    mesh->temp_tangents.clear();
    return 0;
}

int L_EcsSetMeshMaterial(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh) return 0;

    // 检查第二个参数是否为字符串（dmat 路径）
    if (lua_type(L, 2) == LUA_TSTRING) {
        std::string dmat_path = lua_tostring(L, 2);
        const std::size_t material_index = lua_gettop(L) >= 3 && lua_isinteger(L, 3)
            ? static_cast<std::size_t>(lua_tointeger(L, 3))
            : 0u;
        auto material = GetAssetManager().LoadMaterialInstanceFromDmat(dmat_path, material_index);
        if (material) {
            mesh->material_instance_id = material->GetId();
            mesh->material_data_source = MeshRendererComponent::MaterialDataSource::MaterialInstance;
            mesh->shader_variant = material->GetShaderVariant();
            mesh->color = material->GetBaseColor();
            mesh->emissive = material->GetEmissiveColor();
            mesh->albedo_texture_handle = material->GetTextureSlots().albedo;
            mesh->normal_texture_handle = material->GetTextureSlots().normal;
            mesh->metallic_roughness_texture_handle = material->GetTextureSlots().metallic_roughness;
            mesh->emissive_texture_handle = material->GetTextureSlots().emissive;
            mesh->occlusion_texture_handle = material->GetTextureSlots().occlusion;
            mesh->metallic = material->GetScalarOverrides().metallic;
            mesh->roughness = material->GetScalarOverrides().roughness;
            mesh->ao = material->GetScalarOverrides().ao;
            mesh->normal_strength = material->GetScalarOverrides().normal_strength;
            mesh->material_alpha_cutoff = material->GetScalarOverrides().alpha_cutoff;
            mesh->material_alpha_test = material->GetScalarOverrides().alpha_test;
            mesh->material_double_sided = material->GetRasterOverrides().double_sided;
        }
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

// MeshRenderer shader variant setter
DSE_LUA_COMPONENT_SETTER(MeshShaderVariant, MeshRendererComponent, shader_variant, std::string, std::string(helper::CheckString(L, 2)))

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
    const std::string slot = luaL_checkstring(L, 2);
    const char* texture_path = luaL_checkstring(L, 3);
    auto* mesh = helper::TryGetComponent<MeshRendererComponent>(*world, e);
    if (!mesh) {
        lua_pushboolean(L, 0);
        return 1;
    }

    auto texture = GetAssetManager().LoadTexture(texture_path);
    if (!texture) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const unsigned int handle = texture->GetHandle();
    if (slot == "albedo" || slot == "base_color" || slot == "diffuse") {
        mesh->albedo_texture_handle = handle;
    } else if (slot == "normal" || slot == "normal_map") {
        mesh->normal_texture_handle = handle;
    } else if (slot == "metallic_roughness" || slot == "roughness" || slot == "mr") {
        mesh->metallic_roughness_texture_handle = handle;
    } else if (slot == "emissive" || slot == "emission") {
        mesh->emissive_texture_handle = handle;
    } else if (slot == "occlusion" || slot == "ao") {
        mesh->occlusion_texture_handle = handle;
    } else {
        lua_pushboolean(L, 0);
        return 1;
    }

    mesh->material_data_source = MeshRendererComponent::MaterialDataSource::ComponentFallback;
    lua_pushboolean(L, 1);
    helper::PushInt(L, static_cast<int>(handle));
    helper::PushInt(L, texture->GetWidth());
    helper::PushInt(L, texture->GetHeight());
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
// Lights
// ============================================================

int L_EcsAddDirectionalLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float dir_x = helper::OptFloat(L, 2, -0.4f);
    float dir_y = helper::OptFloat(L, 3, -1.0f);
    float dir_z = helper::OptFloat(L, 4, -0.3f);
    float r = helper::OptFloat(L, 5, 1.0f);
    float g = helper::OptFloat(L, 6, 1.0f);
    float b = helper::OptFloat(L, 7, 1.0f);
    float intensity = helper::OptFloat(L, 8, 1.0f);
    float ambient_intensity = helper::OptFloat(L, 9, 0.2f);
    float shadow_strength = helper::OptFloat(L, 10, 0.35f);
    auto& light = world->registry().emplace_or_replace<DirectionalLight3DComponent>(e);
    light.enabled = true;
    light.direction = glm::normalize(glm::vec3(dir_x, dir_y, dir_z));
    light.color = glm::vec3(r, g, b);
    light.intensity = intensity;
    light.ambient_intensity = ambient_intensity;
    light.shadow_strength = shadow_strength;
    return 0;
}

int L_EcsSetDirectionalLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* light = helper::TryGetComponent<DirectionalLight3DComponent>(*world, e);
    if (!light) return 0;
    if (lua_gettop(L) >= 2) {
        light->enabled = helper::CheckBool(L, 2);
    }
    float dir_x = helper::OptFloat(L, 3, light->direction.x);
    float dir_y = helper::OptFloat(L, 4, light->direction.y);
    float dir_z = helper::OptFloat(L, 5, light->direction.z);
    light->direction = glm::normalize(glm::vec3(dir_x, dir_y, dir_z));
    light->color.r = helper::OptFloat(L, 6, light->color.r);
    light->color.g = helper::OptFloat(L, 7, light->color.g);
    light->color.b = helper::OptFloat(L, 8, light->color.b);
    light->intensity = helper::OptFloat(L, 9, light->intensity);
    light->ambient_intensity = helper::OptFloat(L, 10, light->ambient_intensity);
    light->shadow_strength = helper::OptFloat(L, 11, light->shadow_strength);
    return 0;
}

int L_EcsSetDirectionalLightShadow(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* light = helper::TryGetComponent<DirectionalLight3DComponent>(*world, e);
    if (!light) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!lua_isnoneornil(L, 2)) {
        light->cast_shadow = helper::CheckBool(L, 2);
    }
    light->shadow_strength = std::clamp(helper::OptFloat(L, 3, light->shadow_strength), 0.0f, 1.0f);
    const float c0 = helper::OptFloat(L, 4, light->cascade_splits[0]);
    const float c1 = helper::OptFloat(L, 5, light->cascade_splits[1]);
    const float c2 = helper::OptFloat(L, 6, light->cascade_splits[2]);
    light->cascade_splits[0] = std::max(0.1f, c0);
    light->cascade_splits[1] = std::max(light->cascade_splits[0] + 0.1f, c1);
    light->cascade_splits[2] = std::max(light->cascade_splits[1] + 0.1f, c2);
    light->cascade_split_lambda = std::clamp(helper::OptFloat(L, 7, light->cascade_split_lambda), 0.0f, 1.0f);

    lua_pushboolean(L, 1);
    helper::PushBool(L, light->cast_shadow);
    helper::PushFloat(L, light->shadow_strength);
    helper::PushFloat(L, light->cascade_splits[0]);
    helper::PushFloat(L, light->cascade_splits[1]);
    helper::PushFloat(L, light->cascade_splits[2]);
    helper::PushFloat(L, light->cascade_split_lambda);
    return 7;
}

int L_EcsAddPointLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float r = helper::OptFloat(L, 2, 1.0f);
    float g = helper::OptFloat(L, 3, 1.0f);
    float b = helper::OptFloat(L, 4, 1.0f);
    float intensity = helper::OptFloat(L, 5, 1.0f);
    float radius = helper::OptFloat(L, 6, 10.0f);
    auto& light = world->registry().emplace_or_replace<PointLightComponent>(e);
    light.enabled = true;
    light.color = glm::vec3(r, g, b);
    light.intensity = intensity;
    light.radius = radius;
    return 0;
}

/** 设置 PointLight 阴影参数
 *  @param entity    灯光实体
 *  @param cast_shadow  是否投射阴影（bool，默认 true）
 *  @return bool 是否设置成功
 */
int L_EcsSetPointLightShadow(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* light = helper::TryGetComponent<PointLightComponent>(*world, e);
    if (!light) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!lua_isnoneornil(L, 2)) {
        light->cast_shadow = helper::CheckBool(L, 2);
    }
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsAddSpotLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float dir_x = helper::OptFloat(L, 2, 0.0f);
    float dir_y = helper::OptFloat(L, 3, -1.0f);
    float dir_z = helper::OptFloat(L, 4, 0.0f);
    float r = helper::OptFloat(L, 5, 1.0f);
    float g = helper::OptFloat(L, 6, 1.0f);
    float b = helper::OptFloat(L, 7, 1.0f);
    float intensity = helper::OptFloat(L, 8, 1.0f);
    float radius = helper::OptFloat(L, 9, 20.0f);
    float inner_angle = helper::OptFloat(L, 10, 12.5f);
    float outer_angle = helper::OptFloat(L, 11, 17.5f);
    auto& light = world->registry().emplace_or_replace<SpotLightComponent>(e);
    light.enabled = true;
    light.direction = glm::normalize(glm::vec3(dir_x, dir_y, dir_z));
    light.color = glm::vec3(r, g, b);
    light.intensity = intensity;
    light.radius = radius;
    light.inner_cone_angle = inner_angle;
    light.outer_cone_angle = outer_angle;
    return 0;
}

// set_point_light_3d(entity, r, g, b, intensity, radius)
int L_EcsSetPointLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* light = helper::TryGetComponent<PointLightComponent>(*world, e);
    if (!light) return 0;
    if (!lua_isnoneornil(L, 2)) light->color.r = helper::CheckFloat(L, 2);
    if (!lua_isnoneornil(L, 3)) light->color.g = helper::CheckFloat(L, 3);
    if (!lua_isnoneornil(L, 4)) light->color.b = helper::CheckFloat(L, 4);
    if (!lua_isnoneornil(L, 5)) light->intensity = helper::CheckFloat(L, 5);
    if (!lua_isnoneornil(L, 6)) light->radius = helper::CheckFloat(L, 6);
    return 0;
}

// set_spot_light_3d(entity, dir_x, dir_y, dir_z, r, g, b, intensity, radius, inner_angle, outer_angle)
int L_EcsSetSpotLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* light = helper::TryGetComponent<SpotLightComponent>(*world, e);
    if (!light) return 0;
    if (!lua_isnoneornil(L, 2) && !lua_isnoneornil(L, 3) && !lua_isnoneornil(L, 4)) {
        light->direction = glm::normalize(glm::vec3(
            helper::CheckFloat(L, 2),
            helper::CheckFloat(L, 3),
            helper::CheckFloat(L, 4)));
    }
    if (!lua_isnoneornil(L, 5)) light->color.r = helper::CheckFloat(L, 5);
    if (!lua_isnoneornil(L, 6)) light->color.g = helper::CheckFloat(L, 6);
    if (!lua_isnoneornil(L, 7)) light->color.b = helper::CheckFloat(L, 7);
    if (!lua_isnoneornil(L, 8)) light->intensity = helper::CheckFloat(L, 8);
    if (!lua_isnoneornil(L, 9)) light->radius = helper::CheckFloat(L, 9);
    if (!lua_isnoneornil(L, 10)) light->inner_cone_angle = helper::CheckFloat(L, 10);
    if (!lua_isnoneornil(L, 11)) light->outer_cone_angle = helper::CheckFloat(L, 11);
    return 0;
}

// set_spot_light_shadow(entity, cast_shadow)
int L_EcsSetSpotLightShadow(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    auto* light = helper::TryGetComponent<SpotLightComponent>(*world, e);
    if (!light) { lua_pushboolean(L, 0); return 1; }
    if (!lua_isnoneornil(L, 2)) {
        light->cast_shadow = helper::CheckBool(L, 2);
    }
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsAddSkyLight(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& light = world->registry().emplace_or_replace<SkyLightComponent>(e);
    light.enabled = true;
    light.up_color = glm::vec3(
        helper::OptFloat(L, 2, 0.22f),
        helper::OptFloat(L, 3, 0.28f),
        helper::OptFloat(L, 4, 0.38f));
    light.down_color = glm::vec3(
        helper::OptFloat(L, 5, 0.04f),
        helper::OptFloat(L, 6, 0.05f),
        helper::OptFloat(L, 7, 0.08f));
    light.intensity = helper::OptFloat(L, 8, 1.0f);
    return 0;
}

int L_EcsSetSkyLight(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* light = helper::TryGetComponent<SkyLightComponent>(*world, e);
    if (!light) return 0;
    light->up_color = glm::vec3(
        helper::OptFloat(L, 2, light->up_color.r),
        helper::OptFloat(L, 3, light->up_color.g),
        helper::OptFloat(L, 4, light->up_color.b));
    light->down_color = glm::vec3(
        helper::OptFloat(L, 5, light->down_color.r),
        helper::OptFloat(L, 6, light->down_color.g),
        helper::OptFloat(L, 7, light->down_color.b));
    light->intensity = helper::OptFloat(L, 8, light->intensity);
    light->enabled = lua_isnoneornil(L, 9) ? light->enabled : helper::CheckBool(L, 9);
    return 0;
}

// ============================================================
// Skybox
// ============================================================

int L_EcsAddSkybox(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* cubemap_path = luaL_optstring(L, 2, "");
    auto& skybox = world->registry().emplace_or_replace<SkyboxComponent>(e);
    skybox.cubemap_path = cubemap_path;
    skybox.enabled = true;
    return 0;
}

// ============================================================
// Terrain
// ============================================================

void SampleTerrainHeightmap(TerrainComponent& terrain, const std::vector<unsigned char>& pixels,
                            int image_width, int image_height) {
    if (image_width <= 0 || image_height <= 0 || terrain.resolution_x < 2 || terrain.resolution_z < 2) {
        return;
    }

    terrain.height_data.assign(static_cast<std::size_t>(terrain.resolution_x * terrain.resolution_z), 0.0f);
    for (int z = 0; z < terrain.resolution_z; ++z) {
        const float v = terrain.resolution_z == 1 ? 0.0f : static_cast<float>(z) / static_cast<float>(terrain.resolution_z - 1);
        const int src_z = std::clamp(static_cast<int>(std::round(v * static_cast<float>(image_height - 1))), 0, image_height - 1);
        for (int x = 0; x < terrain.resolution_x; ++x) {
            const float u = terrain.resolution_x == 1 ? 0.0f : static_cast<float>(x) / static_cast<float>(terrain.resolution_x - 1);
            const int src_x = std::clamp(static_cast<int>(std::round(u * static_cast<float>(image_width - 1))), 0, image_width - 1);
            const std::size_t index = (static_cast<std::size_t>(src_z) * static_cast<std::size_t>(image_width) + static_cast<std::size_t>(src_x)) * 4u;
            if (index + 2 >= pixels.size()) continue;
            const float r = static_cast<float>(pixels[index + 0]) / 255.0f;
            const float g = static_cast<float>(pixels[index + 1]) / 255.0f;
            const float b = static_cast<float>(pixels[index + 2]) / 255.0f;
            const float luminance = r * 0.2126f + g * 0.7152f + b * 0.0722f;
            terrain.height_data[static_cast<std::size_t>(z * terrain.resolution_x + x)] = luminance * terrain.max_height;
        }
    }
    terrain.heightmap_width = image_width;
    terrain.heightmap_height = image_height;
}

bool LoadTerrainHeightmap(TerrainComponent& terrain, const std::string& heightmap_path) {
    if (heightmap_path.empty()) {
        terrain.heightmap_width = 0;
        terrain.heightmap_height = 0;
        terrain.heightmap_channels = 0;
        return false;
    }

    std::vector<unsigned char> pixels;
    int image_width = 0;
    int image_height = 0;
    int image_channels = 0;
    if (!GetAssetManager().LoadImageRgba(heightmap_path, pixels, image_width, image_height, image_channels)) {
        return false;
    }

    terrain.heightmap_path = heightmap_path;
    terrain.heightmap_channels = image_channels;
    SampleTerrainHeightmap(terrain, pixels, image_width, image_height);
    terrain.is_dirty = true;
    return true;
}

int L_EcsAddTerrain(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* heightmap_path = luaL_optstring(L, 2, "");
    float width = helper::OptFloat(L, 3, 100.0f);
    float depth = helper::OptFloat(L, 4, 100.0f);
    float max_height = helper::OptFloat(L, 5, 20.0f);
    auto& terrain = world->registry().emplace_or_replace<TerrainComponent>(e);
    terrain.enabled = true;
    terrain.heightmap_path = heightmap_path;
    terrain.width = width;
    terrain.depth = depth;
    terrain.max_height = max_height;
    if (heightmap_path[0] != '\0') {
        LoadTerrainHeightmap(terrain, heightmap_path);
    }
    terrain.is_dirty = true;
    return 0;
}

int L_EcsSetTerrainParams(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain) return 0;
    terrain->resolution_x = std::max(2, helper::OptInt(L, 2, terrain->resolution_x));
    terrain->resolution_z = std::max(2, helper::OptInt(L, 3, terrain->resolution_z));
    terrain->max_lod_levels = std::max(1, helper::OptInt(L, 4, terrain->max_lod_levels));
    terrain->lod_distance_factor = std::max(0.1f, helper::OptFloat(L, 5, terrain->lod_distance_factor));
    if (lua_gettop(L) >= 6) {
        terrain->use_dynamic_lod = helper::CheckBool(L, 6);
    }
    terrain->height_data.assign(static_cast<std::size_t>(terrain->resolution_x * terrain->resolution_z), 0.0f);
    if (!terrain->heightmap_path.empty()) {
        LoadTerrainHeightmap(*terrain, terrain->heightmap_path);
    }
    terrain->current_lod = std::clamp(terrain->current_lod, 0, terrain->max_lod_levels - 1);
    terrain->is_dirty = true;
    return 0;
}

int L_EcsSetTerrainHeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int x = helper::CheckInt(L, 2);
    int z = helper::CheckInt(L, 3);
    float height = helper::CheckFloat(L, 4);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain) return 0;
    if (terrain->resolution_x < 2 || terrain->resolution_z < 2) return 0;
    if (terrain->height_data.size() != static_cast<std::size_t>(terrain->resolution_x * terrain->resolution_z)) {
        terrain->height_data.assign(static_cast<std::size_t>(terrain->resolution_x * terrain->resolution_z), 0.0f);
    }
    if (x < 0 || z < 0 || x >= terrain->resolution_x || z >= terrain->resolution_z) return 0;
    terrain->height_data[static_cast<std::size_t>(z * terrain->resolution_x + x)] = height;
    terrain->is_dirty = true;
    return 0;
}

int L_EcsLoadTerrainHeightmap(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const char* heightmap_path = luaL_checkstring(L, 2);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const bool ok = LoadTerrainHeightmap(*terrain, heightmap_path);
    lua_pushboolean(L, ok ? 1 : 0);
    if (ok) {
        helper::PushInt(L, terrain->heightmap_width);
        helper::PushInt(L, terrain->heightmap_height);
        helper::PushInt(L, terrain->heightmap_channels);
        helper::PushInt(L, terrain->resolution_x);
        helper::PushInt(L, terrain->resolution_z);
        return 6;
    }
    return 1;
}

int L_EcsSetTerrainTexture(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const char* texture_path = luaL_checkstring(L, 2);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain) {
        lua_pushboolean(L, 0);
        return 1;
    }

    auto texture = GetAssetManager().LoadTexture(texture_path);
    if (!texture) {
        lua_pushboolean(L, 0);
        return 1;
    }

    terrain->texture_path = texture_path;
    terrain->texture_handle = texture->GetHandle();
    terrain->is_dirty = true;
    lua_pushboolean(L, 1);
    helper::PushInt(L, static_cast<int>(terrain->texture_handle));
    helper::PushInt(L, texture->GetWidth());
    helper::PushInt(L, texture->GetHeight());
    return 4;
}

int L_EcsGetTerrainLod(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const auto* terrain = helper::TryGetComponentConst<TerrainComponent>(*world, e);
    if (!terrain) return 0;
    helper::PushInt(L, terrain->current_lod);
    helper::PushInt(L, terrain->resolution_x);
    helper::PushInt(L, terrain->resolution_z);
    helper::PushInt(L, terrain->max_lod_levels);
    helper::PushFloat(L, terrain->lod_distance_factor);
    return 5;
}

int L_EcsSampleTerrainHeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float wx = helper::CheckFloat(L, 2);
    float wz = helper::CheckFloat(L, 3);
    const auto* terrain = helper::TryGetComponentConst<TerrainComponent>(*world, e);
    const auto* transform = helper::TryGetComponentConst<TransformComponent>(*world, e);
    if (!terrain || !transform) {
        helper::PushFloat(L, 0.0f);
        return 1;
    }
    float h = dse::SampleTerrainHeight(*terrain, *transform, wx, wz);
    helper::PushFloat(L, h);
    return 1;
}

int L_EcsSetTerrainSplatTexture(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int layer = helper::CheckInt(L, 2);
    const char* path = luaL_checkstring(L, 3);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain || layer < 0 || layer > 3) {
        lua_pushboolean(L, 0);
        return 1;
    }
    auto tex = GetAssetManager().LoadTexture(path);
    if (!tex) { lua_pushboolean(L, 0); return 1; }
    terrain->splat_texture_paths[layer] = path;
    terrain->splat_texture_handles[layer] = tex->GetHandle();
    terrain->splat_dirty = true;
    lua_pushboolean(L, 1);
    return 1;
}

// ============================================================
// PostProcess
// ============================================================

int L_EcsAddPostProcess(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& pp = world->registry().emplace_or_replace<PostProcessComponent>(e);
    pp.enabled = true;
    pp.bloom_enabled = helper::CheckBool(L, 2);
    pp.bloom_threshold = helper::OptFloat(L, 3, 1.0f);
    pp.bloom_intensity = helper::OptFloat(L, 4, 1.0f);
    pp.exposure = helper::OptFloat(L, 5, pp.exposure);
    return 0;
}

int L_EcsSetPostProcessBloom(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    pp->enabled = lua_isnoneornil(L, 2) ? pp->enabled : helper::CheckBool(L, 2);
    pp->bloom_enabled = lua_isnoneornil(L, 3) ? pp->bloom_enabled : helper::CheckBool(L, 3);
    pp->bloom_threshold = helper::OptFloat(L, 4, pp->bloom_threshold);
    pp->bloom_intensity = helper::OptFloat(L, 5, pp->bloom_intensity);
    pp->exposure = helper::OptFloat(L, 6, pp->exposure);
    pp->bloom_knee = helper::OptFloat(L, 7, pp->bloom_knee);
    pp->bloom_mip_weight = helper::OptFloat(L, 8, pp->bloom_mip_weight);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessColor(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    pp->color_grading_enabled = lua_isnoneornil(L, 2) ? pp->color_grading_enabled : helper::CheckBool(L, 2);
    pp->exposure = helper::OptFloat(L, 3, pp->exposure);
    pp->gamma = helper::OptFloat(L, 4, pp->gamma);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessSSAO(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    pp->ssao_enabled = lua_isnoneornil(L, 2) ? pp->ssao_enabled : helper::CheckBool(L, 2);
    pp->ssao_radius = helper::OptFloat(L, 3, pp->ssao_radius);
    pp->ssao_bias = helper::OptFloat(L, 4, pp->ssao_bias);
    pp->ssao_sample_count = static_cast<int>(helper::OptFloat(L, 5, static_cast<float>(pp->ssao_sample_count)));
    pp->ssao_power = helper::OptFloat(L, 6, pp->ssao_power);
    pp->ssao_intensity = helper::OptFloat(L, 7, pp->ssao_intensity);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessSSR(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    pp->ssr_enabled = lua_isnoneornil(L, 2) ? pp->ssr_enabled : helper::CheckBool(L, 2);
    pp->ssr_max_distance = helper::OptFloat(L, 3, pp->ssr_max_distance);
    pp->ssr_fade_distance = helper::OptFloat(L, 4, pp->ssr_fade_distance);
    pp->ssr_max_roughness = helper::OptFloat(L, 5, pp->ssr_max_roughness);
    pp->ssr_thickness = helper::OptFloat(L, 6, pp->ssr_thickness);
    pp->ssr_step_size = helper::OptFloat(L, 7, pp->ssr_step_size);
    pp->ssr_max_steps = static_cast<int>(helper::OptFloat(L, 8, static_cast<float>(pp->ssr_max_steps)));
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessFXAA(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    pp->fxaa_enabled = lua_isnoneornil(L, 2) ? pp->fxaa_enabled : helper::CheckBool(L, 2);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessAutoExposure(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    pp->auto_exposure_enabled = lua_isnoneornil(L, 2) ? pp->auto_exposure_enabled : helper::CheckBool(L, 2);
    pp->exposure_min = helper::OptFloat(L, 3, pp->exposure_min);
    pp->exposure_max = helper::OptFloat(L, 4, pp->exposure_max);
    pp->adaptation_speed_up = helper::OptFloat(L, 5, pp->adaptation_speed_up);
    pp->adaptation_speed_down = helper::OptFloat(L, 6, pp->adaptation_speed_down);
    pp->exposure_compensation = helper::OptFloat(L, 7, pp->exposure_compensation);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessVignette(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    pp->vignette_enabled = lua_isnoneornil(L, 2) ? pp->vignette_enabled : helper::CheckBool(L, 2);
    pp->vignette_intensity = helper::OptFloat(L, 3, pp->vignette_intensity);
    pp->vignette_radius = helper::OptFloat(L, 4, pp->vignette_radius);
    pp->vignette_softness = helper::OptFloat(L, 5, pp->vignette_softness);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessFilmGrain(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    pp->film_grain_enabled = lua_isnoneornil(L, 2) ? pp->film_grain_enabled : helper::CheckBool(L, 2);
    pp->film_grain_intensity = helper::OptFloat(L, 3, pp->film_grain_intensity);
    pp->film_grain_time_scale = helper::OptFloat(L, 4, pp->film_grain_time_scale);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessColorLut(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const char* path = luaL_optstring(L, 2, nullptr);
    float intensity = helper::OptFloat(L, 3, 1.0f);
    pp->color_lut_intensity = intensity;
    if (path) {
        auto& am = GetAssetManager();
        RhiDevice* rhi = am.rhi_device();
        if (rhi) {
            // 释放旧的 LUT 纹理
            if (pp->color_lut_handle != 0) {
                rhi->DeleteTexture(pp->color_lut_handle);
                pp->color_lut_handle = 0;
            }
            std::string full_path = am.ResolveAssetPath(path);
            dse::assets::LutData lut;
            if (dse::assets::LoadCubeLut(full_path, lut) && lut.size > 0 && !lut.rgba8.empty()) {
                pp->color_lut_handle = rhi->CreateTexture3D(lut.size, lut.size, lut.size, lut.rgba8.data(), true);
            }
        }
    } else {
        // 清除 LUT 时也释放旧纹理
        auto& am = GetAssetManager();
        RhiDevice* rhi = am.rhi_device();
        if (rhi && pp->color_lut_handle != 0) {
            rhi->DeleteTexture(pp->color_lut_handle);
        }
        pp->color_lut_handle = 0;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessOutline(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    pp->outline_enabled = lua_isnoneornil(L, 2) ? pp->outline_enabled : helper::CheckBool(L, 2);
    pp->outline_color.r = helper::OptFloat(L, 3, pp->outline_color.r);
    pp->outline_color.g = helper::OptFloat(L, 4, pp->outline_color.g);
    pp->outline_color.b = helper::OptFloat(L, 5, pp->outline_color.b);
    pp->outline_thickness = helper::OptFloat(L, 6, pp->outline_thickness);
    pp->outline_depth_threshold = helper::OptFloat(L, 7, pp->outline_depth_threshold);
    pp->outline_normal_threshold = helper::OptFloat(L, 8, pp->outline_normal_threshold);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessFog(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) { lua_pushboolean(L, 0); return 1; }
    pp->fog_enabled        = lua_isnoneornil(L, 2)  ? pp->fog_enabled        : helper::CheckBool(L, 2);
    pp->fog_density        = helper::OptFloat(L,  3, pp->fog_density);
    pp->fog_height_falloff = helper::OptFloat(L,  4, pp->fog_height_falloff);
    pp->fog_height_offset  = helper::OptFloat(L,  5, pp->fog_height_offset);
    pp->fog_start          = helper::OptFloat(L,  6, pp->fog_start);
    pp->fog_end            = helper::OptFloat(L,  7, pp->fog_end);
    if (!lua_isnoneornil(L, 8)) pp->fog_steps = static_cast<int>(luaL_checknumber(L, 8));
    pp->fog_sun_scatter    = helper::OptFloat(L,  9, pp->fog_sun_scatter);
    pp->fog_color.r        = helper::OptFloat(L, 10, pp->fog_color.r);
    pp->fog_color.g        = helper::OptFloat(L, 11, pp->fog_color.g);
    pp->fog_color.b        = helper::OptFloat(L, 12, pp->fog_color.b);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetPostProcessLightShaft(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    auto* pp = helper::TryGetComponent<PostProcessComponent>(*world, e);
    if (!pp) { lua_pushboolean(L, 0); return 1; }
    pp->light_shaft_enabled   = lua_isnoneornil(L, 2) ? pp->light_shaft_enabled : helper::CheckBool(L, 2);
    pp->light_shaft_density   = helper::OptFloat(L, 3, pp->light_shaft_density);
    pp->light_shaft_weight    = helper::OptFloat(L, 4, pp->light_shaft_weight);
    pp->light_shaft_decay     = helper::OptFloat(L, 5, pp->light_shaft_decay);
    pp->light_shaft_exposure  = helper::OptFloat(L, 6, pp->light_shaft_exposure);
    pp->light_shaft_intensity = helper::OptFloat(L, 7, pp->light_shaft_intensity);
    if (!lua_isnoneornil(L, 8)) pp->light_shaft_samples = static_cast<int>(luaL_checknumber(L, 8));
    pp->light_shaft_color.r   = helper::OptFloat(L, 9,  pp->light_shaft_color.r);
    pp->light_shaft_color.g   = helper::OptFloat(L, 10, pp->light_shaft_color.g);
    pp->light_shaft_color.b   = helper::OptFloat(L, 11, pp->light_shaft_color.b);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsAddDecal(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    if (!world->registry().all_of<dse::DecalComponent>(e))
        world->registry().emplace<dse::DecalComponent>(e);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetDecal(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    auto* dc = helper::TryGetComponent<dse::DecalComponent>(*world, e);
    if (!dc) { lua_pushboolean(L, 0); return 1; }
    dc->enabled        = lua_isnoneornil(L, 2) ? dc->enabled : helper::CheckBool(L, 2);
    if (!lua_isnoneornil(L, 3)) dc->albedo_texture = static_cast<unsigned int>(luaL_checknumber(L, 3));
    dc->color.r        = helper::OptFloat(L, 4, dc->color.r);
    dc->color.g        = helper::OptFloat(L, 5, dc->color.g);
    dc->color.b        = helper::OptFloat(L, 6, dc->color.b);
    dc->color.a        = helper::OptFloat(L, 7, dc->color.a);
    dc->angle_fade     = helper::OptFloat(L, 8, dc->angle_fade);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsGetPostProcessState(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const auto* pp = helper::TryGetComponentConst<PostProcessComponent>(*world, e);
    if (!pp) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    helper::PushBool(L, pp->enabled);
    helper::PushBool(L, pp->bloom_enabled);
    helper::PushFloat(L, pp->bloom_threshold);
    helper::PushFloat(L, pp->bloom_intensity);
    helper::PushBool(L, pp->color_grading_enabled);
    helper::PushFloat(L, pp->exposure);
    helper::PushFloat(L, pp->gamma);
    helper::PushBool(L, pp->ssao_enabled);
    helper::PushFloat(L, pp->ssao_radius);
    helper::PushFloat(L, pp->ssao_bias);
    helper::PushBool(L, pp->fxaa_enabled);
    helper::PushBool(L, pp->vignette_enabled);
    helper::PushFloat(L, pp->vignette_intensity);
    helper::PushFloat(L, pp->vignette_radius);
    helper::PushFloat(L, pp->vignette_softness);
    helper::PushBool(L, pp->film_grain_enabled);
    helper::PushFloat(L, pp->film_grain_intensity);
    helper::PushFloat(L, pp->film_grain_time_scale);
    return 19;
}

// ============================================================
// Water
// ============================================================

int L_EcsAddWater(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    if (!world->registry().all_of<dse::WaterComponent>(e))
        world->registry().emplace<dse::WaterComponent>(e);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetWater(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    auto* wc = helper::TryGetComponent<dse::WaterComponent>(*world, e);
    if (!wc) { lua_pushboolean(L, 0); return 1; }
    wc->enabled             = lua_isnoneornil(L, 2) ? wc->enabled : helper::CheckBool(L, 2);
    wc->water_level         = helper::OptFloat(L, 3, wc->water_level);
    wc->deep_color.r        = helper::OptFloat(L, 4, wc->deep_color.r);
    wc->deep_color.g        = helper::OptFloat(L, 5, wc->deep_color.g);
    wc->deep_color.b        = helper::OptFloat(L, 6, wc->deep_color.b);
    wc->shallow_color.r     = helper::OptFloat(L, 7, wc->shallow_color.r);
    wc->shallow_color.g     = helper::OptFloat(L, 8, wc->shallow_color.g);
    wc->shallow_color.b     = helper::OptFloat(L, 9, wc->shallow_color.b);
    wc->max_depth           = helper::OptFloat(L, 10, wc->max_depth);
    wc->transparency        = helper::OptFloat(L, 11, wc->transparency);
    wc->wave_amplitude      = helper::OptFloat(L, 12, wc->wave_amplitude);
    wc->wave_frequency      = helper::OptFloat(L, 13, wc->wave_frequency);
    wc->wave_speed          = helper::OptFloat(L, 14, wc->wave_speed);
    wc->wave_direction.x    = helper::OptFloat(L, 15, wc->wave_direction.x);
    wc->wave_direction.y    = helper::OptFloat(L, 16, wc->wave_direction.y);
    wc->refraction_strength = helper::OptFloat(L, 17, wc->refraction_strength);
    wc->reflection_strength = helper::OptFloat(L, 18, wc->reflection_strength);
    wc->specular_power      = helper::OptFloat(L, 19, wc->specular_power);
    wc->caustic_intensity   = helper::OptFloat(L, 20, wc->caustic_intensity);
    wc->caustic_scale       = helper::OptFloat(L, 21, wc->caustic_scale);
    wc->foam_intensity      = helper::OptFloat(L, 22, wc->foam_intensity);
    wc->foam_depth_threshold = helper::OptFloat(L, 23, wc->foam_depth_threshold);
    wc->underwater_fog_density = helper::OptFloat(L, 24, wc->underwater_fog_density);
    wc->underwater_fog_color.r = helper::OptFloat(L, 25, wc->underwater_fog_color.r);
    wc->underwater_fog_color.g = helper::OptFloat(L, 26, wc->underwater_fog_color.g);
    wc->underwater_fog_color.b = helper::OptFloat(L, 27, wc->underwater_fog_color.b);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsGetWater(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* wc = helper::TryGetComponentConst<dse::WaterComponent>(*world, e);
    if (!wc) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, 1);
    helper::PushBool(L, wc->enabled);
    helper::PushFloat(L, wc->water_level);
    helper::PushFloat(L, wc->deep_color.r);
    helper::PushFloat(L, wc->deep_color.g);
    helper::PushFloat(L, wc->deep_color.b);
    helper::PushFloat(L, wc->shallow_color.r);
    helper::PushFloat(L, wc->shallow_color.g);
    helper::PushFloat(L, wc->shallow_color.b);
    helper::PushFloat(L, wc->max_depth);
    helper::PushFloat(L, wc->transparency);
    helper::PushFloat(L, wc->wave_amplitude);
    helper::PushFloat(L, wc->wave_frequency);
    helper::PushFloat(L, wc->wave_speed);
    helper::PushFloat(L, wc->wave_direction.x);
    helper::PushFloat(L, wc->wave_direction.y);
    helper::PushFloat(L, wc->refraction_strength);
    helper::PushFloat(L, wc->reflection_strength);
    helper::PushFloat(L, wc->specular_power);
    return 19;
}

// ============================================================
// Steering
// ============================================================

int L_EcsAddSteering(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float max_vel = helper::OptFloat(L, 2, 5.0f);
    float max_force = helper::OptFloat(L, 3, 10.0f);
    float mass = helper::OptFloat(L, 4, 1.0f);
    auto& steering = world->registry().emplace_or_replace<SteeringComponent>(e);
    steering.enabled = true;
    steering.max_velocity = max_vel;
    steering.max_force = max_force;
    steering.mass = mass;
    return 0;
}

int L_EcsSetSteeringTarget(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const char* behavior = luaL_checkstring(L, 2);
    float tx = helper::CheckFloat(L, 3);
    float ty = helper::CheckFloat(L, 4);
    float tz = helper::CheckFloat(L, 5);
    auto* steering = helper::TryGetComponent<SteeringComponent>(*world, e);
    if (!steering) {
        lua_pushboolean(L, 0);
        return 1;
    }
    std::string b = behavior;
    if (b == "seek") {
        steering->seek_enabled = true;
        steering->flee_enabled = false;
        steering->arrive_enabled = false;
        steering->seek_target = glm::vec3(tx, ty, tz);
        lua_pushboolean(L, 1);
        return 1;
    } else if (b == "flee") {
        steering->seek_enabled = false;
        steering->flee_enabled = true;
        steering->arrive_enabled = false;
        steering->flee_target = glm::vec3(tx, ty, tz);
        lua_pushboolean(L, 1);
        return 1;
    } else if (b == "arrive") {
        steering->seek_enabled = false;
        steering->flee_enabled = false;
        steering->arrive_enabled = true;
        steering->arrive_target = glm::vec3(tx, ty, tz);
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

int L_EcsGetSteeringState(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const auto* steering = helper::TryGetComponentConst<SteeringComponent>(*world, e);
    if (!steering) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    helper::PushBool(L, steering->enabled);
    helper::PushBool(L, steering->seek_enabled);
    helper::PushBool(L, steering->flee_enabled);
    helper::PushBool(L, steering->arrive_enabled);
    helper::PushVec3(L, steering->velocity);
    helper::PushFloat(L, glm::length(steering->velocity));
    helper::PushFloat(L, steering->max_velocity);
    helper::PushFloat(L, steering->max_force);
    helper::PushFloat(L, steering->mass);
    helper::PushFloat(L, steering->arrive_deceleration_radius);
    helper::PushVec3(L, steering->seek_target);
    helper::PushVec3(L, steering->flee_target);
    helper::PushVec3(L, steering->arrive_target);
    return 22;
}

// world_to_screen: project a 3D world position to 2D screen coordinates
// Returns: screen_x, screen_y, is_visible (boolean, false if behind camera)
int L_EcsWorldToScreen(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushboolean(L, 0);
        return 3;
    }
    float wx = helper::CheckFloat(L, 1);
    float wy = helper::CheckFloat(L, 2);
    float wz = helper::CheckFloat(L, 3);

    // Find main camera (highest priority enabled Camera3DComponent)
    auto cam_view = world->registry().view<Camera3DComponent, TransformComponent>();
    entt::entity main_cam = entt::null;
    int max_priority = -9999;
    for (auto entity : cam_view) {
        auto& cam = cam_view.get<Camera3DComponent>(entity);
        if (cam.enabled && cam.priority > max_priority) {
            max_priority = cam.priority;
            main_cam = entity;
        }
    }
    if (main_cam == entt::null) {
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushboolean(L, 0);
        return 3;
    }

    auto& cam = cam_view.get<Camera3DComponent>(main_cam);
    auto& transform = cam_view.get<TransformComponent>(main_cam);

    // Build view matrix from transform
    glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view_mat = glm::lookAt(transform.position, transform.position + front, up);
    glm::mat4 proj_mat = glm::perspective(glm::radians(cam.fov), cam.aspect_ratio, cam.near_clip, cam.far_clip);

    glm::vec4 clip = proj_mat * view_mat * glm::vec4(wx, wy, wz, 1.0f);
    bool visible = clip.w > 0.0f;
    if (clip.w == 0.0f) clip.w = 0.0001f;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    // NDC [-1,1] to screen [0, width/height]
    float screen_w = static_cast<float>(Screen::width());
    float screen_h = static_cast<float>(Screen::height());
    float sx = (ndc.x * 0.5f + 0.5f) * screen_w;
    float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * screen_h;  // flip Y

    lua_pushnumber(L, static_cast<lua_Number>(sx));
    lua_pushnumber(L, static_cast<lua_Number>(sy));
    lua_pushboolean(L, visible && ndc.z >= -1.0f && ndc.z <= 1.0f ? 1 : 0);
    return 3;
}

// ============================================================
// LOD
// ============================================================

/// lod.add_level(entity, mesh_path, threshold)
int L_EcsLodAddLevel(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* mesh_path = luaL_checkstring(L, 2);
    float threshold = static_cast<float>(luaL_checknumber(L, 3));
    if (!world->registry().all_of<LODGroupComponent>(e)) {
        world->registry().emplace<LODGroupComponent>(e);
    }
    auto& lod = world->registry().get<LODGroupComponent>(e);
    LODLevelConfig level;
    level.mesh_path = mesh_path;
    level.screen_size_threshold = threshold;
    lod.levels.push_back(std::move(level));
    return 0;
}

/// lod.set_scale(entity, scale)
int L_EcsLodSetScale(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float scale = static_cast<float>(luaL_checknumber(L, 2));
    auto* lod = helper::TryGetComponent<LODGroupComponent>(*world, e);
    if (!lod) return 0;
    lod->global_scale = scale;
    return 0;
}

/// lod.set_min_screen_size(entity, min_size)  -- 低于此屏幕占比时隐藏实体（LOD 距离裁剪）
int L_EcsLodSetMinScreenSize(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float min_size = static_cast<float>(luaL_checknumber(L, 2));
    if (!world->registry().all_of<LODGroupComponent>(e)) {
        world->registry().emplace<LODGroupComponent>(e);
    }
    auto& lod = world->registry().get<LODGroupComponent>(e);
    lod.min_screen_size = min_size;
    return 0;
}

/// lod.set_enabled(entity, enabled)
int L_EcsLodSetEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool enabled = lua_toboolean(L, 2) != 0;
    auto* lod = helper::TryGetComponent<LODGroupComponent>(*world, e);
    if (!lod) return 0;
    lod->enabled = enabled;
    return 0;
}

// ============================================================
// Grass
// ============================================================

int L_EcsAddGrass(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& g = world->registry().emplace_or_replace<GrassComponent>(e);
    g.enabled = true;
    g.density       = helper::OptFloat(L, 2, 1.0f);
    g.spawn_radius  = helper::OptFloat(L, 3, 50.0f);
    g.blade_height  = helper::OptFloat(L, 4, 1.0f);
    g.blade_width   = helper::OptFloat(L, 5, 0.1f);
    return 0;
}

int L_EcsSetGrassParams(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    if (lua_gettop(L) >= 2) g->density       = helper::CheckFloat(L, 2);
    if (lua_gettop(L) >= 3) g->spawn_radius  = helper::CheckFloat(L, 3);
    if (lua_gettop(L) >= 4) g->blade_height  = helper::CheckFloat(L, 4);
    if (lua_gettop(L) >= 5) g->blade_width   = helper::CheckFloat(L, 5);
    if (lua_gettop(L) >= 6) g->blade_height_variation = helper::CheckFloat(L, 6);
    if (lua_gettop(L) >= 7) g->chunk_size    = helper::CheckFloat(L, 7);
    if (lua_gettop(L) >= 8) g->seed          = static_cast<unsigned int>(helper::CheckInt(L, 8));
    return 0;
}

int L_EcsSetGrassColor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    g->base_color = glm::vec3(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));
    if (lua_gettop(L) >= 7) {
        g->tip_color = glm::vec3(helper::CheckFloat(L, 5), helper::CheckFloat(L, 6), helper::CheckFloat(L, 7));
    }
    return 0;
}

int L_EcsSetGrassWind(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    g->wind_direction = glm::vec2(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3));
    if (lua_gettop(L) >= 4) g->wind_speed      = helper::CheckFloat(L, 4);
    if (lua_gettop(L) >= 5) g->wind_strength    = helper::CheckFloat(L, 5);
    if (lua_gettop(L) >= 6) g->wind_turbulence  = helper::CheckFloat(L, 6);
    return 0;
}

int L_EcsSetGrassLod(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    g->lod_near = helper::CheckFloat(L, 2);
    g->lod_far  = helper::CheckFloat(L, 3);
    if (lua_gettop(L) >= 4) g->cast_shadow      = lua_toboolean(L, 4) != 0;
    if (lua_gettop(L) >= 5) g->shadow_distance   = helper::CheckFloat(L, 5);
    return 0;
}

int L_EcsSetGrassEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    g->enabled = lua_toboolean(L, 2) != 0;
    return 0;
}

int L_EcsGetGrassStats(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const auto* g = helper::TryGetComponentConst<GrassComponent>(*world, e);
    if (!g) return 0;
    helper::PushInt(L, g->cached_instance_count_);
    return 1;
}

// ============================================================
// Hair
// ============================================================

// add_hair(entity, asset_path [, num_follow_per_guide])
int L_EcsAddHair(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& h = world->registry().emplace_or_replace<HairComponent>(e);
    h.enabled = true;
    h.hair_asset_path = helper::CheckString(L, 2);
    if (lua_gettop(L) >= 3) h.num_follow_per_guide = helper::CheckInt(L, 3);
    return 0;
}

// set_hair_physics(entity, damping, stiffness_local, stiffness_global, gravity)
int L_EcsSetHairPhysics(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    if (lua_gettop(L) >= 2) h->damping          = helper::CheckFloat(L, 2);
    if (lua_gettop(L) >= 3) h->stiffness_local  = helper::CheckFloat(L, 3);
    if (lua_gettop(L) >= 4) h->stiffness_global = helper::CheckFloat(L, 4);
    if (lua_gettop(L) >= 5) h->gravity          = helper::CheckFloat(L, 5);
    return 0;
}

// set_hair_render(entity, root_r,g,b,a, tip_r,g,b,a, fiber_radius, opacity)
int L_EcsSetHairRender(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    if (lua_gettop(L) >= 5)
        h->root_color = glm::vec4(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3),
                                   helper::CheckFloat(L, 4), helper::CheckFloat(L, 5));
    if (lua_gettop(L) >= 9)
        h->tip_color = glm::vec4(helper::CheckFloat(L, 6), helper::CheckFloat(L, 7),
                                  helper::CheckFloat(L, 8), helper::CheckFloat(L, 9));
    if (lua_gettop(L) >= 10) h->fiber_radius = helper::CheckFloat(L, 10);
    if (lua_gettop(L) >= 11) h->opacity      = helper::CheckFloat(L, 11);
    return 0;
}

// set_hair_wind(entity, wx, wy, wz [, turbulence])
int L_EcsSetHairWind(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    h->wind = glm::vec3(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));
    if (lua_gettop(L) >= 5) h->wind_turbulence = helper::CheckFloat(L, 5);
    return 0;
}

// set_hair_enabled(entity, bool)
int L_EcsSetHairEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    h->enabled = lua_toboolean(L, 2) != 0;
    return 0;
}

// set_hair_lod(entity, lod0, lod1, lod2, cull)
int L_EcsSetHairLod(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    if (lua_gettop(L) >= 2) h->lod0_distance = helper::CheckFloat(L, 2);
    if (lua_gettop(L) >= 3) h->lod1_distance = helper::CheckFloat(L, 3);
    if (lua_gettop(L) >= 4) h->lod2_distance = helper::CheckFloat(L, 4);
    if (lua_gettop(L) >= 5) h->cull_distance = helper::CheckFloat(L, 5);
    return 0;
}

// ============================================================
// GI Probe Volume (DDGI)
// ============================================================

// add_gi_probe(entity [, ox,oy,oz, ex,ey,ez, rx,ry,rz])
int L_EcsAddGIProbe(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& gi = world->registry().emplace_or_replace<GIProbeVolumeComponent>(e);
    gi.enabled = true;
    if (lua_gettop(L) >= 4)
        gi.origin = glm::vec3(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));
    if (lua_gettop(L) >= 7)
        gi.extent = glm::vec3(helper::CheckFloat(L, 5), helper::CheckFloat(L, 6), helper::CheckFloat(L, 7));
    if (lua_gettop(L) >= 10) {
        gi.resolution_x = helper::CheckInt(L, 8);
        gi.resolution_y = helper::CheckInt(L, 9);
        gi.resolution_z = helper::CheckInt(L, 10);
    }
    gi.needs_reinit_ = true;
    return 0;
}

// set_gi_probe(entity, origin_x,y,z, extent_x,y,z, res_x,y,z [, gi_intensity, normal_bias, hysteresis])
int L_EcsSetGIProbe(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* gi = helper::TryGetComponent<GIProbeVolumeComponent>(*world, e);
    if (!gi) return 0;
    if (lua_gettop(L) >= 4)
        gi->origin = glm::vec3(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));
    if (lua_gettop(L) >= 7)
        gi->extent = glm::vec3(helper::CheckFloat(L, 5), helper::CheckFloat(L, 6), helper::CheckFloat(L, 7));
    if (lua_gettop(L) >= 10) {
        gi->resolution_x = helper::CheckInt(L, 8);
        gi->resolution_y = helper::CheckInt(L, 9);
        gi->resolution_z = helper::CheckInt(L, 10);
        gi->needs_reinit_ = true;
    }
    if (lua_gettop(L) >= 11) gi->gi_intensity = helper::CheckFloat(L, 11);
    if (lua_gettop(L) >= 12) gi->normal_bias = helper::CheckFloat(L, 12);
    if (lua_gettop(L) >= 13) gi->hysteresis = helper::CheckFloat(L, 13);
    return 0;
}

// set_gi_probe_enabled(entity, bool)
int L_EcsSetGIProbeEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* gi = helper::TryGetComponent<GIProbeVolumeComponent>(*world, e);
    if (!gi) return 0;
    gi->enabled = lua_toboolean(L, 2) != 0;
    return 0;
}

// get_gi_probe(entity) -> enabled, ox,oy,oz, ex,ey,ez, rx,ry,rz, gi_intensity, normal_bias
int L_EcsGetGIProbe(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const auto* gi = helper::TryGetComponentConst<GIProbeVolumeComponent>(*world, e);
    if (!gi) return 0;
    helper::PushBool(L, gi->enabled);
    helper::PushFloat(L, gi->origin.x); helper::PushFloat(L, gi->origin.y); helper::PushFloat(L, gi->origin.z);
    helper::PushFloat(L, gi->extent.x); helper::PushFloat(L, gi->extent.y); helper::PushFloat(L, gi->extent.z);
    lua_pushinteger(L, gi->resolution_x); lua_pushinteger(L, gi->resolution_y); lua_pushinteger(L, gi->resolution_z);
    helper::PushFloat(L, gi->gi_intensity);
    helper::PushFloat(L, gi->normal_bias);
    return 12;
}

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

// ============================================================
// LightProbeComponent 绑定
// ============================================================

// add_light_probe(entity, [influence_radius])
int L_EcsAddLightProbe(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& probe = world->registry().emplace_or_replace<LightProbeComponent>(e);
    probe.enabled = true;
    probe.influence_radius = helper::OptFloat(L, 2, 10.0f);
    return 0;
}

// set_light_probe(entity, influence_radius, [capture_resolution])
int L_EcsSetLightProbe(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* probe = helper::TryGetComponent<LightProbeComponent>(*world, e);
    if (!probe) return 0;
    if (!lua_isnoneornil(L, 2)) probe->influence_radius = helper::CheckFloat(L, 2);
    if (!lua_isnoneornil(L, 3)) probe->needs_rebake = helper::CheckBool(L, 3);
    return 0;
}

DSE_LUA_COMPONENT_SETTER(LightProbeEnabled, LightProbeComponent, enabled, bool, helper::CheckBool(L, 2))

// ============================================================
// ReflectionProbeComponent 绑定
// ============================================================

// add_reflection_probe(entity, [influence_radius])
int L_EcsAddReflectionProbe(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& probe = world->registry().emplace_or_replace<ReflectionProbeComponent>(e);
    probe.enabled = true;
    probe.influence_radius = helper::OptFloat(L, 2, 15.0f);
    return 0;
}

// set_reflection_probe(entity, influence_radius, box_size_x, box_size_y, box_size_z, [capture_resolution])
int L_EcsSetReflectionProbe(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* probe = helper::TryGetComponent<ReflectionProbeComponent>(*world, e);
    if (!probe) return 0;
    if (!lua_isnoneornil(L, 2)) probe->influence_radius = helper::CheckFloat(L, 2);
    if (!lua_isnoneornil(L, 3)) probe->box_size_x = helper::CheckFloat(L, 3);
    if (!lua_isnoneornil(L, 4)) probe->box_size_y = helper::CheckFloat(L, 4);
    if (!lua_isnoneornil(L, 5)) probe->box_size_z = helper::CheckFloat(L, 5);
    if (!lua_isnoneornil(L, 6)) probe->resolution = helper::CheckInt(L, 6);
    return 0;
}

DSE_LUA_COMPONENT_SETTER(ReflectionProbeEnabled, ReflectionProbeComponent, enabled, bool, helper::CheckBool(L, 2))

// ============================================================
// TreeComponent — add_tree + mesh_path getter/setter
// ============================================================

int L_EcsAddTree(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* mesh_path = luaL_optstring(L, 2, "");
    auto& tree = world->registry().emplace_or_replace<dse::TreeComponent>(e);
    tree.enabled = true;
    tree.mesh_path = mesh_path;
    return 0;
}

// ============================================================
// TerrainTileManagerComponent — add
// ============================================================

int L_EcsAddTerrainTileManager(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& ttm = world->registry().emplace_or_replace<dse::TerrainTileManagerComponent>(e);
    ttm.enabled = true;
    return 0;
}

// ============================================================
// DynamicObstacleComponent — add
// ============================================================

int L_EcsAddDynamicObstacle(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& obs = world->registry().emplace_or_replace<dse::DynamicObstacleComponent>(e);
    obs.enabled = true;
    if (!lua_isnoneornil(L, 2)) {
        int shape_val = helper::CheckInt(L, 2);
        obs.shape = (shape_val == 1)
            ? dse::DynamicObstacleComponent::Shape::Cylinder
            : dse::DynamicObstacleComponent::Shape::Box;
    }
    return 0;
}

// ============================================================
// FoliageComponent
// ============================================================

int L_EcsAddFoliage(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& fc = world->registry().emplace_or_replace<dse::FoliageComponent>(e);
    fc.enabled = true;
    return 0;
}

int L_EcsSetFoliageWindStrength(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fc = helper::TryGetComponent<dse::FoliageComponent>(*world, e);
    if (!fc) return 0;
    fc->wind_strength = helper::CheckFloat(L, 2);
    return 0;
}

int L_EcsGetFoliageWindStrength(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnumber(L, 0.0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* fc = helper::TryGetComponentConst<dse::FoliageComponent>(*world, e);
    if (!fc) { lua_pushnumber(L, 0.0); return 1; }
    helper::PushFloat(L, fc->wind_strength);
    return 1;
}

int L_EcsSetFoliageStiffness(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fc = helper::TryGetComponent<dse::FoliageComponent>(*world, e);
    if (!fc) return 0;
    fc->stiffness = helper::CheckFloat(L, 2);
    return 0;
}

int L_EcsGetFoliageStiffness(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnumber(L, 0.0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* fc = helper::TryGetComponentConst<dse::FoliageComponent>(*world, e);
    if (!fc) { lua_pushnumber(L, 0.0); return 1; }
    helper::PushFloat(L, fc->stiffness);
    return 1;
}

int L_EcsSetFoliageEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fc = helper::TryGetComponent<dse::FoliageComponent>(*world, e);
    if (!fc) return 0;
    fc->enabled = helper::CheckBool(L, 2);
    return 0;
}

int L_EcsGetFoliageEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* fc = helper::TryGetComponentConst<dse::FoliageComponent>(*world, e);
    if (!fc) { lua_pushboolean(L, 0); return 1; }
    helper::PushBool(L, fc->enabled);
    return 1;
}

// ============================================================
// NavMeshAutoRebakeComponent
// ============================================================

int L_EcsAddNavMeshAutoRebake(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& nr = world->registry().emplace_or_replace<dse::NavMeshAutoRebakeComponent>(e);
    nr.enabled = true;
    return 0;
}

} // namespace

void RegisterEcsRenderingBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        // Camera
        {"add_camera",                L_EcsAddCamera},
        {"add_camera_3d",             L_EcsAddCamera3D},
        {"set_camera_priority",       L_EcsSetCameraPriority},
        {"set_camera_enabled",        L_EcsSetCameraEnabled},
        {"set_camera_follow",         L_EcsSetCameraFollow},
        {"add_free_camera_controller", L_EcsAddFreeCameraController},
        // Sprite
        {"add_sprite",                L_EcsAddSprite},
        {"set_sprite_uv_scroll",      L_EcsSetSpriteUvScroll},
        {"set_sprite_uv_offset",      L_EcsSetSpriteUvOffset},
        // MeshRenderer
        {"add_mesh_renderer",         L_EcsAddMeshRenderer},
        {"set_mesh_path",             L_EcsSetMeshPath},
        {"set_mesh_material",         L_EcsSetMeshMaterial},
        {"set_mesh_shader_variant",   L_EcsSetMeshShaderVariant},
        {"set_mesh_depth_state",      L_EcsSetMeshDepthState},
        {"set_mesh_material_scalar",  L_EcsSetMeshMaterialScalar},
        {"set_mesh_texture",          L_EcsSetMeshTexture},
        {"set_mesh_uvs",              L_EcsSetMeshUv},
        {"set_mesh_normals",          L_EcsSetMeshNormals},
        {"set_mesh_tangents",         L_EcsSetMeshTangents},
        {"set_mesh_emissive",         L_EcsSetMeshEmissive},
        {"set_mesh_advanced_material", L_EcsSetMeshAdvancedMaterial},
        // Lights
        {"add_directional_light_3d",  L_EcsAddDirectionalLight3D},
        {"set_directional_light_3d",  L_EcsSetDirectionalLight3D},
        {"set_directional_light_shadow", L_EcsSetDirectionalLightShadow},
        {"add_point_light_3d",        L_EcsAddPointLight3D},
        {"set_point_light_3d",        L_EcsSetPointLight3D},
        {"set_point_light_shadow",    L_EcsSetPointLightShadow},
        {"add_spot_light_3d",         L_EcsAddSpotLight3D},
        {"set_spot_light_3d",         L_EcsSetSpotLight3D},
        {"set_spot_light_shadow",     L_EcsSetSpotLightShadow},
        {"add_sky_light",             L_EcsAddSkyLight},
        {"set_sky_light",             L_EcsSetSkyLight},
        // Skybox
        {"add_skybox",                L_EcsAddSkybox},
        // Terrain
        {"add_terrain",               L_EcsAddTerrain},
        {"set_terrain_params",        L_EcsSetTerrainParams},
        {"set_terrain_height",        L_EcsSetTerrainHeight},
        {"load_terrain_heightmap",    L_EcsLoadTerrainHeightmap},
        {"set_terrain_texture",       L_EcsSetTerrainTexture},
        {"get_terrain_lod",           L_EcsGetTerrainLod},
        {"sample_terrain_height",     L_EcsSampleTerrainHeight},
        {"set_terrain_splat_texture", L_EcsSetTerrainSplatTexture},
        // PostProcess
        {"add_post_process",          L_EcsAddPostProcess},
        {"set_post_process_bloom",    L_EcsSetPostProcessBloom},
        {"set_post_process_color",    L_EcsSetPostProcessColor},
        {"set_post_process_ssao",     L_EcsSetPostProcessSSAO},
        {"set_post_process_ssr",      L_EcsSetPostProcessSSR},
        {"set_post_process_fxaa",     L_EcsSetPostProcessFXAA},
        {"set_post_process_auto_exposure", L_EcsSetPostProcessAutoExposure},
        {"set_post_process_vignette", L_EcsSetPostProcessVignette},
        {"set_post_process_film_grain", L_EcsSetPostProcessFilmGrain},
        {"set_post_process_color_lut", L_EcsSetPostProcessColorLut},
        {"set_post_process_outline",  L_EcsSetPostProcessOutline},
        {"set_post_process_fog",      L_EcsSetPostProcessFog},
        {"set_post_process_light_shaft", L_EcsSetPostProcessLightShaft},
        {"add_decal",                 L_EcsAddDecal},
        {"set_decal",                 L_EcsSetDecal},
        {"get_post_process_state",    L_EcsGetPostProcessState},
        // Water
        {"add_water",                 L_EcsAddWater},
        {"set_water",                 L_EcsSetWater},
        {"get_water",                 L_EcsGetWater},
        // Steering
        {"add_steering",              L_EcsAddSteering},
        {"set_steering_target",       L_EcsSetSteeringTarget},
        {"get_steering_state",        L_EcsGetSteeringState},
        // LOD
        {"lod_add_level",             L_EcsLodAddLevel},
        {"lod_set_scale",             L_EcsLodSetScale},
        {"lod_set_min_screen_size",   L_EcsLodSetMinScreenSize},
        {"lod_set_enabled",           L_EcsLodSetEnabled},
        // Grass
        {"add_grass",                 L_EcsAddGrass},
        {"set_grass_params",          L_EcsSetGrassParams},
        {"set_grass_color",           L_EcsSetGrassColor},
        {"set_grass_wind",            L_EcsSetGrassWind},
        {"set_grass_lod",             L_EcsSetGrassLod},
        {"set_grass_enabled",         L_EcsSetGrassEnabled},
        {"get_grass_stats",           L_EcsGetGrassStats},
        // Hair
        {"add_hair",                  L_EcsAddHair},
        {"set_hair_physics",          L_EcsSetHairPhysics},
        {"set_hair_render",           L_EcsSetHairRender},
        {"set_hair_wind",             L_EcsSetHairWind},
        {"set_hair_enabled",          L_EcsSetHairEnabled},
        {"set_hair_lod",              L_EcsSetHairLod},
        // GI Probe (DDGI)
        {"add_gi_probe",              L_EcsAddGIProbe},
        {"set_gi_probe",              L_EcsSetGIProbe},
        {"set_gi_probe_enabled",      L_EcsSetGIProbeEnabled},
        {"get_gi_probe",              L_EcsGetGIProbe},
        // Morph Target
        {"add_morph",                 L_EcsAddMorph},
        {"morph_add_target",          L_EcsMorphAddTarget},
        {"morph_set_weight",          L_EcsMorphSetWeight},
        {"morph_get_weight",          L_EcsMorphGetWeight},
        {"set_morph_enabled",         L_EcsSetMorphEnabled},
        // Light Probe
        {"add_light_probe",           L_EcsAddLightProbe},
        {"set_light_probe",           L_EcsSetLightProbe},
        {"set_light_probe_enabled",   L_EcsSetLightProbeEnabled},
        // Reflection Probe
        {"add_reflection_probe",      L_EcsAddReflectionProbe},
        {"set_reflection_probe",      L_EcsSetReflectionProbe},
        {"set_reflection_probe_enabled", L_EcsSetReflectionProbeEnabled},
        // Tree
        {"add_tree",                  L_EcsAddTree},
        // TerrainTileManager
        {"add_terrain_tile_manager",  L_EcsAddTerrainTileManager},
        // DynamicObstacle
        {"add_dynamic_obstacle",      L_EcsAddDynamicObstacle},
        // Foliage
        {"add_foliage",               L_EcsAddFoliage},
        {"set_foliage_wind_strength", L_EcsSetFoliageWindStrength},
        {"get_foliage_wind_strength", L_EcsGetFoliageWindStrength},
        {"set_foliage_stiffness",     L_EcsSetFoliageStiffness},
        {"get_foliage_stiffness",     L_EcsGetFoliageStiffness},
        {"set_foliage_enabled",       L_EcsSetFoliageEnabled},
        {"get_foliage_enabled",       L_EcsGetFoliageEnabled},
        // NavMesh Auto Rebake（字段 get/set 见 codegen navmesh_rebake 绑定）
        {"add_navmesh_auto_rebake",   L_EcsAddNavMeshAutoRebake},
        // Utility
        {"world_to_screen",           L_EcsWorldToScreen},
    });
}

} // namespace dse::runtime::lua_binding
