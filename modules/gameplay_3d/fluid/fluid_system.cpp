#include "modules/gameplay_3d/fluid/fluid_system.h"
#include "engine/ecs/components_3d_fluid.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr uint32_t MAX_PARTICLES = 50000;

/// Simple pseudo-random float [0, 1) based on seed
float HashRand(uint32_t seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed ^= seed >> 4u;
    seed *= 0x27d4eb2du;
    seed ^= seed >> 15u;
    return static_cast<float>(seed) / static_cast<float>(0xFFFFFFFF);
}

} // namespace

namespace dse {
namespace gameplay3d {

// ─── SPH Kernels ───────────────────────────────────────────────────────

float FluidSystem::KernelPoly6(float r2, float h) {
    // W_poly6(r, h) = 315 / (64 * pi * h^9) * (h^2 - r^2)^3
    float h2 = h * h;
    if (r2 >= h2) return 0.0f;
    float diff = h2 - r2;
    float h9 = h2 * h2 * h2 * h2 * h;
    return (315.0f / (64.0f * PI * h9)) * diff * diff * diff;
}

float FluidSystem::KernelSpikyGrad(float r, float h) {
    // Gradient magnitude of W_spiky
    // dW/dr = -45 / (pi * h^6) * (h - r)^2
    if (r >= h || r < 1e-8f) return 0.0f;
    float diff = h - r;
    float h6 = h * h * h * h * h * h;
    return -45.0f / (PI * h6) * diff * diff;
}

float FluidSystem::KernelViscosityLaplacian(float r, float h) {
    // Laplacian of W_viscosity = 45 / (pi * h^6) * (h - r)
    if (r >= h) return 0.0f;
    float h6 = h * h * h * h * h * h;
    return 45.0f / (PI * h6) * (h - r);
}

// ─── Spatial Hash ──────────────────────────────────────────────────────

void FluidSystem::SpatialHash::Clear() {
    cells.clear();
}

void FluidSystem::SpatialHash::Insert(uint32_t index, const glm::vec3& pos) {
    int cx = static_cast<int>(std::floor(pos.x / cell_size));
    int cy = static_cast<int>(std::floor(pos.y / cell_size));
    int cz = static_cast<int>(std::floor(pos.z / cell_size));
    cells[Hash(cx, cy, cz)].push_back(index);
}

uint64_t FluidSystem::SpatialHash::Hash(int x, int y, int z) const {
    // Simple hash combining three ints
    uint64_t h = 0;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(x) * 73856093u);
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(y) * 19349663u) << 16;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(z) * 83492791u) << 32;
    return h;
}

void FluidSystem::SpatialHash::GetNeighborCells(const glm::vec3& pos, float radius,
                                                  std::vector<uint32_t>& out_indices) const {
    int min_cx = static_cast<int>(std::floor((pos.x - radius) / cell_size));
    int max_cx = static_cast<int>(std::floor((pos.x + radius) / cell_size));
    int min_cy = static_cast<int>(std::floor((pos.y - radius) / cell_size));
    int max_cy = static_cast<int>(std::floor((pos.y + radius) / cell_size));
    int min_cz = static_cast<int>(std::floor((pos.z - radius) / cell_size));
    int max_cz = static_cast<int>(std::floor((pos.z + radius) / cell_size));

    for (int cx = min_cx; cx <= max_cx; ++cx) {
        for (int cy = min_cy; cy <= max_cy; ++cy) {
            for (int cz = min_cz; cz <= max_cz; ++cz) {
                auto it = cells.find(Hash(cx, cy, cz));
                if (it != cells.end()) {
                    out_indices.insert(out_indices.end(), it->second.begin(), it->second.end());
                }
            }
        }
    }
}

// ─── FluidSystem ───────────────────────────────────────────────────────

void FluidSystem::Init(World& world, RhiDevice* rhi) {
    (void)world;
    rhi_ = rhi;
}

void FluidSystem::Shutdown(World& world) {
    if (!rhi_) return;
    auto view = world.registry().view<FluidEmitterComponent>();
    for (auto entity : view) {
        auto& fluid = view.get<FluidEmitterComponent>(entity);
        if (fluid.instance_vbo != 0) {
            rhi_->DeleteGpuBuffer(dse::render::BufferHandle{fluid.instance_vbo});
            fluid.instance_vbo = 0;
        }
    }
}

void FluidSystem::Update(World& world, float delta_time) {
    if (delta_time <= 0.0f) return;

    auto view = world.registry().view<FluidEmitterComponent, TransformComponent>();
    for (auto entity : view) {
        auto& fluid = view.get<FluidEmitterComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (!fluid.enabled) continue;

        // 1. Emit new particles
        EmitParticles(fluid, transform.position, transform.rotation, delta_time);

        // 2. SPH simulation
        SimulateSPH(fluid, delta_time);

        // 3. Compact dead particles
        CompactParticles(fluid);

        fluid.gpu_dirty = true;

        // Upload particle data to GPU for rendering
        UploadGpuData(fluid);
    }
}

