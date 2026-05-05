/**
 * @file physics2d_system.cpp
 * @brief 物理系统，封装 2D/3D 物理引擎(如 Box2D/PhysX)，处理碰撞和刚体模拟
 */

#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/ecs/physics_2d.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include <box2d/b2_revolute_joint.h>
#include <box2d/b2_distance_joint.h>
#include <box2d/b2_prismatic_joint.h>
#include <box2d/b2_weld_joint.h>
#include <glm/gtx/quaternion.hpp>
#include <cmath>

namespace {

bool IsValidBody(const b2Body* body) {
    return body != nullptr;
}

bool IsValidFixture(const b2Fixture* fixture) {
    return fixture != nullptr;
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

Entity BodyEntity(const b2Body* body) {
    if (body == nullptr) {
        return entt::null;
    }
    return static_cast<Entity>(const_cast<b2Body*>(body)->GetUserData().pointer);
}

void NotifyContactEnter(World& world, Entity entityA, Entity entityB, bool is_trigger) {
    if (world.registry().valid(entityA) && world.registry().all_of<RigidBody2DComponent>(entityA)) {
        auto& rbA = world.registry().get<RigidBody2DComponent>(entityA);
        rbA.pending_contact_events.push_back({entityB, is_trigger, true});
        if (is_trigger) {
            if (rbA.on_trigger_enter) rbA.on_trigger_enter(entityB);
        } else {
            if (rbA.on_collision_enter) rbA.on_collision_enter(entityB);
        }
    }

    if (world.registry().valid(entityB) && world.registry().all_of<RigidBody2DComponent>(entityB)) {
        auto& rbB = world.registry().get<RigidBody2DComponent>(entityB);
        rbB.pending_contact_events.push_back({entityA, is_trigger, true});
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
        rbA.pending_contact_events.push_back({entityB, is_trigger, false});
        if (is_trigger) {
            if (rbA.on_trigger_exit) rbA.on_trigger_exit(entityB);
        } else {
            if (rbA.on_collision_exit) rbA.on_collision_exit(entityB);
        }
    }

    if (world.registry().valid(entityB) && world.registry().all_of<RigidBody2DComponent>(entityB)) {
        auto& rbB = world.registry().get<RigidBody2DComponent>(entityB);
        rbB.pending_contact_events.push_back({entityA, is_trigger, false});
        if (is_trigger) {
            if (rbB.on_trigger_exit) rbB.on_trigger_exit(entityA);
        } else {
            if (rbB.on_collision_exit) rbB.on_collision_exit(entityA);
        }
    }
}

bool IsSensorFixture(const b2Fixture* fixture) {
    return fixture != nullptr && fixture->IsSensor();
}

class ClosestRayCastCallback final : public b2RayCastCallback {
public:
    float ReportFixture(b2Fixture* fixture,
                        const b2Vec2& point,
                        const b2Vec2& normal,
                        float fraction) override {
        hit = true;
        hit_fixture = fixture;
        hit_point = point;
        hit_normal = normal;
        return fraction;
    }

    bool hit = false;
    b2Fixture* hit_fixture = nullptr;
    b2Vec2 hit_point{0.0f, 0.0f};
    b2Vec2 hit_normal{0.0f, 0.0f};
};

} // namespace

Physics2DSystem::Physics2DSystem() {
}

Physics2DSystem::~Physics2DSystem() {
}

void Physics2DSystem::Init(World& world) {
    Shutdown();

    auto collider_view = world.registry().view<BoxCollider2DComponent>();
    for (auto entity : collider_view) {
        collider_view.get<BoxCollider2DComponent>(entity).runtime_fixture = nullptr;
    }
    auto rb_view = world.registry().view<RigidBody2DComponent>();
    for (auto entity : rb_view) {
        rb_view.get<RigidBody2DComponent>(entity).runtime_body = nullptr;
    }
    auto joint_view_init = world.registry().view<Joint2DComponent>();
    for (auto entity : joint_view_init) {
        joint_view_init.get<Joint2DComponent>(entity).runtime_joint = nullptr;
    }

    physics_world_ = new b2World(b2Vec2(0.0f, -9.81f));
    DEBUG_LOG_INFO("[Physics2D] Init world_valid={}", physics_world_ != nullptr);

    // 立即同步 ECS 物理组件到 Box2D。这样 Init 后即可进行 Raycast，
    // 也避免 Shutdown 后再次 Init 时复用已失效的 runtime 指针。
    FixedUpdate(world, 0.0f);
}

void Physics2DSystem::Shutdown() {
    active_contact_pairs_.clear();
    if (physics_world_ != nullptr) {
        delete physics_world_;
        physics_world_ = nullptr;
    }
}


void Physics2DSystem::FixedUpdate(World& world, float fixed_delta_time) {
    if (physics_world_ == nullptr) return;

    auto collider_view = world.registry().view<BoxCollider2DComponent>();
    for (auto entity : collider_view) {
        auto& collider = collider_view.get<BoxCollider2DComponent>(entity);
        if (!world.registry().all_of<RigidBody2DComponent>(entity)) {
            collider.runtime_fixture = nullptr;
            continue;
        }
        auto& rb = world.registry().get<RigidBody2DComponent>(entity);
        if (!IsValidBody(rb.runtime_body)) {
            collider.runtime_fixture = nullptr;
        }
    }

    auto rb_view = world.registry().view<RigidBody2DComponent>();
    for (auto entity : rb_view) {
        auto& rb = rb_view.get<RigidBody2DComponent>(entity);
        if (!IsValidBody(rb.runtime_body)) {
            rb.runtime_body = nullptr;
            if (world.registry().all_of<BoxCollider2DComponent>(entity)) {
                world.registry().get<BoxCollider2DComponent>(entity).runtime_fixture = nullptr;
            }
        }
    }

    // 1. Create Box2D bodies for new entities or update existing ones
    auto view = world.registry().view<RigidBody2DComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb = view.get<RigidBody2DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (!IsValidBody(rb.runtime_body)) {
            b2BodyDef body_def;
            body_def.position = b2Vec2{transform.position.x, transform.position.y};
            body_def.angle = glm::roll(transform.rotation);
            body_def.type = ToBox2DBodyType(rb.type);
            body_def.gravityScale = rb.gravity_scale;
            body_def.fixedRotation = rb.fixed_rotation;

            rb.runtime_body = physics_world_->CreateBody(&body_def);
            rb.runtime_body->GetUserData().pointer = static_cast<uintptr_t>(entity);
            DEBUG_LOG_INFO("[Physics2D] CreateBody entity={} valid={}", static_cast<uint32_t>(entity), rb.runtime_body != nullptr);

            if (world.registry().all_of<BoxCollider2DComponent>(entity)) {
                auto& bc = world.registry().get<BoxCollider2DComponent>(entity);
                b2PolygonShape box_shape;
                box_shape.SetAsBox(
                    bc.size.x * transform.scale.x / 2.0f,
                    bc.size.y * transform.scale.y / 2.0f,
                    b2Vec2{bc.offset.x, bc.offset.y},
                    0.0f);

                b2FixtureDef fixture_def;
                fixture_def.shape = &box_shape;
                fixture_def.density = bc.density;
                fixture_def.friction = bc.friction;
                fixture_def.restitution = bc.restitution;
                fixture_def.isSensor = bc.is_trigger;
                bc.runtime_fixture = rb.runtime_body->CreateFixture(&fixture_def);
            }

            rb.runtime_body->SetLinearVelocity(b2Vec2{rb.velocity.x, rb.velocity.y});
        } else {
            const b2Vec2 body_position = rb.runtime_body->GetPosition();
            const float body_angle = rb.runtime_body->GetAngle();
            const float ecs_angle = glm::roll(transform.rotation);
            const bool transform_mismatch = std::fabs(body_position.x - transform.position.x) > 0.0001f ||
                                            std::fabs(body_position.y - transform.position.y) > 0.0001f ||
                                            std::fabs(body_angle - ecs_angle) > 0.0001f;
            if (IsBodyTypeDynamic(rb.type)) {
                if (transform.dirty) {
                    rb.runtime_body->SetTransform(b2Vec2{transform.position.x, transform.position.y}, ecs_angle);
                    rb.runtime_body->SetLinearVelocity(b2Vec2{rb.velocity.x, rb.velocity.y});
                    rb.runtime_body->SetAwake(true);
                    transform.dirty = false;
                } else if (transform_mismatch) {
                    transform.position.x = body_position.x;
                    transform.position.y = body_position.y;
                    transform.rotation = glm::angleAxis(body_angle, glm::vec3(0.0f, 0.0f, 1.0f));
                    transform.dirty = false;
                }
            } else {
                if (transform.dirty) {
                    rb.runtime_body->SetTransform(b2Vec2{transform.position.x, transform.position.y}, ecs_angle);
                    transform.dirty = false;
                } else if (transform_mismatch) {
                    transform.position.x = body_position.x;
                    transform.position.y = body_position.y;
                    transform.rotation = glm::angleAxis(body_angle, glm::vec3(0.0f, 0.0f, 1.0f));
                    transform.dirty = false;
                }

                if (rb.type == RigidBody2DType::Kinematic) {
                    rb.runtime_body->SetLinearVelocity(b2Vec2{rb.velocity.x, rb.velocity.y});
                }
            }
        }
    }

