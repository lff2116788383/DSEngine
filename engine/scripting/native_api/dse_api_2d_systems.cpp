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
#include "engine/ecs/parallax_2d.h"
#include "engine/ecs/light_2d.h"
#include "engine/ecs/camera_controller_2d.h"
#include "engine/ecs/trail_renderer_2d.h"
#include "engine/ecs/line_renderer_2d.h"
#include "engine/ecs/audio_spatial_2d.h"
#include "engine/assets/sprite_sheet_asset.h"
#include "engine/assets/atlas_asset.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

using Entity = entt::entity;
using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }

// SpriteSheet / Atlas resource storage (matches Lua binding's static vectors)
std::vector<SpriteSheetAsset> g_sheets;
std::vector<AtlasAsset> g_atlases;

struct SpriteAtlasRuntime {
    SpriteSheetAsset sheet;
    std::string path;
    uint32_t texture = 0;
    uint32_t normal_texture = 0;
    uint32_t emissive_texture = 0;
};
std::vector<SpriteAtlasRuntime> g_sprite3d_atlases;

const SpriteClip* FindSpriteClip(const SpriteAtlasRuntime& runtime, const char* clip_name) {
    if (!clip_name || !*clip_name) return nullptr;
    for (const auto& clip : runtime.sheet.clips) {
        if (clip.name == clip_name) return &clip;
    }
    return nullptr;
}

int SpriteClipFrameCount(const SpriteAtlasRuntime& runtime, const char* clip_name) {
    const SpriteClip* clip = FindSpriteClip(runtime, clip_name);
    if (clip) return static_cast<int>(clip->frames.size());
    return static_cast<int>(runtime.sheet.frames.size());
}

void SpriteClipFrameUV(const SpriteAtlasRuntime& runtime, const char* clip_name,
                       int frame, float* out_uv) {
    if (!out_uv) return;
    const SpriteClip* clip = FindSpriteClip(runtime, clip_name);
    int index = frame;
    if (clip && !clip->frames.empty()) {
        index = clip->frames[static_cast<size_t>(std::clamp(frame, 0, static_cast<int>(clip->frames.size()) - 1))];
    }
    const glm::vec4 uv = runtime.sheet.GetFrameUV(index);
    out_uv[0] = uv.x; out_uv[1] = uv.y; out_uv[2] = uv.z; out_uv[3] = uv.w;
}

std::string ResolveRelativeToAtlas(const std::string& atlas_path, const std::string& texture_path) {
    if (texture_path.empty()) return texture_path;
    std::filesystem::path tex(texture_path);
    if (tex.is_absolute()) return texture_path;
    std::filesystem::path atlas(atlas_path);
    if (!atlas.has_parent_path()) return texture_path;
    return (atlas.parent_path() / tex).lexically_normal().generic_string();
}

uint32_t LoadAtlasTexture(const std::string& atlas_path, const std::string& texture_path,
                          int filter, int wrap) {
    if (texture_path.empty()) return 0;
    // The legacy .dsprite files store paths relative to the repo root, while
    // the generated B+ atlases store a sibling file name.  Check the raw path
    // first without logging, then try the atlas-relative path, and finally let
    // the asset loader emit its diagnostic for a truly missing file.
    std::error_code ec;
    if (std::filesystem::exists(texture_path, ec)) {
        return dse_assets_load_texture_ex(texture_path.c_str(), filter, wrap);
    }
    const std::string resolved = ResolveRelativeToAtlas(atlas_path, texture_path);
    if (resolved != texture_path) {
        if (uint32_t handle = dse_assets_load_texture_ex(resolved.c_str(), filter, wrap)) {
            return handle;
        }
    }
    return dse_assets_load_texture_ex(texture_path.c_str(), filter, wrap);
}

} // namespace

// ============================================================
// Parallax
// ============================================================

extern "C" void dse_parallax_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<ParallaxComponent>(TE(e));
}

