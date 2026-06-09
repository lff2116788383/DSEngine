/**
 * @file lua_binding_ecs_rendering_camera.cpp
 * @brief Camera / Sprite Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）
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


} // namespace

void RegisterEcsRenderingCameraBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_camera",                L_EcsAddCamera},
        {"add_camera_3d",             L_EcsAddCamera3D},
        {"set_camera_priority",       L_EcsSetCameraPriority},
        {"set_camera_enabled",        L_EcsSetCameraEnabled},
        {"set_camera_follow",         L_EcsSetCameraFollow},
        {"add_free_camera_controller", L_EcsAddFreeCameraController},
        {"add_sprite",                L_EcsAddSprite},
        {"set_sprite_uv_scroll",      L_EcsSetSpriteUvScroll},
        {"set_sprite_uv_offset",      L_EcsSetSpriteUvOffset},
    });
}

} // namespace dse::runtime::lua_binding
