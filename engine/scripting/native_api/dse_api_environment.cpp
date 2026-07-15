/**
 * @file dse_api_p1_6.cpp
 * @brief DSEngine Native C ABI — P1-6 批次实现
 *
 * 实现 PostProcess 组件创建/LUT 加载、Decal 扩展、Terrain/Water/Grass/Foliage
 * 环境组件操作、以及扩展 UI 控件。
 * 供 Lua / C# 共享同一实现，语义与原 Lua 绑定逐一等价。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/ecs/components_3d_terrain_tile.h"
#include "engine/ecs/components_3d_navmesh.h"
#include "engine/ecs/components_3d_foliage.h"
#include "engine/ecs/ui.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/lut_loader.h"
#include "engine/render/rhi/rhi_device.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline AssetManager* GAM() { return static_cast<AssetManager*>(dse_get_asset_manager_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }

template <typename T>
inline T* GetComp(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return nullptr;
    return w->registry().try_get<T>(TE(e));
}

template <typename T>
inline T& EmplaceComp(uint32_t e) {
    return GW()->registry().emplace_or_replace<T>(TE(e));
}

// Terrain heightmap sampling helper (from original Lua binding)
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
    auto* am = GAM();
    if (!am) return false;
    std::vector<unsigned char> pixels;
    int image_width = 0, image_height = 0, image_channels = 0;
    if (!am->LoadImageRgba(heightmap_path, pixels, image_width, image_height, image_channels)) {
        return false;
    }
    terrain.heightmap_path = heightmap_path;
    terrain.heightmap_channels = image_channels;
    SampleTerrainHeightmap(terrain, pixels, image_width, image_height);
    terrain.is_dirty = true;
    return true;
}

}  // namespace

// ============================================================
// PostProcess — 组件创建 / LUT 加载
// ============================================================

extern "C" void dse_post_process_add(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& pp = w->registry().emplace_or_replace<PostProcessComponent>(TE(e));
    pp.enabled = true;
}

extern "C" void dse_post_process_set_color_lut(uint32_t e, const char* path, float intensity) {
    auto* pp = GetComp<PostProcessComponent>(e);
    if (!pp) return;
    pp->color_lut_intensity = intensity;
    auto* am = GAM();
    if (!am) return;
    RhiDevice* rhi = am->rhi_device();
    if (!rhi) return;
    if (path) {
        if (pp->color_lut_handle != 0) {
            rhi->DeleteTexture(dse::render::TextureHandle::from_raw(pp->color_lut_handle));
            pp->color_lut_handle = 0;
        }
        std::string full_path = am->ResolveAssetPath(path);
        dse::assets::LutData lut;
        if (dse::assets::LoadCubeLut(full_path, lut) && lut.size > 0 && !lut.rgba8.empty()) {
            pp->color_lut_handle =
                rhi->CreateTexture3D(lut.size, lut.size, lut.size, lut.rgba8.data(), true).raw();
        }
    } else {
        if (pp->color_lut_handle != 0) {
            rhi->DeleteTexture(dse::render::TextureHandle::from_raw(pp->color_lut_handle));
        }
        pp->color_lut_handle = 0;
    }
}

// ============================================================
// Decal — 组件创建 / 扩展字段设置
// ============================================================

extern "C" void dse_decal_add_simple(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    if (!w->registry().all_of<DecalComponent>(TE(e)))
        w->registry().emplace<DecalComponent>(TE(e));
}

extern "C" void dse_decal_set_full(uint32_t e, int enabled, int has_texture, uint32_t texture,
                                    float r, float g, float b, float a, float angle_fade) {
    auto* dc = GetComp<DecalComponent>(e);
    if (!dc) return;
    dc->enabled = (enabled != 0);
    if (has_texture) {
        dc->albedo_texture = dse::render::TextureHandle::from_raw(texture);
    }
    dc->color.r = r;
    dc->color.g = g;
    dc->color.b = b;
    dc->color.a = a;
    dc->angle_fade = angle_fade;
}

// ============================================================
// Terrain
// ============================================================

extern "C" void dse_terrain_add(uint32_t e, const char* heightmap_path,
                                 float width, float depth, float max_height) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& terrain = w->registry().emplace_or_replace<TerrainComponent>(TE(e));
    terrain.enabled = true;
    terrain.heightmap_path = heightmap_path ? heightmap_path : "";
    terrain.width = width;
    terrain.depth = depth;
    terrain.max_height = max_height;
    if (heightmap_path && heightmap_path[0] != '\0') {
        LoadTerrainHeightmap(terrain, heightmap_path);
    }
    terrain.is_dirty = true;
}

extern "C" void dse_terrain_set_params(uint32_t e, int res_x, int res_z,
                                        int max_lod, float lod_factor, int use_dynamic_lod) {
    auto* terrain = GetComp<TerrainComponent>(e);
    if (!terrain) return;
    terrain->resolution_x = std::max(2, res_x);
    terrain->resolution_z = std::max(2, res_z);
    terrain->max_lod_levels = std::max(1, max_lod);
    terrain->lod_distance_factor = std::max(0.1f, lod_factor);
    terrain->use_dynamic_lod = (use_dynamic_lod != 0);
    terrain->height_data.assign(static_cast<std::size_t>(terrain->resolution_x * terrain->resolution_z), 0.0f);
    if (!terrain->heightmap_path.empty()) {
        LoadTerrainHeightmap(*terrain, terrain->heightmap_path);
    }
    terrain->current_lod = std::clamp(terrain->current_lod, 0, terrain->max_lod_levels - 1);
    terrain->is_dirty = true;
}

extern "C" void dse_terrain_set_height(uint32_t e, int x, int z, float height) {
    auto* terrain = GetComp<TerrainComponent>(e);
    if (!terrain) return;
    if (terrain->resolution_x < 2 || terrain->resolution_z < 2) return;
    if (terrain->height_data.size() != static_cast<std::size_t>(terrain->resolution_x * terrain->resolution_z)) {
        terrain->height_data.assign(static_cast<std::size_t>(terrain->resolution_x * terrain->resolution_z), 0.0f);
    }
    if (x < 0 || z < 0 || x >= terrain->resolution_x || z >= terrain->resolution_z) return;
    terrain->height_data[static_cast<std::size_t>(z * terrain->resolution_x + x)] = height;
    terrain->is_dirty = true;
}

extern "C" int dse_terrain_load_heightmap(uint32_t e, const char* path,
                                           int* out_w, int* out_h, int* out_ch,
                                           int* out_rx, int* out_rz) {
    auto* terrain = GetComp<TerrainComponent>(e);
    if (!terrain || !path) return 0;
    bool ok = LoadTerrainHeightmap(*terrain, path);
    if (ok) {
        if (out_w) *out_w = terrain->heightmap_width;
        if (out_h) *out_h = terrain->heightmap_height;
        if (out_ch) *out_ch = terrain->heightmap_channels;
        if (out_rx) *out_rx = terrain->resolution_x;
        if (out_rz) *out_rz = terrain->resolution_z;
    }
    return ok ? 1 : 0;
}

extern "C" int dse_terrain_set_texture(uint32_t e, const char* path,
                                        uint32_t* out_handle, int* out_w, int* out_h) {
    auto* terrain = GetComp<TerrainComponent>(e);
    if (!terrain || !path) return 0;
    auto* am = GAM();
    if (!am) return 0;
    auto texture = am->LoadTexture(path);
    if (!texture) return 0;
    terrain->texture_path = path;
    terrain->texture_handle = texture->GetHandle();
    terrain->is_dirty = true;
    if (out_handle) *out_handle = terrain->texture_handle.raw();
    if (out_w) *out_w = texture->GetWidth();
    if (out_h) *out_h = texture->GetHeight();
    return 1;
}

extern "C" void dse_terrain_get_lod(uint32_t e, int* out_lod, int* out_rx, int* out_rz,
                                     int* out_max_lod, float* out_lod_factor) {
    auto* terrain = GetComp<TerrainComponent>(e);
    if (!terrain) return;
    if (out_lod) *out_lod = terrain->current_lod;
    if (out_rx) *out_rx = terrain->resolution_x;
    if (out_rz) *out_rz = terrain->resolution_z;
    if (out_max_lod) *out_max_lod = terrain->max_lod_levels;
    if (out_lod_factor) *out_lod_factor = terrain->lod_distance_factor;
}

extern "C" float dse_terrain_sample_height(uint32_t e, float wx, float wz) {
    World* w = GW();
    if (!w) return 0.0f;
    auto* terrain = w->registry().try_get<TerrainComponent>(TE(e));
    auto* transform = w->registry().try_get<TransformComponent>(TE(e));
    if (!terrain || !transform) return 0.0f;
    return dse::SampleTerrainHeight(*terrain, *transform, wx, wz);
}

extern "C" int dse_terrain_set_splat_texture(uint32_t e, int layer, const char* path) {
    auto* terrain = GetComp<TerrainComponent>(e);
    if (!terrain || !path || layer < 0 || layer > 3) return 0;
    auto* am = GAM();
    if (!am) return 0;
    auto tex = am->LoadTexture(path);
    if (!tex) return 0;
    terrain->splat_texture_paths[layer] = path;
    terrain->splat_texture_handles[layer] = tex->GetHandle();
    terrain->splat_dirty = true;
    return 1;
}

// ============================================================
// Water
// ============================================================

extern "C" void dse_water_add(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    if (!w->registry().all_of<WaterComponent>(TE(e)))
        w->registry().emplace<WaterComponent>(TE(e));
}

extern "C" void dse_water_set(uint32_t e, int enabled, float water_level,
                              float dr, float dg, float db,
                              float sr, float sg, float sb,
                              float max_depth, float transparency,
                              float wave_amp, float wave_freq, float wave_speed,
                              float wdir_x, float wdir_y,
                              float refraction, float reflection, float spec_power,
                              float caustic_int, float caustic_scale,
                              float foam_int, float foam_threshold,
                              float ufog_density, float ufog_r, float ufog_g, float ufog_b) {
    auto* wc = GetComp<WaterComponent>(e);
    if (!wc) return;
    wc->enabled = (enabled != 0);
    wc->water_level = water_level;
    wc->deep_color.r = dr; wc->deep_color.g = dg; wc->deep_color.b = db;
    wc->shallow_color.r = sr; wc->shallow_color.g = sg; wc->shallow_color.b = sb;
    wc->max_depth = max_depth;
    wc->transparency = transparency;
    wc->wave_amplitude = wave_amp;
    wc->wave_frequency = wave_freq;
    wc->wave_speed = wave_speed;
    wc->wave_direction.x = wdir_x;
    wc->wave_direction.y = wdir_y;
    wc->refraction_strength = refraction;
    wc->reflection_strength = reflection;
    wc->specular_power = spec_power;
    wc->caustic_intensity = caustic_int;
    wc->caustic_scale = caustic_scale;
    wc->foam_intensity = foam_int;
    wc->foam_depth_threshold = foam_threshold;
    wc->underwater_fog_density = ufog_density;
    wc->underwater_fog_color.r = ufog_r;
    wc->underwater_fog_color.g = ufog_g;
    wc->underwater_fog_color.b = ufog_b;
}

extern "C" int dse_water_get(uint32_t e, int* out_enabled, float* out_water_level,
                              float* out_deep_rgb, float* out_shallow_rgb,
                              float* out_max_depth, float* out_transparency,
                              float* out_wave, float* out_wdir,
                              float* out_refraction, float* out_reflection, float* out_spec_power) {
    auto* wc = GetComp<WaterComponent>(e);
    if (!wc) return 0;
    if (out_enabled) *out_enabled = wc->enabled ? 1 : 0;
    if (out_water_level) *out_water_level = wc->water_level;
    if (out_deep_rgb) { out_deep_rgb[0] = wc->deep_color.r; out_deep_rgb[1] = wc->deep_color.g; out_deep_rgb[2] = wc->deep_color.b; }
    if (out_shallow_rgb) { out_shallow_rgb[0] = wc->shallow_color.r; out_shallow_rgb[1] = wc->shallow_color.g; out_shallow_rgb[2] = wc->shallow_color.b; }
    if (out_max_depth) *out_max_depth = wc->max_depth;
    if (out_transparency) *out_transparency = wc->transparency;
    if (out_wave) { out_wave[0] = wc->wave_amplitude; out_wave[1] = wc->wave_frequency; out_wave[2] = wc->wave_speed; }
    if (out_wdir) { out_wdir[0] = wc->wave_direction.x; out_wdir[1] = wc->wave_direction.y; }
    if (out_refraction) *out_refraction = wc->refraction_strength;
    if (out_reflection) *out_reflection = wc->reflection_strength;
    if (out_spec_power) *out_spec_power = wc->specular_power;
    return 1;
}

// ============================================================
// Grass
// ============================================================

extern "C" void dse_grass_add(uint32_t e, float density, float spawn_radius,
                               float blade_height, float blade_width) {
    auto& g = EmplaceComp<GrassComponent>(e);
    g.enabled = true;
    g.density = density;
    g.spawn_radius = spawn_radius;
    g.blade_height = blade_height;
    g.blade_width = blade_width;
}

extern "C" void dse_grass_set_params(uint32_t e, float density, float spawn_radius,
                                      float blade_height, float blade_width,
                                      float blade_height_var, float chunk_size, int seed) {
    auto* g = GetComp<GrassComponent>(e);
    if (!g) return;
    g->density = density;
    g->spawn_radius = spawn_radius;
    g->blade_height = blade_height;
    g->blade_width = blade_width;
    g->blade_height_variation = blade_height_var;
    g->chunk_size = chunk_size;
    g->seed = static_cast<unsigned int>(seed);
}

extern "C" void dse_grass_set_color(uint32_t e, float br, float bg, float bb,
                                     float tr, float tg, float tb) {
    auto* g = GetComp<GrassComponent>(e);
    if (!g) return;
    g->base_color = glm::vec3(br, bg, bb);
    g->tip_color = glm::vec3(tr, tg, tb);
}

extern "C" void dse_grass_set_wind(uint32_t e, float dx, float dy,
                                    float speed, float strength, float turbulence) {
    auto* g = GetComp<GrassComponent>(e);
    if (!g) return;
    g->wind_direction = glm::vec2(dx, dy);
    g->wind_speed = speed;
    g->wind_strength = strength;
    g->wind_turbulence = turbulence;
}

extern "C" void dse_grass_set_lod(uint32_t e, float near_dist, float far_dist,
                                   int cast_shadow, float shadow_dist) {
    auto* g = GetComp<GrassComponent>(e);
    if (!g) return;
    g->lod_near = near_dist;
    g->lod_far = far_dist;
    g->cast_shadow = (cast_shadow != 0);
    g->shadow_distance = shadow_dist;
}

extern "C" void dse_grass_set_enabled(uint32_t e, int enabled) {
    auto* g = GetComp<GrassComponent>(e);
    if (!g) return;
    g->enabled = (enabled != 0);
}

extern "C" int dse_grass_get_stats(uint32_t e) {
    auto* g = GetComp<GrassComponent>(e);
    if (!g) return 0;
    return g->cached_instance_count_;
}

// ============================================================
// Foliage
// ============================================================

extern "C" void dse_foliage_add(uint32_t e) {
    auto& fc = EmplaceComp<FoliageComponent>(e);
    fc.enabled = true;
}

// foliage field get/set 由 dse_api_foliage.gen.cpp 提供（codegen 生成）

// ============================================================
// Tree / TerrainTileManager / DynamicObstacle / NavMeshRebake — add
// ============================================================

extern "C" void dse_tree_add(uint32_t e, const char* mesh_path) {
    auto& tree = EmplaceComp<TreeComponent>(e);
    tree.enabled = true;
    tree.mesh_path = mesh_path ? mesh_path : "";
}

extern "C" void dse_terrain_tile_manager_add(uint32_t e) {
    auto& ttm = EmplaceComp<TerrainTileManagerComponent>(e);
    ttm.enabled = true;
}

extern "C" void dse_dynamic_obstacle_add(uint32_t e, int shape) {
    auto& obs = EmplaceComp<DynamicObstacleComponent>(e);
    obs.enabled = true;
    obs.shape = (shape == 1)
        ? DynamicObstacleComponent::Shape::Cylinder
        : DynamicObstacleComponent::Shape::Box;
}

extern "C" void dse_navmesh_rebake_add(uint32_t e) {
    auto& nr = EmplaceComp<NavMeshAutoRebakeComponent>(e);
    nr.enabled = true;
}

// ============================================================
// UI — 扩展控件
// ============================================================

extern "C" void dse_ui_add_label(uint32_t e, const char* text, uint32_t font_tex_handle,
                                  float r, float g, float b, float a,
                                  float glyph_w, float glyph_h, float spacing,
                                  int atlas_cols, int atlas_rows, int ascii_start,
                                  float offset_x, float offset_y) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& ui = w->registry().emplace_or_replace<UIRendererComponent>(TE(e));
    ui.texture_handle = dse::render::TextureHandle::from_raw(font_tex_handle);
    ui.color = glm::vec4(r, g, b, 0.0f);
    ui.visible = true;
    ui.interactable = false;
    ui.position = glm::vec2(offset_x, offset_y);
    ui.size = glm::vec2(0.0f, 0.0f);
    auto& label = w->registry().emplace_or_replace<UILabelComponent>(TE(e));
    label.text = text ? text : "";
    label.font_texture_handle =
        dse::render::TextureHandle::from_raw(font_tex_handle);
    label.color = glm::vec4(r, g, b, a);
    if (glyph_w > 0.0f && glyph_h > 0.0f) label.glyph_size = glm::vec2(glyph_w, glyph_h);
    if (spacing != 0.0f) label.spacing = spacing;
    if (atlas_cols > 0) label.atlas_cols = atlas_cols;
    if (atlas_rows > 0) label.atlas_rows = atlas_rows;
    if (ascii_start > 0) label.ascii_start = ascii_start;
    label.dirty = true;
}

extern "C" void dse_ui_set_label_number(uint32_t e, long long number) {
    auto* label = GetComp<UILabelComponent>(e);
    if (!label) return;
    label->numeric_mode = true;
    label->number_value = number;
    label->dirty = true;
}

extern "C" void dse_ui_set_label_layout(uint32_t e, float max_width, int align,
                                         int overflow, int max_lines, float line_spacing) {
    auto* label = GetComp<UILabelComponent>(e);
    if (!label) return;
    label->max_width = max_width;
    label->text_align = align;
    label->overflow_mode = overflow;
    label->max_lines = max_lines;
    label->line_spacing_extra = line_spacing;
    label->dirty = true;
}

extern "C" void dse_ui_add_mask(uint32_t e, float w, float h, float ox, float oy, int block_outside) {
    auto& mask = EmplaceComp<UIMaskComponent>(e);
    mask.size = glm::vec2(w, h);
    mask.offset = glm::vec2(ox, oy);
    mask.block_outside_input = (block_outside != 0);
    mask.enabled = true;
}

extern "C" void dse_ui_add_rich_text(uint32_t e, const char* text,
                                      float r, float g, float b, float a, int shadow, int outline) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& rich = w->registry().emplace_or_replace<UIRichTextComponent>(TE(e));
    rich.text = text ? text : "";
    rich.default_color = glm::vec4(r, g, b, a);
    rich.enable_shadow = (shadow != 0);
    rich.enable_outline = (outline != 0);
    rich.dirty = true;
    if (!w->registry().all_of<UILabelComponent>(TE(e)))
        w->registry().emplace<UILabelComponent>(TE(e));
    if (!w->registry().all_of<UIRendererComponent>(TE(e)))
        w->registry().emplace<UIRendererComponent>(TE(e));
    w->registry().get<UILabelComponent>(TE(e)).dirty = true;
}

extern "C" void dse_ui_set_rich_text(uint32_t e, const char* text) {
    auto* rich = GetComp<UIRichTextComponent>(e);
    if (!rich) return;
    rich->text = text ? text : "";
    rich->dirty = true;
    auto* label = GetComp<UILabelComponent>(e);
    if (label) label->dirty = true;
}

extern "C" void dse_ui_set_button_scale(uint32_t e, float hover, float pressed, float lerp_speed) {
    auto* ui = GetComp<UIRendererComponent>(e);
    if (!ui) return;
    ui->hover_scale = hover;
    ui->pressed_scale = pressed;
    ui->scale_lerp_speed = lerp_speed;
}

extern "C" void dse_ui_set_uv(uint32_t e, float u, float v, float w, float h) {
    auto* ui = GetComp<UIRendererComponent>(e);
    if (!ui) return;
    ui->uv = glm::vec4(u, v, w, h);
}

extern "C" void dse_ui_set_nine_slice(uint32_t e, int enabled, float l, float b, float r, float t,
                                       float sw, float sh) {
    auto* ui = GetComp<UIRendererComponent>(e);
    if (!ui) return;
    ui->nine_slice_enabled = (enabled != 0);
    ui->nine_slice_border = glm::vec4(l, b, r, t);
    ui->nine_slice_src_size = glm::vec2(sw, sh);
}

extern "C" void dse_ui_add_anchor(uint32_t e, int anchor_type, float ox, float oy) {
    auto& anchor = EmplaceComp<UIAnchorComponent>(e);
    anchor.anchor = anchor_type;
    anchor.offset = glm::vec2(ox, oy);
}

extern "C" void dse_ui_set_anchor_type(uint32_t e, int anchor_type) {
    auto* anchor = GetComp<UIAnchorComponent>(e);
    if (anchor) anchor->anchor = anchor_type;
}

extern "C" void dse_ui_set_anchor_offset(uint32_t e, float ox, float oy) {
    auto* anchor = GetComp<UIAnchorComponent>(e);
    if (anchor) { anchor->offset.x = ox; anchor->offset.y = oy; }
}

extern "C" void dse_ui_add_grid_layout(uint32_t e, int columns, float cw, float ch,
                                        float sx, float sy) {
    auto& grid = EmplaceComp<UIGridLayoutComponent>(e);
    grid.columns = columns;
    grid.cell_size = glm::vec2(cw, ch);
    grid.spacing = glm::vec2(sx, sy);
}

extern "C" void dse_ui_set_grid_layout(uint32_t e, int columns, int rows,
                                        float cw, float ch, float sx, float sy, int alignment) {
    auto* grid = GetComp<UIGridLayoutComponent>(e);
    if (!grid) return;
    grid->columns = columns;
    grid->rows = rows;
    grid->cell_size = glm::vec2(cw, ch);
    grid->spacing = glm::vec2(sx, sy);
    grid->alignment = alignment;
}

extern "C" void dse_ui_add_canvas_scaler(uint32_t e, float ref_w, float ref_h, int match) {
    auto& scaler = EmplaceComp<UICanvasScalerComponent>(e);
    scaler.reference_resolution = glm::vec2(ref_w, ref_h);
    scaler.match_width_or_height = (match != 0);
}

extern "C" void dse_ui_set_canvas_scaler(uint32_t e, float ref_w, float ref_h,
                                          float scale_factor, int match_wh, float match, int pixel_snap) {
    auto* scaler = GetComp<UICanvasScalerComponent>(e);
    if (!scaler) return;
    scaler->reference_resolution = glm::vec2(ref_w, ref_h);
    scaler->scale_factor = scale_factor;
    scaler->match_width_or_height = (match_wh != 0);
    scaler->match = match;
    scaler->pixel_snap = (pixel_snap != 0);
}

extern "C" void dse_ui_add_box_layout(uint32_t e, int vertical, float spacing,
                                       float pad_x, float pad_y, int align_main, int align_cross, int reverse) {
    auto& box = EmplaceComp<UIBoxLayoutComponent>(e);
    box.vertical = (vertical != 0);
    box.spacing = spacing;
    box.padding = glm::vec2(pad_x, pad_y);
    box.align_main = align_main;
    box.align_cross = align_cross;
    box.reverse = (reverse != 0);
}

extern "C" void dse_ui_set_box_layout(uint32_t e, int vertical, float spacing,
                                       float pad_x, float pad_y, int align_main, int align_cross, int reverse) {
    auto* box = GetComp<UIBoxLayoutComponent>(e);
    if (!box) return;
    box->vertical = (vertical != 0);
    box->spacing = spacing;
    box->padding = glm::vec2(pad_x, pad_y);
    box->align_main = align_main;
    box->align_cross = align_cross;
    box->reverse = (reverse != 0);
}

extern "C" void dse_ui_add_content_size_fitter(uint32_t e, int fit_w, int fit_h,
                                                 float min_w, float min_h, float max_w, float max_h) {
    auto& fitter = EmplaceComp<UIContentSizeFitterComponent>(e);
    fitter.fit_width = fit_w;
    fitter.fit_height = fit_h;
    fitter.min_size = glm::vec2(min_w, min_h);
    fitter.max_size = glm::vec2(max_w, max_h);
}

extern "C" void dse_ui_add_animation(uint32_t e, float duration, int easing,
                                      int loop, int ping_pong, float delay) {
    auto& anim = EmplaceComp<UIAnimationComponent>(e);
    anim.duration = duration;
    anim.easing = easing;
    anim.loop = (loop != 0);
    anim.ping_pong = (ping_pong != 0);
    anim.delay = delay;
}

extern "C" void dse_ui_animate_position(uint32_t e, float tx, float ty) {
    auto* anim = GetComp<UIAnimationComponent>(e);
    if (!anim) return;
    anim->target_position = glm::vec2(tx, ty);
    anim->animate_position = true;
    anim->playing = true;
    anim->elapsed = 0.0f;
    anim->delay_remaining = anim->delay;
}

extern "C" void dse_ui_animate_scale(uint32_t e, float sx, float sy) {
    auto* anim = GetComp<UIAnimationComponent>(e);
    if (!anim) return;
    anim->target_scale = glm::vec2(sx, sy);
    anim->animate_scale = true;
    anim->playing = true;
    anim->elapsed = 0.0f;
    anim->delay_remaining = anim->delay;
}

extern "C" void dse_ui_animate_alpha(uint32_t e, float alpha) {
    auto* anim = GetComp<UIAnimationComponent>(e);
    if (!anim) return;
    anim->target_alpha = alpha;
    anim->animate_alpha = true;
    anim->playing = true;
    anim->elapsed = 0.0f;
    anim->delay_remaining = anim->delay;
}

extern "C" void dse_ui_animate_color(uint32_t e, float r, float g, float b, float a) {
    auto* anim = GetComp<UIAnimationComponent>(e);
    if (!anim) return;
    anim->target_color = glm::vec4(r, g, b, a);
    anim->animate_color = true;
    anim->playing = true;
    anim->elapsed = 0.0f;
    anim->delay_remaining = anim->delay;
}

extern "C" void dse_ui_stop_animation(uint32_t e) {
    auto* anim = GetComp<UIAnimationComponent>(e);
    if (anim) anim->playing = false;
}

extern "C" void dse_ui_set_text_input_placeholder(uint32_t e, const char* placeholder) {
    auto* input = GetComp<UITextInputComponent>(e);
    if (input) input->placeholder = placeholder ? placeholder : "";
}

extern "C" void dse_ui_add_scroll_view(uint32_t e, float cw, float ch, int horizontal, int vertical) {
    auto& sv = EmplaceComp<UIScrollViewComponent>(e);
    sv.content_size = glm::vec2(cw, ch);
    sv.horizontal = (horizontal != 0);
    sv.vertical = (vertical != 0);
}

extern "C" void dse_ui_set_scroll_offset(uint32_t e, float x, float y) {
    auto* sv = GetComp<UIScrollViewComponent>(e);
    if (sv) { sv->scroll_offset.x = x; sv->scroll_offset.y = y; }
}

extern "C" void dse_ui_get_scroll_offset(uint32_t e, float* out_x, float* out_y) {
    auto* sv = GetComp<UIScrollViewComponent>(e);
    if (!sv) { if (out_x) *out_x = 0; if (out_y) *out_y = 0; return; }
    if (out_x) *out_x = sv->scroll_offset.x;
    if (out_y) *out_y = sv->scroll_offset.y;
}

extern "C" void dse_ui_set_scroll_content_size(uint32_t e, float w, float h) {
    auto* sv = GetComp<UIScrollViewComponent>(e);
    if (sv) { sv->content_size.x = w; sv->content_size.y = h; }
}

extern "C" void dse_ui_set_slider_colors(uint32_t e, float tr, float tg, float tb, float ta,
                                          float fr, float fg, float fb, float fa,
                                          float hr, float hg, float hb, float ha) {
    auto* s = GetComp<UISliderComponent>(e);
    if (!s) return;
    s->track_color = glm::vec4(tr, tg, tb, ta);
    s->fill_color = glm::vec4(fr, fg, fb, fa);
    s->handle_color = glm::vec4(hr, hg, hb, ha);
}

extern "C" void dse_ui_set_slider_handle_size(uint32_t e, float size) {
    auto* s = GetComp<UISliderComponent>(e);
    if (s) s->handle_size = size;
}

extern "C" void dse_ui_set_slider_vertical(uint32_t e, int vertical) {
    auto* s = GetComp<UISliderComponent>(e);
    if (s) s->vertical = (vertical != 0);
}

extern "C" void dse_ui_set_slider_range(uint32_t e, float min_val, float max_val) {
    auto* s = GetComp<UISliderComponent>(e);
    if (s) { s->min_value = min_val; s->max_value = max_val; }
}

extern "C" void dse_ui_add_dropdown(uint32_t e, float item_height, int max_visible) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& dd = w->registry().emplace_or_replace<UIDropdownComponent>(TE(e));
    dd.item_height = item_height;
    dd.max_visible_items = max_visible;
    if (!w->registry().all_of<UIRendererComponent>(TE(e)))
        w->registry().emplace<UIRendererComponent>(TE(e));
}

extern "C" void dse_ui_dropdown_add_option(uint32_t e, const char* text, const char* value) {
    auto* dd = GetComp<UIDropdownComponent>(e);
    if (!dd || !text) return;
    UIDropdownOption opt;
    opt.text = text;
    opt.value = value ? value : opt.text;
    dd->options.push_back(std::move(opt));
}

extern "C" void dse_ui_dropdown_clear_options(uint32_t e) {
    auto* dd = GetComp<UIDropdownComponent>(e);
    if (dd) dd->options.clear();
}

extern "C" void dse_ui_set_dropdown_index(uint32_t e, int index) {
    auto* dd = GetComp<UIDropdownComponent>(e);
    if (dd) dd->selected_index = index;
}

extern "C" int dse_ui_get_dropdown_index(uint32_t e) {
    auto* dd = GetComp<UIDropdownComponent>(e);
    return dd ? dd->selected_index : -1;
}

extern "C" int dse_ui_get_dropdown_value(uint32_t e, char* out, int cap) {
    auto* dd = GetComp<UIDropdownComponent>(e);
    if (!dd || !out || cap <= 0) return 0;
    std::string val = dd->GetSelectedValue();
    int n = static_cast<int>(val.size());
    if (n > cap - 1) n = cap - 1;
    std::memcpy(out, val.data(), static_cast<size_t>(n));
    out[n] = '\0';
    return n;
}

extern "C" void dse_ui_set_dropdown_open(uint32_t e, int open) {
    auto* dd = GetComp<UIDropdownComponent>(e);
    if (dd) dd->is_open = (open != 0);
}

extern "C" void dse_ui_add_filled_image(uint32_t e, float fill_amount, int method, int origin, int clockwise) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& fi = w->registry().emplace_or_replace<UIFilledImageComponent>(TE(e));
    fi.fill_amount = fill_amount;
    fi.fill_method = (method >= 0 && method <= 4) ? static_cast<UIFillMethod>(method) : UIFillMethod::Horizontal;
    fi.fill_origin = (origin >= 0 && origin <= 4) ? static_cast<UIFillOrigin>(origin) : UIFillOrigin::Left;
    fi.clockwise = (clockwise != 0);
    if (!w->registry().all_of<UIRendererComponent>(TE(e)))
        w->registry().emplace<UIRendererComponent>(TE(e));
}

extern "C" void dse_ui_set_fill_amount(uint32_t e, float amount) {
    auto* fi = GetComp<UIFilledImageComponent>(e);
    if (fi) fi->fill_amount = amount;
}

extern "C" float dse_ui_get_fill_amount(uint32_t e) {
    auto* fi = GetComp<UIFilledImageComponent>(e);
    return fi ? fi->fill_amount : 0.0f;
}

extern "C" void dse_ui_set_fill_method(uint32_t e, int method, int origin, int clockwise) {
    auto* fi = GetComp<UIFilledImageComponent>(e);
    if (!fi) return;
    fi->fill_method = (method >= 0 && method <= 4) ? static_cast<UIFillMethod>(method) : UIFillMethod::Horizontal;
    fi->fill_origin = (origin >= 0 && origin <= 4) ? static_cast<UIFillOrigin>(origin) : UIFillOrigin::Left;
    fi->clockwise = (clockwise != 0);
}

extern "C" void dse_ui_add_focus_navigable(uint32_t e, int tab_index) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& fn = w->registry().emplace_or_replace<UIFocusNavigableComponent>(TE(e));
    fn.tab_index = tab_index;
    if (!w->registry().all_of<UIRendererComponent>(TE(e)))
        w->registry().emplace<UIRendererComponent>(TE(e));
}

extern "C" void dse_ui_set_focus_nav(uint32_t e, uint32_t up, uint32_t down, uint32_t left, uint32_t right) {
    auto* fn = GetComp<UIFocusNavigableComponent>(e);
    if (!fn) return;
    fn->nav_up = TE(up);
    fn->nav_down = TE(down);
    fn->nav_left = TE(left);
    fn->nav_right = TE(right);
}

extern "C" int dse_ui_is_focused(uint32_t e) {
    auto* fn = GetComp<UIFocusNavigableComponent>(e);
    return (fn && fn->is_focused) ? 1 : 0;
}

extern "C" void dse_ui_set_focus_tint(uint32_t e, float r, float g, float b, float a) {
    auto* fn = GetComp<UIFocusNavigableComponent>(e);
    if (fn) fn->focus_tint = glm::vec4(r, g, b, a);
}

extern "C" void dse_ui_add_event_propagation(uint32_t e, int bubbles_click, int bubbles_hover) {
    auto& ep = EmplaceComp<UIEventPropagationComponent>(e);
    ep.bubbles_click = (bubbles_click != 0);
    ep.bubbles_hover = (bubbles_hover != 0);
}

extern "C" void dse_ui_stop_propagation(uint32_t e) {
    auto* ep = GetComp<UIEventPropagationComponent>(e);
    if (ep) ep->stop_propagation = true;
}

extern "C" void dse_ui_add_visual_effect(uint32_t e) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    w->registry().emplace_or_replace<UIVisualEffectComponent>(TE(e));
    if (!w->registry().all_of<UIRendererComponent>(TE(e)))
        w->registry().emplace<UIRendererComponent>(TE(e));
}

extern "C" void dse_ui_set_corner_radius(uint32_t e, float radius) {
    auto* vfx = GetComp<UIVisualEffectComponent>(e);
    if (vfx) vfx->corner_radius = radius;
}

extern "C" void dse_ui_set_gradient(uint32_t e, float sr, float sg, float sb, float sa,
                                     float er, float eg, float eb, float ea, int direction) {
    auto* vfx = GetComp<UIVisualEffectComponent>(e);
    if (!vfx) return;
    vfx->gradient_color_start = glm::vec4(sr, sg, sb, sa);
    vfx->gradient_color_end = glm::vec4(er, eg, eb, ea);
    vfx->gradient_direction = (direction >= 0 && direction <= 2)
        ? static_cast<UIGradientDirection>(direction) : UIGradientDirection::Vertical;
}

extern "C" void dse_ui_set_blur(uint32_t e, float radius, float intensity) {
    auto* vfx = GetComp<UIVisualEffectComponent>(e);
    if (!vfx) return;
    vfx->blur_radius = radius;
    vfx->blur_intensity = intensity;
}

extern "C" void dse_ui_add_virtual_scroll(uint32_t e, int total_items, float item_height) {
    World* w = GW();
    if (!w || !w->registry().valid(TE(e))) return;
    auto& vs = w->registry().emplace_or_replace<UIVirtualScrollComponent>(TE(e));
    vs.total_item_count = total_items;
    vs.item_height = item_height;
    if (!w->registry().all_of<UIScrollViewComponent>(TE(e)))
        w->registry().emplace<UIScrollViewComponent>(TE(e));
    if (!w->registry().all_of<UIRendererComponent>(TE(e)))
        w->registry().emplace<UIRendererComponent>(TE(e));
}

extern "C" void dse_ui_set_virtual_scroll_count(uint32_t e, int count) {
    auto* vs = GetComp<UIVirtualScrollComponent>(e);
    if (!vs) return;
    vs->total_item_count = count;
    vs->dirty = true;
}

extern "C" void dse_ui_get_virtual_scroll_range(uint32_t e, int* out_start, int* out_end) {
    auto* vs = GetComp<UIVirtualScrollComponent>(e);
    if (!vs) { if (out_start) *out_start = 0; if (out_end) *out_end = 0; return; }
    if (out_start) *out_start = vs->visible_start_index;
    if (out_end) *out_end = vs->visible_end_index;
}

extern "C" void dse_ui_destroy_virtual_scroll(uint32_t e) {
    World* w = GW();
    if (!w) return;
    auto* vs = w->registry().try_get<UIVirtualScrollComponent>(TE(e));
    if (!vs) return;
    for (auto pool_e : vs->pool_entities) {
        if (w->registry().valid(pool_e)) w->registry().destroy(pool_e);
    }
    vs->pool_entities.clear();
    vs->pool_size = 0;
    w->registry().remove<UIVirtualScrollComponent>(TE(e));
}