void FluidSystem::UploadGpuData(FluidEmitterComponent& fluid) {
    if (!rhi_ || !fluid.gpu_dirty) return;
    fluid.gpu_dirty = false;

    const uint32_t count = fluid.active_count;
    if (count == 0) return;

    // 首次使用创建每实例 SSBO（std430：{ vec4 pos_size; vec4 color } = 8 floats/粒子），
    // 供 ParticleRenderer 经通用原语 BindStorageBuffer 绑定。
    const size_t max_particles = 16384;
    if (fluid.instance_vbo == 0) {
        dse::render::GpuBufferDesc inst_desc;
        inst_desc.size = max_particles * 8 * sizeof(float);
        inst_desc.usage = dse::render::GpuBufferUsage::kStorage;
        inst_desc.is_dynamic = true;
        fluid.instance_vbo = rhi_->CreateGpuBuffer(inst_desc, nullptr).raw();
    }

    // std430 布局：pos_size = (pos.xyz, size)，color = (r,g,b,a)。
    std::vector<float> gpu_data;
    gpu_data.reserve(static_cast<size_t>(count) * 8);

    for (const auto& p : fluid.particles) {
        if (p.life <= 0.0f) continue;
        // Fade alpha based on remaining life / total lifetime
        float alpha = fluid.color.a * glm::clamp(p.life / fluid.particle_lifetime, 0.0f, 1.0f);
        gpu_data.push_back(p.position.x);
        gpu_data.push_back(p.position.y);
        gpu_data.push_back(p.position.z);
        gpu_data.push_back(fluid.particle_radius * 20.0f); // Visual size scale
        gpu_data.push_back(fluid.color.r);
        gpu_data.push_back(fluid.color.g);
        gpu_data.push_back(fluid.color.b);
        gpu_data.push_back(alpha);
    }

    if (!gpu_data.empty()) {
        rhi_->UpdateGpuBuffer(dse::render::BufferHandle{fluid.instance_vbo}, 0,
                              gpu_data.size() * sizeof(float), gpu_data.data());
    }
}

