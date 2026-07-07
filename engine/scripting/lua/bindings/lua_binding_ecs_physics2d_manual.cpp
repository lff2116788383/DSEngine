/**
 * @file lua_binding_ecs_physics2d.cpp
 * @brief ECS Lua 绑定 — 2D 物理（RigidBody2D、BoxCollider2D、Raycast、碰撞事件）+ Tilemap
 *
 * 薄包装：仅做 Lua 参数读取与结果入栈，所有逻辑委托 C ABI（dse_api_physics2d.cpp）。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
#include <vector>
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

int L_EcsAddRigidBody(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int type = helper::OptInt(L, 2, 2);
    float gravity_scale = helper::OptFloat(L, 3, 1.0f);
    int fixed_rotation = helper::OptInt(L, 4, 0);
    dse_physics2d_add_rigidbody(e, type, gravity_scale, fixed_rotation);
    return 0;
}

int L_EcsSetRigidBodyVelocity(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float vx = helper::CheckFloat(L, 2);
    float vy = helper::CheckFloat(L, 3);
    dse_physics2d_set_rigidbody_velocity(e, vx, vy);
    return 0;
}

int L_EcsAddBoxCollider(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float w = helper::CheckFloat(L, 2);
    float h = helper::CheckFloat(L, 3);
    float density = helper::OptFloat(L, 4, 1.0f);
    float friction = helper::OptFloat(L, 5, 0.3f);
    float restitution = helper::OptFloat(L, 6, 0.0f);
    dse_physics2d_add_box_collider(e, w, h, density, friction, restitution);
    return 0;
}

int L_EcsSetBoxColliderTrigger(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int is_trigger = helper::CheckBool(L, 2) ? 1 : 0;
    dse_physics2d_set_box_collider_trigger(e, is_trigger);
    return 0;
}

int L_EcsAddCircleCollider(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float radius = helper::CheckFloat(L, 2);
    float density = helper::OptFloat(L, 3, 1.0f);
    float friction = helper::OptFloat(L, 4, 0.3f);
    float restitution = helper::OptFloat(L, 5, 0.0f);
    dse_physics2d_add_circle_collider(e, radius, density, friction, restitution);
    return 0;
}

int L_EcsSetCircleColliderTrigger(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int is_trigger = helper::CheckBool(L, 2) ? 1 : 0;
    dse_physics2d_set_circle_collider_trigger(e, is_trigger);
    return 0;
}

int L_EcsAddPolygonCollider(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = static_cast<int>(lua_rawlen(L, 2));
    std::vector<float> verts(n * 2);
    for (int i = 0; i < n; ++i) {
        lua_rawgeti(L, 2, i + 1);
        lua_rawgeti(L, -1, 1); verts[i * 2]     = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, -1, 2); verts[i * 2 + 1] = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_pop(L, 1);
    }
    float density = helper::OptFloat(L, 3, 1.0f);
    float friction = helper::OptFloat(L, 4, 0.3f);
    float restitution = helper::OptFloat(L, 5, 0.0f);
    dse_physics2d_add_polygon_collider(e, verts.data(), n, density, friction, restitution);
    return 0;
}

int L_EcsSetPolygonColliderTrigger(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int is_trigger = helper::CheckBool(L, 2) ? 1 : 0;
    dse_physics2d_set_polygon_collider_trigger(e, is_trigger);
    return 0;
}

int L_EcsAddJoint2D(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int type_int = helper::CheckInt(L, 2);
    uint32_t ea = EID(helper::CheckEntity(L, 3));
    uint32_t eb = EID(helper::CheckEntity(L, 4));
    float ax = helper::OptFloat(L, 5, 0.0f);
    float ay = helper::OptFloat(L, 6, 0.0f);
    float bx = helper::OptFloat(L, 7, 0.0f);
    float by = helper::OptFloat(L, 8, 0.0f);
    int collide = helper::OptBool(L, 9, false) ? 1 : 0;
    dse_physics2d_add_joint(e, type_int, ea, eb, ax, ay, bx, by, collide);
    return 0;
}

int L_EcsSetJoint2DRevolute(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int enable_limit = helper::OptBool(L, 2, false) ? 1 : 0;
    float lower = helper::OptFloat(L, 3, 0.0f);
    float upper = helper::OptFloat(L, 4, 0.0f);
    int enable_motor = helper::OptBool(L, 5, false) ? 1 : 0;
    float speed = helper::OptFloat(L, 6, 0.0f);
    float torque = helper::OptFloat(L, 7, 0.0f);
    dse_physics2d_set_joint_revolute(e, enable_limit, lower, upper, enable_motor, speed, torque);
    return 0;
}

int L_EcsSetJoint2DDistance(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float min_len = helper::OptFloat(L, 2, 0.0f);
    float max_len = helper::OptFloat(L, 3, 1.0f);
    float stiffness = helper::OptFloat(L, 4, 0.0f);
    float damping = helper::OptFloat(L, 5, 0.0f);
    dse_physics2d_set_joint_distance(e, min_len, max_len, stiffness, damping);
    return 0;
}

int L_EcsSetJoint2DPrismatic(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    float axis_x = helper::OptFloat(L, 2, 1.0f);
    float axis_y = helper::OptFloat(L, 3, 0.0f);
    int enable_limit = helper::OptBool(L, 4, false) ? 1 : 0;
    float lower = helper::OptFloat(L, 5, 0.0f);
    float upper = helper::OptFloat(L, 6, 0.0f);
    int enable_motor = helper::OptBool(L, 7, false) ? 1 : 0;
    float speed = helper::OptFloat(L, 8, 0.0f);
    float max_force = helper::OptFloat(L, 9, 0.0f);
    dse_physics2d_set_joint_prismatic(e, axis_x, axis_y, enable_limit, lower, upper, enable_motor, speed, max_force);
    return 0;
}

int L_EcsDestroyJoint2D(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    dse_physics2d_destroy_joint(e);
    return 0;
}

int L_EcsRaycast2D(lua_State* L) {
    float sx = helper::CheckFloat(L, 1);
    float sy = helper::CheckFloat(L, 2);
    float ex = helper::CheckFloat(L, 3);
    float ey = helper::CheckFloat(L, 4);
    uint32_t hit_entity = 0;
    float point[2] = {0, 0};
    float normal[2] = {0, 0};
    if (!dse_physics2d_raycast(sx, sy, ex, ey, &hit_entity, point, normal)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    helper::PushEntity(L, static_cast<Entity>(static_cast<entt::id_type>(hit_entity)));
    helper::PushFloat(L, point[0]);
    helper::PushFloat(L, point[1]);
    helper::PushFloat(L, normal[0]);
    helper::PushFloat(L, normal[1]);
    return 6;
}

int L_EcsPollCollisionEvent(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    uint32_t other = 0;
    int is_trigger = 0, is_enter = 0;
    if (!dse_physics2d_poll_collision_event(e, &other, &is_trigger, &is_enter)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    helper::PushEntity(L, static_cast<Entity>(static_cast<entt::id_type>(other)));
    helper::PushBool(L, is_trigger != 0);
    helper::PushBool(L, is_enter != 0);
    return 4;
}

int L_EcsAddTilemap(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int width = helper::CheckInt(L, 2);
    int height = helper::CheckInt(L, 3);
    float tile_size = helper::OptFloat(L, 4, 1.0f);
    uint32_t tex_handle = static_cast<uint32_t>(helper::OptInt(L, 5, 0));
    dse_physics2d_add_tilemap(e, width, height, tile_size, tex_handle);
    return 0;
}

int L_EcsSetTile(lua_State* L) {
    uint32_t e = EID(helper::CheckEntity(L, 1));
    int x = helper::CheckInt(L, 2);
    int y = helper::CheckInt(L, 3);
    int tile_id = helper::CheckInt(L, 4);
    dse_physics2d_set_tile(e, x, y, tile_id);
    return 0;
}

} // namespace

void RegisterEcsPhysics2DBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_rigid_body",          L_EcsAddRigidBody},
        {"set_rigid_body_velocity", L_EcsSetRigidBodyVelocity},
        {"add_box_collider",        L_EcsAddBoxCollider},
        {"set_box_collider_trigger", L_EcsSetBoxColliderTrigger},
        {"add_circle_collider",     L_EcsAddCircleCollider},
        {"set_circle_collider_trigger", L_EcsSetCircleColliderTrigger},
        {"add_polygon_collider",    L_EcsAddPolygonCollider},
        {"set_polygon_collider_trigger", L_EcsSetPolygonColliderTrigger},
        {"add_joint_2d",            L_EcsAddJoint2D},
        {"set_joint_2d_revolute",   L_EcsSetJoint2DRevolute},
        {"set_joint_2d_distance",   L_EcsSetJoint2DDistance},
        {"set_joint_2d_prismatic",  L_EcsSetJoint2DPrismatic},
        {"destroy_joint_2d",        L_EcsDestroyJoint2D},
        {"raycast_2d",              L_EcsRaycast2D},
        {"poll_collision_event",    L_EcsPollCollisionEvent},
        {"add_tilemap",             L_EcsAddTilemap},
        {"set_tile",                L_EcsSetTile},
    });
}

} // namespace dse::runtime::lua_binding
