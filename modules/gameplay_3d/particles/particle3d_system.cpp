#include "modules/gameplay_3d/particles/particle3d_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/time_scale_component.h"
#include "engine/assets/asset_manager.h"
#include <glm/gtc/constants.hpp>
#include <random>
#include <stdexcept>
#include <cmath>

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
    float theta = u * glm::two_pi<float>();
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
    // per-in-flight ring 拥有全部槽位缓冲，统一释放（组件 instance_vbo 只是当前槽位视图）。
    for (auto& [id, ring] : instance_rings_) {
        ring.Shutdown(*rhi_);
    }
    instance_rings_.clear();
    auto view = world.registry().view<ParticleSystem3DComponent>();
    for (auto entity : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(entity);
        ps.instance_vbo = {};
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
    if (ps.spawn_radius > 0.0f) {
        float angle = RandomFloat(0.0f, glm::two_pi<float>());
        float r = ps.spawn_radius * std::sqrt(RandomFloat(0.0f, 1.0f));
        p.position.x += r * std::cos(angle);
        p.position.z += r * std::sin(angle);
    }
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

    // 实例 SSBO 上传发生在主线程 Update 阶段（BeginFrame/AcquireNextImage 之前，当前帧槽位
    // fence 尚未等待）。改 per-in-flight ring 后须在覆写当前槽位前显式等一次「本槽位」fence
    // （仅此槽位、不等其它在飞帧，保留 2 帧重叠），否则 host 写与 N 帧前仍在读该槽位的 GPU
    // 竞争 → 设备丢失。与 mesh_render_system::PrepareGPUScene 同理，每帧仅需等一次；此处惰性
    // 触发——仅当本帧确有实例上传时才等，避免无粒子帧的无谓停顿。
    bool slot_fence_waited = false;

    auto particle_view = world.registry().view<ParticleSystem3DComponent>();
    auto transform_view = world.registry().view<TransformComponent>();
    for (auto entity : particle_view) {
        auto& ps = particle_view.get<ParticleSystem3DComponent>(entity);
        if (!transform_view.contains(entity)) {
            continue;
        }
        auto& transform = transform_view.get<TransformComponent>(entity);

        if (!ps.initialized) {
            ps.particles.resize(ps.max_particles);
            for (auto& p : ps.particles) p.life = -1.0f; // All dead initially
            // 每实例 SSBO 改 per-in-flight ring（见下方 Acquire），此处不再预建单份缓冲。
            ps.initialized = true;
        }

        if (!ps.enabled) {
            continue;
        }

        // 逐实体时间缩放：全局 scaled_dt × 该实体 TimeScaleComponent.scale
        const float entity_dt = dse::ResolveEntityDt(delta_time, world.registry(), entity);

        // 1. Emission
        // 钳制累加器上限为 max_particles，防止首帧超大 dt 导致 while 循环百万级迭代假死
        ps.emission_accumulator += entity_dt * ps.emission_rate;
        if (ps.emission_accumulator > static_cast<float>(ps.max_particles)) {
            ps.emission_accumulator = static_cast<float>(ps.max_particles);
        }
        int emitted = 0;
        while (ps.emission_accumulator > 1.0f && emitted < ps.max_particles) {
            EmitParticle(ps, transform);
            ps.emission_accumulator -= 1.0f;
            ++emitted;
        }

        // 2. CPU Simulation (Move to Compute Shader in next iteration if performance is an issue)
        int highest_active = 0;

        // We pack data to upload
        std::vector<float> gpu_data;
        gpu_data.reserve(ps.max_particles * 8);

        for (int i = 0; i < ps.max_particles; ++i) {
            auto& p = ps.particles[i];
            if (p.life > 0.0f) {
                p.life -= entity_dt;
                if (p.life > 0.0f) {
                    p.velocity += ps.gravity * entity_dt;
                    p.position += p.velocity * entity_dt;
                    // Optional: Fade out color over time based on start_life vs life

                    // std430 布局：pos_size = (pos.xyz, size)，color = (r,g,b,a)。
                    gpu_data.push_back(p.position.x);
                    gpu_data.push_back(p.position.y);
                    gpu_data.push_back(p.position.z);
                    gpu_data.push_back(p.size);
                    gpu_data.push_back(p.color.r);
                    gpu_data.push_back(p.color.g);
                    gpu_data.push_back(p.color.b);
                    gpu_data.push_back(p.color.a);

                    highest_active++;
                }
            }
        }

        ps.active_particle_count = highest_active;

        // 3. Upload to GPU
        if (ps.active_particle_count > 0 && !gpu_data.empty()) {
            // std430：{ vec4 pos_size; vec4 color } = 8 floats/粒子。ring 当前槽位供
            // ParticleRenderer 经通用原语 BindStorageBuffer 绑定（跨三后端 SSBO/StructuredBuffer）。
            if (!slot_fence_waited) {
                rhi_->WaitForCurrentFrameSlotGpu();
                slot_fence_waited = true;
            }
            auto& ring = instance_rings_[static_cast<std::uint32_t>(entity)];
            ps.instance_vbo = ring.Acquire(
                *rhi_, static_cast<size_t>(ps.max_particles) * 8 * sizeof(float),
                dse::render::GpuBufferUsage::kStorage);
            rhi_->UpdateGpuBuffer(ps.instance_vbo, 0,
                                  gpu_data.size() * sizeof(float), gpu_data.data());
        }

        // 4. Resolve Texture
        if (!ps.texture_handle && !ps.texture_path.empty()) {
            auto tex = asset_manager.LoadTexture(ps.texture_path);
            if (tex) ps.texture_handle = tex->GetHandle();
        }

    }
}

} // namespace gameplay3d
} // namespace dse