extern "C" int dse_parallax_add_layer(uint32_t e, float scroll_x, float scroll_y) {
    World* world = GW();
    if (!world) return -1;
    auto* p = world->registry().try_get<ParallaxComponent>(TE(e));
    if (!p) return -1;
    ParallaxLayer layer;
    layer.scroll_factor_x = scroll_x;
    layer.scroll_factor_y = scroll_y;
    layer.sorting_order = static_cast<int>(p->layers.size());
    p->layers.push_back(layer);
    return static_cast<int>(p->layers.size()) - 1;
}

extern "C" void dse_parallax_set_layer_scroll(uint32_t e, int layer, float sx, float sy) {
    World* world = GW();
    if (!world) return;
    auto* p = world->registry().try_get<ParallaxComponent>(TE(e));
    if (!p || layer < 0 || layer >= static_cast<int>(p->layers.size())) return;
    p->layers[layer].scroll_factor_x = sx;
    p->layers[layer].scroll_factor_y = sy;
}

extern "C" void dse_parallax_set_layer_auto_scroll(uint32_t e, int layer, float sx, float sy) {
    World* world = GW();
    if (!world) return;
    auto* p = world->registry().try_get<ParallaxComponent>(TE(e));
    if (!p || layer < 0 || layer >= static_cast<int>(p->layers.size())) return;
    p->layers[layer].auto_scroll_x = sx;
    p->layers[layer].auto_scroll_y = sy;
}

extern "C" void dse_parallax_set_layer_opacity(uint32_t e, int layer, float opacity) {
    World* world = GW();
    if (!world) return;
    auto* p = world->registry().try_get<ParallaxComponent>(TE(e));
    if (!p || layer < 0 || layer >= static_cast<int>(p->layers.size())) return;
    p->layers[layer].opacity = opacity;
}

extern "C" int dse_parallax_get_layer_count(uint32_t e) {
    World* world = GW();
    if (!world) return 0;
    auto* p = world->registry().try_get<ParallaxComponent>(TE(e));
    return p ? static_cast<int>(p->layers.size()) : 0;
}

// ============================================================
// Light2D
// ============================================================

extern "C" void dse_light2d_add(uint32_t e, int type) {
    World* world = GW();
    if (!world) return;
    auto& lc = world->registry().emplace_or_replace<Light2DComponent>(TE(e));
    lc.type = static_cast<Light2DType>(type);
}

extern "C" void dse_light2d_set_color(uint32_t e, float r, float g, float b) {
    World* world = GW();
    if (!world) return;
    auto* lc = world->registry().try_get<Light2DComponent>(TE(e));
    if (lc) lc->color = glm::vec3(r, g, b);
}

extern "C" void dse_light2d_set_intensity(uint32_t e, float intensity) {
    World* world = GW();
    if (!world) return;
    auto* lc = world->registry().try_get<Light2DComponent>(TE(e));
    if (lc) lc->intensity = intensity;
}

extern "C" void dse_light2d_set_range(uint32_t e, float range) {
    World* world = GW();
    if (!world) return;
    auto* lc = world->registry().try_get<Light2DComponent>(TE(e));
    if (lc) lc->range = range;
}

extern "C" void dse_light2d_set_shadow(uint32_t e, int mode) {
    World* world = GW();
    if (!world) return;
    auto* lc = world->registry().try_get<Light2DComponent>(TE(e));
    if (lc) lc->shadow_mode = static_cast<Shadow2DMode>(mode);
}

extern "C" void dse_light2d_set_ambient(uint32_t e, float r, float g, float b, float intensity) {
    World* world = GW();
    if (!world) return;
    auto& amb = world->registry().emplace_or_replace<Ambient2DComponent>(TE(e));
    amb.color = glm::vec3(r, g, b);
    amb.intensity = intensity;
}

extern "C" void dse_normal_map_2d_add(uint32_t e, float strength) {
    World* world = GW();
    if (!world) return;
    auto& nm = world->registry().emplace_or_replace<NormalMap2DComponent>(TE(e));
    nm.normal_strength = strength;
}

// ============================================================
// SpriteSheet
// ============================================================

