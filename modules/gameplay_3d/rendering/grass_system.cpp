#include "modules/gameplay_3d/rendering/grass_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/base/debug.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dse {
namespace gameplay3d {

// ============================================================
// GPU Wind Compute Shader (GLSL 430, OpenGL 4.3+)
// ============================================================

static const char* kGrassWindComputeSource = R"(
#version 430 core
layout(local_size_x = 64) in;

struct GrassInstance {
    vec4 pos_yaw;
    vec4 wh_phase_fade;
};

layout(std430, binding = 0) readonly buffer InputBuffer {
    GrassInstance instances[];
};

layout(std430, binding = 1) buffer OutputBuffer {
    mat4 matrices[];
};

uniform vec2 u_wind_dir;
uniform float u_wind_speed;
uniform float u_wind_strength;
uniform float u_wind_turbulence;
uniform float u_time;
uniform int u_instance_count;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (int(idx) >= u_instance_count) return;

    GrassInstance inst = instances[idx];
    vec3 pos = inst.pos_yaw.xyz;
    float yaw = inst.pos_yaw.w;
    float w = inst.wh_phase_fade.x;
    float h = inst.wh_phase_fade.y;
    float wind_phase = inst.wh_phase_fade.z;
    float fade = inst.wh_phase_fade.w;

    float phase = wind_phase + u_time * u_wind_speed;
    float bend = sin(phase) * u_wind_strength;
    float turb = sin(phase * 3.7 + wind_phase * 2.3) * u_wind_turbulence;
    float total_bend = clamp(bend + turb, -0.436, 0.436);

    float rx = -total_bend * u_wind_dir.y;
    float rz =  total_bend * u_wind_dir.x;

    float cx = cos(rx), sx = sin(rx);
    float cz = cos(rz), sz = sin(rz);
    float cy = cos(yaw), sy = sin(yaw);

    float a00 = cz * cy;   float a01 = -sz;  float a02 = cz * sy;
    float a10 = sz * cy;   float a11 = cz;   float a12 = sz * sy;
    float a20 = -sy;       float a21 = 0.0;  float a22 = cy;

    float r00 = a00;                     float r01 = a01;                    float r02 = a02;
    float r10 = cx * a10 - sx * a20;     float r11 = cx * a11 - sx * a21;   float r12 = cx * a12 - sx * a22;
    float r20 = sx * a10 + cx * a20;     float r21 = sx * a11 + cx * a21;   float r22 = sx * a12 + cx * a22;

    float hf = h * fade;
    mat4 m;
    m[0] = vec4(r00 * w, r10 * w, r20 * w, 0.0);
    m[1] = vec4(r01 * hf, r11 * hf, r21 * hf, 0.0);
    m[2] = vec4(r02 * w, r12 * w, r22 * w, 0.0);
    m[3] = vec4(pos, 1.0);

    matrices[idx] = m;
}
)";

static const char* kGrassWindComputeSourceVK = R"(
#version 450
layout(local_size_x = 64) in;

struct GrassInstance {
    vec4 pos_yaw;
    vec4 wh_phase_fade;
};

layout(set=0, binding=0, std430) readonly buffer InputBuffer {
    GrassInstance instances[];
};

layout(set=0, binding=1, std430) buffer OutputBuffer {
    mat4 matrices[];
};

layout(push_constant) uniform PC {
    vec2 u_wind_dir;
    float u_wind_speed;
    float u_wind_strength;
    float u_wind_turbulence;
    float u_time;
    int u_instance_count;
} pc;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (int(idx) >= pc.u_instance_count) return;

    GrassInstance inst = instances[idx];
    vec3 pos = inst.pos_yaw.xyz;
    float yaw = inst.pos_yaw.w;
    float w = inst.wh_phase_fade.x;
    float h = inst.wh_phase_fade.y;
    float wind_phase = inst.wh_phase_fade.z;
    float fade = inst.wh_phase_fade.w;

    float phase = wind_phase + pc.u_time * pc.u_wind_speed;
    float bend = sin(phase) * pc.u_wind_strength;
    float turb = sin(phase * 3.7 + wind_phase * 2.3) * pc.u_wind_turbulence;
    float total_bend = clamp(bend + turb, -0.436, 0.436);

    float rx = -total_bend * pc.u_wind_dir.y;
    float rz =  total_bend * pc.u_wind_dir.x;

    float cx = cos(rx), sx = sin(rx);
    float cz = cos(rz), sz = sin(rz);
    float cy = cos(yaw), sy = sin(yaw);

    float a00 = cz * cy;   float a01 = -sz;  float a02 = cz * sy;
    float a10 = sz * cy;   float a11 = cz;   float a12 = sz * sy;
    float a20 = -sy;       float a21 = 0.0;  float a22 = cy;

    float r00 = a00;                     float r01 = a01;                    float r02 = a02;
    float r10 = cx * a10 - sx * a20;     float r11 = cx * a11 - sx * a21;   float r12 = cx * a12 - sx * a22;
    float r20 = sx * a10 + cx * a20;     float r21 = sx * a11 + cx * a21;   float r22 = sx * a12 + cx * a22;

    float hf = h * fade;
    mat4 m;
    m[0] = vec4(r00 * w, r10 * w, r20 * w, 0.0);
    m[1] = vec4(r01 * hf, r11 * hf, r21 * hf, 0.0);
    m[2] = vec4(r02 * w, r12 * w, r22 * w, 0.0);
    m[3] = vec4(pos, 1.0);

    matrices[idx] = m;
}
)";

static const char* kGrassWindComputeSourceHLSL = R"(
struct GrassInstance {
    float4 pos_yaw;
    float4 wh_phase_fade;
};

