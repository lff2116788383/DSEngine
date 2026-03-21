#include "phase1/systems/particle_system.h"
#include "phase1/ecs/components_2d.h"
#include <algorithm>

void ParticleSystem::Update(Phase1World& world, float delta_time) {
    auto view = world.registry().view<ParticleEmitterComponent, TransformComponent>();
    
    for (auto entity : view) {
        auto& emitter = view.get<ParticleEmitterComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        // Update existing particles
        for (auto it = emitter.particles.begin(); it != emitter.particles.end(); ) {
            it->life_remaining -= delta_time;
            if (it->life_remaining <= 0.0f) {
                it = emitter.particles.erase(it);
            } else {
                it->position += it->velocity * delta_time;
                ++it;
            }
        }
        
        // Emit new particles
        if (emitter.emitting) {
            emitter.emit_accumulator += delta_time;
            float emit_interval = 1.0f / emitter.emit_rate;
            
            while (emitter.emit_accumulator >= emit_interval && emitter.particles.size() < emitter.max_particles) {
                emitter.emit_accumulator -= emit_interval;
                
                Particle2D p;
                p.position = transform.position;
                p.velocity = glm::vec3(0.0f, 1.0f, 0.0f); // default up
                p.color = emitter.start_color;
                p.life_time = emitter.start_life_time;
                p.life_remaining = emitter.start_life_time;
                p.size = emitter.start_size;
                
                emitter.particles.push_back(p);
            }
        }
    }
}

void ParticleSystem::Render(Phase1World& world, CommandBuffer& cmd_buffer) {
    auto view = world.registry().view<ParticleEmitterComponent>();
    std::vector<Phase1SpriteDrawItem> items;
    
    for (auto entity : view) {
        auto& emitter = view.get<ParticleEmitterComponent>(entity);
        
        for (const auto& p : emitter.particles) {
            Phase1SpriteDrawItem item;
            item.texture_handle = emitter.texture_handle;
            
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, p.position);
            model = glm::scale(model, glm::vec3(p.size, p.size, 1.0f));
            
            item.model = model;
            item.color = p.color;
            // fading out
            item.color.a *= (p.life_remaining / p.life_time);
            item.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
            item.sorting_layer = 10; // particles usually render above sprites
            item.order_in_layer = 0;
            
            items.push_back(item);
        }
    }
    
    if (!items.empty()) {
        cmd_buffer.DrawSpriteBatch(items);
    }
}
