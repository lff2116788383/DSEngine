/**
 * @file physics2d_system.cpp
 * @brief 物理系统，封装 2D/3D 物理引擎(如 Box2D/PhysX)，处理碰撞和刚体模拟
 */

#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/base/debug.h"
#include <glm/gtx/quaternion.hpp>

// --- Contact Listener ---
class PhysicsContactListener : public b2ContactListener {
public:
    PhysicsContactListener(World* world, Physics2DSystem* system) : world_(world), system_(system) {}

    void BeginContact(b2Contact* contact) override {
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();

        Entity entityA = (Entity)(uintptr_t)fixtureA->GetBody()->GetUserData().pointer;
        Entity entityB = (Entity)(uintptr_t)fixtureB->GetBody()->GetUserData().pointer;
        const bool is_trigger = fixtureA->IsSensor() || fixtureB->IsSensor();

        Entity orderedA = entityA;
        Entity orderedB = entityB;
        if (orderedB < orderedA) {
            std::swap(orderedA, orderedB);
        }
        system_->active_contact_pairs_.emplace(orderedA, orderedB, is_trigger);

        if (world_->registry().valid(entityA) && world_->registry().all_of<RigidBody2DComponent>(entityA)) {
            auto& rbA = world_->registry().get<RigidBody2DComponent>(entityA);
            if (is_trigger) {
                if (rbA.on_trigger_enter) rbA.on_trigger_enter(entityB);
            } else {
                if (rbA.on_collision_enter) rbA.on_collision_enter(entityB);
            }
        }

        if (world_->registry().valid(entityB) && world_->registry().all_of<RigidBody2DComponent>(entityB)) {
            auto& rbB = world_->registry().get<RigidBody2DComponent>(entityB);
            if (is_trigger) {
                if (rbB.on_trigger_enter) rbB.on_trigger_enter(entityA);
            } else {
                if (rbB.on_collision_enter) rbB.on_collision_enter(entityA);
            }
        }
    }

    void EndContact(b2Contact* contact) override {
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();

        Entity entityA = (Entity)(uintptr_t)fixtureA->GetBody()->GetUserData().pointer;
        Entity entityB = (Entity)(uintptr_t)fixtureB->GetBody()->GetUserData().pointer;
        const bool is_trigger = fixtureA->IsSensor() || fixtureB->IsSensor();

        Entity orderedA = entityA;
        Entity orderedB = entityB;
        if (orderedB < orderedA) {
            std::swap(orderedA, orderedB);
        }
        system_->active_contact_pairs_.erase(std::make_tuple(orderedA, orderedB, is_trigger));

        if (world_->registry().valid(entityA) && world_->registry().all_of<RigidBody2DComponent>(entityA)) {
            auto& rbA = world_->registry().get<RigidBody2DComponent>(entityA);
            if (is_trigger) {
                if (rbA.on_trigger_exit) rbA.on_trigger_exit(entityB);
            } else {
                if (rbA.on_collision_exit) rbA.on_collision_exit(entityB);
            }
        }

        if (world_->registry().valid(entityB) && world_->registry().all_of<RigidBody2DComponent>(entityB)) {
            auto& rbB = world_->registry().get<RigidBody2DComponent>(entityB);
            if (is_trigger) {
                if (rbB.on_trigger_exit) rbB.on_trigger_exit(entityA);
            } else {
                if (rbB.on_collision_exit) rbB.on_collision_exit(entityA);
            }
        }
    }

private:
    World* world_;
    Physics2DSystem* system_;
};

Physics2DSystem::Physics2DSystem() {
}

Physics2DSystem::~Physics2DSystem() {
}

void Physics2DSystem::Init(World& world) {
    Shutdown();
    b2Vec2 gravity(0.0f, -9.81f);
    physics_world_ = std::make_unique<b2World>(gravity);
    contact_listener_ = std::make_unique<PhysicsContactListener>(&world, this);
    physics_world_->SetContactListener(contact_listener_.get());
}

void Physics2DSystem::Shutdown() {
    active_contact_pairs_.clear();
    contact_listener_.reset();
    physics_world_.reset();
}


