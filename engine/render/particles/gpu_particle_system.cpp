/**
 * @file gpu_particle_system.cpp
 * @brief GPU Compute Shader 驱动粒子系统实现
 */

#include "engine/render/particles/gpu_particle_system.h"
#include "engine/render/rhi/rhi_device.h"

namespace dse {
namespace render {

namespace {

// ─── Compute Shader 源码（GLSL 430） ─────────────────────────────────────

// 粒子 SSBO 布局（每粒子 48 bytes = 12 floats）:
//   vec3 position (12B) + float life (4B)
//   vec3 velocity (12B) + float max_life (4B)
//   vec4 color (16B) -- not stored, computed from life ratio in render
//   float size (4B) + 3 padding (12B) -- total 64B per particle for alignment

// 简化：每粒子 32 bytes (8 floats):
//   float4 pos_life   (xyz=position, w=remaining_life)
//   float4 vel_maxlife (xyz=velocity, w=max_life)
// 渲染时 color/size 由 life_ratio 插值计算

const char* kParticleUpdateCS = R"(
#version 430 core
layout(local_size_x = 256) in;

struct Particle {
    vec4 pos_life;      // xyz=position, w=remaining_life (<0 = dead)
    vec4 vel_maxlife;   // xyz=velocity, w=max_life
};

layout(std430, binding = 0) buffer ParticlesIn  { Particle particles_in[]; };
layout(std430, binding = 1) buffer ParticlesOut { Particle particles_out[]; };
layout(std430, binding = 2) buffer Counters {
    uint alive_count;     // 输出：存活粒子数
    uint dead_count;      // 输出：死亡粒子数
    uint dead_indices[1]; // 死亡粒子索引列表（动态大小，跟在 dead_count 后）
};

layout(std140, binding = 3) uniform SimParams {
    vec4 u_gravity_dt;       // xyz=gravity, w=delta_time
    vec4 u_wind_turbulence;  // xyz=wind, w=turbulence_strength
    vec4 u_collision;        // x=enabled, y=plane_y, z=bounce, w=friction
    vec4 u_vortex;           // x=strength, yzw=axis(0,1,0)
    uint u_max_particles;
    uint u_pad0;
    uint u_pad1;
    uint u_pad2;
};

// 简单伪随机
float hash(float n) { return fract(sin(n) * 43758.5453123); }

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_max_particles) return;

    Particle p = particles_in[idx];
    float dt = u_gravity_dt.w;

    if (p.pos_life.w > 0.0) {
        // 存活粒子：积分
        vec3 pos = p.pos_life.xyz;
        vec3 vel = p.vel_maxlife.xyz;
        float life = p.pos_life.w;

        // 力场
        vel += u_gravity_dt.xyz * dt;
        vel += u_wind_turbulence.xyz * dt;

        // 涡旋力
        if (u_vortex.x > 0.0) {
            vec3 to_axis = vec3(-pos.z, 0.0, pos.x); // 绕 Y 轴
            vel += normalize(to_axis + vec3(0.001)) * u_vortex.x * dt;
        }

        // 湍流（简易噪声）
        if (u_wind_turbulence.w > 0.0) {
            float n = hash(float(idx) + life * 17.3);
            vel += vec3(n - 0.5, hash(n * 7.1) - 0.5, hash(n * 13.7) - 0.5)
                   * u_wind_turbulence.w * dt;
        }

        pos += vel * dt;
        life -= dt;

        // 碰撞检测（Y 平面）
        if (u_collision.x > 0.5 && pos.y < u_collision.y) {
            pos.y = u_collision.y;
            vel.y = -vel.y * u_collision.z; // bounce
            vel.xz *= u_collision.w;        // friction
        }

        p.pos_life = vec4(pos, life);
        p.vel_maxlife.xyz = vel;

        if (life > 0.0) {
            atomicAdd(alive_count, 1u);
        } else {
            uint di = atomicAdd(dead_count, 1u);
            dead_indices[di] = idx;
        }
    } else {
        // 已死亡粒子：写入死亡列表
        uint di = atomicAdd(dead_count, 1u);
        dead_indices[di] = idx;
    }

    particles_out[idx] = p;
}
)";

const char* kParticleEmitCS = R"(
#version 430 core
layout(local_size_x = 64) in;

struct Particle {
    vec4 pos_life;
    vec4 vel_maxlife;
};

layout(std430, binding = 1) buffer ParticlesOut { Particle particles_out[]; };
layout(std430, binding = 2) buffer Counters {
    uint alive_count;
    uint dead_count;
    uint dead_indices[1];
};

layout(std140, binding = 4) uniform EmitParams {
    vec4 u_emitter_pos;       // xyz=world pos, w=shape_radius
    vec4 u_life_speed;        // x=life_min, y=life_max, z=speed_min, w=speed_max
    vec4 u_emit_dir;          // xyz=cone direction, w=cone_angle_cos
    uint u_emit_count;        // 本帧发射数
    uint u_shape;             // EmitterShape enum
    uint u_seed;              // 随机种子
    uint u_pad;
};

