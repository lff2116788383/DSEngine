#include "steering_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_2d.h"
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>

namespace dse::gameplay3d {

void SteeringSystem::Update(World& world, float delta_time) {
    if (delta_time <= 0.0f) return;

    auto view = world.registry().view<TransformComponent, SteeringComponent>();

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& steering = view.get<SteeringComponent>(entity);

        if (!steering.enabled) continue;

        glm::vec3 steering_force(0.0f);

        // 1. Seek
        if (steering.seek_enabled) {
            glm::vec3 desired_velocity = steering.seek_target - transform.position;
            if (glm::length2(desired_velocity) > 0.0001f) {
                desired_velocity = glm::normalize(desired_velocity) * steering.max_velocity;
                steering_force += (desired_velocity - steering.velocity);
            }
        }

        // 2. Flee
        if (steering.flee_enabled) {
            glm::vec3 desired_velocity = transform.position - steering.flee_target;
            if (glm::length2(desired_velocity) > 0.0001f) {
                desired_velocity = glm::normalize(desired_velocity) * steering.max_velocity;
                steering_force += (desired_velocity - steering.velocity);
            }
        }

        // 3. Arrive
        if (steering.arrive_enabled) {
            glm::vec3 target_offset = steering.arrive_target - transform.position;
            float distance = glm::length(target_offset);
            
            if (distance > 0.0001f) {
                float speed = steering.max_velocity;
                if (distance < steering.arrive_deceleration_radius) {
                    speed *= (distance / steering.arrive_deceleration_radius);
                }
                
                glm::vec3 desired_velocity = (target_offset / distance) * speed;
                steering_force += (desired_velocity - steering.velocity);
            } else {
                steering.velocity = glm::vec3(0.0f);
            }
        }

        // Apply force
        if (glm::length2(steering_force) > 0.0001f) {
            // Truncate force
            if (glm::length(steering_force) > steering.max_force) {
                steering_force = glm::normalize(steering_force) * steering.max_force;
            }
            
            glm::vec3 acceleration = steering_force / steering.mass;
            steering.velocity += acceleration * delta_time;
        }

        // Truncate velocity
        if (glm::length(steering.velocity) > steering.max_velocity) {
            steering.velocity = glm::normalize(steering.velocity) * steering.max_velocity;
        }

        // Update position
        transform.position += steering.velocity * delta_time;

        // Update rotation (face moving direction)
        if (glm::length2(steering.velocity) > 0.001f) {
            glm::vec3 forward = glm::normalize(steering.velocity);
            // Default forward is usually -Z or +Z. Assuming -Z is forward:
            glm::vec3 up(0.0f, 1.0f, 0.0f);
            glm::mat4 look_at = glm::lookAt(glm::vec3(0.0f), forward, up);
            transform.rotation = glm::quat_cast(glm::inverse(look_at));
        }
    }
}

} // namespace dse::gameplay3d