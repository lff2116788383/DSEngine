#include "modules/gameplay_3d/particles/particle3d_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/assets/asset_manager.h"
#include <random>
#include <stdexcept>

namespace dse {
namespace gameplay3d {

namespace {
AssetManager& RequireAssetManager(AssetManager* asset_manager) {
    if (asset_manager != nullptr) {
        return *asset_manager;
    }
    throw std::runtime_error("Particle3DSystem requires an injected AssetManager");
}
}

static float RandomFloat(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

static glm::vec3 RandomDirection() {
    float u = RandomFloat(0.0f, 1.0f);
    float v = RandomFloat(0.0f, 1.0f);
    float theta = u * 2.0f * 3.14159265358979323846f;
    float phi = acos(2.0f * v - 1.0f);
    float r = cbrt(RandomFloat(0.0f, 1.0f));
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    float sinPhi = sin(phi);
    float cosPhi = cos(phi);
    return glm::vec3(r * sinPhi * cosTheta, r * sinPhi * sinTheta, r * cosPhi);
}

void Particle3DSystem::Init(World& world, RhiDevice* rhi) {
    rhi_ = rhi;
}

void Particle3DSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

void Particle3DSystem::Shutdown(World& world) {
    if (!rhi_) return;
    auto view = world.registry().view<ParticleSystem3DComponent>();
    for (auto entity : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(entity);
        if (ps.instance_vbo != 0) {
            rhi_->DeleteBuffer(ps.instance_vbo);
            ps.instance_vbo = 0;
        }
    }
}

void Particle3DSystem::EmitParticle(ParticleSystem3DComponent& ps, const TransformComponent& transform) {
    if (ps.max_particles <= 0) {
        return;
    }

    if (static_cast<int>(ps.particles.size()) < ps.max_particles) {
        ps.particles.resize(ps.max_particles);
        for (auto& particle : ps.particles) {
            if (particle.life == 0.0f) {
                particle.life = -1.0f;
            }
        }
    }

    // Find first dead particle (life <= 0)
    int p_index = -1;
    for (int i = 0; i < ps.max_particles; ++i) {
        if (ps.particles[i].life <= 0.0f) {
            p_index = i;
            break;
        }
    }

    if (p_index == -1) return;

    auto& p = ps.particles[p_index];
    p.position = transform.position;
    p.color = ps.start_color;
    p.size = RandomFloat(ps.start_size_min, ps.start_size_max);
    p.life = RandomFloat(ps.start_life_min, ps.start_life_max);
    p.velocity = RandomDirection() * RandomFloat(ps.start_speed_min, ps.start_speed_max);
    
    if (p_index >= ps.active_particle_count) {
        ps.active_particle_count = p_index + 1;
    }
}

void Particle3DSystem::Update(World& world, float delta_time) {
    if (!rhi_) return;
    auto& asset_manager = RequireAssetManager(asset_manager_);

    auto view = world.registry().view<ParticleSystem3DComponent, TransformComponent>();
    
    for (auto entity : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (!ps.initialized) {
            ps.particles.resize(ps.max_particles);
            for (auto& p : ps.particles) p.life = -1.0f; // All dead initially
            
            // Create instance VBO (size = max_particles * 8 floats)
            ps.instance_vbo = rhi_->CreateBuffer(ps.max_particles * 8 * sizeof(float), nullptr, true, false);
            ps.initialized = true;
        }

        if (!ps.enabled) continue;

        // 1. Emission
        ps.emission_accumulator += delta_time * ps.emission_rate;
        while (ps.emission_accumulator > 1.0f) {
            EmitParticle(ps, transform);
            ps.emission_accumulator -= 1.0f;
        }

        // 2. CPU Simulation (Move to Compute Shader in next iteration if performance is an issue)
        int highest_active = 0;
        
        // We pack data to upload
        std::vector<float> gpu_data;
        gpu_data.reserve(ps.max_particles * 8);

        for (int i = 0; i < ps.max_particles; ++i) {
            auto& p = ps.particles[i];
            if (p.life > 0.0f) {
                p.life -= delta_time;
                if (p.life > 0.0f) {
                    p.velocity += ps.gravity * delta_time;
                    p.position += p.velocity * delta_time;
                    // Optional: Fade out color over time based on start_life vs life
                    
                    gpu_data.push_back(p.position.x);
                    gpu_data.push_back(p.position.y);
                    gpu_data.push_back(p.position.z);
                    gpu_data.push_back(p.color.r);
                    gpu_data.push_back(p.color.g);
                    gpu_data.push_back(p.color.b);
                    gpu_data.push_back(p.color.a);
                    gpu_data.push_back(p.size);
                    
                    highest_active++;
                }
            }
        }
        
        ps.active_particle_count = highest_active;

        // 3. Upload to GPU
        if (ps.active_particle_count > 0 && !gpu_data.empty()) {
            rhi_->UpdateBuffer(ps.instance_vbo, 0, gpu_data.size() * sizeof(float), gpu_data.data(), false);
        }

        // 4. Resolve Texture
        if (ps.texture_handle == 0 && !ps.texture_path.empty()) {
            auto tex = asset_manager.LoadTexture(ps.texture_path);
            if (tex) ps.texture_handle = tex->GetHandle();
        }
    }
}

} // namespace gameplay3d
} // namespace dse