float hash(uint n) {
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / float(0x7fffffff);
}

vec3 randomDirection(uint seed) {
    float u = hash(seed);
    float v = hash(seed + 1u);
    float theta = u * 6.2831853;
    float phi = acos(2.0 * v - 1.0);
    return vec3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_emit_count) return;

    // 从死亡列表取一个空位
    uint dead_idx = atomicAdd(dead_count, 0xFFFFFFFF); // decrement
    if (dead_idx == 0u || dead_idx > 0x7FFFFFFFu) return; // 无空位
    dead_idx -= 1u;
    uint slot = dead_indices[dead_idx];

    uint s = u_seed + idx * 7u;
    float life = mix(u_life_speed.x, u_life_speed.y, hash(s));
    float speed = mix(u_life_speed.z, u_life_speed.w, hash(s + 3u));

    vec3 pos = u_emitter_pos.xyz;
    vec3 dir = randomDirection(s + 5u);

    // 形状
    if (u_shape == 1u) { // Sphere
        pos += dir * u_emitter_pos.w * hash(s + 10u);
    } else if (u_shape == 2u) { // Cone
        float cos_angle = u_emit_dir.w;
        dir = mix(u_emit_dir.xyz, dir, 1.0 - cos_angle);
        dir = normalize(dir);
    } else if (u_shape == 3u) { // Ring
        float angle = hash(s + 20u) * 6.2831853;
        pos += vec3(cos(angle), 0.0, sin(angle)) * u_emitter_pos.w;
    }

    Particle p;
    p.pos_life = vec4(pos, life);
    p.vel_maxlife = vec4(dir * speed, life);
    particles_out[slot] = p;

    atomicAdd(alive_count, 1u);
}
)";

} // anonymous namespace

// ─── GpuParticleManager 实现 ─────────────────────────────────────────────

bool GpuParticleManager::Init(RhiDevice* rhi) {
    if (!rhi || inited_) return inited_;
    if (!rhi->SupportsCompute()) return false;

    update_shader_ = rhi->CreateComputeShader(std::string(kParticleUpdateCS));
    emit_shader_ = rhi->CreateComputeShader(std::string(kParticleEmitCS));

    inited_ = (update_shader_ != 0 && emit_shader_ != 0);
    return inited_;
}

void GpuParticleManager::InitComponent(GpuParticleComponent& comp, RhiDevice* rhi) {
    if (!rhi || comp.initialized) return;

    uint32_t max_p = comp.config.max_particles;
    size_t particle_size = 8 * sizeof(float); // pos_life(4f) + vel_maxlife(4f)
    size_t buffer_size = max_p * particle_size;

    // 双缓冲粒子 SSBO
    GpuBufferDesc desc;
    desc.size = buffer_size;
    desc.usage = GpuBufferUsage::kStorage;
    desc.is_dynamic = false;

    // 初始化全部粒子为死亡（life < 0）
    std::vector<float> init_data(max_p * 8, 0.0f);
    for (uint32_t i = 0; i < max_p; ++i) {
        init_data[i * 8 + 3] = -1.0f;  // pos_life.w = -1 (dead)
    }

    comp.particle_buffer_a = rhi->CreateGpuBuffer(desc, init_data.data()).raw();
    comp.particle_buffer_b = rhi->CreateGpuBuffer(desc, init_data.data()).raw();

    // Counter buffer: alive_count(4) + dead_count(4) + dead_indices(max_p * 4)
    GpuBufferDesc counter_desc;
    counter_desc.size = 8 + max_p * sizeof(uint32_t);
    counter_desc.usage = GpuBufferUsage::kStorage;
    counter_desc.is_dynamic = false;

    // 初始化：alive=0, dead=max_p, dead_indices=[0,1,2,...,max_p-1]
    std::vector<uint32_t> counter_init(2 + max_p);
    counter_init[0] = 0;       // alive_count
    counter_init[1] = max_p;   // dead_count
    for (uint32_t i = 0; i < max_p; ++i) {
        counter_init[2 + i] = i;
    }
    comp.counter_buffer = rhi->CreateGpuBuffer(counter_desc, counter_init.data()).raw();

    // Indirect draw buffer (DrawArraysIndirectCommand: count, instance_count, first, base_instance)
    GpuBufferDesc indirect_desc;
    indirect_desc.size = 4 * sizeof(uint32_t);
    indirect_desc.usage = GpuBufferUsage::kIndirect;
    indirect_desc.is_dynamic = true;
    uint32_t indirect_init[4] = {4, 0, 0, 0}; // 4 verts per quad, 0 instances initially
    comp.indirect_buffer = rhi->CreateGpuBuffer(indirect_desc, indirect_init).raw();

    comp.initialized = true;
    comp.ping = true;
    comp.emit_accumulator = 0.0f;
}

