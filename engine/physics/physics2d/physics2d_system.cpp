#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/ecs/components_2d.h"
#include <glm/gtx/quaternion.hpp>
#include <iostream>

// --- Contact Listener ---
class PhysicsContactListener : public b2ContactListener {
public:
    PhysicsContactListener(World* world) : world_(world) {}

    void BeginContact(b2Contact* contact) override {
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();

        Entity entityA = (Entity)(uintptr_t)fixtureA->GetBody()->GetUserData().pointer;
        Entity entityB = (Entity)(uintptr_t)fixtureB->GetBody()->GetUserData().pointer;

        if (world_->registry().valid(entityA) && world_->registry().all_of<RigidBody2DComponent>(entityA)) {
            auto& rbA = world_->registry().get<RigidBody2DComponent>(entityA);
            if (fixtureA->IsSensor() || fixtureB->IsSensor()) {
                if (rbA.on_trigger_enter) rbA.on_trigger_enter(entityB);
            } else {
                if (rbA.on_collision_enter) rbA.on_collision_enter(entityB);
            }
        }

        if (world_->registry().valid(entityB) && world_->registry().all_of<RigidBody2DComponent>(entityB)) {
            auto& rbB = world_->registry().get<RigidBody2DComponent>(entityB);
            if (fixtureA->IsSensor() || fixtureB->IsSensor()) {
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

        if (world_->registry().valid(entityA) && world_->registry().all_of<RigidBody2DComponent>(entityA)) {
            auto& rbA = world_->registry().get<RigidBody2DComponent>(entityA);
            if (fixtureA->IsSensor() || fixtureB->IsSensor()) {
                if (rbA.on_trigger_exit) rbA.on_trigger_exit(entityB);
            } else {
                if (rbA.on_collision_exit) rbA.on_collision_exit(entityB);
            }
        }

        if (world_->registry().valid(entityB) && world_->registry().all_of<RigidBody2DComponent>(entityB)) {
            auto& rbB = world_->registry().get<RigidBody2DComponent>(entityB);
            if (fixtureA->IsSensor() || fixtureB->IsSensor()) {
                if (rbB.on_trigger_exit) rbB.on_trigger_exit(entityA);
            } else {
                if (rbB.on_collision_exit) rbB.on_collision_exit(entityA);
            }
        }
    }

private:
    World* world_;
};

Physics2DSystem::Physics2DSystem() {
}

Physics2DSystem::~Physics2DSystem() {
}

void Physics2DSystem::Init(World& world) {
    b2Vec2 gravity(0.0f, -9.81f);
    physics_world_ = std::make_unique<b2World>(gravity);
    contact_listener_ = std::make_unique<PhysicsContactListener>(&world);
    physics_world_->SetContactListener(contact_listener_.get());
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
            // Update kinematic bodies or manual transform changes if needed
            if (rb.type == RigidBody2DType::Kinematic) {
                rb.runtime_body->SetLinearVelocity(b2Vec2(rb.velocity.x, rb.velocity.y));
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
            // Assuming Z remains unchanged
            
            // Set rotation around Z axis
            transform.rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            transform.dirty = true;
            
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