    // 1.5 Create Box2D joints for new Joint2DComponents
    static constexpr float kDeg2Rad = 3.14159265359f / 180.0f;
    auto joint_view = world.registry().view<Joint2DComponent>();
    for (auto entity : joint_view) {
        auto& jc = joint_view.get<Joint2DComponent>(entity);
        if (jc.runtime_joint != nullptr) continue;
        b2Body* bodyA = nullptr;
        b2Body* bodyB = nullptr;
        if (world.registry().valid(jc.entity_a) &&
            world.registry().all_of<RigidBody2DComponent>(jc.entity_a)) {
            bodyA = world.registry().get<RigidBody2DComponent>(jc.entity_a).runtime_body;
        }
        if (world.registry().valid(jc.entity_b) &&
            world.registry().all_of<RigidBody2DComponent>(jc.entity_b)) {
            bodyB = world.registry().get<RigidBody2DComponent>(jc.entity_b).runtime_body;
        }
        if (bodyA == nullptr || bodyB == nullptr) continue;
        b2Vec2 anchorA_world = bodyA->GetWorldPoint(b2Vec2{jc.anchor_a.x, jc.anchor_a.y});
        b2Vec2 anchorB_world = bodyB->GetWorldPoint(b2Vec2{jc.anchor_b.x, jc.anchor_b.y});
        switch (jc.type) {
            case Joint2DType::Revolute: {
                b2RevoluteJointDef def;
                def.bodyA = bodyA;
                def.bodyB = bodyB;
                def.localAnchorA = b2Vec2{jc.anchor_a.x, jc.anchor_a.y};
                def.localAnchorB = b2Vec2{jc.anchor_b.x, jc.anchor_b.y};
                def.referenceAngle = bodyB->GetAngle() - bodyA->GetAngle();
                def.collideConnected = jc.collide_connected;
                def.enableLimit = jc.enable_limit;
                def.lowerAngle = jc.lower_angle * kDeg2Rad;
                def.upperAngle = jc.upper_angle * kDeg2Rad;
                def.enableMotor = jc.enable_motor;
                def.motorSpeed = jc.motor_speed * kDeg2Rad;
                def.maxMotorTorque = jc.max_motor_torque;
                jc.runtime_joint = physics_world_->CreateJoint(&def);
                break;
            }
            case Joint2DType::Distance: {
                b2DistanceJointDef def;
                def.Initialize(bodyA, bodyB, anchorA_world, anchorB_world);
                def.collideConnected = jc.collide_connected;
                def.minLength = jc.min_length;
                def.maxLength = jc.max_length;
                def.stiffness = jc.stiffness;
                def.damping = jc.damping;
                jc.runtime_joint = physics_world_->CreateJoint(&def);
                break;
            }
            case Joint2DType::Prismatic: {
                b2PrismaticJointDef def;
                b2Vec2 axis{jc.prismatic_axis.x, jc.prismatic_axis.y};
                float axisLen = axis.Length();
                if (axisLen > 0.0001f) { axis.x /= axisLen; axis.y /= axisLen; }
                def.Initialize(bodyA, bodyB, anchorA_world, axis);
                def.collideConnected = jc.collide_connected;
                def.enableLimit = jc.enable_limit;
                def.lowerTranslation = jc.lower_translation;
                def.upperTranslation = jc.upper_translation;
                def.enableMotor = jc.enable_motor;
                def.motorSpeed = jc.prismatic_motor_speed;
                def.maxMotorForce = jc.max_motor_force;
                jc.runtime_joint = physics_world_->CreateJoint(&def);
                break;
            }
            case Joint2DType::Weld: {
                b2WeldJointDef def;
                def.Initialize(bodyA, bodyB, anchorA_world);
                def.collideConnected = jc.collide_connected;
                jc.runtime_joint = physics_world_->CreateJoint(&def);
                break;
            }
        }
    }

