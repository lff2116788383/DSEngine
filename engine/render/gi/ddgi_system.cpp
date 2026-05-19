#include "engine/render/gi/ddgi_system.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include <cstring>
#include <cmath>

namespace dse {
namespace render {
namespace gi {

// ============================================================================
// DDGI Probe Update Compute Shader (GLSL 430)
// 从 RSM (Reflective Shadow Map) VPL 累积间接辐照度到 Probe Atlas
// ============================================================================

static const char* kDDGIUpdateComputeSource = R"(
#version 430 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Probe irradiance atlas (read/write)
layout(rgba16f, binding = 0) uniform image2D u_irradiance_atlas;

// Probe visibility atlas (read/write)
layout(rg16f, binding = 1) uniform image2D u_visibility_atlas;

// RSM textures (read-only samplers)
layout(binding = 0) uniform sampler2D u_rsm_position;
layout(binding = 1) uniform sampler2D u_rsm_normal;
layout(binding = 2) uniform sampler2D u_rsm_flux;

// Probe states SSBO
layout(std430, binding = 0) readonly buffer ProbeStates {
    vec4 probe_position_status[];  // xyz=pos, w=active
};

// Push constants / uniforms
uniform int u_probe_count;
uniform int u_probe_start;         // 级联轮转起始索引
uniform int u_probes_to_update;    // 本帧更新数量
uniform int u_irradiance_texels;   // 每探针 texel 数 (含边界)
uniform int u_visibility_texels;
uniform ivec3 u_grid_resolution;
uniform vec3 u_grid_origin;
uniform vec3 u_grid_spacing;
uniform vec3 u_light_dir;          // 归一化方向光方向 (toward light)
uniform vec3 u_light_color;        // 方向光颜色 * 强度
uniform float u_hysteresis;        // temporal blending factor
uniform int u_rsm_width;
uniform int u_rsm_height;
uniform int u_frame_index;

// Octahedral encoding
vec2 oct_encode(vec3 n) {
    float sum = abs(n.x) + abs(n.y) + abs(n.z);
    vec2 oct = n.xy / sum;
    if (n.z < 0.0) {
        oct = (1.0 - abs(oct.yx)) * sign(oct.xy);
    }
    return oct * 0.5 + 0.5;
}

vec3 oct_decode(vec2 uv) {
    vec2 f = uv * 2.0 - 1.0;
    vec3 n = vec3(f.xy, 1.0 - abs(f.x) - abs(f.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return normalize(n);
}

// 伪随机数 (基于 probe index + frame)
float hash(uint seed) {
    seed = seed * 747796405u + 2891336453u;
    seed = ((seed >> 16u) ^ seed) * 0x45d9f3bu;
    seed = ((seed >> 16u) ^ seed) * 0x45d9f3bu;
    seed = (seed >> 16u) ^ seed;
    return float(seed) / 4294967295.0;
}

void main() {
    // 每个 workgroup 处理一个探针的一行 texel
    // gl_GlobalInvocationID.x = texel_x (0..irradiance_texels-1)
    // gl_GlobalInvocationID.y = probe_local_index (0..probes_to_update-1)
    int texel_x = int(gl_GlobalInvocationID.x);
    int probe_local = int(gl_GlobalInvocationID.y);

    if (texel_x >= u_irradiance_texels) return;
    if (probe_local >= u_probes_to_update) return;

    int probe_index = (u_probe_start + probe_local) % u_probe_count;

    vec4 probe_data = probe_position_status[probe_index];
    vec3 probe_pos = probe_data.xyz;
    float probe_active = probe_data.w;

    if (probe_active < 0.5) return;

    // 计算 atlas 中此探针的像素偏移
    int grid_x = probe_index % u_grid_resolution.x;
    int grid_y = (probe_index / u_grid_resolution.x) % u_grid_resolution.y;
    int grid_z = probe_index / (u_grid_resolution.x * u_grid_resolution.y);
    int atlas_col = grid_x + grid_z * u_grid_resolution.x;
    int atlas_row = grid_y;

    // 对 irradiance atlas 的每一行 texel 进行处理
    // 需要遍历所有 texel_y 行，但这里我们用 texel_x 作为列
    // 每个 invocation 处理一个 (texel_x, texel_y) 对
    // 重新解释: gl_GlobalInvocationID.z 未使用，我们迭代 texel_y
    for (int texel_y = 0; texel_y < u_irradiance_texels; ++texel_y) {
        // 该 texel 对应的球面方向
        vec2 texel_uv = (vec2(texel_x, texel_y) + 0.5) / float(u_irradiance_texels);
        vec3 texel_dir = oct_decode(texel_uv);

        // 从 RSM 随机采样 VPL，累积间接辐照度
        vec3 irradiance = vec3(0.0);
        float total_weight = 0.0;
        float visibility_depth = 0.0;
        float visibility_depth2 = 0.0;

        uint rng_seed = uint(probe_index * 1031 + u_frame_index * 7919 + texel_x * 113 + texel_y * 37);
        int num_samples = min(64, u_rsm_width * u_rsm_height);

        for (int s = 0; s < num_samples; ++s) {
            // 随机 RSM 像素坐标
            float r1 = hash(rng_seed + uint(s * 2));
            float r2 = hash(rng_seed + uint(s * 2 + 1));
            ivec2 rsm_coord = ivec2(
                int(r1 * float(u_rsm_width)),
                int(r2 * float(u_rsm_height))
            );
            rsm_coord = clamp(rsm_coord, ivec2(0), ivec2(u_rsm_width - 1, u_rsm_height - 1));

            vec2 rsm_uv = (vec2(rsm_coord) + 0.5) / vec2(u_rsm_width, u_rsm_height);

            // 读取 RSM 数据
            vec3 vpl_pos = texture(u_rsm_position, rsm_uv).xyz;
            vec3 vpl_normal = normalize(texture(u_rsm_normal, rsm_uv).xyz * 2.0 - 1.0);
            vec3 vpl_flux = texture(u_rsm_flux, rsm_uv).rgb;

            // 跳过无效 VPL
            if (dot(vpl_flux, vpl_flux) < 1e-6) continue;

            // VPL → Probe 的方向和距离
            vec3 to_probe = probe_pos - vpl_pos;
            float dist = length(to_probe);
            if (dist < 0.01) continue;
            vec3 dir_to_probe = to_probe / dist;

            // VPL 是漫反射发射：cos(vpl_normal, dir_to_probe) 权重
            float vpl_cos = max(0.0, dot(vpl_normal, dir_to_probe));
            if (vpl_cos < 1e-4) continue;

            // 接收方向权重：cos(texel_dir, -dir_to_probe)
            // 即探针该方向接收来自 VPL 的辐照度
            float receive_cos = max(0.0, dot(texel_dir, -dir_to_probe));
            if (receive_cos < 1e-4) continue;

            // 平方衰减
            float attenuation = 1.0 / (dist * dist + 1.0);

            // VPL 贡献 = flux * cos_vpl * cos_receive * attenuation
            float weight = vpl_cos * receive_cos * attenuation;
            irradiance += vpl_flux * weight;
            total_weight += weight;

            // 可见性（记录 VPL 到 probe 的距离统计）
            visibility_depth += dist * weight;
            visibility_depth2 += dist * dist * weight;
        }

        // 归一化
        if (total_weight > 1e-6) {
            irradiance /= total_weight;
            visibility_depth /= total_weight;
            visibility_depth2 /= total_weight;
        }

        // RSM 覆盖面积归一化因子
        float rsm_area_factor = float(u_rsm_width * u_rsm_height) / float(num_samples);
        irradiance *= rsm_area_factor * 0.01;  // 经验缩放

        // Temporal blending with existing atlas value
        ivec2 atlas_texel = ivec2(
            atlas_col * u_irradiance_texels + texel_x,
            atlas_row * u_irradiance_texels + texel_y
        );

        vec4 prev_irr = imageLoad(u_irradiance_atlas, atlas_texel);
        vec3 blended_irr = mix(irradiance, prev_irr.rgb, u_hysteresis);
        imageStore(u_irradiance_atlas, atlas_texel, vec4(blended_irr, 1.0));

        // Visibility atlas (skip if texel exceeds visibility resolution)
        if (texel_x >= u_visibility_texels || texel_y >= u_visibility_texels) continue;
        ivec2 vis_texel = ivec2(
            atlas_col * u_visibility_texels + texel_x,
            atlas_row * u_visibility_texels + texel_y
        );
        vec2 prev_vis = imageLoad(u_visibility_atlas, vis_texel).rg;
        vec2 new_vis = vec2(visibility_depth, visibility_depth2);
        vec2 blended_vis = mix(new_vis, prev_vis, u_hysteresis);
        imageStore(u_visibility_atlas, vis_texel, vec4(blended_vis, 0.0, 0.0));
    }
}
)";

// ============================================================================
// DDGI Probe Update Compute Shader (Vulkan GLSL 450)
// 与 GL 430 版本算法相同，改用 push constant 标量块 + 显式 descriptor set 绑定
// binding layout (与 CreateComputeShaderEx ssbo=1,img=2,smp=3 对应):
//   binding 0 = SSBO (ProbeStates)
//   binding 1 = storage image irradiance atlas
//   binding 2 = storage image visibility atlas
//   binding 3 = sampler rsm_position
//   binding 4 = sampler rsm_normal
//   binding 5 = sampler rsm_flux
// push constant 96B (24 scalars, 无 vec3/ivec3, 与 CPU 顺序完全一致)
// ============================================================================

static const char* kDDGIUpdateComputeSourceVK = R"(
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set=0, binding=0, std430) readonly buffer ProbeStates {
    vec4 probe_position_status[];
};
layout(set=0, binding=1, rgba16f) uniform image2D u_irradiance_atlas;
layout(set=0, binding=2, rg16f)   uniform image2D u_visibility_atlas;
layout(set=0, binding=3) uniform sampler2D u_rsm_position;
layout(set=0, binding=4) uniform sampler2D u_rsm_normal;
layout(set=0, binding=5) uniform sampler2D u_rsm_flux;

layout(push_constant) uniform PC {
    int   u_probe_count;
    int   u_probe_start;
    int   u_probes_to_update;
    int   u_irradiance_texels;
    int   u_visibility_texels;
    int   u_rsm_width;
    int   u_rsm_height;
    int   u_frame_index;
    float u_hysteresis;
    int   u_grid_res_x;
    int   u_grid_res_y;
    int   u_grid_res_z;
    float u_grid_origin_x;
    float u_grid_origin_y;
    float u_grid_origin_z;
    float u_grid_spacing_x;
    float u_grid_spacing_y;
    float u_grid_spacing_z;
    float u_light_dir_x;
    float u_light_dir_y;
    float u_light_dir_z;
    float u_light_color_x;
    float u_light_color_y;
    float u_light_color_z;
} pc;

vec2 oct_encode(vec3 n) {
    float s = abs(n.x) + abs(n.y) + abs(n.z);
    vec2 o = n.xy / s;
    if (n.z < 0.0) o = (1.0 - abs(o.yx)) * sign(o.xy);
    return o * 0.5 + 0.5;
}
vec3 oct_decode(vec2 uv) {
    vec2 f = uv * 2.0 - 1.0;
    vec3 n = vec3(f.xy, 1.0 - abs(f.x) - abs(f.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}
float hash_u(uint seed) {
    seed = seed * 747796405u + 2891336453u;
    seed = ((seed >> 16u) ^ seed) * 0x45d9f3bu;
    seed = ((seed >> 16u) ^ seed) * 0x45d9f3bu;
    return float((seed >> 16u) ^ seed) / 4294967295.0;
}

void main() {
    int texel_x     = int(gl_GlobalInvocationID.x);
    int probe_local = int(gl_GlobalInvocationID.y);
    ivec3 grid_res  = ivec3(pc.u_grid_res_x, pc.u_grid_res_y, pc.u_grid_res_z);
    if (texel_x >= pc.u_irradiance_texels) return;
    if (probe_local >= pc.u_probes_to_update) return;
    int probe_index = (pc.u_probe_start + probe_local) % pc.u_probe_count;
    vec4 probe_data = probe_position_status[probe_index];
    if (probe_data.w < 0.5) return;
    vec3 probe_pos = probe_data.xyz;
    int grid_x = probe_index % grid_res.x;
    int grid_y = (probe_index / grid_res.x) % grid_res.y;
    int grid_z = probe_index / (grid_res.x * grid_res.y);
    int atlas_col = grid_x + grid_z * grid_res.x;
    int atlas_row = grid_y;
    int num_samples = min(64, pc.u_rsm_width * pc.u_rsm_height);
    for (int texel_y = 0; texel_y < pc.u_irradiance_texels; ++texel_y) {
        vec3 texel_dir = oct_decode((vec2(texel_x, texel_y) + 0.5) / float(pc.u_irradiance_texels));
        vec3 irradiance = vec3(0.0);
        float total_w = 0.0, vd = 0.0, vd2 = 0.0;
        uint seed = uint(probe_index * 1031 + pc.u_frame_index * 7919 + texel_x * 113 + texel_y * 37);
        for (int s = 0; s < num_samples; ++s) {
            vec2 rsm_uv = vec2(hash_u(seed + uint(s*2)), hash_u(seed + uint(s*2+1)));
            vec3 vpl_pos    = texture(u_rsm_position, rsm_uv).xyz;
            vec3 vpl_normal = normalize(texture(u_rsm_normal, rsm_uv).xyz * 2.0 - 1.0);
            vec3 vpl_flux   = texture(u_rsm_flux, rsm_uv).rgb;
            if (dot(vpl_flux, vpl_flux) < 1e-6) continue;
            vec3 to_probe = probe_pos - vpl_pos;
            float dist = length(to_probe);
            if (dist < 0.01) continue;
            vec3 dir = to_probe / dist;
            float cos_vpl = max(0.0, dot(vpl_normal, dir));
            float cos_rcv = max(0.0, dot(texel_dir, -dir));
            if (cos_vpl < 1e-4 || cos_rcv < 1e-4) continue;
            float att = 1.0 / (dist * dist + 1.0);
            float w = cos_vpl * cos_rcv * att;
            irradiance += vpl_flux * w;
            total_w += w; vd += dist * w; vd2 += dist * dist * w;
        }
        if (total_w > 1e-6) { irradiance /= total_w; vd /= total_w; vd2 /= total_w; }
        irradiance *= float(pc.u_rsm_width * pc.u_rsm_height) / float(num_samples) * 0.01;
        ivec2 at = ivec2(atlas_col * pc.u_irradiance_texels + texel_x,
                         atlas_row * pc.u_irradiance_texels + texel_y);
        vec4 prev = imageLoad(u_irradiance_atlas, at);
        imageStore(u_irradiance_atlas, at, vec4(mix(irradiance, prev.rgb, pc.u_hysteresis), 1.0));
        if (texel_x < pc.u_visibility_texels && texel_y < pc.u_visibility_texels) {
            ivec2 vt = ivec2(atlas_col * pc.u_visibility_texels + texel_x,
                             atlas_row * pc.u_visibility_texels + texel_y);
            vec2 pv = imageLoad(u_visibility_atlas, vt).rg;
            imageStore(u_visibility_atlas, vt,
                vec4(mix(vec2(vd, vd2), pv, pc.u_hysteresis), 0.0, 0.0));
        }
    }
}
)";

// ============================================================================
// DDGI Probe Update Compute Shader (HLSL CS 5.0 for D3D11)
// cbuffer b0 顺序与 SetComputeUniform* 调用顺序一致
// t0-t2 = RSM textures, t16 = ProbeStates SSBO, u0-u1 = Write targets
// ============================================================================

static const char* kDDGIUpdateComputeSourceHLSL = R"(
cbuffer Params : register(b0) {
    int   u_probe_count;
    int   u_probe_start;
    int   u_probes_to_update;
    int   u_irradiance_texels;
    int   u_visibility_texels;
    int   u_rsm_width;
    int   u_rsm_height;
    int   u_frame_index;
    float u_hysteresis;
    int   u_grid_res_x;
    int   u_grid_res_y;
    int   u_grid_res_z;
    float u_grid_origin_x;
    float u_grid_origin_y;
    float u_grid_origin_z;
    float u_grid_spacing_x;
    float u_grid_spacing_y;
    float u_grid_spacing_z;
    float u_light_dir_x;
    float u_light_dir_y;
    float u_light_dir_z;
    float u_light_color_x;
    float u_light_color_y;
    float u_light_color_z;
};

Texture2D<float4> u_rsm_position  : register(t0);
Texture2D<float4> u_rsm_normal    : register(t1);
Texture2D<float4> u_rsm_flux      : register(t2);
SamplerState LinearSampler        : register(s0);

struct ProbeState { float4 pos_status; };
StructuredBuffer<ProbeState> probe_states : register(t16);

RWTexture2D<float4> u_irradiance_atlas : register(u0);
RWTexture2D<float4> u_visibility_atlas : register(u1);

float2 oct_encode(float3 n) {
    float s = abs(n.x) + abs(n.y) + abs(n.z);
    float2 o = n.xy / s;
    if (n.z < 0.0) o = (1.0 - abs(o.yx)) * sign(o.xy);
    return o * 0.5 + 0.5;
}
float3 oct_decode(float2 uv) {
    float2 f = uv * 2.0 - 1.0;
    float3 n = float3(f.xy, 1.0 - abs(f.x) - abs(f.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}
float hash_u(uint seed) {
    seed = seed * 747796405u + 2891336453u;
    seed = ((seed >> 16u) ^ seed) * 0x45d9f3bu;
    seed = ((seed >> 16u) ^ seed) * 0x45d9f3bu;
    return float((seed >> 16u) ^ seed) / 4294967295.0;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    int texel_x     = (int)id.x;
    int probe_local = (int)id.y;
    int3 grid_res   = int3(u_grid_res_x, u_grid_res_y, u_grid_res_z);
    if (texel_x >= u_irradiance_texels || probe_local >= u_probes_to_update) return;
    int probe_index = (u_probe_start + probe_local) % u_probe_count;
    float4 probe_data = probe_states[probe_index].pos_status;
    if (probe_data.w < 0.5) return;
    float3 probe_pos = probe_data.xyz;
    int grid_x = probe_index % grid_res.x;
    int grid_y = (probe_index / grid_res.x) % grid_res.y;
    int grid_z = probe_index / (grid_res.x * grid_res.y);
    int atlas_col = grid_x + grid_z * grid_res.x;
    int atlas_row = grid_y;
    int num_samples = min(64, u_rsm_width * u_rsm_height);
    for (int texel_y = 0; texel_y < u_irradiance_texels; ++texel_y) {
        float3 texel_dir = oct_decode((float2(texel_x, texel_y) + 0.5) / (float)u_irradiance_texels);
        float3 irradiance = (float3)0;
        float total_w = 0, vd = 0, vd2 = 0;
        uint seed = (uint)(probe_index*1031 + u_frame_index*7919 + texel_x*113 + texel_y*37);
        for (int s = 0; s < num_samples; ++s) {
            float2 uv = float2(hash_u(seed+(uint)(s*2)), hash_u(seed+(uint)(s*2+1)));
            float3 vpl_pos    = u_rsm_position.SampleLevel(LinearSampler, uv, 0).xyz;
            float3 vpl_normal = normalize(u_rsm_normal.SampleLevel(LinearSampler, uv, 0).xyz * 2.0 - 1.0);
            float3 vpl_flux   = u_rsm_flux.SampleLevel(LinearSampler, uv, 0).rgb;
            if (dot(vpl_flux, vpl_flux) < 1e-6) continue;
            float3 to_probe = probe_pos - vpl_pos;
            float dist = length(to_probe);
            if (dist < 0.01) continue;
            float3 dir = to_probe / dist;
            float cv = max(0.0, dot(vpl_normal, dir));
            float cr = max(0.0, dot(texel_dir, -dir));
            if (cv < 1e-4 || cr < 1e-4) continue;
            float w = cv * cr / (dist * dist + 1.0);
            irradiance += vpl_flux * w;
            total_w += w; vd += dist * w; vd2 += dist * dist * w;
        }
        if (total_w > 1e-6) { irradiance /= total_w; vd /= total_w; vd2 /= total_w; }
        irradiance *= (float)(u_rsm_width * u_rsm_height) / (float)num_samples * 0.01;
        int2 at = int2(atlas_col * u_irradiance_texels + texel_x,
                       atlas_row * u_irradiance_texels + texel_y);
        float4 prev = u_irradiance_atlas[at];
        u_irradiance_atlas[at] = float4(lerp(irradiance, prev.rgb, u_hysteresis), 1.0);
        if (texel_x < u_visibility_texels && texel_y < u_visibility_texels) {
            int2 vt = int2(atlas_col * u_visibility_texels + texel_x,
                           atlas_row * u_visibility_texels + texel_y);
            float2 pv = u_visibility_atlas[vt].rg;
            u_visibility_atlas[vt] = float4(lerp(float2(vd, vd2), pv, u_hysteresis), 0.0, 0.0);
        }
    }
}
)";

// ============================================================================
// DDGISystem 实现
// ============================================================================

bool DDGISystem::Init(RhiDevice* rhi, const DDGIVolumeConfig& config) {
    if (!rhi) return false;
    if (!rhi->SupportsCompute()) {
        DEBUG_LOG_WARN("[DDGI] Compute shader not supported, DDGI disabled");
        return false;
    }

    config_ = config;

    if (!CreateAtlasTextures(rhi)) return false;
    if (!CreateComputeShader(rhi)) return false;
    InitProbeStates(rhi);

    // 每帧更新的探针数：总数/8（~8帧更新一轮），最小 1
    int total = config_.TotalProbeCount();
    probes_per_frame_ = std::max(1, total / 8);

    resources_.initialized = true;
    DEBUG_LOG_INFO("[DDGI] Initialized: {}x{}x{} probes ({} total), atlas {}x{}, {}/frame update",
                  config_.resolution.x, config_.resolution.y, config_.resolution.z,
                  total,
                  config_.IrradianceAtlasSize().x, config_.IrradianceAtlasSize().y,
                  probes_per_frame_);
    return true;
}

void DDGISystem::Shutdown(RhiDevice* rhi) {
    if (!rhi) return;

    if (resources_.irradiance_atlas != 0) {
        rhi->DeleteTexture(resources_.irradiance_atlas);
        resources_.irradiance_atlas = 0;
    }
    if (resources_.visibility_atlas != 0) {
        rhi->DeleteTexture(resources_.visibility_atlas);
        resources_.visibility_atlas = 0;
    }
    if (resources_.probe_state_ssbo != 0) {
        rhi->DeleteSSBO(resources_.probe_state_ssbo);
        resources_.probe_state_ssbo = 0;
    }
    if (resources_.update_compute_shader != 0) {
        rhi->DeleteComputeShader(resources_.update_compute_shader);
        resources_.update_compute_shader = 0;
    }

    resources_.initialized = false;
    current_update_offset_ = 0;
    frame_counter_ = 0;
}

void DDGISystem::Reconfigure(RhiDevice* rhi, const DDGIVolumeConfig& config) {
    Shutdown(rhi);
    Init(rhi, config);
}

void DDGISystem::EnsureRSMResources(RhiDevice* rhi) {
    if (!rhi || !resources_.initialized) return;

    // RSM RTs 由 RenderPassContext 管理（在 FramePipeline 中创建）
    // 这里只做存在性检查
}

void DDGISystem::UpdateProbes(RhiDevice* rhi,
                               unsigned int rsm_position, unsigned int rsm_normal, unsigned int rsm_flux,
                               int rsm_width, int rsm_height,
                               const glm::vec3& light_dir, const glm::vec3& light_color) {
    if (!rhi || !resources_.initialized) return;
    if (resources_.update_compute_shader == 0) return;
    if (resources_.irradiance_atlas == 0 || resources_.visibility_atlas == 0) return;
    if (rsm_width <= 0 || rsm_height <= 0) return;
    if (rsm_position == 0 || rsm_normal == 0 || rsm_flux == 0) return;

    int total_probes = config_.TotalProbeCount();
    int probes_this_frame = std::min(probes_per_frame_, total_probes);

    // Bind resources
    rhi->SetComputeTextureImage(0, resources_.irradiance_atlas, false);
    rhi->SetComputeTextureImage(1, resources_.visibility_atlas, false);

    // RSM textures as samplers (externally managed by FramePipeline)
    rhi->SetComputeTextureSampler(0, rsm_position);
    rhi->SetComputeTextureSampler(1, rsm_normal);
    rhi->SetComputeTextureSampler(2, rsm_flux);

    // Probe state SSBO
    rhi->BindSSBO(resources_.probe_state_ssbo, 0);

    // Set uniforms
    unsigned int shader = resources_.update_compute_shader;
    rhi->SetComputeUniformInt(shader, "u_probe_count", total_probes);
    rhi->SetComputeUniformInt(shader, "u_probe_start", current_update_offset_);
    rhi->SetComputeUniformInt(shader, "u_probes_to_update", probes_this_frame);
    rhi->SetComputeUniformInt(shader, "u_irradiance_texels", config_.irradiance_texels);
    rhi->SetComputeUniformInt(shader, "u_visibility_texels", config_.visibility_texels);
    rhi->SetComputeUniformInt(shader, "u_rsm_width", rsm_width);
    rhi->SetComputeUniformInt(shader, "u_rsm_height", rsm_height);
    rhi->SetComputeUniformInt(shader, "u_frame_index", frame_counter_);
    rhi->SetComputeUniformFloat(shader, "u_hysteresis", config_.hysteresis);

    // grid 参数
    glm::vec3 spacing = config_.ProbeSpacing();
    rhi->SetComputeUniformIVec3(shader, "u_grid_resolution",
                                 config_.resolution.x, config_.resolution.y, config_.resolution.z);
    rhi->SetComputeUniformVec3(shader, "u_grid_origin",
                                config_.origin.x, config_.origin.y, config_.origin.z);
    rhi->SetComputeUniformVec3(shader, "u_grid_spacing",
                                spacing.x, spacing.y, spacing.z);
    rhi->SetComputeUniformVec3(shader, "u_light_dir",
                                light_dir.x, light_dir.y, light_dir.z);
    rhi->SetComputeUniformVec3(shader, "u_light_color",
                                light_color.x, light_color.y, light_color.z);

    // Dispatch: X = irradiance_texels, Y = probes_this_frame
    unsigned int groups_x = static_cast<unsigned int>((config_.irradiance_texels + 7) / 8);
    unsigned int groups_y = static_cast<unsigned int>((probes_this_frame + 7) / 8);
    rhi->DispatchCompute(shader, groups_x, groups_y, 1);
    rhi->ComputeMemoryBarrier();

    // 推进级联轮转
    current_update_offset_ = (current_update_offset_ + probes_this_frame) % total_probes;
    frame_counter_++;
}

void DDGISystem::BindForSampling(RhiDevice* rhi, unsigned int irradiance_unit,
                                  unsigned int visibility_unit) const {
    if (!rhi || !resources_.initialized) return;
    if (resources_.irradiance_atlas != 0) {
        rhi->SetComputeTextureSampler(irradiance_unit, resources_.irradiance_atlas);
    }
    if (resources_.visibility_atlas != 0) {
        rhi->SetComputeTextureSampler(visibility_unit, resources_.visibility_atlas);
    }
}

// ============================================================================
// 私有方法
// ============================================================================

bool DDGISystem::CreateAtlasTextures(RhiDevice* rhi) {
    glm::ivec2 irr_size = config_.IrradianceAtlasSize();
    glm::ivec2 vis_size = config_.VisibilityAtlasSize();

    // 创建可供 compute shader 写入的 atlas（含 storage image / UAV 标志）
    resources_.irradiance_atlas = rhi->CreateComputeWriteTexture2D(irr_size.x, irr_size.y);
    if (resources_.irradiance_atlas == 0) {
        DEBUG_LOG_ERROR("[DDGI] Failed to create irradiance atlas ({}x{})", irr_size.x, irr_size.y);
        return false;
    }

    resources_.visibility_atlas = rhi->CreateComputeWriteTexture2D(vis_size.x, vis_size.y);
    if (resources_.visibility_atlas == 0) {
        DEBUG_LOG_ERROR("[DDGI] Failed to create visibility atlas ({}x{})", vis_size.x, vis_size.y);
        return false;
    }

    return true;
}

bool DDGISystem::CreateComputeShader(RhiDevice* rhi) {
    // ssbo=1(ProbeStates), img=2(irradiance+visibility), smp=3(RSM×3), pc=96B
    resources_.update_compute_shader = rhi->CreateComputeShaderEx(
        kDDGIUpdateComputeSource,
        kDDGIUpdateComputeSourceVK,
        kDDGIUpdateComputeSourceHLSL,
        /*ssbo_count=*/1,
        /*storage_image_count=*/2,
        /*sampler_count=*/3,
        /*push_constant_bytes=*/96);
    if (resources_.update_compute_shader == 0) {
        DEBUG_LOG_ERROR("[DDGI] Failed to compile probe update compute shader");
        return false;
    }
    return true;
}

void DDGISystem::InitProbeStates(RhiDevice* rhi) {
    int total = config_.TotalProbeCount();
    std::vector<ProbeState> states(total);

    for (int i = 0; i < total; ++i) {
        glm::vec3 pos = config_.ProbePosition(i);
        states[i].position_and_status = glm::vec4(pos, 1.0f);  // 默认全部激活
    }

    size_t byte_size = total * sizeof(ProbeState);
    resources_.probe_state_ssbo = rhi->CreateSSBO(byte_size, states.data());
    if (resources_.probe_state_ssbo == 0) {
        DEBUG_LOG_ERROR("[DDGI] Failed to create probe state SSBO ({} probes)", total);
    }
}

} // namespace gi
} // namespace render
} // namespace dse
