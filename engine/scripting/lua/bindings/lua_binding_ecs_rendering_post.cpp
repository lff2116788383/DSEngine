/**
 * @file lua_binding_ecs_rendering_post.cpp
 * @brief PostProcess / Decal Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）
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


} // namespace

void RegisterEcsRenderingPostBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
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
    });
}

} // namespace dse::runtime::lua_binding