    // 2. Step the physics world
    physics_world_->Step(fixed_delta_time, velocity_iterations_, position_iterations_);

    std::set<ContactPair> current_contact_pairs;
    for (b2Contact* contact = physics_world_->GetContactList(); contact != nullptr; contact = contact->GetNext()) {
        if (!contact->IsTouching()) {
            continue;
        }

        b2Fixture* fixture_a = contact->GetFixtureA();
        b2Fixture* fixture_b = contact->GetFixtureB();
        const Entity entityA = BodyEntity(fixture_a ? fixture_a->GetBody() : nullptr);
        const Entity entityB = BodyEntity(fixture_b ? fixture_b->GetBody() : nullptr);
        const bool is_trigger = IsSensorFixture(fixture_a) || IsSensorFixture(fixture_b);

        Entity orderedA = entityA;
        Entity orderedB = entityB;
        if (orderedB < orderedA) {
            std::swap(orderedA, orderedB);
        }

        ContactPair pair = std::make_tuple(orderedA, orderedB, is_trigger);
        current_contact_pairs.emplace(pair);
        if (active_contact_pairs_.find(pair) == active_contact_pairs_.end()) {
            NotifyContactEnter(world, entityA, entityB, is_trigger);
        }
    }

    for (const auto& old_pair : active_contact_pairs_) {
        if (current_contact_pairs.find(old_pair) == current_contact_pairs.end()) {
            const Entity entityA = std::get<0>(old_pair);
            const Entity entityB = std::get<1>(old_pair);
            const bool is_trigger = std::get<2>(old_pair);
            NotifyContactExit(world, entityA, entityB, is_trigger);
        }
    }
    active_contact_pairs_ = std::move(current_contact_pairs);