extern "C" int dse_sprite_sheet_load(const char* path) {
    if (!path) return -1;
    SpriteSheetAsset sheet;
    if (sheet.LoadFromFile(path)) {
        g_sheets.push_back(std::move(sheet));
        return static_cast<int>(g_sheets.size()) - 1;
    }
    return -1;
}

extern "C" int dse_sprite_sheet_frame_count(int sheet) {
    if (sheet >= 0 && sheet < static_cast<int>(g_sheets.size())) {
        return static_cast<int>(g_sheets[sheet].frames.size());
    }
    return 0;
}

extern "C" void dse_sprite_sheet_get_frame_uv(int sheet, int frame, float* out_uv) {
    if (!out_uv) return;
    if (sheet >= 0 && sheet < static_cast<int>(g_sheets.size())) {
        glm::vec4 uv = g_sheets[sheet].GetFrameUV(frame);
        out_uv[0] = uv.x; out_uv[1] = uv.y; out_uv[2] = uv.z; out_uv[3] = uv.w;
        return;
    }
    out_uv[0] = 0.0f; out_uv[1] = 0.0f; out_uv[2] = 1.0f; out_uv[3] = 1.0f;
}

// ============================================================
// Sprite3D atlas (.dsprite + clips)
// ============================================================

extern "C" int dse_sprite_atlas_load(const char* path, int filter, int wrap) {
    if (!path) return -1;
    SpriteAtlasRuntime runtime;
    if (!runtime.sheet.LoadFromFile(path)) return -1;
    runtime.path = path;
    runtime.texture = LoadAtlasTexture(runtime.path, runtime.sheet.texture_path, filter, wrap);
    if (!runtime.texture) return -1;
    runtime.normal_texture = LoadAtlasTexture(runtime.path, runtime.sheet.normal_texture_path, filter, wrap);
    runtime.emissive_texture = LoadAtlasTexture(runtime.path, runtime.sheet.emissive_texture_path, filter, wrap);
    g_sprite3d_atlases.push_back(std::move(runtime));
    return static_cast<int>(g_sprite3d_atlases.size()) - 1;
}

extern "C" int dse_sprite_atlas_path(int atlas, char* out_path, int out_size) {
    if (!out_path || out_size <= 0) return 0;
    if (atlas < 0 || atlas >= static_cast<int>(g_sprite3d_atlases.size())) {
        out_path[0] = '\0';
        return 0;
    }
    const std::string& path = g_sprite3d_atlases[atlas].path;
    std::snprintf(out_path, static_cast<size_t>(out_size), "%s", path.c_str());
    return 1;
}

extern "C" uint32_t dse_sprite_atlas_texture(int atlas) {
    if (atlas < 0 || atlas >= static_cast<int>(g_sprite3d_atlases.size())) return 0;
    return g_sprite3d_atlases[atlas].texture;
}

extern "C" uint32_t dse_sprite_atlas_normal_texture(int atlas) {
    if (atlas < 0 || atlas >= static_cast<int>(g_sprite3d_atlases.size())) return 0;
    return g_sprite3d_atlases[atlas].normal_texture;
}

extern "C" uint32_t dse_sprite_atlas_emissive_texture(int atlas) {
    if (atlas < 0 || atlas >= static_cast<int>(g_sprite3d_atlases.size())) return 0;
    return g_sprite3d_atlases[atlas].emissive_texture;
}

extern "C" int dse_sprite_atlas_clip_frame_count(int atlas, const char* clip_name) {
    if (atlas < 0 || atlas >= static_cast<int>(g_sprite3d_atlases.size())) return 0;
    return SpriteClipFrameCount(g_sprite3d_atlases[atlas], clip_name);
}

extern "C" void dse_sprite_atlas_clip_frame_uv(int atlas, const char* clip_name, int frame, float* out_uv) {
    if (!out_uv) return;
    if (atlas < 0 || atlas >= static_cast<int>(g_sprite3d_atlases.size())) {
        out_uv[0] = 0.0f; out_uv[1] = 0.0f; out_uv[2] = 1.0f; out_uv[3] = 1.0f;
        return;
    }
    SpriteClipFrameUV(g_sprite3d_atlases[atlas], clip_name, frame, out_uv);
}