void GpuParticleManager::Update(GpuParticleComponent& comp, RhiDevice* rhi,
                                 const glm::vec3& emitter_pos, float delta_time) {
    if (!rhi || !inited_ || !comp.initialized || !comp.config.enabled) return;

    auto& cfg = comp.config;
    uint32_t max_p = cfg.max_particles;

    // 重置 counters
    uint32_t zero[2] = {0, 0}; // alive=0, dead=0
    rhi->UpdateGpuBuffer(BufferHandle{comp.counter_buffer}, 0, 8, zero);

    // 绑定 SSBO
    unsigned int read_buf = comp.ping ? comp.particle_buffer_a : comp.particle_buffer_b;
    unsigned int write_buf = comp.ping ? comp.particle_buffer_b : comp.particle_buffer_a;

    rhi->BindGpuBuffer(BufferHandle{read_buf}, 0);
    rhi->BindGpuBuffer(BufferHandle{write_buf}, 1, true);
    rhi->BindGpuBuffer(BufferHandle{comp.counter_buffer}, 2, true);

    // Update pass: simulate existing particles
    rhi->SetComputeUniformVec4(update_shader_, "u_gravity_dt",
        cfg.gravity.x, cfg.gravity.y, cfg.gravity.z, delta_time);
    rhi->SetComputeUniformVec4(update_shader_, "u_wind_turbulence",
        cfg.wind.x, cfg.wind.y, cfg.wind.z, cfg.turbulence);
    rhi->SetComputeUniformVec4(update_shader_, "u_collision",
        cfg.collision_enabled ? 1.0f : 0.0f, cfg.collision_plane_y,
        cfg.collision_bounce, cfg.collision_friction);
    rhi->SetComputeUniformVec4(update_shader_, "u_vortex",
        cfg.vortex_strength, 0.0f, 1.0f, 0.0f);
    rhi->SetComputeUniformInt(update_shader_, "u_max_particles", static_cast<int>(max_p));

    uint32_t groups = (max_p + 255) / 256;
    rhi->DispatchCompute(update_shader_, groups, 1, 1);
    rhi->ComputeMemoryBarrier();

    // Emit pass: spawn new particles
    comp.emit_accumulator += delta_time * cfg.emission_rate;
    uint32_t emit_count = static_cast<uint32_t>(comp.emit_accumulator);
    if (emit_count > max_p) emit_count = max_p;
    comp.emit_accumulator -= static_cast<float>(emit_count);

    if (emit_count > 0) {
        rhi->BindGpuBuffer(BufferHandle{write_buf}, 1, true);
        rhi->BindGpuBuffer(BufferHandle{comp.counter_buffer}, 2, true);

        rhi->SetComputeUniformVec4(emit_shader_, "u_emitter_pos",
            emitter_pos.x, emitter_pos.y, emitter_pos.z, cfg.shape_radius);
        rhi->SetComputeUniformVec4(emit_shader_, "u_life_speed",
            cfg.life_min, cfg.life_max, cfg.speed_min, cfg.speed_max);

        float cone_cos = std::cos(cfg.cone_angle * 3.14159f / 180.0f);
        rhi->SetComputeUniformVec4(emit_shader_, "u_emit_dir",
            0.0f, 1.0f, 0.0f, cone_cos);
        rhi->SetComputeUniformInt(emit_shader_, "u_emit_count", static_cast<int>(emit_count));
        rhi->SetComputeUniformInt(emit_shader_, "u_shape", static_cast<int>(cfg.shape));

        // 简易随机种子
        static uint32_t frame_seed = 0;
        rhi->SetComputeUniformInt(emit_shader_, "u_seed", static_cast<int>(++frame_seed * 1337));

        uint32_t emit_groups = (emit_count + 63) / 64;
        rhi->DispatchCompute(emit_shader_, emit_groups, 1, 1);
        rhi->ComputeMemoryBarrier();
    }

    // 翻转 ping-pong
    comp.ping = !comp.ping;
}

void GpuParticleManager::Shutdown(RhiDevice* rhi) {
    if (!rhi) return;
    if (update_shader_) { rhi->DeleteComputeShader(update_shader_); update_shader_ = 0; }
    if (emit_shader_) { rhi->DeleteComputeShader(emit_shader_); emit_shader_ = 0; }
    inited_ = false;
}

void GpuParticleManager::ShutdownComponent(GpuParticleComponent& comp, RhiDevice* rhi) {
    if (!rhi) return;
    if (comp.particle_buffer_a) { rhi->DeleteGpuBuffer(BufferHandle{comp.particle_buffer_a}); comp.particle_buffer_a = 0; }
    if (comp.particle_buffer_b) { rhi->DeleteGpuBuffer(BufferHandle{comp.particle_buffer_b}); comp.particle_buffer_b = 0; }
    if (comp.counter_buffer) { rhi->DeleteGpuBuffer(BufferHandle{comp.counter_buffer}); comp.counter_buffer = 0; }
    if (comp.indirect_buffer) { rhi->DeleteGpuBuffer(BufferHandle{comp.indirect_buffer}); comp.indirect_buffer = 0; }
    comp.initialized = false;
}

} // namespace render
} // namespace dse
