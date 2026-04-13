/**
 * @file physics2d_system.cpp
 * @brief 物理系统，封装 2D/3D 物理引擎(如 Box2D/PhysX)，处理碰撞和刚体模拟
 */

#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/base/debug.h"
#include <glm/gtx/quaternion.hpp>

namespace {

bool IsValidBodyId(const b2BodyId& body_id) {
    return B2_IS_NON_NULL(body_id);
}

bool IsValidShapeId(const b2ShapeId& shape_id) {
    return B2_IS_NON_NULL(shape_id);
}

bool IsBodyTypeDynamic(RigidBody2DType type) {
    return type == RigidBody2DType::Dynamic;
}

b2BodyType ToBox2DBodyType(RigidBody2DType type) {
    switch (type) {
        case RigidBody2DType::Static: return b2_staticBody;
        case RigidBody2DType::Kinematic: return b2_kinematicBody;
        case RigidBody2DType::Dynamic: return b2_dynamicBody;
        default: return b2_staticBody;
    }
}

Entity BodyEntity(const b2BodyId& body_id) {
    return static_cast<Entity>(reinterpret_cast<uintptr_t>(b2Body_GetUserData(body_id)));
}

void NotifyContactEnter(World& world, Entity entityA, Entity entityB, bool is_trigger) {
    if (world.registry().valid(entityA) && world.registry().all_of<RigidBody2DComponent>(entityA)) {
        auto& rbA = world.registry().get<RigidBody2DComponent>(entityA);
        if (is_trigger) {
            if (rbA.on_trigger_enter) rbA.on_trigger_enter(entityB);
        } else {
            if (rbA.on_collision_enter) rbA.on_collision_enter(entityB);
        }
    }

    if (world.registry().valid(entityB) && world.registry().all_of<RigidBody2DComponent>(entityB)) {
        auto& rbB = world.registry().get<RigidBody2DComponent>(entityB);
        if (is_trigger) {
            if (rbB.on_trigger_enter) rbB.on_trigger_enter(entityA);
        } else {
            if (rbB.on_collision_enter) rbB.on_collision_enter(entityA);
        }
    }
}

void NotifyContactExit(World& world, Entity entityA, Entity entityB, bool is_trigger) {
    if (world.registry().valid(entityA) && world.registry().all_of<RigidBody2DComponent>(entityA)) {
        auto& rbA = world.registry().get<RigidBody2DComponent>(entityA);
        if (is_trigger) {
            if (rbA.on_trigger_exit) rbA.on_trigger_exit(entityB);
        } else {
            if (rbA.on_collision_exit) rbA.on_collision_exit(entityB);
        }
    }

    if (world.registry().valid(entityB) && world.registry().all_of<RigidBody2DComponent>(entityB)) {
        auto& rbB = world.registry().get<RigidBody2DComponent>(entityB);
        if (is_trigger) {
            if (rbB.on_trigger_exit) rbB.on_trigger_exit(entityA);
        } else {
            if (rbB.on_collision_exit) rbB.on_collision_exit(entityA);
        }
    }
}

} // namespace

Physics2DSystem::Physics2DSystem() {
}

Physics2DSystem::~Physics2DSystem() {
}

void Physics2DSystem::Init(World& world) {
    (void)world;
    Shutdown();
    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = b2Vec2{0.0f, -9.81f};
    physics_world_ = b2CreateWorld(&world_def);
    DEBUG_LOG_INFO("[Physics2D] Init world_valid={}", b2World_IsValid(physics_world_));
}

void Physics2DSystem::Shutdown() {
    active_contact_pairs_.clear();
    if (b2World_IsValid(physics_world_)) {
        b2DestroyWorld(physics_world_);
    }
    physics_world_ = b2WorldId{};
}


