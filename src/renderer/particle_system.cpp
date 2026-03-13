#include "particle_system.h"
#include "batch_renderer_2d.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/compatibility.hpp>

ParticleSystem::ParticleSystem() {
    particle_pool_.resize(1000);
}

ParticleSystem::~ParticleSystem() {
}

void ParticleSystem::Update() {
    for (auto& particle : particle_pool_) {
        if (!particle.Active)
            continue;

        if (particle.LifeRemaining <= 0.0f) {
            particle.Active = false;
            continue;
        }

        particle.LifeRemaining -= 0.016f; // Time::DeltaTime()
        particle.Position += particle.Velocity * 0.016f; // Time::DeltaTime()
        particle.Rotation += 0.01f * 0.016f;
    }
}

void ParticleSystem::OnRender() {
    // Render particles using BatchRenderer2D
    // Ideally, BatchRenderer2D should handle Begin/End scene, or we assume it's already begun.
    // If ParticleSystem is just a component rendered in the scene loop, we assume the renderer is active.
    
    for (auto& particle : particle_pool_) {
        if (!particle.Active)
            continue;

        float life = particle.LifeRemaining / particle.LifeTime;
        glm::vec4 color = glm::lerp(particle.ColorEnd, particle.ColorBegin, life);
        
        float size = glm::lerp(particle.SizeEnd, particle.SizeBegin, life);
        
        // Calculate transform for rotation
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), { particle.Position.x, particle.Position.y, 0.0f })
            * glm::rotate(glm::mat4(1.0f), particle.Rotation, { 0.0f, 0.0f, 1.0f })
            * glm::scale(glm::mat4(1.0f), { size, size, 1.0f });
            
        BatchRenderer2D::DrawQuad(transform, color);
    }
}

void ParticleSystem::Emit(const ParticleProps& particleProps) {
    Particle& particle = particle_pool_[pool_index_];
    particle.Active = true;
    particle.Position = particleProps.Position;
    particle.Rotation = 0.0f; // Randomize?
    
    // Velocity
    particle.Velocity = particleProps.Velocity;
    // ... apply variations ...

    particle.ColorBegin = particleProps.ColorBegin;
    particle.ColorEnd = particleProps.ColorEnd;
    particle.LifeTime = particleProps.LifeTime;
    particle.LifeRemaining = particleProps.LifeTime;
    particle.SizeBegin = particleProps.SizeBegin;
    particle.SizeEnd = particleProps.SizeEnd;

    pool_index_ = --pool_index_ % particle_pool_.size();
}
