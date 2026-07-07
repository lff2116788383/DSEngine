/**
 * @file lua_binding_ecs_rendering_light.cpp
 * @brief Lights / Skybox / Probes Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）。薄包装委托至 C ABI。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <cmath>

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

/// 可选浮点参数：缺省时返回 NaN（C ABI 哨兵 = 保持当前值）。
inline float OptNan(lua_State* L, int i) {
    return lua_isnoneornil(L, i) ? NAN : helper::CheckFloat(L, i);
}

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
    // S1.8-2：委托 C ABI（dse_dir_light_add 内 emplace_or_replace，enabled 默认 true）
    dse_dir_light_add(e);
    dse_dir_light_set_direction(e, dir.x, dir.y, dir.z);
    dse_dir_light_set_color(e, r, g, b);
    dse_dir_light_set_intensity(e, intensity);
    dse_dir_light_set_ambient_intensity(e, ambient_intensity);
    dse_dir_light_set_shadow_strength(e, shadow_strength);
    return 0;
}

int L_EcsSetDirectionalLight3D(lua_State* L) {
    const uint32_t id = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    if (!dse_dir_light_has(id)) return 0;
    // S1.8 Tier B：写入委托 C ABI（归一化/部分更新在 Lua 算好）
    if (lua_gettop(L) >= 2) {
        dse_dir_light_set_enabled(id, helper::CheckBool(L, 2) ? 1 : 0);
    }
    float cur_dx = 0.0f, cur_dy = 0.0f, cur_dz = 0.0f;
    dse_dir_light_get_direction(id, &cur_dx, &cur_dy, &cur_dz);
    const float dir_x = helper::OptFloat(L, 3, cur_dx);
    const float dir_y = helper::OptFloat(L, 4, cur_dy);
    const float dir_z = helper::OptFloat(L, 5, cur_dz);
    const glm::vec3 dir = glm::normalize(glm::vec3(dir_x, dir_y, dir_z));
    dse_dir_light_set_direction(id, dir.x, dir.y, dir.z);
    float cur_r = 0.0f, cur_g = 0.0f, cur_b = 0.0f;
    dse_dir_light_get_color(id, &cur_r, &cur_g, &cur_b);
    dse_dir_light_set_color(id,
        helper::OptFloat(L, 6, cur_r),
        helper::OptFloat(L, 7, cur_g),
        helper::OptFloat(L, 8, cur_b));
    dse_dir_light_set_intensity(id, helper::OptFloat(L, 9, dse_dir_light_get_intensity(id)));
    dse_dir_light_set_ambient_intensity(id, helper::OptFloat(L, 10, dse_dir_light_get_ambient_intensity(id)));
    dse_dir_light_set_shadow_strength(id, helper::OptFloat(L, 11, dse_dir_light_get_shadow_strength(id)));
    return 0;
}

int L_EcsSetDirectionalLightShadow(lua_State* L) {
    const uint32_t id = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    int cur_cast = 0;
    float cur_ss = 0.0f, cur_c0 = 0.0f, cur_c1 = 0.0f, cur_c2 = 0.0f, cur_lambda = 0.0f;
    if (!dse_dir_light_get_shadow_params(id, &cur_cast, &cur_ss, &cur_c0, &cur_c1, &cur_c2, &cur_lambda)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    // S1.8 Tier C：部分更新（nil 沿用现值）在 Lua 合并；cascade 级联约束 + clamp 封装在 C ABI
    const int cast_shadow = lua_isnoneornil(L, 2) ? cur_cast : (helper::CheckBool(L, 2) ? 1 : 0);
    const float ss     = helper::OptFloat(L, 3, cur_ss);
    const float c0     = helper::OptFloat(L, 4, cur_c0);
    const float c1     = helper::OptFloat(L, 5, cur_c1);
    const float c2     = helper::OptFloat(L, 6, cur_c2);
    const float lambda = helper::OptFloat(L, 7, cur_lambda);
    dse_dir_light_set_shadow_params(id, cast_shadow, ss, c0, c1, c2, lambda);

    // 读回（钳制后）现值，保持原 7 值返回契约
    dse_dir_light_get_shadow_params(id, &cur_cast, &cur_ss, &cur_c0, &cur_c1, &cur_c2, &cur_lambda);
    lua_pushboolean(L, 1);
    helper::PushBool(L, cur_cast != 0);
    helper::PushFloat(L, cur_ss);
    helper::PushFloat(L, cur_c0);
    helper::PushFloat(L, cur_c1);
    helper::PushFloat(L, cur_c2);
    helper::PushFloat(L, cur_lambda);
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
    const uint32_t id = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    if (!dse_point_light_has(id)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    if (!lua_isnoneornil(L, 2)) {
        dse_point_light_set_cast_shadow(id, helper::CheckBool(L, 2) ? 1 : 0);
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
    const uint32_t id = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    if (!dse_point_light_has(id)) return 0;
    // S1.8 收尾：写入委托 C ABI（color 为整 vec3 setter，未提供通道沿用当前值）
    if (!lua_isnoneornil(L, 2) || !lua_isnoneornil(L, 3) || !lua_isnoneornil(L, 4)) {
        float cur_r = 0.0f, cur_g = 0.0f, cur_b = 0.0f;
        dse_point_light_get_color(id, &cur_r, &cur_g, &cur_b);
        const float cr = lua_isnoneornil(L, 2) ? cur_r : helper::CheckFloat(L, 2);
        const float cg = lua_isnoneornil(L, 3) ? cur_g : helper::CheckFloat(L, 3);
        const float cb = lua_isnoneornil(L, 4) ? cur_b : helper::CheckFloat(L, 4);
        dse_point_light_set_color(id, cr, cg, cb);
    }
    if (!lua_isnoneornil(L, 5)) dse_point_light_set_intensity(id, helper::CheckFloat(L, 5));
    if (!lua_isnoneornil(L, 6)) dse_point_light_set_radius(id, helper::CheckFloat(L, 6));
    return 0;
}

// set_spot_light_3d(entity, dir_x, dir_y, dir_z, r, g, b, intensity, radius, inner_angle, outer_angle)
int L_EcsSetSpotLight3D(lua_State* L) {
    const uint32_t id = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    if (!dse_spot_light_has(id)) return 0;
    // S1.8 收尾：归一化在 Lua 算好，写入委托 C ABI
    if (!lua_isnoneornil(L, 2) && !lua_isnoneornil(L, 3) && !lua_isnoneornil(L, 4)) {
        const glm::vec3 dir = glm::normalize(glm::vec3(
            helper::CheckFloat(L, 2),
            helper::CheckFloat(L, 3),
            helper::CheckFloat(L, 4)));
        dse_spot_light_set_direction(id, dir.x, dir.y, dir.z);
    }
    if (!lua_isnoneornil(L, 5) || !lua_isnoneornil(L, 6) || !lua_isnoneornil(L, 7)) {
        float cur_r = 0.0f, cur_g = 0.0f, cur_b = 0.0f;
        dse_spot_light_get_color(id, &cur_r, &cur_g, &cur_b);
        const float cr = lua_isnoneornil(L, 5) ? cur_r : helper::CheckFloat(L, 5);
        const float cg = lua_isnoneornil(L, 6) ? cur_g : helper::CheckFloat(L, 6);
        const float cb = lua_isnoneornil(L, 7) ? cur_b : helper::CheckFloat(L, 7);
        dse_spot_light_set_color(id, cr, cg, cb);
    }
    if (!lua_isnoneornil(L, 8)) dse_spot_light_set_intensity(id, helper::CheckFloat(L, 8));
    if (!lua_isnoneornil(L, 9)) dse_spot_light_set_radius(id, helper::CheckFloat(L, 9));
    if (!lua_isnoneornil(L, 10)) dse_spot_light_set_inner_cone_angle(id, helper::CheckFloat(L, 10));
    if (!lua_isnoneornil(L, 11)) dse_spot_light_set_outer_cone_angle(id, helper::CheckFloat(L, 11));
    return 0;
}

// set_spot_light_shadow(entity, cast_shadow)
int L_EcsSetSpotLightShadow(lua_State* L) {
    const uint32_t id = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    if (!dse_spot_light_has(id)) { lua_pushboolean(L, 0); return 1; }
    if (!lua_isnoneornil(L, 2)) {
        dse_spot_light_set_cast_shadow(id, helper::CheckBool(L, 2) ? 1 : 0);
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
    const uint32_t id = static_cast<uint32_t>(helper::CheckEntity(L, 1));
    if (!dse_sky_light_has(id)) return 0;
    float up_r = 0.0f, up_g = 0.0f, up_b = 0.0f;
    dse_sky_light_get_up_color(id, &up_r, &up_g, &up_b);
    dse_sky_light_set_up_color(id,
        helper::OptFloat(L, 2, up_r),
        helper::OptFloat(L, 3, up_g),
        helper::OptFloat(L, 4, up_b));
    float down_r = 0.0f, down_g = 0.0f, down_b = 0.0f;
    dse_sky_light_get_down_color(id, &down_r, &down_g, &down_b);
    dse_sky_light_set_down_color(id,
        helper::OptFloat(L, 5, down_r),
        helper::OptFloat(L, 6, down_g),
        helper::OptFloat(L, 7, down_b));
    dse_sky_light_set_intensity(id, helper::OptFloat(L, 8, dse_sky_light_get_intensity(id)));
    if (!lua_isnoneornil(L, 9)) {
        dse_sky_light_set_enabled(id, helper::CheckBool(L, 9) ? 1 : 0);
    }
    return 0;
}


// ============================================================
// Skybox
// ============================================================

int L_EcsAddSkybox(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* cubemap_path = luaL_optstring(L, 2, "");
    dse_rendering_add_skybox(EID(e), cubemap_path);
    return 0;
}


// ============================================================
// GI Probe Volume (DDGI)
// ============================================================

// add_gi_probe(entity [, ox,oy,oz, ex,ey,ez, rx,ry,rz])
int L_EcsAddGIProbe(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_rendering_add_gi_probe(EID(e));
    float ox = NAN, oy = NAN, oz = NAN, ex = NAN, ey = NAN, ez = NAN;
    int rx = 0, ry = 0, rz = 0;
    if (lua_gettop(L) >= 4) {
        ox = helper::CheckFloat(L, 2); oy = helper::CheckFloat(L, 3); oz = helper::CheckFloat(L, 4);
    }
    if (lua_gettop(L) >= 7) {
        ex = helper::CheckFloat(L, 5); ey = helper::CheckFloat(L, 6); ez = helper::CheckFloat(L, 7);
    }
    if (lua_gettop(L) >= 10) {
        rx = helper::CheckInt(L, 8); ry = helper::CheckInt(L, 9); rz = helper::CheckInt(L, 10);
    }
    dse_rendering_set_gi_probe(EID(e), NAN, ox, oy, oz, ex, ey, ez, rx, ry, rz);
    dse_rendering_set_gi_probe_enabled(EID(e), 1);
    return 0;
}

// set_gi_probe(entity, origin_x,y,z, extent_x,y,z, res_x,y,z [, gi_intensity, normal_bias, hysteresis])
int L_EcsSetGIProbe(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float ox = NAN, oy = NAN, oz = NAN, ex = NAN, ey = NAN, ez = NAN;
    int rx = 0, ry = 0, rz = 0;
    if (lua_gettop(L) >= 4) {
        ox = helper::CheckFloat(L, 2); oy = helper::CheckFloat(L, 3); oz = helper::CheckFloat(L, 4);
    }
    if (lua_gettop(L) >= 7) {
        ex = helper::CheckFloat(L, 5); ey = helper::CheckFloat(L, 6); ez = helper::CheckFloat(L, 7);
    }
    if (lua_gettop(L) >= 10) {
        rx = helper::CheckInt(L, 8); ry = helper::CheckInt(L, 9); rz = helper::CheckInt(L, 10);
    }
    const float gi_intensity = lua_gettop(L) >= 11 ? helper::CheckFloat(L, 11) : NAN;
    dse_rendering_set_gi_probe(EID(e), gi_intensity, ox, oy, oz, ex, ey, ez, rx, ry, rz);
    const float normal_bias = lua_gettop(L) >= 12 ? helper::CheckFloat(L, 12) : NAN;
    const float hysteresis = lua_gettop(L) >= 13 ? helper::CheckFloat(L, 13) : NAN;
    dse_rendering_set_gi_probe_bias(EID(e), normal_bias, hysteresis);
    return 0;
}

// set_gi_probe_enabled(entity, bool)
int L_EcsSetGIProbeEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_rendering_set_gi_probe_enabled(EID(e), lua_toboolean(L, 2) != 0 ? 1 : 0);
    return 0;
}

// get_gi_probe(entity) -> enabled, ox,oy,oz, ex,ey,ez, rx,ry,rz, gi_intensity, normal_bias
int L_EcsGetGIProbe(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float gi_intensity = 0.0f, origin[3] = {0.0f, 0.0f, 0.0f}, extent[3] = {0.0f, 0.0f, 0.0f};
    int resolution[3] = {0, 0, 0};
    if (!dse_rendering_get_gi_probe(EID(e), &gi_intensity, origin, extent, resolution)) return 0;
    int enabled = 0;
    float normal_bias = 0.0f;
    dse_rendering_get_gi_probe_ex(EID(e), &enabled, &normal_bias);
    helper::PushBool(L, enabled != 0);
    helper::PushFloat(L, origin[0]); helper::PushFloat(L, origin[1]); helper::PushFloat(L, origin[2]);
    helper::PushFloat(L, extent[0]); helper::PushFloat(L, extent[1]); helper::PushFloat(L, extent[2]);
    lua_pushinteger(L, resolution[0]); lua_pushinteger(L, resolution[1]); lua_pushinteger(L, resolution[2]);
    helper::PushFloat(L, gi_intensity);
    helper::PushFloat(L, normal_bias);
    return 12;
}


// ============================================================
// LightProbeComponent 绑定
// ============================================================

// add_light_probe(entity, [influence_radius])
int L_EcsAddLightProbe(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_rendering_add_light_probe(EID(e));
    dse_rendering_set_light_probe_ex(EID(e), helper::OptFloat(L, 2, 10.0f), -1);
    dse_rendering_set_light_probe_enabled(EID(e), 1);
    return 0;
}

// set_light_probe(entity, influence_radius, [needs_rebake])
int L_EcsSetLightProbe(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const float influence_radius = OptNan(L, 2);
    const int needs_rebake = lua_isnoneornil(L, 3) ? -1 : (helper::CheckBool(L, 3) ? 1 : 0);
    dse_rendering_set_light_probe_ex(EID(e), influence_radius, needs_rebake);
    return 0;
}

int L_EcsSetLightProbeEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_rendering_set_light_probe_enabled(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}


// ============================================================
// ReflectionProbeComponent 绑定
// ============================================================

// add_reflection_probe(entity, [influence_radius])
int L_EcsAddReflectionProbe(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_rendering_add_reflection_probe(EID(e));
    dse_rendering_set_reflection_probe_ex(EID(e), helper::OptFloat(L, 2, 15.0f), NAN, NAN, NAN, -1);
    dse_rendering_set_reflection_probe_enabled(EID(e), 1);
    return 0;
}

// set_reflection_probe(entity, influence_radius, box_size_x, box_size_y, box_size_z, [resolution])
int L_EcsSetReflectionProbe(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const int resolution = lua_isnoneornil(L, 6) ? -1 : helper::CheckInt(L, 6);
    dse_rendering_set_reflection_probe_ex(EID(e), OptNan(L, 2),
                                          OptNan(L, 3), OptNan(L, 4), OptNan(L, 5), resolution);
    return 0;
}

int L_EcsSetReflectionProbeEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_rendering_set_reflection_probe_enabled(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}


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
