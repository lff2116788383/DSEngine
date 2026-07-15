/**
 * @file dse_api_rendering_camera.cpp
 * @brief DSEngine C ABI - Rendering Camera 扩展
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/components_3d.h"

using namespace dse;
using namespace dse_api_internal;


extern "C" void dse_camera_add(uint32_t e, float ortho_size, int priority) {
    World* world = GW();
    if (!world) return;
    auto& cam = world->registry().emplace_or_replace<CameraComponent>(TE(e));
    cam.enabled = true;
    cam.priority = priority;
    cam.orthographic = true;
    cam.orthographic_size = ortho_size;
}

extern "C" void dse_camera_set_priority(uint32_t e, int priority) {
    World* world = GW();
    if (!world) return;
    if (auto* cam3d = world->registry().try_get<Camera3DComponent>(TE(e))) cam3d->priority = priority;
    if (auto* cam = world->registry().try_get<CameraComponent>(TE(e))) cam->priority = priority;
}

extern "C" void dse_camera_set_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    if (auto* cam3d = world->registry().try_get<Camera3DComponent>(TE(e))) cam3d->enabled = (enabled != 0);
    if (auto* cam = world->registry().try_get<CameraComponent>(TE(e))) cam->enabled = (enabled != 0);
}

extern "C" void dse_camera_set_follow(uint32_t e, uint32_t target, float damping,
                                      float dead_zone_x, float dead_zone_y,
                                      float offset_x, float offset_y) {
    World* world = GW();
    if (!world || !world->registry().valid(TE(e))) return;
    auto& follow = world->registry().emplace_or_replace<CameraFollowComponent>(TE(e));
    follow.target = TE(target);
    follow.damping = damping;
    follow.dead_zone = glm::vec2(dead_zone_x, dead_zone_y);
    follow.offset = glm::vec3(offset_x, offset_y, 0.0f);
    follow.enabled = true;
}

extern "C" void dse_free_camera_add(uint32_t e, float move_speed, float mouse_sensitivity) {
    World* world = GW();
    if (!world) return;
    auto& controller = world->registry().emplace_or_replace<FreeCameraControllerComponent>(TE(e));
    controller.enabled = true;
    controller.move_speed = move_speed;
    controller.mouse_sensitivity = mouse_sensitivity;
}

extern "C" void dse_sprite_add(uint32_t e, float r, float g, float b, float a,
                               int order_in_layer, uint32_t texture_handle) {
    World* world = GW();
    if (!world) return;
    auto& sprite = world->registry().emplace_or_replace<SpriteRendererComponent>(TE(e));
    sprite.color = glm::vec4(r, g, b, a);
    sprite.order_in_layer = order_in_layer;
    sprite.texture_handle = dse::render::TextureHandle::from_raw(texture_handle);
    sprite.visible = true;
}

extern "C" void dse_sprite_set_uv_scroll(uint32_t e, float sx, float sy) {
    World* world = GW();
    if (!world) return;
    auto* sp = world->registry().try_get<SpriteRendererComponent>(TE(e));
    if (sp) sp->uv_scroll_speed = glm::vec2(sx, sy);
}

extern "C" void dse_sprite_set_uv_offset(uint32_t e, float ox, float oy) {
    World* world = GW();
    if (!world) return;
    auto* sp = world->registry().try_get<SpriteRendererComponent>(TE(e));
    if (sp) sp->uv_offset = glm::vec2(ox, oy);
}