extern "C" float dse_sprite_atlas_clip_fps(int atlas, const char* clip_name) {
    if (atlas < 0 || atlas >= static_cast<int>(g_sprite3d_atlases.size())) return 10.0f;
    const SpriteClip* clip = FindSpriteClip(g_sprite3d_atlases[atlas], clip_name);
    return clip ? clip->fps : 10.0f;
}

extern "C" int dse_sprite_atlas_clip_loop(int atlas, const char* clip_name) {
    if (atlas < 0 || atlas >= static_cast<int>(g_sprite3d_atlases.size())) return 1;
    const SpriteClip* clip = FindSpriteClip(g_sprite3d_atlases[atlas], clip_name);
    return clip ? (clip->loop ? 1 : 0) : 1;
}

// ============================================================
// Atlas
// ============================================================

extern "C" int dse_atlas_load(const char* path) {
    if (!path) return -1;
    AtlasAsset atlas;
    if (atlas.LoadFromFile(path)) {
        g_atlases.push_back(std::move(atlas));
        return static_cast<int>(g_atlases.size()) - 1;
    }
    return -1;
}

extern "C" int dse_atlas_entry_count(int atlas) {
    if (atlas >= 0 && atlas < static_cast<int>(g_atlases.size())) {
        return static_cast<int>(g_atlases[atlas].entries.size());
    }
    return 0;
}

extern "C" void dse_atlas_get_entry_uv(int atlas, const char* name, float* out_uv) {
    if (!out_uv) return;
    if (atlas >= 0 && atlas < static_cast<int>(g_atlases.size()) && name) {
        glm::vec4 uv = g_atlases[atlas].GetEntryUV(name);
        out_uv[0] = uv.x; out_uv[1] = uv.y; out_uv[2] = uv.z; out_uv[3] = uv.w;
        return;
    }
    out_uv[0] = 0.0f; out_uv[1] = 0.0f; out_uv[2] = 1.0f; out_uv[3] = 1.0f;
}

// ============================================================
// Camera2D
// ============================================================

extern "C" void dse_camera_controller_2d_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<CameraController2DComponent>(TE(e));
}

extern "C" void dse_camera_2d_shake(uint32_t e, float trauma) {
    World* world = GW();
    if (!world) return;
    auto* ctrl = world->registry().try_get<CameraController2DComponent>(TE(e));
    if (ctrl) {
        ctrl->shake.trauma = std::min(ctrl->shake.trauma + trauma, 1.0f);
    }
}

extern "C" void dse_camera_2d_set_zoom(uint32_t e, float zoom) {
    World* world = GW();
    if (!world) return;
    auto* ctrl = world->registry().try_get<CameraController2DComponent>(TE(e));
    if (ctrl) ctrl->target_zoom = zoom;
}

extern "C" void dse_camera_2d_set_bounds(uint32_t e, float min_x, float min_y, float max_x, float max_y) {
    World* world = GW();
    if (!world) return;
    auto* ctrl = world->registry().try_get<CameraController2DComponent>(TE(e));
    if (ctrl) {
        ctrl->bounds.enabled = true;
        ctrl->bounds.min_x = min_x;
        ctrl->bounds.min_y = min_y;
        ctrl->bounds.max_x = max_x;
        ctrl->bounds.max_y = max_y;
    }
}

extern "C" void dse_camera_2d_set_look_ahead(uint32_t e, float lax, float lay) {
    World* world = GW();
    if (!world) return;
    auto* ctrl = world->registry().try_get<CameraController2DComponent>(TE(e));
    if (ctrl) {
        ctrl->look_ahead_x = lax;
        ctrl->look_ahead_y = lay;
    }
}

// ============================================================
// Trail Renderer
// ============================================================

extern "C" void dse_trail_renderer_add(uint32_t e, float lifetime, float start_width, float end_width) {
    World* world = GW();
    if (!world) return;
    auto& trail = world->registry().emplace_or_replace<TrailRenderer2DComponent>(TE(e));
    trail.lifetime = lifetime;
    trail.start_width = start_width;
    trail.end_width = end_width;
}