void Physics2DSystem::FixedUpdate(World& world, float fixed_delta_time) {
    if (!b2World_IsValid(physics_world_)) return;

    auto collider_view = world.registry().view<BoxCollider2DComponent>();
    for (auto entity : collider_view) {
        auto& collider = collider_view.get<BoxCollider2DComponent>(entity);
        if (!world.registry().all_of<RigidBody2DComponent>(entity)) {
            collider.runtime_fixture = b2ShapeId{};
            continue;
        }
        auto& rb = world.registry().get<RigidBody2DComponent>(entity);
        if (!IsValidBodyId(rb.runtime_body)) {
            collider.runtime_fixture = b2ShapeId{};
        }
    }

    auto rb_view = world.registry().view<RigidBody2DComponent>();
    for (auto entity : rb_view) {
        auto& rb = rb_view.get<RigidBody2DComponent>(entity);
        if (!IsValidBodyId(rb.runtime_body) || !b2Body_IsValid(rb.runtime_body)) {
            rb.runtime_body = b2BodyId{};
            if (world.registry().all_of<BoxCollider2DComponent>(entity)) {
                world.registry().get<BoxCollider2DComponent>(entity).runtime_fixture = b2ShapeId{};
            }
        }
    }

    // 1. Create Box2D bodies for new entities or update existing ones
    auto view = world.registry().view<RigidBody2DComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb = view.get<RigidBody2DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (!IsValidBodyId(rb.runtime_body)) {
            b2BodyDef body_def = b2DefaultBodyDef();
            body_def.position = b2Vec2{transform.position.x, transform.position.y};
            body_def.rotation = b2MakeRot(glm::roll(transform.rotation));
            body_def.type = ToBox2DBodyType(rb.type);
            body_def.gravityScale = rb.gravity_scale;
            body_def.motionLocks.angularZ = rb.fixed_rotation;

            rb.runtime_body = b2CreateBody(physics_world_, &body_def);
            b2Body_SetUserData(rb.runtime_body, reinterpret_cast<void*>(static_cast<uintptr_t>(entity)));
            DEBUG_LOG_INFO("[Physics2D] CreateBody entity={} valid={}", static_cast<uint32_t>(entity), b2Body_IsValid(rb.runtime_body));

            if (world.registry().all_of<BoxCollider2DComponent>(entity)) {
                auto& bc = world.registry().get<BoxCollider2DComponent>(entity);
                b2Polygon box_shape = b2MakeOffsetBox(
                    bc.size.x * transform.scale.x / 2.0f,
                    bc.size.y * transform.scale.y / 2.0f,
                    b2Vec2{bc.offset.x, bc.offset.y},
                    b2MakeRot(0.0f));

                b2ShapeDef shape_def = b2DefaultShapeDef();
                shape_def.density = bc.density;
                shape_def.isSensor = bc.is_trigger;
                shape_def.enableSensorEvents = bc.is_trigger;
                shape_def.enableContactEvents = !bc.is_trigger;
                bc.runtime_fixture = b2CreatePolygonShape(rb.runtime_body, &shape_def, &box_shape);
                if (IsValidShapeId(bc.runtime_fixture)) {
                    b2Shape_SetFriction(bc.runtime_fixture, bc.friction);
                    b2Shape_SetRestitution(bc.runtime_fixture, bc.restitution);
                }
            }
        } else {
            const b2Vec2 body_position = b2Body_GetPosition(rb.runtime_body);
            const float body_angle = b2Rot_GetAngle(b2Body_GetRotation(rb.runtime_body));
            const float ecs_angle = glm::roll(transform.rotation);
            const bool transform_mismatch = std::abs(body_position.x - transform.position.x) > 0.0001f ||
                                            std::abs(body_position.y - transform.position.y) > 0.0001f ||
                                            std::abs(body_angle - ecs_angle) > 0.0001f;
            if (IsBodyTypeDynamic(rb.type)) {
                if (transform.dirty) {
                    b2Body_SetTransform(rb.runtime_body, b2Vec2{transform.position.x, transform.position.y}, b2MakeRot(ecs_angle));
                    b2Body_SetLinearVelocity(rb.runtime_body, b2Vec2{rb.velocity.x, rb.velocity.y});
                    b2Body_SetAwake(rb.runtime_body, true);
                    transform.dirty = false;
                } else if (transform_mismatch) {
                    transform.position.x = body_position.x;
                    transform.position.y = body_position.y;
                    transform.rotation = glm::angleAxis(body_angle, glm::vec3(0.0f, 0.0f, 1.0f));
                    transform.dirty = false;
                }
            } else {
                if (transform.dirty) {
                    b2Body_SetTransform(rb.runtime_body, b2Vec2{transform.position.x, transform.position.y}, b2MakeRot(ecs_angle));
                    transform.dirty = false;
                } else if (transform_mismatch) {
                    transform.position.x = body_position.x;
                    transform.position.y = body_position.y;
                    transform.rotation = glm::angleAxis(body_angle, glm::vec3(0.0f, 0.0f, 1.0f));
                    transform.dirty = false;
                }

                if (rb.type == RigidBody2DType::Kinematic) {
                    b2Body_SetLinearVelocity(rb.runtime_body, b2Vec2{rb.velocity.x, rb.velocity.y});
                }
            }
        }
    }

    // 2. Step the physics world
    b2World_Step(physics_world_, fixed_delta_time, 4);

    b2ContactEvents contact_events = b2World_GetContactEvents(physics_world_);
    for (int i = 0; i < contact_events.beginCount; ++i) {
        const b2ContactBeginTouchEvent& event = contact_events.beginEvents[i];
        const Entity entityA = BodyEntity(b2Shape_GetBody(event.shapeIdA));
        const Entity entityB = BodyEntity(b2Shape_GetBody(event.shapeIdB));
        const bool is_trigger = b2Shape_IsSensor(event.shapeIdA) || b2Shape_IsSensor(event.shapeIdB);
        Entity orderedA = entityA;
        Entity orderedB = entityB;
        if (orderedB < orderedA) {
            std::swap(orderedA, orderedB);
        }
        active_contact_pairs_.emplace(orderedA, orderedB, is_trigger);
        NotifyContactEnter(world, entityA, entityB, is_trigger);
    }
    for (int i = 0; i < contact_events.endCount; ++i) {
        const b2ContactEndTouchEvent& event = contact_events.endEvents[i];
        const Entity entityA = BodyEntity(b2Shape_GetBody(event.shapeIdA));
        const Entity entityB = BodyEntity(b2Shape_GetBody(event.shapeIdB));
        const bool is_trigger = b2Shape_IsSensor(event.shapeIdA) || b2Shape_IsSensor(event.shapeIdB);
        Entity orderedA = entityA;
        Entity orderedB = entityB;
        if (orderedB < orderedA) {
            std::swap(orderedA, orderedB);
        }
        active_contact_pairs_.erase(std::make_tuple(orderedA, orderedB, is_trigger));
        NotifyContactExit(world, entityA, entityB, is_trigger);
    }


    // 3. Sync Box2D transforms back to ECS
    for (auto entity : view) {
        auto& rb = view.get<RigidBody2DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (IsValidBodyId(rb.runtime_body) && rb.type == RigidBody2DType::Dynamic) {
            b2Vec2 position = b2Body_GetPosition(rb.runtime_body);
            float angle = b2Rot_GetAngle(b2Body_GetRotation(rb.runtime_body));

            transform.position.x = position.x;
            transform.position.y = position.y;
            transform.rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            transform.dirty = false;

            b2Vec2 velocity = b2Body_GetLinearVelocity(rb.runtime_body);
            rb.velocity.x = velocity.x;
            rb.velocity.y = velocity.y;
        }
    }
}

// Raycast uses Box2D handle-style closest-hit query in current Box2D version.

bool Physics2DSystem::Raycast(const glm::vec2& start, const glm::vec2& end, Entity& out_entity, glm::vec2& out_point, glm::vec2& out_normal) {
    if (!b2World_IsValid(physics_world_)) return false;

    const b2Vec2 origin{start.x, start.y};
    const b2Vec2 translation{end.x - start.x, end.y - start.y};
    const b2QueryFilter filter = b2DefaultQueryFilter();
    const b2RayResult result = b2World_CastRayClosest(physics_world_, origin, translation, filter);
    if (!IsValidShapeId(result.shapeId)) {
        return false;
    }

    const b2BodyId hit_body = b2Shape_GetBody(result.shapeId);
    out_entity = BodyEntity(hit_body);
    out_point = glm::vec2(result.point.x, result.point.y);
    out_normal = glm::vec2(result.normal.x, result.normal.y);
    return true;
}
