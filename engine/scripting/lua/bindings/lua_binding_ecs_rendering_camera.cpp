/**
 * @file lua_binding_ecs_rendering_camera.cpp
 * @brief Camera / Sprite Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <algorithm>

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

// ============================================================
// Camera（2D + 3D）
// ============================================================

int L_EcsAddCamera(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float ortho_size = helper::OptFloat(L, 2, 10.0f);
    int priority = helper::OptInt(L, 3, 0);
    dse_camera_add(EID(e), ortho_size, priority);
    return 0;
}

int L_EcsAddCamera3D(lua_State* L) {
    const uint32_t e = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    const float fov = helper::OptFloat(L, 2, 60.0f);
    const int priority = helper::OptInt(L, 3, 0);
    float near_clip = helper::OptFloat(L, 4, 0.1f);
    float far_clip = helper::OptFloat(L, 5, 1000.0f);
    if (near_clip <= 0.0f) near_clip = 0.1f;
    if (far_clip <= near_clip) far_clip = near_clip + 1000.0f;
    // S1.8-2：委托 C ABI（dse_camera3d_add 内部 emplace_or_replace 重置组件，enabled 默认 true）
    dse_camera3d_add(e, fov, near_clip, far_clip);
    dse_camera3d_set_enabled(e, 1);
    dse_camera3d_set_priority(e, priority);
    return 0;
}

int L_EcsSetCameraPriority(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_camera_set_priority(EID(e), helper::CheckInt(L, 2));
    return 0;
}

int L_EcsSetCameraEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_camera_set_enabled(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

int L_EcsSetCameraFollow(lua_State* L) {
    Entity camera_entity = helper::CheckEntity(L, 1);
    Entity target_entity = helper::CheckEntity(L, 2);
    float damping = helper::OptFloat(L, 3, 0.12f);
    float dead_zone_x = helper::OptFloat(L, 4, 0.0f);
    float dead_zone_y = helper::OptFloat(L, 5, 0.0f);
    float offset_x = helper::OptFloat(L, 6, 0.0f);
    float offset_y = helper::OptFloat(L, 7, 0.0f);
    dse_camera_set_follow(EID(camera_entity), EID(target_entity), damping,
                          dead_zone_x, dead_zone_y, offset_x, offset_y);
    return 0;
}

int L_EcsAddFreeCameraController(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_free_camera_add(EID(e), helper::OptFloat(L, 2, 5.0f), helper::OptFloat(L, 3, 0.1f));
    return 0;
}


// ============================================================
// Sprite
// ============================================================

int L_EcsAddSprite(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float r = helper::OptFloat(L, 2, 1.0f);
    float g = helper::OptFloat(L, 3, 1.0f);
    float b = helper::OptFloat(L, 4, 1.0f);
    float a = helper::OptFloat(L, 5, 1.0f);
    int order = helper::OptInt(L, 6, 0);
    uint32_t texture_handle = static_cast<uint32_t>(helper::OptInt(L, 7, 0));
    dse_sprite_add(EID(e), r, g, b, a, order, texture_handle);
    return 0;
}

int L_EcsSetSpriteUvScroll(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    glm::vec2 v = helper::CheckVec2(L, 2);
    dse_sprite_set_uv_scroll(EID(e), v.x, v.y);
    return 0;
}

int L_EcsSetSpriteUvOffset(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    glm::vec2 v = helper::CheckVec2(L, 2);
    dse_sprite_set_uv_offset(EID(e), v.x, v.y);
    return 0;
}


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
