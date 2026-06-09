/**
 * @file lua_binding_ecs_rendering_light.cpp
 * @brief Lights / Skybox / Probes Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）
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

// ============================================================
// Lights
// ============================================================

int L_EcsAddDirectionalLight3D(lua_State* L) {
    const uint32_t e = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    const float dir_x = helper::OptFloat(L, 2, -0.4f);
    const float dir_y = helper::OptFloat(L, 3, -1.0f);
    const float dir_z = helper::OptFloat(L, 4, -0.3f);
    const float r = helper::OptFloat(L, 5, 1.0f);
    const float g = helper::OptFloat(L, 6, 1.0f);
    const float b = helper::OptFloat(L, 7, 1.0f);
    const float intensity = helper::OptFloat(L, 8, 1.0f);
    const float ambient_intensity = helper::OptFloat(L, 9, 0.2f);
    const float shadow_strength = helper::OptFloat(L, 10, 0.35f);
    const glm::vec3 dir = glm::normalize(glm::vec3(dir_x, dir_y, dir_z));
    // S1.8-2：委托 C ABI（dse_dir_light_add 内 emplace_or_replace，enabled 默认 true；dir_light 无 set_enabled）
    dse_dir_light_add(e);
    dse_dir_light_set_direction(e, dir.x, dir.y, dir.z);
    dse_dir_light_set_color(e, r, g, b);
    dse_dir_light_set_intensity(e, intensity);
    dse_dir_light_set_ambient_intensity(e, ambient_intensity);
    dse_dir_light_set_shadow_strength(e, shadow_strength);
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
    const uint32_t e = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    const float r = helper::OptFloat(L, 2, 1.0f);
    const float g = helper::OptFloat(L, 3, 1.0f);
    const float b = helper::OptFloat(L, 4, 1.0f);
    const float intensity = helper::OptFloat(L, 5, 1.0f);
    const float radius = helper::OptFloat(L, 6, 10.0f);
    // S1.8-2：委托 C ABI（enabled 默认 true，与原显式赋值一致）
    dse_point_light_add(e);
    dse_point_light_set_enabled(e, 1);
    dse_point_light_set_color(e, r, g, b);
    dse_point_light_set_intensity(e, intensity);
    dse_point_light_set_radius(e, radius);
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
    const uint32_t e = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    const float dir_x = helper::OptFloat(L, 2, 0.0f);
    const float dir_y = helper::OptFloat(L, 3, -1.0f);
    const float dir_z = helper::OptFloat(L, 4, 0.0f);
    const float r = helper::OptFloat(L, 5, 1.0f);
    const float g = helper::OptFloat(L, 6, 1.0f);
    const float b = helper::OptFloat(L, 7, 1.0f);
    const float intensity = helper::OptFloat(L, 8, 1.0f);
    const float radius = helper::OptFloat(L, 9, 20.0f);
    const float inner_angle = helper::OptFloat(L, 10, 12.5f);
    const float outer_angle = helper::OptFloat(L, 11, 17.5f);
    const glm::vec3 dir = glm::normalize(glm::vec3(dir_x, dir_y, dir_z));
    // S1.8-2：委托 C ABI（enabled 默认 true）
    dse_spot_light_add(e);
    dse_spot_light_set_enabled(e, 1);
    dse_spot_light_set_direction(e, dir.x, dir.y, dir.z);
    dse_spot_light_set_color(e, r, g, b);
    dse_spot_light_set_intensity(e, intensity);
    dse_spot_light_set_radius(e, radius);
    dse_spot_light_set_inner_cone_angle(e, inner_angle);
    dse_spot_light_set_outer_cone_angle(e, outer_angle);
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
    const uint32_t e = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    const float up_r = helper::OptFloat(L, 2, 0.22f);
    const float up_g = helper::OptFloat(L, 3, 0.28f);
    const float up_b = helper::OptFloat(L, 4, 0.38f);
    const float down_r = helper::OptFloat(L, 5, 0.04f);
    const float down_g = helper::OptFloat(L, 6, 0.05f);
    const float down_b = helper::OptFloat(L, 7, 0.08f);
    const float intensity = helper::OptFloat(L, 8, 1.0f);
    // S1.8-2：委托 C ABI（enabled 默认 true）
    dse_sky_light_add(e);
    dse_sky_light_set_enabled(e, 1);
    dse_sky_light_set_up_color(e, up_r, up_g, up_b);
    dse_sky_light_set_down_color(e, down_r, down_g, down_b);
    dse_sky_light_set_intensity(e, intensity);
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


} // namespace

void RegisterEcsRenderingLightBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
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
        {"add_skybox",                L_EcsAddSkybox},
        {"add_gi_probe",              L_EcsAddGIProbe},
        {"set_gi_probe",              L_EcsSetGIProbe},
        {"set_gi_probe_enabled",      L_EcsSetGIProbeEnabled},
        {"get_gi_probe",              L_EcsGetGIProbe},
        {"add_light_probe",           L_EcsAddLightProbe},
        {"set_light_probe",           L_EcsSetLightProbe},
        {"set_light_probe_enabled",   L_EcsSetLightProbeEnabled},
        {"add_reflection_probe",      L_EcsAddReflectionProbe},
        {"set_reflection_probe",      L_EcsSetReflectionProbe},
        {"set_reflection_probe_enabled", L_EcsSetReflectionProbeEnabled},
    });
}

} // namespace dse::runtime::lua_binding
