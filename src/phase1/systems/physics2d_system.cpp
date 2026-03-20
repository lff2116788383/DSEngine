#include "phase1/systems/physics2d_system.h"
#include <glm/gtx/quaternion.hpp>

Physics2DSystem::Physics2DSystem() {
}

Physics2DSystem::~Physics2DSystem() {
}

void Physics2DSystem::Init() {
    b2Vec2 gravity(0.0f, -9.81f);
    physics_world_ = std::make_unique<b2World>(gravity);
}

void Physics2DSystem::FixedUpdate(Phase1World& world, float fixed_delta_time) {
    if (!physics_world_) return;

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