cbuffer Params : register(b0) {
    float2 u_wind_dir;
    float u_wind_speed;
    float u_wind_strength;
    float u_wind_turbulence;
    float u_time;
    int u_instance_count;
    int _pad0;
};

StructuredBuffer<GrassInstance> instances : register(t16);
RWStructuredBuffer<float4x4> matrices : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint idx = id.x;
    if ((int)idx >= u_instance_count) return;

    GrassInstance inst = instances[idx];
    float3 pos = inst.pos_yaw.xyz;
    float yaw = inst.pos_yaw.w;
    float w = inst.wh_phase_fade.x;
    float h = inst.wh_phase_fade.y;
    float wind_phase = inst.wh_phase_fade.z;
    float fade = inst.wh_phase_fade.w;

    float phase = wind_phase + u_time * u_wind_speed;
    float bend = sin(phase) * u_wind_strength;
    float turb = sin(phase * 3.7 + wind_phase * 2.3) * u_wind_turbulence;
    float total_bend = clamp(bend + turb, -0.436, 0.436);

    float rx = -total_bend * u_wind_dir.y;
    float rz =  total_bend * u_wind_dir.x;

    float cx = cos(rx), sx = sin(rx);
    float cz = cos(rz), sz = sin(rz);
    float cy = cos(yaw), sy = sin(yaw);

    float a00 = cz * cy;   float a01 = -sz;  float a02 = cz * sy;
    float a10 = sz * cy;   float a11 = cz;   float a12 = sz * sy;
    float a20 = -sy;       float a21 = 0.0;  float a22 = cy;

    float r00 = a00;                     float r01 = a01;                    float r02 = a02;
    float r10 = cx * a10 - sx * a20;     float r11 = cx * a11 - sx * a21;   float r12 = cx * a12 - sx * a22;
    float r20 = sx * a10 + cx * a20;     float r21 = sx * a11 + cx * a21;   float r22 = sx * a12 + cx * a22;

    float hf = h * fade;
    float4x4 m;
    m[0] = float4(r00 * w, r10 * w, r20 * w, 0.0);
    m[1] = float4(r01 * hf, r11 * hf, r21 * hf, 0.0);
    m[2] = float4(r02 * w, r12 * w, r22 * w, 0.0);
    m[3] = float4(pos, 1.0);

    matrices[idx] = m;
}
)";

// ============================================================
// WebGPU WGSL 变体（手译，无离线 GLSL→WGSL 工具；与上方 GLSL 430 逐句一致）
//   SSBO 走 group3 b0/b1（WebGPU RHI compute 统一 BufferBindingType_Storage → 全声明 read_write，
//   含只读 instances）；命名 uniform 走 group1 b8 保留 binding，各成员 @align(16)、声明同名同序，
//   对齐 SetComputeUniform* 调用序（u_wind_dir→speed→strength→turbulence→time→instance_count）的
//   16B 定位（见 webgpu_rhi_device.cpp kComputeNamedUboBinding=8）。输出列主序 mat4x4<f32>（std430，
//   每列 vec4）。逐句镜像 CPU BuildWindMatrix（h*fade 折叠 hf、列主序装配）。
// ============================================================
static const char* kGrassWindComputeSourceWGSL = R"WGSL(// dse-wgsl
struct GrassInstance {
  pos_yaw : vec4<f32>,
  wh_phase_fade : vec4<f32>,
};
@group(3) @binding(0) var<storage, read_write> instances : array<GrassInstance>;
@group(3) @binding(1) var<storage, read_write> matrices : array<mat4x4<f32>>;
struct PC {
  @align(16) u_wind_dir : vec2<f32>,
  @align(16) u_wind_speed : f32,
  @align(16) u_wind_strength : f32,
  @align(16) u_wind_turbulence : f32,
  @align(16) u_time : f32,
  @align(16) u_instance_count : i32,
};
@group(1) @binding(8) var<uniform> pc : PC;
@compute @workgroup_size(64, 1, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let idx = gid.x;
  if (i32(idx) >= pc.u_instance_count) { return; }
  let inst = instances[idx];
  let pos = inst.pos_yaw.xyz;
  let yaw = inst.pos_yaw.w;
  let w = inst.wh_phase_fade.x;
  let h = inst.wh_phase_fade.y;
  let wind_phase = inst.wh_phase_fade.z;
  let fade = inst.wh_phase_fade.w;
  let phase = wind_phase + pc.u_time * pc.u_wind_speed;
  let bend = sin(phase) * pc.u_wind_strength;
  let turb = sin(phase * 3.7 + wind_phase * 2.3) * pc.u_wind_turbulence;
  let total_bend = clamp(bend + turb, -0.436, 0.436);
  let rx = -total_bend * pc.u_wind_dir.y;
  let rz =  total_bend * pc.u_wind_dir.x;
  let cx = cos(rx); let sx = sin(rx);
  let cz = cos(rz); let sz = sin(rz);
  let cy = cos(yaw); let sy = sin(yaw);
  let a00 = cz * cy; let a01 = -sz; let a02 = cz * sy;
  let a10 = sz * cy; let a11 = cz;  let a12 = sz * sy;
  let a20 = -sy;     let a21 = 0.0; let a22 = cy;
  let r00 = a00;                  let r01 = a01;                  let r02 = a02;
  let r10 = cx * a10 - sx * a20;  let r11 = cx * a11 - sx * a21;  let r12 = cx * a12 - sx * a22;
  let r20 = sx * a10 + cx * a20;  let r21 = sx * a11 + cx * a21;  let r22 = sx * a12 + cx * a22;
  let hf = h * fade;
  matrices[idx] = mat4x4<f32>(
    vec4<f32>(r00 * w, r10 * w, r20 * w, 0.0),
    vec4<f32>(r01 * hf, r11 * hf, r21 * hf, 0.0),
    vec4<f32>(r02 * w, r12 * w, r22 * w, 0.0),
    vec4<f32>(pos, 1.0));
}
)WGSL";

