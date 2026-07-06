/**
 * @file dse_api_2d_systems.cpp
 * @brief DSEngine Native C ABI — 2D Systems（Parallax / Light2D / SpriteSheet / Atlas / Camera2D / Trail / LineRenderer / AudioSpatial2D）
 *
 * 封装 2D 游戏所需的 ECS 组件操作和资源管理。
 * 语义与 Lua 绑定逐一等价。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/sprite.h"
#include "engine/ecs/parallax_2d.h"
#include "engine/ecs/light_2d.h"
#include "engine/ecs/camera_controller_2d.h"
#include "engine/ecs/trail_renderer_2d.h"
#include "engine/ecs/line_renderer_2d.h"
#include "engine/ecs/audio_spatial_2d.h"
#include "engine/assets/asset_manager.h"

#include <glm/glm.hpp>

using Entity = entt::entity;
using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
inline AssetManager* GAM() { return static_cast<AssetManager*>(dse_get_asset_manager_ptr()); }

} // namespace

// ============================================================
// Parallax
// ============================================================

extern "C" void dse_parallax_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<Parallax2DComponent>(TE(e));
}

extern "C" int dse_parallax_add_layer(uint32_t e, float scroll_scale, int auto_scroll, float speed_x, float speed_y) {
    World* world = GW();
    if (!world) return -1;
    auto* p = world->registry().try_get<Parallax2DComponent>(TE(e));
    if (!p) return -1;
    ParallaxLayer layer;
    layer.scroll_scale = scroll_scale;
    layer.auto_scroll = (auto_scroll != 0);
    layer.auto_scroll_speed = glm::vec2(speed_x, speed_y);
    p->layers.push_back(layer);
    return static_cast<int>(p->layers.size()) - 1;
}

extern "C" void dse_parallax_set_layer_scroll(uint32_t e, int layer, float scroll_scale) {
    World* world = GW();
    if (!world) return;
    auto* p = world->registry().try_get<Parallax2DComponent>(TE(e));
    if (!p || layer < 0 || layer >= static_cast<int>(p->layers.size())) return;
    p->layers[layer].scroll_scale = scroll_scale;
}

extern "C" void dse_parallax_set_layer_auto_scroll(uint32_t e, int layer, float speed_x, float speed_y) {
    World* world = GW();
    if (!world) return;
    auto* p = world->registry().try_get<Parallax2DComponent>(TE(e));
    if (!p || layer < 0 || layer >= static_cast<int>(p->layers.size())) return;
    p->layers[layer].auto_scroll_speed = glm::vec2(speed_x, speed_y);
}

extern "C" void dse_parallax_set_layer_opacity(uint32_t e, int layer, float opacity) {
    World* world = GW();
    if (!world) return;
    auto* p = world->registry().try_get<Parallax2DComponent>(TE(e));
    if (!p || layer < 0 || layer >= static_cast<int>(p->layers.size())) return;
    p->layers[layer].opacity = opacity;
}

extern "C" int dse_parallax_get_layer_count(uint32_t e) {
    World* world = GW();
    if (!world) return 0;
    const auto* p = world->registry().try_get<Parallax2DComponent>(TE(e));
    return p ? static_cast<int>(p->layers.size()) : 0;
}

// ============================================================
// Light2D
// ============================================================

extern "C" void dse_light2d_add(uint32_t e, int type, float r, float g, float b, float intensity,
                                float range, int cast_shadow) {
    World* world = GW();
    if (!world) return;
    auto& light = world->registry().emplace_or_replace<Light2DComponent>(TE(e));
    light.type = static_cast<Light2DType>(type);
    light.color = glm::vec3(r, g, b);
    light.intensity = intensity;
    light.range = range;
    light.cast_shadow = (cast_shadow != 0);
}

extern "C" void dse_light2d_set_color(uint32_t e, float r, float g, float b) {
    World* world = GW();
    if (!world) return;
    auto* light = world->registry().try_get<Light2DComponent>(TE(e));
    if (light) light->color = glm::vec3(r, g, b);
}

extern "C" void dse_light2d_set_intensity(uint32_t e, float intensity) {
    World* world = GW();
    if (!world) return;
    auto* light = world->registry().try_get<Light2DComponent>(TE(e));
    if (light) light->intensity = intensity;
}

extern "C" void dse_light2d_set_range(uint32_t e, float range) {
    World* world = GW();
    if (!world) return;
    auto* light = world->registry().try_get<Light2DComponent>(TE(e));
    if (light) light->range = range;
}

extern "C" void dse_light2d_set_shadow(uint32_t e, int cast_shadow) {
    World* world = GW();
    if (!world) return;
    auto* light = world->registry().try_get<Light2DComponent>(TE(e));
    if (light) light->cast_shadow = (cast_shadow != 0);
}

static glm::vec3 g_ambient_2d(0.2f, 0.2f, 0.2f);

extern "C" void dse_light2d_set_ambient(float r, float g, float b) {
    g_ambient_2d = glm::vec3(r, g, b);
}

extern "C" void dse_normal_map_2d_add(uint32_t e, uint32_t texture_handle) {
    World* world = GW();
    if (!world) return;
    auto& nm = world->registry().emplace_or_replace<NormalMap2DComponent>(TE(e));
    nm.texture_handle = texture_handle;
}

// ============================================================
// SpriteSheet / Atlas (resource handles — stub implementations)
// ============================================================

extern "C" uint32_t dse_sprite_sheet_load(const char* path, int frame_w, int frame_h) {
    auto* am = GAM();
    if (!am || !path) return 0;
    auto tex = am->LoadTexture(path);
    if (!tex) return 0;
    // Pack frame dimensions into handle high bits for client use
    return (tex->GetHandle() & 0x00FFFFFF) | (static_cast<uint32_t>(frame_w) << 24);
}

extern "C" int dse_sprite_sheet_frame_count(uint32_t sheet) {
    // Computed client-side; return 0 as stub
    return 0;
}

extern "C" void dse_sprite_sheet_get_frame_uv(uint32_t sheet, int frame, float* out_uv) {
    if (!out_uv) return;
    out_uv[0] = 0.0f; out_uv[1] = 0.0f; out_uv[2] = 1.0f; out_uv[3] = 1.0f;
}

extern "C" uint32_t dse_atlas_load(const char* path) {
    auto* am = GAM();
    if (!am || !path) return 0;
    auto tex = am->LoadTexture(path);
    return tex ? tex->GetHandle() : 0;
}

extern "C" int dse_atlas_entry_count(uint32_t atlas) { return 0; }

extern "C" void dse_atlas_get_entry_uv(uint32_t atlas, int index, float* out_uv) {
    if (!out_uv) return;
    out_uv[0] = 0.0f; out_uv[1] = 0.0f; out_uv[2] = 1.0f; out_uv[3] = 1.0f;
}

// ============================================================
// Camera2D
// ============================================================

extern "C" void dse_camera_controller_2d_add(uint32_t e, uint32_t target) {
    World* world = GW();
    if (!world) return;
    auto& cc = world->registry().emplace_or_replace<CameraController2DComponent>(TE(e));
    cc.target = TE(target);
}

extern "C" void dse_camera_2d_shake(float intensity, float duration) {
    World* world = GW();
    if (!world) return;
    auto view = world->registry().view<CameraController2DComponent>();
    for (auto e : view) {
        auto& cc = view.get<CameraController2DComponent>(e);
        cc.shake_intensity = intensity;
        cc.shake_duration = duration;
    }
}

extern "C" void dse_camera_2d_set_zoom(float zoom) {
    World* world = GW();
    if (!world) return;
    auto view = world->registry().view<CameraController2DComponent>();
    for (auto e : view) {
        auto& cc = view.get<CameraController2DComponent>(e);
        cc.zoom = zoom;
    }
}

extern "C" void dse_camera_2d_set_bounds(float min_x, float min_y, float max_x, float max_y) {
    World* world = GW();
    if (!world) return;
    auto view = world->registry().view<CameraController2DComponent>();
    for (auto e : view) {
        auto& cc = view.get<CameraController2DComponent>(e);
        cc.bounds_min = glm::vec2(min_x, min_y);
        cc.bounds_max = glm::vec2(max_x, max_y);
        cc.use_bounds = true;
    }
}

extern "C" void dse_camera_2d_set_look_ahead(float distance, float speed) {
    World* world = GW();
    if (!world) return;
    auto view = world->registry().view<CameraController2DComponent>();
    for (auto e : view) {
        auto& cc = view.get<CameraController2DComponent>(e);
        cc.look_ahead_distance = distance;
        cc.look_ahead_speed = speed;
    }
}

// ============================================================
// Trail Renderer
// ============================================================

extern "C" void dse_trail_renderer_add(uint32_t e, float width, float r, float g, float b, float a) {
    World* world = GW();
    if (!world) return;
    auto& tr = world->registry().emplace_or_replace<TrailRenderer2DComponent>(TE(e));
    tr.width = width;
    tr.color = glm::vec4(r, g, b, a);
}

extern "C" void dse_trail_set_emitting(uint32_t e, int emitting) {
    World* world = GW();
    if (!world) return;
    auto* tr = world->registry().try_get<TrailRenderer2DComponent>(TE(e));
    if (tr) tr->emitting = (emitting != 0);
}

extern "C" void dse_trail_set_colors(uint32_t e, float r1, float g1, float b1, float a1,
                                    float r2, float g2, float b2, float a2) {
    World* world = GW();
    if (!world) return;
    auto* tr = world->registry().try_get<TrailRenderer2DComponent>(TE(e));
    if (tr) {
        tr->color_start = glm::vec4(r1, g1, b1, a1);
        tr->color_end = glm::vec4(r2, g2, b2, a2);
    }
}

extern "C" void dse_trail_clear(uint32_t e) {
    World* world = GW();
    if (!world) return;
    auto* tr = world->registry().try_get<TrailRenderer2DComponent>(TE(e));
    if (tr) tr->points.clear();
}

// ============================================================
// Line Renderer
// ============================================================

extern "C" void dse_line_renderer_add(uint32_t e, float width, float r, float g, float b, float a) {
    World* world = GW();
    if (!world) return;
    auto& lr = world->registry().emplace_or_replace<LineRenderer2DComponent>(TE(e));
    lr.width = width;
    lr.color = glm::vec4(r, g, b, a);
}

extern "C" void dse_line_renderer_set_points(uint32_t e, const float* points, int count) {
    World* world = GW();
    if (!world) return;
    auto* lr = world->registry().try_get<LineRenderer2DComponent>(TE(e));
    if (!lr || !points) return;
    lr->points.clear();
    lr->points.reserve(count);
    for (int i = 0; i < count; ++i) {
        lr->points.push_back(glm::vec2(points[i * 2], points[i * 2 + 1]));
    }
}

extern "C" void dse_line_renderer_set_width(uint32_t e, float width) {
    World* world = GW();
    if (!world) return;
    auto* lr = world->registry().try_get<LineRenderer2DComponent>(TE(e));
    if (lr) lr->width = width;
}

extern "C" void dse_line_renderer_set_color(uint32_t e, float r, float g, float b, float a) {
    World* world = GW();
    if (!world) return;
    auto* lr = world->registry().try_get<LineRenderer2DComponent>(TE(e));
    if (lr) lr->color = glm::vec4(r, g, b, a);
}

extern "C" void dse_line_renderer_set_closed(uint32_t e, int closed) {
    World* world = GW();
    if (!world) return;
    auto* lr = world->registry().try_get<LineRenderer2DComponent>(TE(e));
    if (lr) lr->closed = (closed != 0);
}

// ============================================================
// Audio Spatial 2D
// ============================================================

extern "C" void dse_audio_spatial_2d_add(uint32_t e, uint32_t source_entity, float min_dist, float max_dist) {
    World* world = GW();
    if (!world) return;
    auto& as = world->registry().emplace_or_replace<AudioSpatial2DComponent>(TE(e));
    as.source_entity = TE(source_entity);
    as.min_distance = min_dist;
    as.max_distance = max_dist;
}

extern "C" void dse_audio_spatial_2d_set_range(uint32_t e, float min_dist, float max_dist) {
    World* world = GW();
    if (!world) return;
    auto* as = world->registry().try_get<AudioSpatial2DComponent>(TE(e));
    if (as) { as->min_distance = min_dist; as->max_distance = max_dist; }
}

extern "C" void dse_audio_spatial_2d_set_attenuation(uint32_t e, float rolloff) {
    World* world = GW();
    if (!world) return;
    auto* as = world->registry().try_get<AudioSpatial2DComponent>(TE(e));
    if (as) as->rolloff = rolloff;
}

extern "C" void dse_audio_listener_2d_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<AudioListener2DComponent>(TE(e));
}