void FluidSystem::EmitParticles(FluidEmitterComponent& fluid, const glm::vec3& emitter_pos,
                                 const glm::quat& emitter_rot, float dt) {
    fluid.emit_accumulator += fluid.emission_rate * dt;
    uint32_t to_emit = static_cast<uint32_t>(fluid.emit_accumulator);
    fluid.emit_accumulator -= static_cast<float>(to_emit);

    uint32_t current_count = static_cast<uint32_t>(fluid.particles.size());
    uint32_t capacity = std::min(MAX_PARTICLES, current_count + to_emit);

    static uint32_t emit_seed = 0;

    for (uint32_t i = 0; i < to_emit && fluid.particles.size() < capacity; ++i) {
        FluidParticle p;

        // Position based on emitter shape
        glm::vec3 local_offset(0.0f);
        switch (fluid.shape) {
            case FluidEmitterShape::Point:
                break;
            case FluidEmitterShape::Sphere: {
                // Random point in sphere
                float u = HashRand(emit_seed++) * 2.0f - 1.0f;
                float theta = HashRand(emit_seed++) * 2.0f * PI;
                float r = std::cbrt(HashRand(emit_seed++)) * fluid.sphere_radius;
                float s = std::sqrt(1.0f - u * u);
                local_offset = glm::vec3(s * std::cos(theta), u, s * std::sin(theta)) * r;
                break;
            }
            case FluidEmitterShape::Box: {
                local_offset.x = (HashRand(emit_seed++) * 2.0f - 1.0f) * fluid.box_half_extents.x;
                local_offset.y = (HashRand(emit_seed++) * 2.0f - 1.0f) * fluid.box_half_extents.y;
                local_offset.z = (HashRand(emit_seed++) * 2.0f - 1.0f) * fluid.box_half_extents.z;
                break;
            }
        }

        p.position = emitter_pos + emitter_rot * local_offset;

        // Velocity: emit_direction with spread
        glm::vec3 dir = emitter_rot * fluid.emit_direction;
        if (fluid.emit_spread > 0.0f) {
            float theta = HashRand(emit_seed++) * 2.0f * PI;
            float phi = HashRand(emit_seed++) * fluid.emit_spread;
            float sp = std::sin(phi);
            glm::vec3 perturb(sp * std::cos(theta), sp * std::sin(theta), std::cos(phi));

            // Construct basis from dir
            glm::vec3 up = std::abs(dir.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            glm::vec3 right = glm::normalize(glm::cross(up, dir));
            glm::vec3 actual_up = glm::cross(dir, right);
            dir = glm::normalize(right * perturb.x + actual_up * perturb.y + dir * perturb.z);
        }
        p.velocity = dir * fluid.emit_speed;
        p.life = fluid.particle_lifetime;
        p.density = fluid.rest_density;
        p.pressure = 0.0f;

        fluid.particles.push_back(p);
    }
}

void FluidSystem::SimulateSPH(FluidEmitterComponent& fluid, float dt) {
    const uint32_t count = static_cast<uint32_t>(fluid.particles.size());
    if (count == 0) return;

    const float h = fluid.particle_radius * 4.0f; // Smoothing radius
    const float h2 = h * h;

    // Build spatial hash
    spatial_hash_.cell_size = h;
    spatial_hash_.Clear();
    for (uint32_t i = 0; i < count; ++i) {
        if (fluid.particles[i].life > 0.0f) {
            spatial_hash_.Insert(i, fluid.particles[i].position);
        }
    }

    // Density estimation
    for (uint32_t i = 0; i < count; ++i) {
        auto& pi = fluid.particles[i];
        if (pi.life <= 0.0f) continue;

        std::vector<uint32_t> neighbors;
        spatial_hash_.GetNeighborCells(pi.position, h, neighbors);

        float density = 0.0f;
        for (uint32_t j : neighbors) {
            auto& pj = fluid.particles[j];
            if (pj.life <= 0.0f) continue;
            glm::vec3 diff = pi.position - pj.position;
            float r2 = glm::dot(diff, diff);
            density += KernelPoly6(r2, h);
        }
        pi.density = density;
        if (pi.density < 1e-6f) pi.density = fluid.rest_density;

        // Equation of state (Tait equation variant)
        pi.pressure = fluid.gas_stiffness * (pi.density / fluid.rest_density - 1.0f);
        if (pi.pressure < 0.0f) pi.pressure = 0.0f;
    }

    // Force accumulation: pressure + viscosity
    std::vector<glm::vec3> forces(count, glm::vec3(0.0f));

    for (uint32_t i = 0; i < count; ++i) {
        auto& pi = fluid.particles[i];
        if (pi.life <= 0.0f) continue;

        std::vector<uint32_t> neighbors;
        spatial_hash_.GetNeighborCells(pi.position, h, neighbors);

        glm::vec3 f_pressure(0.0f);
        glm::vec3 f_viscosity(0.0f);

        for (uint32_t j : neighbors) {
            if (j == i) continue;
            auto& pj = fluid.particles[j];
            if (pj.life <= 0.0f) continue;

            glm::vec3 diff = pi.position - pj.position;
            float r = glm::length(diff);
            if (r < 1e-6f || r >= h) continue;
            glm::vec3 dir = diff / r;

            // Pressure force (symmetric)
            float avg_pressure = (pi.pressure + pj.pressure) * 0.5f;
            float spiky = KernelSpikyGrad(r, h);
            f_pressure += dir * (avg_pressure / (pj.density + 1e-6f)) * spiky;

            // Viscosity force
            float visc_lap = KernelViscosityLaplacian(r, h);
            f_viscosity += (pj.velocity - pi.velocity) * (fluid.viscosity / (pj.density + 1e-6f)) * visc_lap;
        }

        forces[i] = -f_pressure + f_viscosity;
    }

    // Integration
    for (uint32_t i = 0; i < count; ++i) {
        auto& pi = fluid.particles[i];
        if (pi.life <= 0.0f) continue;

        // External forces
        glm::vec3 acceleration = fluid.gravity + forces[i] / (pi.density + 1e-6f);

        // Damping
        pi.velocity += acceleration * dt;
        pi.velocity *= (1.0f - fluid.damping);

        // Position update
        pi.position += pi.velocity * dt;

        // Ground collision
        if (pi.position.y < fluid.floor_y + fluid.particle_radius) {
            pi.position.y = fluid.floor_y + fluid.particle_radius;
            pi.velocity.y = -pi.velocity.y * fluid.collision_restitution;
            // Friction on ground
            pi.velocity.x *= 0.95f;
            pi.velocity.z *= 0.95f;
        }

        // Lifetime
        pi.life -= dt;
    }

    fluid.active_count = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (fluid.particles[i].life > 0.0f) {
            fluid.active_count++;
        }
    }
}

void FluidSystem::CompactParticles(FluidEmitterComponent& fluid) {
    // Remove dead particles (swap-and-pop for efficiency)
    auto& p = fluid.particles;
    size_t write = 0;
    for (size_t read = 0; read < p.size(); ++read) {
        if (p[read].life > 0.0f) {
            if (write != read) {
                p[write] = p[read];
            }
            ++write;
        }
    }
    p.resize(write);
    fluid.active_count = static_cast<uint32_t>(write);
}

} // namespace gameplay3d
} // namespace dse
