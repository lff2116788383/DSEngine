/**
 * @file gpu_particle_system.cpp
 * @brief GPU Compute Shader 驱动粒子系统实现
 */

#include "engine/render/particles/gpu_particle_system.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"

// 单一来源（option 2-strengthened）：两个 compute shader 的 GL430 / GLSL450 / HLSL 变体
// 均由 engine/render/shaders/src/particle_update.comp、particle_emit.comp 离线交叉编译生成，
// 分别嵌入以下头文件；三后端（OpenGL / Vulkan / D3D11）共用同一份语义。
//
// 粒子 SSBO 布局（每粒子 32 bytes = 8 floats）:
//   float4 pos_life   (xyz=position, w=remaining_life)
//   float4 vel_maxlife (xyz=velocity, w=max_life)
// 渲染时 color/size 由 life_ratio 插值计算。
#include "engine/render/shaders/generated/embed/particle_update_comp.gen.h"
#include "engine/render/shaders/generated/embed/particle_emit_comp.gen.h"

namespace dse {
namespace render {

// ─── WebGPU WGSL（手译 particle_update.comp / particle_emit.comp；无离线转译）───
// 绑定约定与 Hi-Z/gpu_cull 一致：group1 b8 = 命名 uniform 块（16B 对齐累积），
// group3 = SSBO（BindGpuBuffer slot → binding；compute 统一 read_write storage）。
// 原子计数器用 atomic<u32>（内存布局与 u32 相同，CPU 侧读写兼容）。
// 16B 对齐 push 块（SetComputeUniform* 调用序）：
//   update: u_gravity_dt@0, u_wind_turbulence@16, u_collision@32, u_vortex@48, u_max_particles@64 (80B)
//   emit:   u_emitter_pos@0, u_life_speed@16, u_emit_dir@32, u_emit_count@48, u_shape@64, u_seed@80 (96B)
namespace {
const char* kParticleUpdateWGSL = R"WGSL(// dse-wgsl
struct Particle { pos_life : vec4<f32>, vel_maxlife : vec4<f32>, };
struct Counters { alive : atomic<u32>, dead : atomic<u32>, dead_indices : array<atomic<u32>>, };
struct PC {
  u_gravity_dt : vec4<f32>,
  @align(16) u_wind_turbulence : vec4<f32>,
  @align(16) u_collision : vec4<f32>,
  @align(16) u_vortex : vec4<f32>,
  @align(16) u_max_particles : i32,
};
@group(1) @binding(8) var<uniform> pc : PC;
@group(3) @binding(0) var<storage, read_write> particles_in : array<Particle>;
@group(3) @binding(1) var<storage, read_write> particles_out : array<Particle>;
@group(3) @binding(2) var<storage, read_write> counters : Counters;
fn hash(n : f32) -> f32 { return fract(sin(n) * 43758.5453123); }
@compute @workgroup_size(256)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let idx = gid.x;
  if (i32(idx) >= pc.u_max_particles) { return; }
  let p_in = particles_in[idx];
  let dt = pc.u_gravity_dt.w;
  var pos = p_in.pos_life.xyz;
  var vel = p_in.vel_maxlife.xyz;
  var life = p_in.pos_life.w;
  if (life > 0.0) {
    vel = vel + pc.u_gravity_dt.xyz * dt;
    vel = vel + pc.u_wind_turbulence.xyz * dt;
    if (pc.u_vortex.x > 0.0) {
      let to_axis = vec3<f32>(-pos.z, 0.0, pos.x);
      vel = vel + normalize(to_axis + vec3<f32>(0.001)) * pc.u_vortex.x * dt;
    }
    if (pc.u_wind_turbulence.w > 0.0) {
      let n = hash(f32(idx) + life * 17.3);
      vel = vel + vec3<f32>(n - 0.5, hash(n * 7.1) - 0.5, hash(n * 13.7) - 0.5) * pc.u_wind_turbulence.w * dt;
    }
    pos = pos + vel * dt;
    life = life - dt;
    if (pc.u_collision.x > 0.5 && pos.y < pc.u_collision.y) {
      pos.y = pc.u_collision.y;
      vel.y = -vel.y * pc.u_collision.z;
      vel.xz = vel.xz * pc.u_collision.w;
    }
    if (life > 0.0) {
      atomicAdd(&counters.alive, 1u);
    } else {
      let di = atomicAdd(&counters.dead, 1u);
      atomicStore(&counters.dead_indices[di], idx);
    }
  } else {
    let di = atomicAdd(&counters.dead, 1u);
    atomicStore(&counters.dead_indices[di], idx);
  }
  particles_out[idx] = Particle(pos_life : vec4<f32>(pos, life), vel_maxlife : vec4<f32>(vel, p_in.vel_maxlife.w));
}
)WGSL";

const char* kParticleEmitWGSL = R"WGSL(// dse-wgsl
struct Particle { pos_life : vec4<f32>, vel_maxlife : vec4<f32>, };
struct Counters { alive : atomic<u32>, dead : atomic<u32>, dead_indices : array<atomic<u32>>, };
struct PC {
  u_emitter_pos : vec4<f32>,
  @align(16) u_life_speed : vec4<f32>,
  @align(16) u_emit_dir : vec4<f32>,
  @align(16) u_emit_count : i32,
  @align(16) u_shape : i32,
  @align(16) u_seed : i32,
};
@group(1) @binding(8) var<uniform> pc : PC;
@group(3) @binding(1) var<storage, read_write> particles_out : array<Particle>;
@group(3) @binding(2) var<storage, read_write> counters : Counters;
fn hash(n : u32) -> f32 {
  var h = (n << 13u) ^ n;
  h = h * (h * h * 15731u + 789221u) + 1376312589u;
  return f32(h & 0x7fffffffu) / f32(0x7fffffffu);
}
fn random_direction(seed : u32) -> vec3<f32> {
  let u = hash(seed);
  let v = hash(seed + 1u);
  let theta = u * 6.2831853;
  let phi = acos(2.0 * v - 1.0);
  return vec3<f32>(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
}
@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let idx = gid.x;
  if (i32(idx) >= pc.u_emit_count) { return; }
  var dead_idx = atomicAdd(&counters.dead, 0xFFFFFFFFu);
  if (dead_idx == 0u || dead_idx > 0x7FFFFFFFu) { return; }
  dead_idx = dead_idx - 1u;
  let slot = atomicLoad(&counters.dead_indices[dead_idx]);
  let s = u32(pc.u_seed) + idx * 7u;
  let life = mix(pc.u_life_speed.x, pc.u_life_speed.y, hash(s));
  let speed = mix(pc.u_life_speed.z, pc.u_life_speed.w, hash(s + 3u));
  var pos = pc.u_emitter_pos.xyz;
  var dir = random_direction(s + 5u);
  if (pc.u_shape == 1) {
    pos = pos + dir * pc.u_emitter_pos.w * hash(s + 10u);
  } else if (pc.u_shape == 2) {
    let cos_angle = pc.u_emit_dir.w;
    dir = normalize(mix(pc.u_emit_dir.xyz, dir, 1.0 - cos_angle));
  } else if (pc.u_shape == 3) {
    let angle = hash(s + 20u) * 6.2831853;
    pos = pos + vec3<f32>(cos(angle), 0.0, sin(angle)) * pc.u_emitter_pos.w;
  }
  particles_out[slot] = Particle(pos_life : vec4<f32>(pos, life), vel_maxlife : vec4<f32>(dir * speed, life));
  atomicAdd(&counters.alive, 1u);
}
)WGSL";
} // namespace

