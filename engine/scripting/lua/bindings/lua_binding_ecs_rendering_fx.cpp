/**
 * @file lua_binding_ecs_rendering_fx.cpp
 * @brief Steering / LOD / Hair / Utility Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）。薄包装委托至 C ABI。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <cmath>

// codegen 逐字段 setter（声明于 dse_api.gen.h，过渡期不与 dse_api.h 同时 include）
extern "C" void dse_hair_set_enabled(uint32_t e, int v);

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

// ============================================================
// Steering
// ============================================================

int L_EcsAddSteering(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float max_vel = helper::OptFloat(L, 2, 5.0f);
    float max_force = helper::OptFloat(L, 3, 10.0f);
    float mass = helper::OptFloat(L, 4, 1.0f);
    dse_steering_add(EID(e), max_vel, max_force, mass);
    return 0;
}

int L_EcsSetSteeringTarget(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* behavior = luaL_checkstring(L, 2);
    float tx = helper::CheckFloat(L, 3);
    float ty = helper::CheckFloat(L, 4);
    float tz = helper::CheckFloat(L, 5);
    int behavior_id = -1;
    const std::string b = behavior;
    if (b == "seek") behavior_id = 0;
    else if (b == "flee") behavior_id = 1;
    else if (b == "arrive") behavior_id = 2;
    if (behavior_id < 0) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, dse_steering_set_target(EID(e), behavior_id, tx, ty, tz));
    return 1;
}

int L_EcsGetSteeringState(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int flags[4] = {0, 0, 0, 0};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    float params[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float targets[9] = {0.0f};
    if (!dse_steering_get_state(EID(e), flags, velocity, params, targets)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const glm::vec3 vel(velocity[0], velocity[1], velocity[2]);
    lua_pushboolean(L, 1);
    helper::PushBool(L, flags[0] != 0);
    helper::PushBool(L, flags[1] != 0);
    helper::PushBool(L, flags[2] != 0);
    helper::PushBool(L, flags[3] != 0);
    helper::PushVec3(L, vel);
    helper::PushFloat(L, glm::length(vel));
    helper::PushFloat(L, params[0]);
    helper::PushFloat(L, params[1]);
    helper::PushFloat(L, params[2]);
    helper::PushFloat(L, params[3]);
    helper::PushVec3(L, glm::vec3(targets[0], targets[1], targets[2]));
    helper::PushVec3(L, glm::vec3(targets[3], targets[4], targets[5]));
    helper::PushVec3(L, glm::vec3(targets[6], targets[7], targets[8]));
    return 22;
}

// world_to_screen: project a 3D world position to 2D screen coordinates
// Returns: screen_x, screen_y, is_visible (boolean, false if behind camera)
// 委托 dse_render_world_to_screen（主相机投影，逐值等价）
int L_EcsWorldToScreen(lua_State* L) {
    float wx = helper::CheckFloat(L, 1);
    float wy = helper::CheckFloat(L, 2);
    float wz = helper::CheckFloat(L, 3);
    float sx = 0.0f, sy = 0.0f;
    int visible = dse_render_world_to_screen(wx, wy, wz, &sx, &sy);
    lua_pushnumber(L, static_cast<lua_Number>(sx));
    lua_pushnumber(L, static_cast<lua_Number>(sy));
    lua_pushboolean(L, visible);
    return 3;
}

// screen_to_world_ray(sx, sy) -> ox,oy,oz, dx,dy,dz | nil
// 由屏幕像素用主相机反投影出世界空间拾取射线（起点=相机位置，方向已归一化）。
// 无可用主相机时返回 nil。
int L_EcsScreenToWorldRay(lua_State* L) {
    float sx = helper::CheckFloat(L, 1);
    float sy = helper::CheckFloat(L, 2);
    float origin[3] = {0.0f, 0.0f, 0.0f};
    float dir[3]    = {0.0f, 0.0f, 0.0f};
    if (!dse_render_screen_to_world_ray(sx, sy, origin, dir)) {
        lua_pushnil(L);
        return 1;
    }
    helper::PushVec3(L, glm::vec3(origin[0], origin[1], origin[2]));
    helper::PushVec3(L, glm::vec3(dir[0], dir[1], dir[2]));
    return 6;
}

// pick_entity(sx, sy, [max_dist=1000]) -> entity, hx,hy,hz, nx,ny,nz, dist | nil
// 便捷拾取：屏幕像素 → 主相机射线 → 3D 物理 raycast，返回命中的实体及命中信息。
// 无主相机或未命中返回 nil。需要场景内有 3D 物理碰撞体。
int L_EcsPickEntity(lua_State* L) {
    float sx = helper::CheckFloat(L, 1);
    float sy = helper::CheckFloat(L, 2);
    float max_dist = helper::OptFloat(L, 3, 1000.0f);

    float origin[3] = {0.0f, 0.0f, 0.0f};
    float dir[3]    = {0.0f, 0.0f, 0.0f};
    if (!dse_render_screen_to_world_ray(sx, sy, origin, dir)) {
        lua_pushnil(L);
        return 1;
    }

    uint32_t hit_entity = 0;
    float point[3]  = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
    float distance  = 0.0f;
    const int hit = dse_physics3d_raycast(origin[0], origin[1], origin[2],
                                          dir[0], dir[1], dir[2],
                                          max_dist, &hit_entity, point, normal, &distance);
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }
    helper::PushEntity(L, static_cast<Entity>(static_cast<entt::id_type>(hit_entity)));
    helper::PushVec3(L, glm::vec3(point[0], point[1], point[2]));
    helper::PushVec3(L, glm::vec3(normal[0], normal[1], normal[2]));
    helper::PushFloat(L, distance);
    return 8;
}


// ============================================================
// LOD
// ============================================================

/// lod.add_level(entity, mesh_path, threshold)
int L_EcsLodAddLevel(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* mesh_path = luaL_checkstring(L, 2);
    float threshold = static_cast<float>(luaL_checknumber(L, 3));
    dse_lod_add_level(EID(e), mesh_path, threshold);
    return 0;
}

/// lod.set_scale(entity, scale)
int L_EcsLodSetScale(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float scale = static_cast<float>(luaL_checknumber(L, 2));
    dse_lod_set_scale(EID(e), scale);
    return 0;
}

/// lod.set_min_screen_size(entity, min_size)  -- 低于此屏幕占比时隐藏实体（LOD 距离裁剪）
int L_EcsLodSetMinScreenSize(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float min_size = static_cast<float>(luaL_checknumber(L, 2));
    dse_lod_set_min_screen_size(EID(e), min_size);
    return 0;
}

/// lod.set_enabled(entity, enabled)
int L_EcsLodSetEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_lod_set_enabled(EID(e), lua_toboolean(L, 2) != 0 ? 1 : 0);
    return 0;
}


// ============================================================
// Hair
// ============================================================

// add_hair(entity, asset_path [, num_follow_per_guide])
int L_EcsAddHair(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* asset_path = helper::CheckString(L, 2);
    const int num_follow = lua_gettop(L) >= 3 ? helper::CheckInt(L, 3) : -1;
    dse_hair_add(EID(e), asset_path, num_follow);
    return 0;
}

// set_hair_physics(entity, damping, stiffness_local, stiffness_global, gravity)
int L_EcsSetHairPhysics(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const float damping          = lua_gettop(L) >= 2 ? helper::CheckFloat(L, 2) : NAN;
    const float stiffness_local  = lua_gettop(L) >= 3 ? helper::CheckFloat(L, 3) : NAN;
    const float stiffness_global = lua_gettop(L) >= 4 ? helper::CheckFloat(L, 4) : NAN;
    const float gravity          = lua_gettop(L) >= 5 ? helper::CheckFloat(L, 5) : NAN;
    dse_hair_set_physics(EID(e), damping, stiffness_local, stiffness_global, gravity);
    return 0;
}

// set_hair_render(entity, root_r,g,b,a, tip_r,g,b,a, fiber_radius, opacity)
int L_EcsSetHairRender(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float root_r = NAN, root_g = NAN, root_b = NAN, root_a = NAN;
    float tip_r = NAN, tip_g = NAN, tip_b = NAN, tip_a = NAN;
    if (lua_gettop(L) >= 5) {
        root_r = helper::CheckFloat(L, 2); root_g = helper::CheckFloat(L, 3);
        root_b = helper::CheckFloat(L, 4); root_a = helper::CheckFloat(L, 5);
    }
    if (lua_gettop(L) >= 9) {
        tip_r = helper::CheckFloat(L, 6); tip_g = helper::CheckFloat(L, 7);
        tip_b = helper::CheckFloat(L, 8); tip_a = helper::CheckFloat(L, 9);
    }
    const float fiber_radius = lua_gettop(L) >= 10 ? helper::CheckFloat(L, 10) : NAN;
    const float opacity      = lua_gettop(L) >= 11 ? helper::CheckFloat(L, 11) : NAN;
    dse_hair_set_render(EID(e), root_r, root_g, root_b, root_a,
                        tip_r, tip_g, tip_b, tip_a, fiber_radius, opacity);
    return 0;
}

// set_hair_wind(entity, wx, wy, wz [, turbulence])
int L_EcsSetHairWind(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const float wx = helper::CheckFloat(L, 2);
    const float wy = helper::CheckFloat(L, 3);
    const float wz = helper::CheckFloat(L, 4);
    const float turbulence = lua_gettop(L) >= 5 ? helper::CheckFloat(L, 5) : NAN;
    dse_hair_set_wind_full(EID(e), wx, wy, wz, turbulence);
    return 0;
}

// set_hair_enabled(entity, bool)
int L_EcsSetHairEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_hair_set_enabled(EID(e), lua_toboolean(L, 2) != 0 ? 1 : 0);
    return 0;
}

// set_hair_lod(entity, lod0, lod1, lod2, cull)
int L_EcsSetHairLod(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const float lod0 = lua_gettop(L) >= 2 ? helper::CheckFloat(L, 2) : NAN;
    const float lod1 = lua_gettop(L) >= 3 ? helper::CheckFloat(L, 3) : NAN;
    const float lod2 = lua_gettop(L) >= 4 ? helper::CheckFloat(L, 4) : NAN;
    const float cull = lua_gettop(L) >= 5 ? helper::CheckFloat(L, 5) : NAN;
    dse_hair_set_lod(EID(e), lod0, lod1, lod2, cull);
    return 0;
}


} // namespace

void RegisterEcsRenderingFxBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_steering",              L_EcsAddSteering},
        {"set_steering_target",       L_EcsSetSteeringTarget},
        {"get_steering_state",        L_EcsGetSteeringState},
        {"lod_add_level",             L_EcsLodAddLevel},
        {"lod_set_scale",             L_EcsLodSetScale},
        {"lod_set_min_screen_size",   L_EcsLodSetMinScreenSize},
        {"lod_set_enabled",           L_EcsLodSetEnabled},
        {"add_hair",                  L_EcsAddHair},
        {"set_hair_physics",          L_EcsSetHairPhysics},
        {"set_hair_render",           L_EcsSetHairRender},
        {"set_hair_wind",             L_EcsSetHairWind},
        {"set_hair_enabled",          L_EcsSetHairEnabled},
        {"set_hair_lod",              L_EcsSetHairLod},
        {"world_to_screen",           L_EcsWorldToScreen},
        {"screen_to_world_ray",       L_EcsScreenToWorldRay},
        {"pick_entity",               L_EcsPickEntity},
    });
}

} // namespace dse::runtime::lua_binding
