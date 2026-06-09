/**
 * @file lua_binding_ecs_rendering_fx.cpp
 * @brief Steering / LOD / Hair / Utility Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）
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
int L_EcsWorldToScreen(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushboolean(L, 0);
        return 3;
    }
    float wx = helper::CheckFloat(L, 1);
    float wy = helper::CheckFloat(L, 2);
    float wz = helper::CheckFloat(L, 3);

    // Find main camera (highest priority enabled Camera3DComponent)
    auto cam_view = world->registry().view<Camera3DComponent, TransformComponent>();
    entt::entity main_cam = entt::null;
    int max_priority = -9999;
    for (auto entity : cam_view) {
        auto& cam = cam_view.get<Camera3DComponent>(entity);
        if (cam.enabled && cam.priority > max_priority) {
            max_priority = cam.priority;
            main_cam = entity;
        }
    }
    if (main_cam == entt::null) {
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushboolean(L, 0);
        return 3;
    }

    auto& cam = cam_view.get<Camera3DComponent>(main_cam);
    auto& transform = cam_view.get<TransformComponent>(main_cam);

    // Build view matrix from transform
    glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view_mat = glm::lookAt(transform.position, transform.position + front, up);
    glm::mat4 proj_mat = glm::perspective(glm::radians(cam.fov), cam.aspect_ratio, cam.near_clip, cam.far_clip);

    glm::vec4 clip = proj_mat * view_mat * glm::vec4(wx, wy, wz, 1.0f);
    bool visible = clip.w > 0.0f;
    if (clip.w == 0.0f) clip.w = 0.0001f;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    // NDC [-1,1] to screen [0, width/height]
    float screen_w = static_cast<float>(Screen::width());
    float screen_h = static_cast<float>(Screen::height());
    float sx = (ndc.x * 0.5f + 0.5f) * screen_w;
    float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * screen_h;  // flip Y

    lua_pushnumber(L, static_cast<lua_Number>(sx));
    lua_pushnumber(L, static_cast<lua_Number>(sy));
    lua_pushboolean(L, visible && ndc.z >= -1.0f && ndc.z <= 1.0f ? 1 : 0);
    return 3;
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
    });
}

} // namespace dse::runtime::lua_binding