// ============================================================
// Halton 低差异序列 + hash
// ============================================================

static float Halton(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;
    int i = index;
    while (i > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(i % base);
        i /= base;
    }
    return r;
}

static uint32_t HashCombine(uint32_t seed, int v) {
    uint32_t h = static_cast<uint32_t>(v);
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return seed ^ (h + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

// ============================================================
// 风场：从静态布局 + 风参数 → 最终 model matrix
// ============================================================

/// 将 GrassInstanceLayout 转换为含风场旋转的 model matrix
/// 风场原理: 模型空间 Y=0 在草叶基部，绕基部旋转使尖端位移最大
static glm::mat4 BuildWindMatrix(const GrassInstanceLayout& layout,
                                  const glm::vec2& wind_dir,
                                  float wind_speed,
                                  float wind_strength,
                                  float wind_turbulence,
                                  float time) {
    // 风相位 = 基于位置的空间频率 + 时间
    float phase = layout.wind_phase + time * wind_speed;
    float bend = std::sin(phase) * wind_strength;
    // 湍流：高频扰动
    float turb = std::sin(phase * 3.7f + layout.wind_phase * 2.3f) * wind_turbulence;
    float total_bend = bend + turb;
    // 限制最大弯曲角度 ±25°
    total_bend = std::max(-0.436f, std::min(0.436f, total_bend));

    // 风向分解为绕 X 和绕 Z 的旋转
    // wind_dir = (wx, wz)，绕 Z 轴旋转使草叶沿 X 弯曲，绕 X 轴使草叶沿 Z 弯曲
    float rx = -total_bend * wind_dir.y;  // 绕 X 轴旋转
    float rz = total_bend * wind_dir.x;   // 绕 Z 轴旋转

    float cx = std::cos(rx), sx = std::sin(rx);
    float cz = std::cos(rz), sz = std::sin(rz);
    float cy = std::cos(layout.yaw), sy = std::sin(layout.yaw);

    float w = layout.width;
    float h = layout.height;

    // 构建: translate(pos) * rotateX(rx) * rotateZ(rz) * rotateY(yaw) * scale(w, h, w)
    // 手动展开避免 glm 矩阵乘法开销
    // R_combined = Rx * Rz * Ry
    // Rx = [[1,0,0],[0,cx,-sx],[0,sx,cx]]
    // Rz = [[cz,-sz,0],[sz,cz,0],[0,0,1]]
    // Ry = [[cy,0,sy],[0,1,0],[-sy,0,cy]]
    // R = Rx * Rz * Ry, then scale columns by (w, h, w)

    // Rz * Ry
    float a00 = cz * cy;   float a01 = -sz;  float a02 = cz * sy;
    float a10 = sz * cy;   float a11 = cz;   float a12 = sz * sy;
    float a20 = -sy;       float a21 = 0.0f; float a22 = cy;

    // Rx * (Rz * Ry)
    float r00 = a00;                     float r01 = a01;                    float r02 = a02;
    float r10 = cx * a10 - sx * a20;     float r11 = cx * a11 - sx * a21;   float r12 = cx * a12 - sx * a22;
    float r20 = sx * a10 + cx * a20;     float r21 = sx * a11 + cx * a21;   float r22 = sx * a12 + cx * a22;

    glm::mat4 m(1.0f);
    m[0] = glm::vec4(r00 * w, r10 * w, r20 * w, 0.0f);
    m[1] = glm::vec4(r01 * h, r11 * h, r21 * h, 0.0f);
    m[2] = glm::vec4(r02 * w, r12 * w, r22 * w, 0.0f);
    m[3] = glm::vec4(layout.position, 1.0f);
    return m;
}

// ============================================================
// 生命周期
// ============================================================

void GrassSystem::Init(RhiDevice* rhi_device) {
    rhi_ = rhi_device;
    BuildBladeMesh();
    BuildBillboardMesh();
    InitComputeShader();
    DEBUG_LOG_INFO("[GrassSystem] Initialized. blade_verts={}, billboard_verts={}, gpu_compute={}",
                   blade_vertices_.size(), billboard_vertices_.size(), gpu_compute_enabled_);
}

void GrassSystem::Shutdown(World& world) {
    (void)world;
    ShutdownComputeResources();
    if (rhi_) mesh_renderer_.Shutdown(*rhi_);
    blade_vertices_.clear();
    blade_indices_.clear();
    billboard_vertices_.clear();
    billboard_indices_.clear();
    entity_caches_.clear();
    rhi_ = nullptr;
}

// ============================================================
// GPU Compute 风场 — 初始化 / 清理 / 容量管理
// ============================================================

void GrassSystem::InitComputeShader() {
    if (!rhi_ || !rhi_->SupportsSSBOCompute()) {
        gpu_compute_enabled_ = false;
        return;
    }
    wind_compute_shader_ = rhi_->CreateComputeShaderEx(
        kGrassWindComputeSource,
        kGrassWindComputeSourceVK,
        kGrassWindComputeSourceHLSL,
        2, 0, 0, 28, kGrassWindComputeSourceWGSL);
    gpu_compute_enabled_ = (wind_compute_shader_ != 0);
    if (gpu_compute_enabled_) {
        DEBUG_LOG_INFO("[GrassSystem] GPU wind compute shader created: {}", wind_compute_shader_);
    } else {
        DEBUG_LOG_INFO("[GrassSystem] GPU compute not available, using CPU fallback");
    }
}

void GrassSystem::ShutdownComputeResources() {
    if (!rhi_) return;
    if (wind_compute_shader_ != 0) {
        rhi_->DeleteComputeShader(wind_compute_shader_);
        wind_compute_shader_ = 0;
    }
    if (input_ssbo_) {
        rhi_->DeleteGpuBuffer(input_ssbo_);
        input_ssbo_ = {};
    }
    if (output_ssbo_) {
        rhi_->DeleteGpuBuffer(output_ssbo_);
        output_ssbo_ = {};
    }
    ssbo_capacity_ = 0;
    gpu_compute_enabled_ = false;
}

void GrassSystem::EnsureSSBOCapacity(size_t required_count) {
    if (required_count <= ssbo_capacity_) return;

    size_t new_cap = std::max(required_count, ssbo_capacity_ * 2);
    new_cap = std::max(new_cap, size_t(1024));

    if (input_ssbo_) rhi_->DeleteGpuBuffer(input_ssbo_);
    if (output_ssbo_) rhi_->DeleteGpuBuffer(output_ssbo_);

    {
        dse::render::GpuBufferDesc d{new_cap * sizeof(GrassGPUInstance), dse::render::GpuBufferUsage::kStorage, true, "grass_input"};
        input_ssbo_ = rhi_->CreateGpuBuffer(d, nullptr);
    }
    {
        dse::render::GpuBufferDesc d{new_cap * sizeof(glm::mat4), dse::render::GpuBufferUsage::kStorage, true, "grass_output"};
        output_ssbo_ = rhi_->CreateGpuBuffer(d, nullptr);
    }

    if (!input_ssbo_ || !output_ssbo_) {
        DEBUG_LOG_ERROR("[GrassSystem] SSBO allocation failed, disabling GPU compute");
        if (input_ssbo_) { rhi_->DeleteGpuBuffer(input_ssbo_); input_ssbo_ = {}; }
        if (output_ssbo_) { rhi_->DeleteGpuBuffer(output_ssbo_); output_ssbo_ = {}; }
        ssbo_capacity_ = 0;
        gpu_compute_enabled_ = false;
        return;
    }
    ssbo_capacity_ = new_cap;

    DEBUG_LOG_INFO("[GrassSystem] SSBO capacity grown to {} instances", new_cap);
}

// ============================================================
// 程序化 Mesh 生成
// ============================================================

void GrassSystem::BuildBladeMesh() {
    // 4 段三角带: 5 层 × 2 列 = 10 顶点, 8 三角形
    // 模型空间: X ∈ [-0.5, 0.5], Y ∈ [0, 1], Z = 0
    const int segments = 4;
    const int vert_count = (segments + 1) * 2;
    blade_vertices_.resize(static_cast<size_t>(vert_count));

    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float half_width = 0.5f * (1.0f - t * 0.8f);

        BatchVertex& vl = blade_vertices_[static_cast<size_t>(i * 2)];
        vl.pos = glm::vec3(-half_width, t, 0.0f);
        vl.color = glm::vec4(1.0f);
        vl.uv = glm::vec2(0.0f, t);
        vl.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        vl.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        vl.weights = glm::vec4(0.0f);
        vl.joints = glm::vec4(0.0f);

        BatchVertex& vr = blade_vertices_[static_cast<size_t>(i * 2 + 1)];
        vr.pos = glm::vec3(half_width, t, 0.0f);
        vr.color = glm::vec4(1.0f);
        vr.uv = glm::vec2(1.0f, t);
        vr.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        vr.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        vr.weights = glm::vec4(0.0f);
        vr.joints = glm::vec4(0.0f);
    }

    blade_indices_.reserve(static_cast<size_t>(segments * 6));
    for (int i = 0; i < segments; ++i) {
        uint32_t bl = static_cast<uint32_t>(i * 2);
        uint32_t br = bl + 1;
        uint32_t tl = bl + 2;
        uint32_t tr = bl + 3;
        blade_indices_.push_back(bl); blade_indices_.push_back(br); blade_indices_.push_back(tl);
        blade_indices_.push_back(br); blade_indices_.push_back(tr); blade_indices_.push_back(tl);
    }
}

void GrassSystem::BuildBillboardMesh() {
    // 交叉十字形: 2 个正交 quad, 8 顶点, 4 三角形
    billboard_vertices_.resize(8);

    auto make_quad = [&](int base, const glm::vec3& right) {
        glm::vec3 half_r = right * 0.5f;
        glm::vec3 n = glm::normalize(glm::cross(right, glm::vec3(0.0f, 1.0f, 0.0f)));

        billboard_vertices_[static_cast<size_t>(base + 0)].pos = -half_r;
        billboard_vertices_[static_cast<size_t>(base + 0)].uv = glm::vec2(0.0f, 0.0f);
        billboard_vertices_[static_cast<size_t>(base + 1)].pos = half_r;
        billboard_vertices_[static_cast<size_t>(base + 1)].uv = glm::vec2(1.0f, 0.0f);
        billboard_vertices_[static_cast<size_t>(base + 2)].pos = half_r + glm::vec3(0.0f, 1.0f, 0.0f);
        billboard_vertices_[static_cast<size_t>(base + 2)].uv = glm::vec2(1.0f, 1.0f);
        billboard_vertices_[static_cast<size_t>(base + 3)].pos = -half_r + glm::vec3(0.0f, 1.0f, 0.0f);
        billboard_vertices_[static_cast<size_t>(base + 3)].uv = glm::vec2(0.0f, 1.0f);

        for (int j = 0; j < 4; ++j) {
            auto& v = billboard_vertices_[static_cast<size_t>(base + j)];
            v.color = glm::vec4(1.0f);
            v.normal = n;
            v.tangent = glm::normalize(right);
            v.weights = glm::vec4(0.0f);
            v.joints = glm::vec4(0.0f);
        }
    };

    make_quad(0, glm::vec3(1.0f, 0.0f, 0.0f));
    make_quad(4, glm::vec3(0.0f, 0.0f, 1.0f));

    billboard_indices_ = {
        0, 1, 2,  0, 2, 3,
        4, 5, 6,  4, 6, 7
    };
}

// ============================================================
// Chunk 空间辅助
// ============================================================

uint64_t GrassSystem::ChunkKey(int cx, int cz) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(cz));
}