    // 3. Sync Box2D transforms back to ECS
    for (auto entity : view) {
        auto& rb = view.get<RigidBody2DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (IsValidBody(rb.runtime_body) && rb.type == RigidBody2DType::Dynamic) {
            b2Vec2 position = rb.runtime_body->GetPosition();
            float angle = rb.runtime_body->GetAngle();

            transform.position.x = position.x;
            transform.position.y = position.y;
            transform.rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            transform.dirty = false;

            b2Vec2 velocity = rb.runtime_body->GetLinearVelocity();
            rb.velocity.x = velocity.x;
            rb.velocity.y = velocity.y;
        }
    }
}

void Physics2DSystem::DestroyJoint(World& world, Entity entity) {
    if (physics_world_ == nullptr) return;
    if (!world.registry().valid(entity)) return;
    if (!world.registry().all_of<Joint2DComponent>(entity)) return;
    auto& jc = world.registry().get<Joint2DComponent>(entity);
    if (jc.runtime_joint != nullptr) {
        physics_world_->DestroyJoint(jc.runtime_joint);
        jc.runtime_joint = nullptr;
    }
}

bool Physics2DSystem::Raycast(const glm::vec2& start, const glm::vec2& end, Entity& out_entity, glm::vec2& out_point, glm::vec2& out_normal) {
    if (physics_world_ == nullptr) return false;

    ClosestRayCastCallback callback;
    physics_world_->RayCast(&callback, b2Vec2{start.x, start.y}, b2Vec2{end.x, end.y});
    if (!callback.hit || !IsValidFixture(callback.hit_fixture)) {
        return false;
    }

    out_entity = BodyEntity(callback.hit_fixture->GetBody());
    out_point = glm::vec2(callback.hit_point.x, callback.hit_point.y);
    out_normal = glm::vec2(callback.hit_normal.x, callback.hit_normal.y);
    return true;
}