void Physics2DSystem::FixedUpdate(World& world, float fixed_delta_time) {
    if (!physics_world_) return;

    for (b2Body* body = physics_world_->GetBodyList(); body != nullptr;) {
        b2Body* next = body->GetNext();
        Entity entity = static_cast<Entity>(body->GetUserData().pointer);
        if (!world.registry().valid(entity) || !world.registry().all_of<RigidBody2DComponent>(entity)) {
            physics_world_->DestroyBody(body);
        }
        body = next;
    }

    auto collider_view = world.registry().view<BoxCollider2DComponent>();
    for (auto entity : collider_view) {
        auto& collider = collider_view.get<BoxCollider2DComponent>(entity);
        if (!world.registry().all_of<RigidBody2DComponent>(entity)) {
            collider.runtime_fixture = nullptr;
            continue;
        }
        auto& rb = world.registry().get<RigidBody2DComponent>(entity);
        if (rb.runtime_body == nullptr) {
            collider.runtime_fixture = nullptr;
        } else if (collider.runtime_fixture != nullptr) {
            collider.runtime_fixture->Refilter();
        }
    }

    auto rb_view = world.registry().view<RigidBody2DComponent>();
    for (auto entity : rb_view) {
        auto& rb = rb_view.get<RigidBody2DComponent>(entity);
        if (rb.runtime_body == nullptr) {
            continue;
        }
        const b2Body* runtime_body = rb.runtime_body;
        bool body_still_alive = false;
        for (b2Body* body = physics_world_->GetBodyList(); body != nullptr; body = body->GetNext()) {
            if (body == runtime_body) {
                body_still_alive = true;
                break;
            }
        }
        if (!body_still_alive) {
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

        if (!rb.runtime_body) {
            b2BodyDef body_def;
            body_def.position.Set(transform.position.x, transform.position.y);
            body_def.angle = glm::roll(transform.rotation); // Assuming rotation is around Z for 2D
            
            switch (rb.type) {
                case RigidBody2DType::Static: body_def.type = b2_staticBody; break;
                case RigidBody2DType::Kinematic: body_def.type = b2_kinematicBody; break;
                case RigidBody2DType::Dynamic: body_def.type = b2_dynamicBody; break;
            }
            
            body_def.gravityScale = rb.gravity_scale;
            body_def.fixedRotation = rb.fixed_rotation;
            
            b2Body* body = physics_world_->CreateBody(&body_def);
            body->GetUserData().pointer = static_cast<uintptr_t>(entity);
            rb.runtime_body = body;

            // Check if it has a collider
            if (world.registry().all_of<BoxCollider2DComponent>(entity)) {
                auto& bc = world.registry().get<BoxCollider2DComponent>(entity);
                
                b2PolygonShape box_shape;
                box_shape.SetAsBox(bc.size.x * transform.scale.x / 2.0f, 
                                   bc.size.y * transform.scale.y / 2.0f,
                                   b2Vec2(bc.offset.x, bc.offset.y), 
                                   0.0f);

                b2FixtureDef fixture_def;
                fixture_def.shape = &box_shape;
                fixture_def.density = bc.density;
                fixture_def.friction = bc.friction;
                fixture_def.restitution = bc.restitution;
                fixture_def.isSensor = bc.is_trigger;

                bc.runtime_fixture = body->CreateFixture(&fixture_def);
            }
        } else {
            const b2Vec2 body_position = rb.runtime_body->GetPosition();
            const float body_angle = rb.runtime_body->GetAngle();
            const float ecs_angle = glm::roll(transform.rotation);
            const bool transform_mismatch = std::abs(body_position.x - transform.position.x) > 0.0001f ||
                                            std::abs(body_position.y - transform.position.y) > 0.0001f ||
                                            std::abs(body_angle - ecs_angle) > 0.0001f;
            if (rb.type == RigidBody2DType::Dynamic) {
                if (transform.dirty) {
                    rb.runtime_body->SetTransform(b2Vec2(transform.position.x, transform.position.y), ecs_angle);
                    rb.runtime_body->SetLinearVelocity(b2Vec2(rb.velocity.x, rb.velocity.y));
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
                    rb.runtime_body->SetTransform(b2Vec2(transform.position.x, transform.position.y), ecs_angle);
                    transform.dirty = false;
                } else if (transform_mismatch) {
                    transform.position.x = body_position.x;
                    transform.position.y = body_position.y;
                    transform.rotation = glm::angleAxis(body_angle, glm::vec3(0.0f, 0.0f, 1.0f));
                    transform.dirty = false;
                }

                if (rb.type == RigidBody2DType::Kinematic) {
                    rb.runtime_body->SetLinearVelocity(b2Vec2(rb.velocity.x, rb.velocity.y));
                }
            }
        }
    }

    // 2. Step the physics world
    physics_world_->Step(fixed_delta_time, velocity_iterations_, position_iterations_);


    // 3. Sync Box2D transforms back to ECS
    for (auto entity : view) {
        auto& rb = view.get<RigidBody2DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (rb.runtime_body && rb.type == RigidBody2DType::Dynamic) {
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

// Raycast Callback
class SimpleRayCastCallback : public b2RayCastCallback {
public:
    Entity hit_entity = entt::null;
    b2Vec2 hit_point;
    b2Vec2 hit_normal;
    float fraction = 1.0f;
    bool hit = false;

    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        this->hit = true;
        this->hit_entity = (Entity)(uintptr_t)fixture->GetBody()->GetUserData().pointer;
        this->hit_point = point;
        this->hit_normal = normal;
        this->fraction = fraction;
        
        // Return fraction to find closest hit
        return fraction;
    }
};

bool Physics2DSystem::Raycast(const glm::vec2& start, const glm::vec2& end, Entity& out_entity, glm::vec2& out_point, glm::vec2& out_normal) {
    if (!physics_world_) return false;

    SimpleRayCastCallback callback;
    physics_world_->RayCast(&callback, b2Vec2(start.x, start.y), b2Vec2(end.x, end.y));

    if (callback.hit) {
        out_entity = callback.hit_entity;
        out_point = glm::vec2(callback.hit_point.x, callback.hit_point.y);
        out_normal = glm::vec2(callback.hit_normal.x, callback.hit_normal.y);
        return true;
    }
    return false;
}