void GrassSystem::ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 out_planes[6]) {
    for (int i = 0; i < 4; ++i) {
        out_planes[0][i] = vp[i][3] + vp[i][0]; // left
        out_planes[1][i] = vp[i][3] - vp[i][0]; // right
        out_planes[2][i] = vp[i][3] + vp[i][1]; // bottom
        out_planes[3][i] = vp[i][3] - vp[i][1]; // top
        out_planes[4][i] = vp[i][3] + vp[i][2]; // near
        out_planes[5][i] = vp[i][3] - vp[i][2]; // far
    }
    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(out_planes[i]));
        if (len > 1e-6f) out_planes[i] /= len;
    }
}

bool GrassSystem::IsAABBInFrustum(const glm::vec4 planes[6],
                                   const glm::vec3& aabb_min,
                                   const glm::vec3& aabb_max) {
    for (int i = 0; i < 6; ++i) {
        glm::vec3 p(
            planes[i].x > 0.0f ? aabb_max.x : aabb_min.x,
            planes[i].y > 0.0f ? aabb_max.y : aabb_min.y,
            planes[i].z > 0.0f ? aabb_max.z : aabb_min.z);
        if (glm::dot(glm::vec3(planes[i]), p) + planes[i].w < 0.0f)
            return false;
    }
    return true;
}

