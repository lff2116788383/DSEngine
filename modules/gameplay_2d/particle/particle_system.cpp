/**
 * @file particle_system.cpp
 * @brief 高级粒子系统，支持随机参数、生命周期曲线、重力和碰撞
 */

#include "modules/gameplay_2d/particle/particle_system.h"
#include "engine/ecs/components_2d.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {

// 线程局部随机数生成器
thread_local std::mt19937 g_rng(std::random_device{}());

float RandomRange(float min_val, float max_val) {
    std::uniform_real_distribution<float> dist(min_val, max_val);
    return dist(g_rng);
}

glm::vec3 RandomVec3Range(const glm::vec3& min_val, const glm::vec3& max_val) {
    return glm::vec3(
        RandomRange(min_val.x, max_val.x),
        RandomRange(min_val.y, max_val.y),
        RandomRange(min_val.z, max_val.z)
    );
}

} // anonymous namespace

void ParticleSystem::Update(World& world, float delta_time) {
    auto view = world.registry().view<ParticleEmitterComponent, TransformComponent>();
    
    for (auto entity : view) {
        auto& emitter = view.get<ParticleEmitterComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        // ================================================================
        // Update existing particles
        // ================================================================
        for (auto it = emitter.particles.begin(); it != emitter.particles.end(); ) {
            it->life_remaining -= delta_time;
            if (it->life_remaining <= 0.0f) {
                it = emitter.particles.erase(it);
                continue;
            }

            // Normalized life progress: 0.0 = just born, 1.0 = about to die
            const float life_t = 1.0f - (it->life_remaining / std::max(it->life_time, 0.001f));

            // Apply gravity
            it->velocity += emitter.gravity * delta_time;

            // Apply speed curve
            if (emitter.use_speed_curve) {
                const float speed_scale = glm::mix(1.0f, emitter.speed_curve_end_scale, life_t);
                it->position += it->velocity * delta_time * speed_scale;
            } else {
                it->position += it->velocity * delta_time;
            }

            // Apply rotation
            it->rotation += it->angular_velocity * delta_time;

            // Apply size curve
            if (emitter.use_size_curve) {
                it->size = glm::mix(emitter.start_size, emitter.size_curve_end, life_t);
            }

            // Apply alpha curve
            if (emitter.use_alpha_curve) {
                it->color.a = glm::mix(emitter.start_color.a, emitter.alpha_curve_end, life_t);
            }

            // Apply color curve
            if (emitter.use_color_curve) {
                it->color = glm::mix(emitter.start_color, emitter.color_curve_end, life_t);
            }

            // Simple ground collision
            if (emitter.use_ground_collision && it->position.y <= emitter.ground_y) {
                it->position.y = emitter.ground_y;
                it->velocity.y = -it->velocity.y * emitter.collision_bounce;
                it->velocity.x *= (1.0f - emitter.collision_friction);
                it->velocity.z *= (1.0f - emitter.collision_friction);
                it->life_remaining -= emitter.collision_life_loss;
            }

            ++it;
        }
        
        // ================================================================
        // Emit new particles
        // ================================================================
        auto spawn_particle = [&]() -> Particle2D {
            Particle2D p;
            p.position = transform.position;

            if (emitter.use_random_params) {
                p.velocity = RandomVec3Range(emitter.velocity_min, emitter.velocity_max);
                p.life_time = RandomRange(emitter.life_time_min, emitter.life_time_max);
                p.life_remaining = p.life_time;
                p.size = RandomRange(emitter.size_min, emitter.size_max);
                p.rotation = RandomRange(emitter.rotation_min, emitter.rotation_max);
                p.angular_velocity = RandomRange(emitter.angular_velocity_min, emitter.angular_velocity_max);
            } else {
                p.velocity = glm::vec3(0.0f, 1.0f, 0.0f);
                p.life_time = emitter.start_life_time;
                p.life_remaining = emitter.start_life_time;
                p.size = emitter.start_size;
                p.rotation = 0.0f;
                p.angular_velocity = 0.0f;
            }

            p.color = emitter.start_color;
            return p;
        };

        if (emitter.emitting) {
            emitter.emit_accumulator += delta_time;
            float actual_emit_rate = emitter.emit_rate * emitter.emit_rate_scale;
            if (actual_emit_rate < 0.01f) {
                actual_emit_rate = 0.01f;
            }
            float emit_interval = 1.0f / actual_emit_rate;
            
            while (emitter.emit_accumulator >= emit_interval && 
                   static_cast<int>(emitter.particles.size()) < emitter.max_particles) {
                emitter.emit_accumulator -= emit_interval;
                emitter.particles.push_back(spawn_particle());
            }
        }

        // Burst emission
        while (emitter.pending_burst > 0 && 
               static_cast<int>(emitter.particles.size()) < emitter.max_particles) {
            emitter.particles.push_back(spawn_particle());
            emitter.pending_burst -= 1;
        }
        if (emitter.pending_burst < 0) {
            emitter.pending_burst = 0;
        }
    }
}

void ParticleSystem::Render(World& world, CommandBuffer& cmd_buffer) {
    auto view = world.registry().view<ParticleEmitterComponent>();
    std::vector<SpriteDrawItem> items;
    
    for (auto entity : view) {
        auto& emitter = view.get<ParticleEmitterComponent>(entity);
        
        for (const auto& p : emitter.particles) {
            SpriteDrawItem item;
            item.texture_handle = emitter.texture_handle;
            
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, p.position);
            model = glm::rotate(model, p.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(p.size, p.size, 1.0f));
            
            item.model = model;
            item.color = p.color;
            // Default alpha fade if no curve is active
            if (!emitter.use_alpha_curve && !emitter.use_color_curve) {
                item.color.a *= (p.life_remaining / std::max(p.life_time, 0.001f));
            }
            item.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
            item.sorting_layer = 10;
            item.order_in_layer = 0;
            
            items.push_back(item);
        }
    }
    
    if (!items.empty()) {
        cmd_buffer.DrawSpriteBatch(items);
    }
}