// ─── GpuParticleManager 实现 ─────────────────────────────────────────────

bool GpuParticleManager::Init(RhiDevice* rhi) {
    if (!rhi || inited_) return inited_;
    if (!rhi->SupportsCompute()) return false;

    using namespace generated_shaders;

    // Update pass：SSBO binding 0=ParticlesIn(readonly)/1=ParticlesOut(rw)/2=Counters(rw)，
    //   push=80B（u_gravity_dt/u_wind_turbulence/u_collision/u_vortex 各 vec4 + u_max_particles int）。
    update_shader_ = rhi->CreateComputeShaderEx(
        kparticle_update_comp_glsl430, kparticle_update_comp_glsl450, kparticle_update_comp_hlsl,
        3, 0, 0, 80, kParticleUpdateWGSL);
    // Emit pass：SSBO binding 1=ParticlesOut(rw)/2=Counters(rw)（binding 0 保留占位以对齐 VK 布局），
    //   push=96B（u_emitter_pos/u_life_speed/u_emit_dir 各 vec4 + u_emit_count/u_shape/u_seed int）。
    emit_shader_ = rhi->CreateComputeShaderEx(
        kparticle_emit_comp_glsl430, kparticle_emit_comp_glsl450, kparticle_emit_comp_hlsl,
        3, 0, 0, 96, kParticleEmitWGSL);

    inited_ = (update_shader_ && emit_shader_);
    if (!inited_) {
        DEBUG_LOG_ERROR("[GpuParticleManager] Compute shader creation failed "
                        "(update={}, emit={}) — GPU particles disabled on this backend",
                        update_shader_.raw(), emit_shader_.raw());
    }
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

    comp.particle_buffer_a = rhi->CreateGpuBuffer(desc, init_data.data());
    comp.particle_buffer_b = rhi->CreateGpuBuffer(desc, init_data.data());

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
    comp.counter_buffer = rhi->CreateGpuBuffer(counter_desc, counter_init.data());

    // Indirect draw buffer (DrawArraysIndirectCommand: count, instance_count, first, base_instance)
    GpuBufferDesc indirect_desc;
    indirect_desc.size = 4 * sizeof(uint32_t);
    indirect_desc.usage = GpuBufferUsage::kIndirect;
    indirect_desc.is_dynamic = true;
    uint32_t indirect_init[4] = {4, 0, 0, 0}; // 4 verts per quad, 0 instances initially
    comp.indirect_buffer = rhi->CreateGpuBuffer(indirect_desc, indirect_init);

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
    rhi->UpdateGpuBuffer(comp.counter_buffer, 0, 8, zero);

    // 绑定 SSBO
    BufferHandle read_buf = comp.ping ? comp.particle_buffer_a : comp.particle_buffer_b;
    BufferHandle write_buf = comp.ping ? comp.particle_buffer_b : comp.particle_buffer_a;

    rhi->BindGpuBuffer(read_buf, 0);
    rhi->BindGpuBuffer(write_buf, 1, true);
    rhi->BindGpuBuffer(comp.counter_buffer, 2, true);

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
        rhi->BindGpuBuffer(write_buf, 1, true);
        rhi->BindGpuBuffer(comp.counter_buffer, 2, true);

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
    if (update_shader_) { rhi->DeleteComputeShader(update_shader_); update_shader_ = {}; }
    if (emit_shader_) { rhi->DeleteComputeShader(emit_shader_); emit_shader_ = {}; }
    inited_ = false;
}

void GpuParticleManager::ShutdownComponent(GpuParticleComponent& comp, RhiDevice* rhi) {
    if (!rhi) return;
    if (comp.particle_buffer_a) { rhi->DeleteGpuBuffer(comp.particle_buffer_a); comp.particle_buffer_a = {}; }
    if (comp.particle_buffer_b) { rhi->DeleteGpuBuffer(comp.particle_buffer_b); comp.particle_buffer_b = {}; }
    if (comp.counter_buffer) { rhi->DeleteGpuBuffer(comp.counter_buffer); comp.counter_buffer = {}; }
    if (comp.indirect_buffer) { rhi->DeleteGpuBuffer(comp.indirect_buffer); comp.indirect_buffer = {}; }
    comp.initialized = false;
}

} // namespace render
} // namespace dse