// ============================================================
// Chunk 实例生成（静态布局，不含风场）
// ============================================================

void GrassSystem::GenerateChunkInstances(const GrassComponent& grass,
                                          const TerrainComponent* terrain,
                                          const TransformComponent* terrain_transform,
                                          const TransformComponent& grass_transform,
                                          int chunk_x, int chunk_z,
                                          GrassChunkData& out) {
    (void)grass_transform;
    out.layouts.clear();
    out.valid = true;

    const float cs = grass.chunk_size;
    const float world_min_x = static_cast<float>(chunk_x) * cs;
    const float world_min_z = static_cast<float>(chunk_z) * cs;

    const float chunk_area = cs * cs;
    const int blade_count = static_cast<int>(grass.density * chunk_area);
    if (blade_count <= 0) { out.valid = false; return; }

    uint32_t chunk_seed = HashCombine(grass.seed, chunk_x);
    chunk_seed = HashCombine(chunk_seed, chunk_z);

    out.aabb_min = glm::vec3(world_min_x, 0.0f, world_min_z);
    out.aabb_max = glm::vec3(world_min_x + cs, grass.blade_height * 1.5f, world_min_z + cs);

    float min_y = 1e9f, max_y = -1e9f;

    // 风方向归一化提到循环外避免重复计算
    glm::vec2 wind_norm_cached = glm::length(grass.wind_direction) > 1e-6f
                                  ? glm::normalize(grass.wind_direction)
                                  : glm::vec2(1.0f, 0.0f);

    out.layouts.reserve(static_cast<size_t>(blade_count));

    for (int i = 0; i < blade_count; ++i) {
        int seq = static_cast<int>(chunk_seed) + i + 1;
        float hx = Halton(seq, 2);
        float hz = Halton(seq, 3);
        float wx = world_min_x + hx * cs;
        float wz = world_min_z + hz * cs;

        float wy = 0.0f;
        if (terrain && terrain_transform) {
            wy = dse::SampleTerrainHeight(*terrain, *terrain_transform, wx, wz);
        }

        float height_var = Halton(seq, 5);
        float height_scale = 1.0f + (height_var * 2.0f - 1.0f) * grass.blade_height_variation;

        GrassInstanceLayout layout;
        layout.position = glm::vec3(wx, wy, wz);
        layout.yaw = Halton(seq, 7) * 6.283185f;
        layout.width = grass.blade_width;
        layout.height = grass.blade_height * height_scale;
        layout.wind_phase = glm::dot(glm::vec2(wx, wz), wind_norm_cached) * 0.5f;

        out.layouts.push_back(layout);

        min_y = std::min(min_y, wy);
        max_y = std::max(max_y, wy + layout.height);
    }

    if (!out.layouts.empty()) {
        out.aabb_min.y = min_y;
        out.aabb_max.y = max_y;
    }
}

// ============================================================
// Update: 增量 chunk 缓存维护
// ============================================================

