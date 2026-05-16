/**
 * @file lua_binding_ecs_physics2d.cpp
 * @brief ECS Lua 绑定 — 2D 物理（RigidBody2D、BoxCollider2D、Raycast、碰撞事件）+ Tilemap
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/physics_2d.h"
#include "engine/ecs/tilemap.h"
#include "engine/core/service_locator.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include <box2d/box2d.h>
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_EcsAddRigidBody(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int type = helper::OptInt(L, 2, 2);
    float gravity_scale = helper::OptFloat(L, 3, 1.0f);
    int fixed_rotation = helper::OptInt(L, 4, 0);
    auto& rb = world->registry().emplace_or_replace<RigidBody2DComponent>(e);
    if (type <= 0) {
        rb.type = RigidBody2DType::Static;
    } else if (type == 1) {
        rb.type = RigidBody2DType::Kinematic;
    } else {
        rb.type = RigidBody2DType::Dynamic;
    }
    rb.gravity_scale = gravity_scale;
    rb.fixed_rotation = fixed_rotation != 0;
    return 0;
}

int L_EcsAddBoxCollider(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float w = helper::CheckFloat(L, 2);
    float h = helper::CheckFloat(L, 3);
    float density = helper::OptFloat(L, 4, 1.0f);
    float friction = helper::OptFloat(L, 5, 0.3f);
    float restitution = helper::OptFloat(L, 6, 0.0f);
    auto& collider = world->registry().emplace_or_replace<BoxCollider2DComponent>(e);
    collider.size = glm::vec2(w, h);
    collider.density = density;
    collider.friction = friction;
    collider.restitution = restitution;
    return 0;
}

int L_EcsSetBoxColliderTrigger(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool is_trigger = helper::CheckBool(L, 2);
    auto* collider = helper::TryGetComponent<BoxCollider2DComponent>(*world, e);
    if (!collider) return 0;
    collider->is_trigger = is_trigger;
    if (collider->runtime_fixture != nullptr) {
        collider->runtime_fixture->SetSensor(is_trigger);
    }
    return 0;
}

int L_EcsSetRigidBodyVelocity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float vx = helper::CheckFloat(L, 2);
    float vy = helper::CheckFloat(L, 3);
    auto* rb = helper::TryGetComponent<RigidBody2DComponent>(*world, e);
    if (!rb) return 0;
    rb->velocity = glm::vec2(vx, vy);
    if (rb->runtime_body != nullptr) {
        rb->runtime_body->SetLinearVelocity(b2Vec2{vx, vy});
        rb->runtime_body->SetAwake(true);
    }
    return 0;
}

int L_EcsRaycast2D(lua_State* L) {
    auto* physics = dse::core::ServiceLocator::Instance().Get<Physics2DSystem>();
    if (physics == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    glm::vec2 start(helper::CheckFloat(L, 1), helper::CheckFloat(L, 2));
    glm::vec2 end(helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));

    Entity hit_entity = entt::null;
    glm::vec2 hit_point(0.0f);
    glm::vec2 hit_normal(0.0f);
    if (!physics->Raycast(start, end, hit_entity, hit_point, hit_normal)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, 1);
    helper::PushEntity(L, hit_entity);
    helper::PushFloat(L, hit_point.x);
    helper::PushFloat(L, hit_point.y);
    helper::PushFloat(L, hit_normal.x);
    helper::PushFloat(L, hit_normal.y);
    return 6;
}

int L_EcsPollCollisionEvent(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* rb = helper::TryGetComponent<RigidBody2DComponent>(*world, e);
    if (!rb) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (rb->pending_contact_events.empty()) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const Physics2DContactEvent event = rb->pending_contact_events.front();
    rb->pending_contact_events.pop_front();
    lua_pushboolean(L, 1);
    helper::PushEntity(L, event.other);
    helper::PushBool(L, event.is_trigger);
    helper::PushBool(L, event.is_enter);
    return 4;
}

int L_EcsAddCircleCollider(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float radius = helper::CheckFloat(L, 2);
    float density = helper::OptFloat(L, 3, 1.0f);
    float friction = helper::OptFloat(L, 4, 0.3f);
    float restitution = helper::OptFloat(L, 5, 0.0f);
    auto& collider = world->registry().emplace_or_replace<CircleCollider2DComponent>(e);
    collider.radius = radius;
    collider.density = density;
    collider.friction = friction;
    collider.restitution = restitution;
    return 0;
}

int L_EcsSetCircleColliderTrigger(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool is_trigger = helper::CheckBool(L, 2);
    auto* collider = helper::TryGetComponent<CircleCollider2DComponent>(*world, e);
    if (!collider) return 0;
    collider->is_trigger = is_trigger;
    if (collider->runtime_fixture != nullptr) {
        collider->runtime_fixture->SetSensor(is_trigger);
    }
    return 0;
}

// ============================================================
// PolygonCollider2DComponent 绑定
// ============================================================

int L_EcsAddPolygonCollider(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    // arg2: Lua table of vertices {{x1,y1},{x2,y2},...}
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = static_cast<int>(lua_rawlen(L, 2));
    float density = helper::OptFloat(L, 3, 1.0f);
    float friction = helper::OptFloat(L, 4, 0.3f);
    float restitution = helper::OptFloat(L, 5, 0.0f);

    auto& pc = world->registry().emplace_or_replace<PolygonCollider2DComponent>(e);
    pc.vertices.clear();
    pc.vertices.reserve(n);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 2, i);
        lua_rawgeti(L, -1, 1);
        float vx = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_rawgeti(L, -1, 2);
        float vy = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_pop(L, 1); // pop sub-table
        pc.vertices.push_back(glm::vec2(vx, vy));
    }
    pc.density = density;
    pc.friction = friction;
    pc.restitution = restitution;
    return 0;
}

int L_EcsSetPolygonColliderTrigger(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool is_trigger = helper::CheckBool(L, 2);
    auto* collider = helper::TryGetComponent<PolygonCollider2DComponent>(*world, e);
    if (!collider) return 0;
    collider->is_trigger = is_trigger;
    if (collider->runtime_fixture != nullptr) {
        collider->runtime_fixture->SetSensor(is_trigger);
    }
    return 0;
}

// ============================================================
// Joint2DComponent 绑定
// ============================================================

/// add_joint_2d(entity, type_int, entity_a, entity_b,
///              anchor_ax, anchor_ay, anchor_bx, anchor_by,
///              collide_connected)
/// type: 0=Revolute, 1=Distance, 2=Prismatic, 3=Weld
int L_EcsAddJoint2D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int type_int = helper::CheckInt(L, 2);
    Entity entity_a = helper::CheckEntity(L, 3);
    Entity entity_b = helper::CheckEntity(L, 4);
    float ax = helper::OptFloat(L, 5, 0.0f);
    float ay = helper::OptFloat(L, 6, 0.0f);
    float bx = helper::OptFloat(L, 7, 0.0f);
    float by = helper::OptFloat(L, 8, 0.0f);
    bool collide = helper::OptBool(L, 9, false);

    auto& jc = world->registry().emplace_or_replace<Joint2DComponent>(e);
    jc.runtime_joint = nullptr;
    switch (type_int) {
        case 0: jc.type = Joint2DType::Revolute; break;
        case 1: jc.type = Joint2DType::Distance; break;
        case 2: jc.type = Joint2DType::Prismatic; break;
        case 3: jc.type = Joint2DType::Weld; break;
        default: jc.type = Joint2DType::Revolute; break;
    }
    jc.entity_a = entity_a;
    jc.entity_b = entity_b;
    jc.anchor_a = glm::vec2(ax, ay);
    jc.anchor_b = glm::vec2(bx, by);
    jc.collide_connected = collide;
    return 0;
}

/// set_joint_2d_revolute(entity, enable_limit, lower_deg, upper_deg,
///                       enable_motor, motor_speed_deg, max_torque)
int L_EcsSetJoint2DRevolute(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* jc = helper::TryGetComponent<Joint2DComponent>(*world, e);
    if (!jc) return 0;
    jc->enable_limit     = helper::OptBool(L, 2, false);
    jc->lower_angle      = helper::OptFloat(L, 3, 0.0f);
    jc->upper_angle      = helper::OptFloat(L, 4, 0.0f);
    jc->enable_motor     = helper::OptBool(L, 5, false);
    jc->motor_speed      = helper::OptFloat(L, 6, 0.0f);
    jc->max_motor_torque = helper::OptFloat(L, 7, 0.0f);
    return 0;
}

/// set_joint_2d_distance(entity, min_len, max_len, stiffness, damping)
int L_EcsSetJoint2DDistance(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* jc = helper::TryGetComponent<Joint2DComponent>(*world, e);
    if (!jc) return 0;
    jc->min_length = helper::OptFloat(L, 2, 0.0f);
    jc->max_length = helper::OptFloat(L, 3, 1.0f);
    jc->stiffness  = helper::OptFloat(L, 4, 0.0f);
    jc->damping    = helper::OptFloat(L, 5, 0.0f);
    return 0;
}

/// set_joint_2d_prismatic(entity, axis_x, axis_y,
///                        enable_limit, lower, upper,
///                        enable_motor, motor_speed, max_force)
int L_EcsSetJoint2DPrismatic(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* jc = helper::TryGetComponent<Joint2DComponent>(*world, e);
    if (!jc) return 0;
    jc->prismatic_axis        = glm::vec2(helper::OptFloat(L, 2, 1.0f), helper::OptFloat(L, 3, 0.0f));
    jc->enable_limit          = helper::OptBool(L, 4, false);
    jc->lower_translation     = helper::OptFloat(L, 5, 0.0f);
    jc->upper_translation     = helper::OptFloat(L, 6, 0.0f);
    jc->enable_motor          = helper::OptBool(L, 7, false);
    jc->prismatic_motor_speed = helper::OptFloat(L, 8, 0.0f);
    jc->max_motor_force       = helper::OptFloat(L, 9, 0.0f);
    return 0;
}

/// destroy_joint_2d(entity) — 销毁关节
int L_EcsDestroyJoint2D(lua_State* L) {
    auto* physics = dse::core::ServiceLocator::Instance().Get<Physics2DSystem>();
    if (!physics) return 0;
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    physics->DestroyJoint(*world, e);
    return 0;
}

int L_EcsAddTilemap(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int width = helper::CheckInt(L, 2);
    int height = helper::CheckInt(L, 3);
    float tile_size = helper::OptFloat(L, 4, 1.0f);
    unsigned int tex_handle = static_cast<unsigned int>(helper::OptInt(L, 5, 0));
    auto& tilemap = world->registry().emplace_or_replace<TilemapComponent>(e);
    tilemap.width = width;
    tilemap.height = height;
    tilemap.tile_size = tile_size;
    tilemap.tileset_handle = tex_handle;
    tilemap.tiles.resize(width * height, -1);
    return 0;
}

int L_EcsSetTile(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int x = helper::CheckInt(L, 2);
    int y = helper::CheckInt(L, 3);
    int tile_id = helper::CheckInt(L, 4);
    auto* tilemap = helper::TryGetComponent<TilemapComponent>(*world, e);
    if (!tilemap) return 0;
    if (x >= 0 && x < tilemap->width && y >= 0 && y < tilemap->height) {
        tilemap->tiles[y * tilemap->width + x] = tile_id;
        tilemap->dirty = true;
    }
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