extern "C" void dse_trail_set_emitting(uint32_t e, int emitting) {
    World* world = GW();
    if (!world) return;
    auto* trail = world->registry().try_get<TrailRenderer2DComponent>(TE(e));
    if (trail) trail->emitting = (emitting != 0);
}

extern "C" void dse_trail_set_colors(uint32_t e, float r1, float g1, float b1, float a1,
                                    float r2, float g2, float b2, float a2) {
    World* world = GW();
    if (!world) return;
    auto* trail = world->registry().try_get<TrailRenderer2DComponent>(TE(e));
    if (trail) {
        trail->start_color = glm::vec4(r1, g1, b1, a1);
        trail->end_color = glm::vec4(r2, g2, b2, a2);
    }
}

extern "C" void dse_trail_clear(uint32_t e) {
    World* world = GW();
    if (!world) return;
    auto* trail = world->registry().try_get<TrailRenderer2DComponent>(TE(e));
    if (trail) trail->points.clear();
}

// ============================================================
// Line Renderer
// ============================================================

extern "C" void dse_line_renderer_add(uint32_t e, float width) {
    World* world = GW();
    if (!world) return;
    auto& line = world->registry().emplace_or_replace<LineRenderer2DComponent>(TE(e));
    line.width = width;
}

extern "C" void dse_line_renderer_set_points(uint32_t e, const float* points, int count) {
    World* world = GW();
    if (!world) return;
    auto* line = world->registry().try_get<LineRenderer2DComponent>(TE(e));
    if (!line || !points) return;
    line->points.clear();
    line->points.reserve(count);
    for (int i = 0; i < count; ++i) {
        line->points.push_back(glm::vec2(points[i * 2], points[i * 2 + 1]));
    }
}

extern "C" void dse_line_renderer_set_width(uint32_t e, float width) {
    World* world = GW();
    if (!world) return;
    auto* line = world->registry().try_get<LineRenderer2DComponent>(TE(e));
    if (line) line->width = width;
}

extern "C" void dse_line_renderer_set_color(uint32_t e, float r, float g, float b, float a) {
    World* world = GW();
    if (!world) return;
    auto* line = world->registry().try_get<LineRenderer2DComponent>(TE(e));
    if (line) {
        line->start_color = glm::vec4(r, g, b, a);
        line->end_color = glm::vec4(r, g, b, a);
    }
}

extern "C" void dse_line_renderer_set_closed(uint32_t e, int closed) {
    World* world = GW();
    if (!world) return;
    auto* line = world->registry().try_get<LineRenderer2DComponent>(TE(e));
    if (line) line->closed = (closed != 0);
}

// ============================================================
// Audio Spatial 2D
// ============================================================

extern "C" void dse_audio_spatial_2d_add(uint32_t e, float min_dist, float max_dist) {
    World* world = GW();
    if (!world) return;
    auto& spatial = world->registry().emplace_or_replace<AudioSpatial2DComponent>(TE(e));
    spatial.min_distance = min_dist;
    spatial.max_distance = max_dist;
}

extern "C" void dse_audio_spatial_2d_set_range(uint32_t e, float min_dist, float max_dist) {
    World* world = GW();
    if (!world) return;
    auto* sp = world->registry().try_get<AudioSpatial2DComponent>(TE(e));
    if (sp) {
        sp->min_distance = min_dist;
        sp->max_distance = max_dist;
    }
}

extern "C" void dse_audio_spatial_2d_set_attenuation(uint32_t e, int model, float rolloff) {
    World* world = GW();
    if (!world) return;
    auto* sp = world->registry().try_get<AudioSpatial2DComponent>(TE(e));
    if (sp) {
        sp->attenuation = static_cast<AudioAttenuation2DModel>(model);
        sp->rolloff = rolloff;
    }
}

extern "C" void dse_audio_listener_2d_add(uint32_t e, float global_volume) {
    World* world = GW();
    if (!world) return;
    auto& al = world->registry().emplace_or_replace<AudioListener2DComponent>(TE(e));
    al.global_volume = global_volume;
}