void GrassSystem::Update(World& world, float delta_time) {
    accumulated_time_ += static_cast<double>(delta_time);

    auto camera_view = world.registry().view<Camera3DComponent, TransformComponent>();
    glm::vec3 camera_pos(0.0f);
    bool has_camera = false;
    for (auto cam_entity : camera_view) {
        const auto& cam = camera_view.get<Camera3DComponent>(cam_entity);
        if (!cam.enabled) continue;
        const auto& cam_t = camera_view.get<TransformComponent>(cam_entity);
        camera_pos = glm::vec3(cam_t.local_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        has_camera = true;
        break;
    }
    if (!has_camera) return;

    const TerrainComponent* terrain = nullptr;
    const TransformComponent* terrain_transform = nullptr;
    {
        auto terrain_view = world.registry().view<TerrainComponent, TransformComponent>();
        for (auto te : terrain_view) {
            const auto& tc = terrain_view.get<TerrainComponent>(te);
            if (tc.enabled) {
                terrain = &tc;
                terrain_transform = &terrain_view.get<TransformComponent>(te);
                break;
            }
        }
    }

    auto grass_view = world.registry().view<GrassComponent, TransformComponent>();
    for (auto entity : grass_view) {
        auto& grass = grass_view.get<GrassComponent>(entity);
        if (!grass.enabled) continue;

        const auto& grass_transform = grass_view.get<TransformComponent>(entity);
        uint32_t eid = static_cast<uint32_t>(entity);
        auto& cache = entity_caches_[eid];

        const float radius = grass.spawn_radius;
        const float cs = grass.chunk_size;

        int cx_min = static_cast<int>(std::floor((camera_pos.x - radius) / cs));
        int cx_max = static_cast<int>(std::floor((camera_pos.x + radius) / cs));
        int cz_min = static_cast<int>(std::floor((camera_pos.z - radius) / cs));
        int cz_max = static_cast<int>(std::floor((camera_pos.z + radius) / cs));

        std::unordered_map<uint64_t, bool> active_keys;
        active_keys.reserve(static_cast<size_t>((cx_max - cx_min + 1) * (cz_max - cz_min + 1)));

        for (int cx = cx_min; cx <= cx_max; ++cx) {
            for (int cz = cz_min; cz <= cz_max; ++cz) {
                float chunk_center_x = (static_cast<float>(cx) + 0.5f) * cs;
                float chunk_center_z = (static_cast<float>(cz) + 0.5f) * cs;
                float dx = chunk_center_x - camera_pos.x;
                float dz = chunk_center_z - camera_pos.z;
                if (dx * dx + dz * dz > radius * radius) continue;

                uint64_t key = ChunkKey(cx, cz);
                active_keys[key] = true;

                if (cache.chunks.find(key) == cache.chunks.end()) {
                    GrassChunkData& cd = cache.chunks[key];
                    GenerateChunkInstances(grass, terrain, terrain_transform,
                                           grass_transform, cx, cz, cd);
                }
            }
        }

        for (auto it = cache.chunks.begin(); it != cache.chunks.end(); ) {
            if (active_keys.find(it->first) == active_keys.end()) {
                it = cache.chunks.erase(it);
            } else {
                ++it;
            }
        }

        int total = 0;
        for (const auto& [k, cd] : cache.chunks) {
            total += static_cast<int>(cd.layouts.size());
        }
        grass.cached_instance_count_ = total;
        cache.last_camera_pos = camera_pos;
    }
}

// ============================================================
// 渲染
// ============================================================

void GrassSystem::Render(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                         const glm::vec3& camera_offset, bool depth_only) {
    RenderInternal(world, cmd_buffer, frame, depth_only, /*shadow_pass=*/false, camera_offset);
}

void GrassSystem::RenderShadow(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                               const glm::vec3& camera_offset) {
    RenderInternal(world, cmd_buffer, frame, /*depth_only=*/true, /*shadow_pass=*/true, camera_offset);
}

void GrassSystem::RenderInternal(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                                  bool depth_only, bool shadow_pass, const glm::vec3& camera_offset) {
    if (blade_vertices_.empty()) return;

    auto camera_view = world.registry().view<Camera3DComponent, TransformComponent>();
    glm::mat4 view_matrix(1.0f);
    glm::mat4 proj_matrix(1.0f);
    glm::vec3 camera_pos(0.0f);
    bool has_camera = false;

    for (auto cam_entity : camera_view) {
        const auto& cam = camera_view.get<Camera3DComponent>(cam_entity);
        if (!cam.enabled) continue;
        const auto& cam_t = camera_view.get<TransformComponent>(cam_entity);
        camera_pos = glm::vec3(cam_t.local_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        glm::vec3 front = cam_t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = cam_t.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        view_matrix = glm::lookAt(camera_pos, camera_pos + front, up);
        proj_matrix = glm::perspective(glm::radians(cam.fov),
                                        cam.aspect_ratio > 0.0f ? cam.aspect_ratio : (16.0f / 9.0f),
                                        cam.near_clip, cam.far_clip);
        has_camera = true;
        break;
    }
    if (!has_camera) return;

    glm::mat4 vp = proj_matrix * view_matrix;
    glm::vec4 frustum_planes[6];
    ExtractFrustumPlanes(vp, frustum_planes);

    // 前向 pass 绘制用 command buffer 的 view/proj（与 DrawMeshBatch 执行器同源，含投影修正）。
    const glm::mat4 draw_view = frame.view;
    const glm::mat4 draw_proj = frame.projection;
    const glm::vec3 draw_cam_pos = glm::vec3(glm::inverse(draw_view)[3]);

    glm::vec3 light_dir(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.2f;
    float shadow_strength_val = 0.35f;
    {
        auto light_view = world.registry().view<DirectionalLight3DComponent>();
        for (auto le : light_view) {
            const auto& light = light_view.get<DirectionalLight3DComponent>(le);
            if (light.enabled) {
                light_dir = light.direction;
                light_color = light.color;
                light_intensity = light.intensity;
                ambient_intensity = light.ambient_intensity;
                shadow_strength_val = light.shadow_strength;
                break;
            }
        }
    }

    const float current_time = static_cast<float>(std::fmod(accumulated_time_, 10000.0));

    auto grass_view = world.registry().view<GrassComponent, TransformComponent>();
    for (auto entity : grass_view) {
        const auto& grass = grass_view.get<GrassComponent>(entity);
        if (!grass.enabled) continue;

        uint32_t eid = static_cast<uint32_t>(entity);
        auto cache_it = entity_caches_.find(eid);
        if (cache_it == entity_caches_.end()) continue;
        const auto& cache = cache_it->second;

        glm::vec2 wind_norm = glm::length(grass.wind_direction) > 1e-6f
                              ? glm::normalize(grass.wind_direction)
                              : glm::vec2(1.0f, 0.0f);

        // === Phase 1: 收集 GPU 实例 + LOD 分类 + fade ===
        std::vector<GrassGPUInstance> lod0_gpu, lod1_gpu;
        lod0_gpu.reserve(static_cast<size_t>(grass.cached_instance_count_));
        lod1_gpu.reserve(static_cast<size_t>(grass.cached_instance_count_) / 4);

        auto pack_instance = [](const GrassInstanceLayout& layout, float fade) -> GrassGPUInstance {
            return GrassGPUInstance{
                glm::vec4(layout.position, layout.yaw),
                glm::vec4(layout.width, layout.height, layout.wind_phase, fade)
            };
        };

        const float fade_range = std::max(grass.fade_range, 0.01f);
        const float near_fade_start = std::max(0.0f, grass.lod_near - fade_range);
        const float far_fade_start  = std::max(grass.lod_near, grass.lod_far - fade_range);

        for (const auto& [key, cd] : cache.chunks) {
            if (!cd.valid || cd.layouts.empty()) continue;
            if (!IsAABBInFrustum(frustum_planes, cd.aabb_min, cd.aabb_max))
                continue;

            float ccx = (cd.aabb_min.x + cd.aabb_max.x) * 0.5f;
            float ccz = (cd.aabb_min.z + cd.aabb_max.z) * 0.5f;
            float dx = ccx - camera_pos.x;
            float dz = ccz - camera_pos.z;
            float dist = std::sqrt(dx * dx + dz * dz);

            if (shadow_pass) {
                if (!grass.cast_shadow || dist > grass.shadow_distance) continue;
                for (const auto& layout : cd.layouts) {
                    lod0_gpu.push_back(pack_instance(layout, 1.0f));
                }
            } else {
                if (dist < near_fade_start) {
                    for (const auto& layout : cd.layouts) {
                        lod0_gpu.push_back(pack_instance(layout, 1.0f));
                    }
                } else if (dist < grass.lod_near) {
                    float t = (dist - near_fade_start) / fade_range;
                    for (const auto& layout : cd.layouts) {
                        lod0_gpu.push_back(pack_instance(layout, 1.0f - t));
                        lod1_gpu.push_back(pack_instance(layout, t));
                    }
                } else if (dist < far_fade_start) {
                    for (const auto& layout : cd.layouts) {
                        lod1_gpu.push_back(pack_instance(layout, 1.0f));
                    }
                } else if (dist < grass.lod_far) {
                    float t = (dist - far_fade_start) / fade_range;
                    for (const auto& layout : cd.layouts) {
                        lod1_gpu.push_back(pack_instance(layout, 1.0f - t));
                    }
                }
            }
        }

        // === Phase 2: 计算 model matrix（GPU compute 或 CPU fallback）===
        const size_t lod0_count = lod0_gpu.size();
        const size_t total_count = lod0_count + lod1_gpu.size();
        if (total_count == 0) continue;

        std::vector<glm::mat4> all_matrices(total_count);

        // CPU 风场回退（镜像 GPU 风场公式经 BuildWindMatrix）：非 GPU 路径与「异步回读上帧结果未就绪」
        // 时复用，保证当帧 all_matrices 正确、无破帧（GPU 风场延迟 1 帧视觉无感）。
        auto compute_cpu = [&](const std::vector<GrassGPUInstance>& gpu_vec, size_t out_offset) {
            for (size_t i = 0; i < gpu_vec.size(); ++i) {
                const auto& inst = gpu_vec[i];
                GrassInstanceLayout layout;
                layout.position = glm::vec3(inst.pos_yaw);
                layout.yaw = inst.pos_yaw.w;
                layout.width = inst.wh_phase_fade.x;
                layout.height = inst.wh_phase_fade.y * inst.wh_phase_fade.w;
                layout.wind_phase = inst.wh_phase_fade.z;
                all_matrices[out_offset + i] = BuildWindMatrix(
                    layout, wind_norm, grass.wind_speed,
                    grass.wind_strength, grass.wind_turbulence, current_time);
            }
        };
        auto compute_cpu_all = [&]() {
            compute_cpu(lod0_gpu, 0);
            compute_cpu(lod1_gpu, lod0_count);
        };

        bool use_gpu = gpu_compute_enabled_ && total_count >= 64;
        if (use_gpu) {
            EnsureSSBOCapacity(total_count);
            use_gpu = gpu_compute_enabled_;
        }

        if (use_gpu) {
            rhi_->UpdateGpuBuffer(input_ssbo_, 0,
                lod0_count * sizeof(GrassGPUInstance), lod0_gpu.data());
            if (!lod1_gpu.empty()) {
                rhi_->UpdateGpuBuffer(input_ssbo_,
                    lod0_count * sizeof(GrassGPUInstance),
                    lod1_gpu.size() * sizeof(GrassGPUInstance), lod1_gpu.data());
            }
            rhi_->BindGpuBuffer(input_ssbo_, 0, false);
            rhi_->BindGpuBuffer(output_ssbo_, 1, true);

            rhi_->SetComputeUniformVec2f(wind_compute_shader_, "u_wind_dir", wind_norm.x, wind_norm.y);
            rhi_->SetComputeUniformFloat(wind_compute_shader_, "u_wind_speed", grass.wind_speed);
            rhi_->SetComputeUniformFloat(wind_compute_shader_, "u_wind_strength", grass.wind_strength);
            rhi_->SetComputeUniformFloat(wind_compute_shader_, "u_wind_turbulence", grass.wind_turbulence);
            rhi_->SetComputeUniformFloat(wind_compute_shader_, "u_time", current_time);
            rhi_->SetComputeUniformInt(wind_compute_shader_, "u_instance_count", static_cast<int>(total_count));

            // dispatch 须包裹在 compute pass 内（WebGPU DispatchCompute 在无 pass 时 no-op）。
            unsigned int groups = (static_cast<unsigned int>(total_count) + 63) / 64;
            rhi_->BeginComputePass();
            rhi_->DispatchCompute(wind_compute_shader_, groups, 1, 1);
            rhi_->ComputeMemoryBarrier();
            rhi_->EndComputePass();

            // 异步双缓冲延迟回读（须在 pass 外）：本帧发起 src→staging 拷贝并取「上一帧」就绪结果。
            //   桌面 GL/VK/DX11 用基类默认同步实现 → 当帧即就绪；WebGPU 覆写为延迟 1 帧 → 首帧/尺寸
            //   不符/未就绪时落 CPU 风场回退，保证当帧 all_matrices 正确。
            const size_t bytes = total_count * sizeof(glm::mat4);
            const bool ready = rhi_->BeginGpuReadback(output_ssbo_, 0, bytes);
            size_t rb_size = 0;
            const void* rb = ready ? rhi_->GetLastReadbackResult(&rb_size) : nullptr;
            if (rb && rb_size >= bytes) {
                std::memcpy(all_matrices.data(), rb, bytes);
            } else {
                compute_cpu_all();
            }
        } else {
            compute_cpu_all();
        }

        std::vector<glm::mat4> lod0_matrices(
            all_matrices.begin(),
            all_matrices.begin() + static_cast<ptrdiff_t>(lod0_count));
        std::vector<glm::mat4> lod1_matrices(
            all_matrices.begin() + static_cast<ptrdiff_t>(lod0_count),
            all_matrices.end());

        // === Phase 3: 顶点色渐变 — base_color → tip_color 按模型空间 Y 插值 ===
        auto make_gradient_verts = [&](const std::vector<BatchVertex>& src) {
            auto verts = src;
            for (auto& v : verts) {
                float t = v.pos.y;
                v.color = glm::vec4(glm::mix(grass.base_color, grass.tip_color, t), 1.0f);
            }
            return verts;
        };
        auto blade_colored = make_gradient_verts(blade_vertices_);
        auto billboard_colored = make_gradient_verts(billboard_vertices_);

        // === Phase 4: 提交 instanced draw ===
        auto submit_batch = [&](std::vector<glm::mat4>& instances,
                               const std::vector<BatchVertex>& verts,
                               const std::vector<uint32_t>& idxs) {
            if (instances.empty() || verts.empty()) return;
            // Camera-Relative: 每个实例 model matrix 减去 camera_offset
            glm::vec4 offset4(camera_offset, 0.0f);
            for (auto& m : instances) {
                m[3] -= offset4;
            }

            if (depth_only) {
                // 深度 pass（PreZ / Shadow）：迁移到 MeshRenderer::DrawDepthOnlyInstanced
                //（逐帧上传顶点 + 每实例 model；ForwardInstancedDepth）。风场已由 BuildWindMatrix
                // 烘进每实例矩阵，故 foliage=false，与彩色前向 pass 完全同变换 → 阴影/深度不错位。
                std::vector<dse::render::MeshVertex> dv;
                dv.reserve(verts.size());
                for (const auto& bv : verts) {
                    dse::render::MeshVertex v;
                    v.position = bv.pos;
                    v.color = bv.color;
                    v.uv = bv.uv;
                    v.normal = bv.normal;
                    v.tangent = bv.tangent;
                    dv.push_back(v);
                }
                std::vector<uint16_t> didx;
                didx.reserve(idxs.size());
                for (uint32_t ix : idxs) didx.push_back(static_cast<uint16_t>(ix));
                mesh_renderer_.DrawDepthOnlyInstanced(
                    cmd_buffer, *rhi_, dv, didx, instances,
                    draw_view, draw_proj, /*foliage=*/false);
                return;
            }

            // 前向 pass：迁移到 MeshRenderer::DrawInstancedShaded（逐帧上传顶点 + 每实例 model）。
            // 风场已由 BuildWindMatrix 烘进每实例矩阵，故 material.foliage 保持 false。
            std::vector<dse::render::MeshVertex> mv;
            mv.reserve(verts.size());
            for (const auto& bv : verts) {
                dse::render::MeshVertex v;
                v.position = bv.pos;
                v.color = bv.color;
                v.uv = bv.uv;
                v.normal = bv.normal;
                v.tangent = bv.tangent;
                mv.push_back(v);
            }
            std::vector<uint16_t> idx16;
            idx16.reserve(idxs.size());
            for (uint32_t ix : idxs) idx16.push_back(static_cast<uint16_t>(ix));

            dse::render::ShadedMaterial material;
            material.albedo = glm::vec3(1.0f);
            material.metallic = 0.0f;
            material.roughness = 0.85f;
            material.ao = 1.0f;
            material.double_sided = true;
            material.shading_mode = 0;
            material.albedo_tex = grass.albedo_texture;
            material.receive_shadow = true;
            material.shadow_strength = shadow_strength_val;

            dse::render::DirectionalLight light;
            light.direction = light_dir;
            light.color = light_color;
            light.intensity = light_intensity;
            light.ambient = ambient_intensity;
            light.enabled = true;

            mesh_renderer_.DrawInstancedShaded(
                cmd_buffer, *rhi_, mv, idx16, instances,
                draw_view, draw_proj, draw_cam_pos, material, light);
        };

        submit_batch(lod0_matrices, blade_colored, blade_indices_);
        if (!shadow_pass) {
            submit_batch(lod1_matrices, billboard_colored, billboard_indices_);
        }
    }
}

} // namespace gameplay3d
} // namespace dse
