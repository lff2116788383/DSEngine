/**
 * @file lua_binding_ecs_rendering_fx.cpp
 * @brief Steering / LOD / Hair / Utility Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）
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
// Steering
// ============================================================

int L_EcsAddSteering(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float max_vel = helper::OptFloat(L, 2, 5.0f);
    float max_force = helper::OptFloat(L, 3, 10.0f);
    float mass = helper::OptFloat(L, 4, 1.0f);
    auto& steering = world->registry().emplace_or_replace<SteeringComponent>(e);
    steering.enabled = true;
    steering.max_velocity = max_vel;
    steering.max_force = max_force;
    steering.mass = mass;
    return 0;
}

int L_EcsSetSteeringTarget(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const char* behavior = luaL_checkstring(L, 2);
    float tx = helper::CheckFloat(L, 3);
    float ty = helper::CheckFloat(L, 4);
    float tz = helper::CheckFloat(L, 5);
    auto* steering = helper::TryGetComponent<SteeringComponent>(*world, e);
    if (!steering) {
        lua_pushboolean(L, 0);
        return 1;
    }
    std::string b = behavior;
    if (b == "seek") {
        steering->seek_enabled = true;
        steering->flee_enabled = false;
        steering->arrive_enabled = false;
        steering->seek_target = glm::vec3(tx, ty, tz);
        lua_pushboolean(L, 1);
        return 1;
    } else if (b == "flee") {
        steering->seek_enabled = false;
        steering->flee_enabled = true;
        steering->arrive_enabled = false;
        steering->flee_target = glm::vec3(tx, ty, tz);
        lua_pushboolean(L, 1);
        return 1;
    } else if (b == "arrive") {
        steering->seek_enabled = false;
        steering->flee_enabled = false;
        steering->arrive_enabled = true;
        steering->arrive_target = glm::vec3(tx, ty, tz);
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

int L_EcsGetSteeringState(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const auto* steering = helper::TryGetComponentConst<SteeringComponent>(*world, e);
    if (!steering) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    helper::PushBool(L, steering->enabled);
    helper::PushBool(L, steering->seek_enabled);
    helper::PushBool(L, steering->flee_enabled);
    helper::PushBool(L, steering->arrive_enabled);
    helper::PushVec3(L, steering->velocity);
    helper::PushFloat(L, glm::length(steering->velocity));
    helper::PushFloat(L, steering->max_velocity);
    helper::PushFloat(L, steering->max_force);
    helper::PushFloat(L, steering->mass);
    helper::PushFloat(L, steering->arrive_deceleration_radius);
    helper::PushVec3(L, steering->seek_target);
    helper::PushVec3(L, steering->flee_target);
    helper::PushVec3(L, steering->arrive_target);
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
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* mesh_path = luaL_checkstring(L, 2);
    float threshold = static_cast<float>(luaL_checknumber(L, 3));
    if (!world->registry().all_of<LODGroupComponent>(e)) {
        world->registry().emplace<LODGroupComponent>(e);
    }
    auto& lod = world->registry().get<LODGroupComponent>(e);
    LODLevelConfig level;
    level.mesh_path = mesh_path;
    level.screen_size_threshold = threshold;
    lod.levels.push_back(std::move(level));
    return 0;
}

/// lod.set_scale(entity, scale)
int L_EcsLodSetScale(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float scale = static_cast<float>(luaL_checknumber(L, 2));
    auto* lod = helper::TryGetComponent<LODGroupComponent>(*world, e);
    if (!lod) return 0;
    lod->global_scale = scale;
    return 0;
}

/// lod.set_min_screen_size(entity, min_size)  -- 低于此屏幕占比时隐藏实体（LOD 距离裁剪）
int L_EcsLodSetMinScreenSize(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float min_size = static_cast<float>(luaL_checknumber(L, 2));
    if (!world->registry().all_of<LODGroupComponent>(e)) {
        world->registry().emplace<LODGroupComponent>(e);
    }
    auto& lod = world->registry().get<LODGroupComponent>(e);
    lod.min_screen_size = min_size;
    return 0;
}

/// lod.set_enabled(entity, enabled)
int L_EcsLodSetEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool enabled = lua_toboolean(L, 2) != 0;
    auto* lod = helper::TryGetComponent<LODGroupComponent>(*world, e);
    if (!lod) return 0;
    lod->enabled = enabled;
    return 0;
}


// ============================================================
// Hair
// ============================================================

// add_hair(entity, asset_path [, num_follow_per_guide])
int L_EcsAddHair(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& h = world->registry().emplace_or_replace<HairComponent>(e);
    h.enabled = true;
    h.hair_asset_path = helper::CheckString(L, 2);
    if (lua_gettop(L) >= 3) h.num_follow_per_guide = helper::CheckInt(L, 3);
    return 0;
}

// set_hair_physics(entity, damping, stiffness_local, stiffness_global, gravity)
int L_EcsSetHairPhysics(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    if (lua_gettop(L) >= 2) h->damping          = helper::CheckFloat(L, 2);
    if (lua_gettop(L) >= 3) h->stiffness_local  = helper::CheckFloat(L, 3);
    if (lua_gettop(L) >= 4) h->stiffness_global = helper::CheckFloat(L, 4);
    if (lua_gettop(L) >= 5) h->gravity          = helper::CheckFloat(L, 5);
    return 0;
}

// set_hair_render(entity, root_r,g,b,a, tip_r,g,b,a, fiber_radius, opacity)
int L_EcsSetHairRender(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    if (lua_gettop(L) >= 5)
        h->root_color = glm::vec4(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3),
                                   helper::CheckFloat(L, 4), helper::CheckFloat(L, 5));
    if (lua_gettop(L) >= 9)
        h->tip_color = glm::vec4(helper::CheckFloat(L, 6), helper::CheckFloat(L, 7),
                                  helper::CheckFloat(L, 8), helper::CheckFloat(L, 9));
    if (lua_gettop(L) >= 10) h->fiber_radius = helper::CheckFloat(L, 10);
    if (lua_gettop(L) >= 11) h->opacity      = helper::CheckFloat(L, 11);
    return 0;
}

// set_hair_wind(entity, wx, wy, wz [, turbulence])
int L_EcsSetHairWind(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    h->wind = glm::vec3(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));
    if (lua_gettop(L) >= 5) h->wind_turbulence = helper::CheckFloat(L, 5);
    return 0;
}

// set_hair_enabled(entity, bool)
int L_EcsSetHairEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    h->enabled = lua_toboolean(L, 2) != 0;
    return 0;
}

// set_hair_lod(entity, lod0, lod1, lod2, cull)
int L_EcsSetHairLod(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* h = helper::TryGetComponent<HairComponent>(*world, e);
    if (!h) return 0;
    if (lua_gettop(L) >= 2) h->lod0_distance = helper::CheckFloat(L, 2);
    if (lua_gettop(L) >= 3) h->lod1_distance = helper::CheckFloat(L, 3);
    if (lua_gettop(L) >= 4) h->lod2_distance = helper::CheckFloat(L, 4);
    if (lua_gettop(L) >= 5) h->cull_distance = helper::CheckFloat(L, 5);
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
