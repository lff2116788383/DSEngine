/**
 * @file lua_binding_2d_systems.cpp
 * @brief Lua 绑定 — 2D 扩展系统 (Parallax, Light2D, Trail, Line, CameraController, AudioSpatial, SpriteSheet, Atlas)
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/parallax_2d.h"
#include "engine/ecs/light_2d.h"
#include "engine/ecs/trail_renderer_2d.h"
#include "engine/ecs/line_renderer_2d.h"
#include "engine/ecs/camera_controller_2d.h"
#include "engine/ecs/audio_spatial_2d.h"
#include "engine/assets/sprite_sheet_asset.h"
#include "engine/assets/atlas_asset.h"
#include "engine/core/service_locator.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

// ============================================================
// #1 Parallax
// ============================================================

int L_AddParallax(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    world->registry().emplace_or_replace<ParallaxComponent>(e);
    return 0;
}

int L_ParallaxAddLayer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pc = helper::TryGetComponent<ParallaxComponent>(*world, e);
    if (!pc) return 0;

    ParallaxLayer layer;
    layer.scroll_factor_x = helper::OptFloat(L, 2, 1.0f);
    layer.scroll_factor_y = helper::OptFloat(L, 3, 1.0f);
    layer.sorting_order = (int)pc->layers.size();
    pc->layers.push_back(layer);

    lua_pushinteger(L, (int)pc->layers.size() - 1);
    return 1;
}

int L_ParallaxSetLayerScroll(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    float sx = helper::CheckFloat(L, 3);
    float sy = helper::CheckFloat(L, 4);
    auto* pc = helper::TryGetComponent<ParallaxComponent>(*world, e);
    if (!pc || idx < 0 || idx >= (int)pc->layers.size()) return 0;
    pc->layers[idx].scroll_factor_x = sx;
    pc->layers[idx].scroll_factor_y = sy;
    return 0;
}

int L_ParallaxSetLayerAutoScroll(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    float asx = helper::CheckFloat(L, 3);
    float asy = helper::OptFloat(L, 4, 0.0f);
    auto* pc = helper::TryGetComponent<ParallaxComponent>(*world, e);
    if (!pc || idx < 0 || idx >= (int)pc->layers.size()) return 0;
    pc->layers[idx].auto_scroll_x = asx;
    pc->layers[idx].auto_scroll_y = asy;
    return 0;
}

int L_ParallaxSetLayerOpacity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    float opacity = helper::CheckFloat(L, 3);
    auto* pc = helper::TryGetComponent<ParallaxComponent>(*world, e);
    if (!pc || idx < 0 || idx >= (int)pc->layers.size()) return 0;
    pc->layers[idx].opacity = opacity;
    return 0;
}

int L_ParallaxGetLayerCount(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* pc = helper::TryGetComponent<ParallaxComponent>(*world, e);
    lua_pushinteger(L, pc ? (int)pc->layers.size() : 0);
    return 1;
}

// ============================================================
// #2 Light2D
// ============================================================

int L_AddLight2D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int type = helper::OptInt(L, 2, 0);
    auto& lc = world->registry().emplace_or_replace<Light2DComponent>(e);
    lc.type = static_cast<Light2DType>(type);
    return 0;
}

int L_SetLight2DColor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float r = helper::CheckFloat(L, 2);
    float g = helper::CheckFloat(L, 3);
    float b = helper::CheckFloat(L, 4);
    auto* lc = helper::TryGetComponent<Light2DComponent>(*world, e);
    if (lc) lc->color = glm::vec3(r, g, b);
    return 0;
}

int L_SetLight2DIntensity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float intensity = helper::CheckFloat(L, 2);
    auto* lc = helper::TryGetComponent<Light2DComponent>(*world, e);
    if (lc) lc->intensity = intensity;
    return 0;
}

int L_SetLight2DRange(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float range = helper::CheckFloat(L, 2);
    auto* lc = helper::TryGetComponent<Light2DComponent>(*world, e);
    if (lc) lc->range = range;
    return 0;
}

int L_SetLight2DShadow(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int mode = helper::CheckInt(L, 2);
    auto* lc = helper::TryGetComponent<Light2DComponent>(*world, e);
    if (lc) lc->shadow_mode = static_cast<Shadow2DMode>(mode);
    return 0;
}

int L_SetAmbient2D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float r = helper::CheckFloat(L, 2);
    float g = helper::CheckFloat(L, 3);
    float b = helper::CheckFloat(L, 4);
    float intensity = helper::OptFloat(L, 5, 0.5f);
    auto& amb = world->registry().emplace_or_replace<Ambient2DComponent>(e);
    amb.color = glm::vec3(r, g, b);
    amb.intensity = intensity;
    return 0;
}

int L_AddNormalMap2D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float strength = helper::OptFloat(L, 2, 1.0f);
    auto& nm = world->registry().emplace_or_replace<NormalMap2DComponent>(e);
    nm.normal_strength = strength;
    return 0;
}

// ============================================================
// #3 SpriteSheet
// ============================================================

static std::vector<SpriteSheetAsset> s_loaded_sheets;

int L_LoadSpriteSheet(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    SpriteSheetAsset sheet;
    if (sheet.LoadFromFile(path)) {
        s_loaded_sheets.push_back(std::move(sheet));
        lua_pushinteger(L, (int)s_loaded_sheets.size() - 1);
    } else {
        lua_pushinteger(L, -1);
    }
    return 1;
}

int L_SpriteSheetFrameCount(lua_State* L) {
    int idx = helper::CheckInt(L, 1);
    if (idx >= 0 && idx < (int)s_loaded_sheets.size()) {
        lua_pushinteger(L, (int)s_loaded_sheets[idx].frames.size());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

int L_SpriteSheetGetFrameUV(lua_State* L) {
    int sheet_idx = helper::CheckInt(L, 1);
    int frame_idx = helper::CheckInt(L, 2);
    if (sheet_idx >= 0 && sheet_idx < (int)s_loaded_sheets.size()) {
        glm::vec4 uv = s_loaded_sheets[sheet_idx].GetFrameUV(frame_idx);
        lua_pushnumber(L, uv.x);
        lua_pushnumber(L, uv.y);
        lua_pushnumber(L, uv.z);
        lua_pushnumber(L, uv.w);
        return 4;
    }
    lua_pushnumber(L, 0); lua_pushnumber(L, 0);
    lua_pushnumber(L, 1); lua_pushnumber(L, 1);
    return 4;
}

// ============================================================
// #4 Atlas
// ============================================================

static std::vector<AtlasAsset> s_loaded_atlases;

int L_LoadAtlas(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    AtlasAsset atlas;
    if (atlas.LoadFromFile(path)) {
        s_loaded_atlases.push_back(std::move(atlas));
        lua_pushinteger(L, (int)s_loaded_atlases.size() - 1);
    } else {
        lua_pushinteger(L, -1);
    }
    return 1;
}

int L_AtlasEntryCount(lua_State* L) {
    int idx = helper::CheckInt(L, 1);
    if (idx >= 0 && idx < (int)s_loaded_atlases.size()) {
        lua_pushinteger(L, (int)s_loaded_atlases[idx].entries.size());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

int L_AtlasGetEntryUV(lua_State* L) {
    int atlas_idx = helper::CheckInt(L, 1);
    const char* name = luaL_checkstring(L, 2);
    if (atlas_idx >= 0 && atlas_idx < (int)s_loaded_atlases.size()) {
        glm::vec4 uv = s_loaded_atlases[atlas_idx].GetEntryUV(name);
        lua_pushnumber(L, uv.x);
        lua_pushnumber(L, uv.y);
        lua_pushnumber(L, uv.z);
        lua_pushnumber(L, uv.w);
        return 4;
    }
    lua_pushnumber(L, 0); lua_pushnumber(L, 0);
    lua_pushnumber(L, 1); lua_pushnumber(L, 1);
    return 4;
}

// ============================================================
// #5 Camera Controller 2D
// ============================================================

int L_AddCameraController2D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    world->registry().emplace_or_replace<CameraController2DComponent>(e);
    return 0;
}

int L_CameraShake(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float trauma = helper::CheckFloat(L, 2);
    auto* ctrl = helper::TryGetComponent<CameraController2DComponent>(*world, e);
    if (ctrl) {
        ctrl->shake.trauma = std::min(ctrl->shake.trauma + trauma, 1.0f);
    }
    return 0;
}

int L_CameraSetZoom(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float zoom = helper::CheckFloat(L, 2);
    auto* ctrl = helper::TryGetComponent<CameraController2DComponent>(*world, e);
    if (ctrl) ctrl->target_zoom = zoom;
    return 0;
}

int L_CameraSetBounds(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float min_x = helper::CheckFloat(L, 2);
    float min_y = helper::CheckFloat(L, 3);
    float max_x = helper::CheckFloat(L, 4);
    float max_y = helper::CheckFloat(L, 5);
    auto* ctrl = helper::TryGetComponent<CameraController2DComponent>(*world, e);
    if (ctrl) {
        ctrl->bounds.enabled = true;
        ctrl->bounds.min_x = min_x;
        ctrl->bounds.min_y = min_y;
        ctrl->bounds.max_x = max_x;
        ctrl->bounds.max_y = max_y;
    }
    return 0;
}

int L_CameraSetLookAhead(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float lax = helper::CheckFloat(L, 2);
    float lay = helper::CheckFloat(L, 3);
    auto* ctrl = helper::TryGetComponent<CameraController2DComponent>(*world, e);
    if (ctrl) {
        ctrl->look_ahead_x = lax;
        ctrl->look_ahead_y = lay;
    }
    return 0;
}

// ============================================================
// #6 Trail Renderer
// ============================================================

int L_AddTrailRenderer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& trail = world->registry().emplace_or_replace<TrailRenderer2DComponent>(e);
    trail.lifetime = helper::OptFloat(L, 2, 0.5f);
    trail.start_width = helper::OptFloat(L, 3, 0.5f);
    trail.end_width = helper::OptFloat(L, 4, 0.0f);
    return 0;
}

int L_SetTrailEmitting(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool emitting = lua_toboolean(L, 2) != 0;
    auto* trail = helper::TryGetComponent<TrailRenderer2DComponent>(*world, e);
    if (trail) trail->emitting = emitting;
    return 0;
}

int L_SetTrailColors(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float sr = helper::CheckFloat(L, 2);
    float sg = helper::CheckFloat(L, 3);
    float sb = helper::CheckFloat(L, 4);
    float sa = helper::OptFloat(L, 5, 1.0f);
    float er = helper::OptFloat(L, 6, sr);
    float eg = helper::OptFloat(L, 7, sg);
    float eb = helper::OptFloat(L, 8, sb);
    float ea = helper::OptFloat(L, 9, 0.0f);
    auto* trail = helper::TryGetComponent<TrailRenderer2DComponent>(*world, e);
    if (trail) {
        trail->start_color = glm::vec4(sr, sg, sb, sa);
        trail->end_color = glm::vec4(er, eg, eb, ea);
    }
    return 0;
}

int L_ClearTrail(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* trail = helper::TryGetComponent<TrailRenderer2DComponent>(*world, e);
    if (trail) trail->points.clear();
    return 0;
}

// ============================================================
// #7 Line Renderer
// ============================================================

int L_AddLineRenderer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& line = world->registry().emplace_or_replace<LineRenderer2DComponent>(e);
    line.width = helper::OptFloat(L, 2, 0.1f);
    return 0;
}

int L_LineRendererSetPoints(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* line = helper::TryGetComponent<LineRenderer2DComponent>(*world, e);
    if (!line) return 0;

    // Expects a table of {x, y} pairs
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 2);
    line->points.clear();
    line->points.reserve(n);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 2, i);
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1);
            float x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            lua_rawgeti(L, -1, 2);
            float y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            line->points.push_back(glm::vec2(x, y));
        }
        lua_pop(L, 1);
    }
    return 0;
}

int L_LineRendererSetWidth(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float w = helper::CheckFloat(L, 2);
    auto* line = helper::TryGetComponent<LineRenderer2DComponent>(*world, e);
    if (line) line->width = w;
    return 0;
}

int L_LineRendererSetColor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float r = helper::CheckFloat(L, 2);
    float g = helper::CheckFloat(L, 3);
    float b = helper::CheckFloat(L, 4);
    float a = helper::OptFloat(L, 5, 1.0f);
    auto* line = helper::TryGetComponent<LineRenderer2DComponent>(*world, e);
    if (line) {
        line->start_color = glm::vec4(r, g, b, a);
        line->end_color = glm::vec4(r, g, b, a);
    }
    return 0;
}

int L_LineRendererSetClosed(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool closed = lua_toboolean(L, 2) != 0;
    auto* line = helper::TryGetComponent<LineRenderer2DComponent>(*world, e);
    if (line) line->closed = closed;
    return 0;
}

// ============================================================
// #8 Audio Spatial 2D
// ============================================================

int L_AddAudioSpatial2D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& spatial = world->registry().emplace_or_replace<AudioSpatial2DComponent>(e);
    spatial.min_distance = helper::OptFloat(L, 2, 1.0f);
    spatial.max_distance = helper::OptFloat(L, 3, 20.0f);
    return 0;
}

int L_SetAudioSpatial2DRange(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float min_d = helper::CheckFloat(L, 2);
    float max_d = helper::CheckFloat(L, 3);
    auto* sp = helper::TryGetComponent<AudioSpatial2DComponent>(*world, e);
    if (sp) {
        sp->min_distance = min_d;
        sp->max_distance = max_d;
    }
    return 0;
}

int L_SetAudioSpatial2DAttenuation(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int model = helper::CheckInt(L, 2);
    float rolloff = helper::OptFloat(L, 3, 1.0f);
    auto* sp = helper::TryGetComponent<AudioSpatial2DComponent>(*world, e);
    if (sp) {
        sp->attenuation = static_cast<AudioAttenuation2DModel>(model);
        sp->rolloff = rolloff;
    }
    return 0;
}

int L_AddAudioListener2D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& al = world->registry().emplace_or_replace<AudioListener2DComponent>(e);
    al.global_volume = helper::OptFloat(L, 2, 1.0f);
    return 0;
}

} // namespace

void Register2DSystemsBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        // Parallax
        {"add_parallax",                L_AddParallax},
        {"parallax_add_layer",          L_ParallaxAddLayer},
        {"parallax_set_layer_scroll",   L_ParallaxSetLayerScroll},
        {"parallax_set_layer_auto_scroll", L_ParallaxSetLayerAutoScroll},
        {"parallax_set_layer_opacity",  L_ParallaxSetLayerOpacity},
        {"parallax_get_layer_count",    L_ParallaxGetLayerCount},
        // Light2D
        {"add_light_2d",                L_AddLight2D},
        {"set_light_2d_color",          L_SetLight2DColor},
        {"set_light_2d_intensity",      L_SetLight2DIntensity},
        {"set_light_2d_range",          L_SetLight2DRange},
        {"set_light_2d_shadow",         L_SetLight2DShadow},
        {"set_ambient_2d",              L_SetAmbient2D},
        {"add_normal_map_2d",           L_AddNormalMap2D},
        // SpriteSheet
        {"load_sprite_sheet",           L_LoadSpriteSheet},
        {"sprite_sheet_frame_count",    L_SpriteSheetFrameCount},
        {"sprite_sheet_get_frame_uv",   L_SpriteSheetGetFrameUV},
        // Atlas
        {"load_atlas",                  L_LoadAtlas},
        {"atlas_entry_count",           L_AtlasEntryCount},
        {"atlas_get_entry_uv",          L_AtlasGetEntryUV},
        // Camera Controller
        {"add_camera_controller_2d",    L_AddCameraController2D},
        {"camera_shake",                L_CameraShake},
        {"camera_set_zoom",             L_CameraSetZoom},
        {"camera_set_bounds",           L_CameraSetBounds},
        {"camera_set_look_ahead",       L_CameraSetLookAhead},
        // Trail Renderer
        {"add_trail_renderer",          L_AddTrailRenderer},
        {"set_trail_emitting",          L_SetTrailEmitting},
        {"set_trail_colors",            L_SetTrailColors},
        {"clear_trail",                 L_ClearTrail},
        // Line Renderer
        {"add_line_renderer",           L_AddLineRenderer},
        {"line_renderer_set_points",    L_LineRendererSetPoints},
        {"line_renderer_set_width",     L_LineRendererSetWidth},
        {"line_renderer_set_color",     L_LineRendererSetColor},
        {"line_renderer_set_closed",    L_LineRendererSetClosed},
        // Audio Spatial 2D
        {"add_audio_spatial_2d",        L_AddAudioSpatial2D},
        {"set_audio_spatial_2d_range",  L_SetAudioSpatial2DRange},
        {"set_audio_spatial_2d_attenuation", L_SetAudioSpatial2DAttenuation},
        {"add_audio_listener_2d",       L_AddAudioListener2D},
    });
}

} // namespace dse::runtime::lua_binding
