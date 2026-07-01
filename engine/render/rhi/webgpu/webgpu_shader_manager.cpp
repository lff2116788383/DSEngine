/**
 * @file webgpu_shader_manager.cpp
 * @brief WebGPUShaderManager 实现（机械抽自 webgpu_rhi_device.cpp）。
 *
 * 内建 WGSL 程序常量 / GPU-driven PBR WGSL 常量随原匿名命名空间块整体迁移至本 TU
 * （内部链接，仅本 manager 消费）；方法体经 gen.py 机械抽取，Shutdown 为手写编排片段。
 */

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/webgpu/webgpu_shader_manager.h"

#include "engine/base/debug.h"
#include "engine/render/shaders/wgsl/postprocess_wgsl.h"

#include <cstring>

namespace dse {
namespace render {

void WebGPUShaderManager::Shutdown() {
    for (auto& [h, e] : shaders_) { (void)h; if (e.module) wgpuShaderModuleRelease(e.module); }
    shaders_.clear();
    for (auto& [h, e] : compute_shaders_) { (void)h; if (e.module) wgpuShaderModuleRelease(e.module); }
    compute_shaders_.clear();
}

namespace {

// 全屏 quad 直拷（copy/passthrough/fxaa 等）：源纹理在 slot0 → group2 binding0/1。
// 顶点来自 PostProcessRenderer 的 PPVertex：pos(vec2)@0、uv(vec2)@1（clip-space）。
const char* kWgslFullscreenBlit = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);  // GL 风格全屏 quad 在 WebGPU(top-left 纹理原点)需翻转 V
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  return textureSample(src_tex, src_smp, i.uv);
}
)WGSL";

// 全屏合成（bloom_composite/tonemapping/ssao_apply）：采样 HDR 场景 → Reinhard tonemap + sRGB。
const char* kWgslComposite = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);  // GL 风格全屏 quad 在 WebGPU(top-left 纹理原点)需翻转 V
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let hdr = textureSample(src_tex, src_smp, i.uv).rgb;
  let mapped = hdr / (hdr + vec3<f32>(1.0));
  let srgb = pow(mapped, vec3<f32>(1.0 / 2.2));
  return vec4<f32>(srgb, 1.0);
}
)WGSL";

// ============================================================
// B-2 HDR-bloom 链（WebGPU 全屏 quad，镜像 engine/render/shaders/src/bloom_*.frag）
// ============================================================
// 渲染器（PostProcessRenderer/BloomRenderer）在无 compute 后端走全屏 quad：
//   bloom_extract → bloom_downsample×N → bloom_upsample×N → bloom_composite。
// 源纹理一律在 slot0 → group2 binding0/1；标量参数经 FS push（cmd.PushConstants Fragment）
//   → group0 binding1 的 uniform。各 PP 程序逐句镜像对应 .frag。V 翻转与 kWgslComposite 一致：
//   每趟均为「翻转 = 保向拷贝」，故整链方向一致、bloom 与场景在 composite 对齐。

// Bloom 亮度提取（镜像 bloom_extract.frag）：软膝阈值仅保留高光。参数 {threshold, knee}。
const char* kWgslBloomExtract = R"WGSL(// dse-wgsl
struct PC { p : vec4<f32> };  // p.x=threshold, p.y=knee
@group(0) @binding(1) var<uniform> pc : PC;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let color = textureSample(src_tex, src_smp, i.uv).rgb;
  let brightness = dot(color, vec3<f32>(0.2126, 0.7152, 0.0722));
  var soft = brightness - (pc.p.x - pc.p.y);
  soft = clamp(soft / (2.0 * pc.p.y + 0.0001), 0.0, 1.0);
  soft = soft * soft;
  let contribution = max(soft, step(pc.p.x, brightness));
  return vec4<f32>(color * contribution, 1.0);
}
)WGSL";

// Bloom 13-tap 降采样（镜像 bloom_downsample.frag）。参数 {srcResX, srcResY}。
const char* kWgslBloomDownsample = R"WGSL(// dse-wgsl
struct PC { p : vec4<f32> };  // p.x=srcResX, p.y=srcResY
@group(0) @binding(1) var<uniform> pc : PC;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
fn S(uv : vec2<f32>) -> vec3<f32> { return textureSample(src_tex, src_smp, uv).rgb; }
@fragment fn fs_main(inp : VsOut) -> @location(0) vec4<f32> {
  let x = 1.0 / pc.p.x;
  let y = 1.0 / pc.p.y;
  let uv = inp.uv;
  let a = S(vec2<f32>(uv.x - 2.0*x, uv.y + 2.0*y));
  let b = S(vec2<f32>(uv.x,         uv.y + 2.0*y));
  let c = S(vec2<f32>(uv.x + 2.0*x, uv.y + 2.0*y));
  let d = S(vec2<f32>(uv.x - 2.0*x, uv.y));
  let e = S(vec2<f32>(uv.x,         uv.y));
  let f = S(vec2<f32>(uv.x + 2.0*x, uv.y));
  let g = S(vec2<f32>(uv.x - 2.0*x, uv.y - 2.0*y));
  let h = S(vec2<f32>(uv.x,         uv.y - 2.0*y));
  let ii = S(vec2<f32>(uv.x + 2.0*x, uv.y - 2.0*y));
  let j = S(vec2<f32>(uv.x - x, uv.y + y));
  let k = S(vec2<f32>(uv.x + x, uv.y + y));
  let l = S(vec2<f32>(uv.x - x, uv.y - y));
  let m = S(vec2<f32>(uv.x + x, uv.y - y));
  var ds = e * 0.125;
  ds = ds + (a + c + g + ii) * 0.03125;
  ds = ds + (b + d + f + h) * 0.0625;
  ds = ds + (j + k + l + m) * 0.125;
  return vec4<f32>(ds, 1.0);
}
)WGSL";

// Bloom 3x3 帐篷升采样（镜像 bloom_upsample.frag）。参数 {filterRadius, blendWeight}；
// 由调用方（BloomRenderer::Upsample）开 alpha 混合累加。
const char* kWgslBloomUpsample = R"WGSL(// dse-wgsl
struct PC { p : vec4<f32> };  // p.x=filterRadius, p.y=blendWeight
@group(0) @binding(1) var<uniform> pc : PC;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
fn S(uv : vec2<f32>) -> vec3<f32> { return textureSample(src_tex, src_smp, uv).rgb; }
@fragment fn fs_main(inp : VsOut) -> @location(0) vec4<f32> {
  let x = pc.p.x;
  let y = pc.p.x;
  let uv = inp.uv;
  let a = S(vec2<f32>(uv.x - x, uv.y + y));
  let b = S(vec2<f32>(uv.x,     uv.y + y));
  let c = S(vec2<f32>(uv.x + x, uv.y + y));
  let d = S(vec2<f32>(uv.x - x, uv.y));
  let e = S(vec2<f32>(uv.x,     uv.y));
  let f = S(vec2<f32>(uv.x + x, uv.y));
  let g = S(vec2<f32>(uv.x - x, uv.y - y));
  let h = S(vec2<f32>(uv.x,     uv.y - y));
  let ii = S(vec2<f32>(uv.x + x, uv.y - y));
  var us = e * 4.0;
  us = us + (b + d + f + h) * 2.0;
  us = us + (a + c + g + ii);
  us = us * (1.0 / 16.0);
  return vec4<f32>(us * pc.p.y, pc.p.y);
}
)WGSL";

// Bloom 合成（镜像 bloom_composite_ssao_ae.frag 的 demo 子集）：场景 + bloom*intensity →
// ACES(色*exposure) → gamma → 可选 vignette → IGN dither。SSAO/AE/LUT/CS/film-grain 在 demo
// 全关，故不声明其纹理绑定（声明未绑定纹理会被 GetOrCreateRenderPipeline 判缺而跳过 draw）。
// 场景在 slot0 → group2 binding0/1；bloom 经 req.Tex(2) → slot1 → group2 binding2/3。
const char* kWgslBloomComposite = R"WGSL(// dse-wgsl
struct PC {
  p0 : vec4<f32>,  // exposure, bloomIntensity, bloomEnabled, ssaoEnabled
  p1 : vec4<f32>,  // autoExposureEnabled, lutEnabled, lutIntensity, csEnabled
  p2 : vec4<f32>,  // csStrength, vignetteEnabled, vignetteIntensity, vignetteRadius
  p3 : vec4<f32>,  // vignetteSoftness, filmGrainEnabled, filmGrainIntensity, filmGrainTime
};
@group(0) @binding(1) var<uniform> pc : PC;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);
  return o;
}
@group(2) @binding(0) var scene_tex : texture_2d<f32>;
@group(2) @binding(1) var scene_smp : sampler;
@group(2) @binding(2) var bloom_tex : texture_2d<f32>;
@group(2) @binding(3) var bloom_smp : sampler;
fn AcesFilmic(v : vec3<f32>) -> vec3<f32> {
  let a = 2.51; let b = 0.03; let c = 2.43; let d = 0.59; let e = 0.14;
  return clamp((v * (a * v + b)) / (v * (c * v + d) + e), vec3<f32>(0.0), vec3<f32>(1.0));
}
@fragment fn fs_main(inp : VsOut) -> @location(0) vec4<f32> {
  var color = textureSample(scene_tex, scene_smp, inp.uv).rgb;
  if (pc.p0.z != 0.0) {
    let bloomColor = textureSample(bloom_tex, bloom_smp, inp.uv).rgb;
    color = color + bloomColor * pc.p0.y;
  }
  color = AcesFilmic(color * pc.p0.x);
  color = pow(color, vec3<f32>(1.0 / 2.2));
  if (pc.p2.y != 0.0) {
    let dist = length(inp.uv - vec2<f32>(0.5));
    let radius = clamp(pc.p2.w, 0.001, 1.5);
    let softness = max(pc.p3.x, 0.0001);
    let vig = 1.0 - smoothstep(radius, radius + softness, dist);
    color = color * mix(1.0, vig, clamp(pc.p2.z, 0.0, 1.0));
  }
  let ign = fract(52.9829189 * fract(0.06711056 * inp.pos.x + 0.00583715 * inp.pos.y));
  color = color + vec3<f32>((ign - 0.5) / 255.0);
  return vec4<f32>(color, 1.0);
}
)WGSL";

// 前向着色（ForwardPbr/ForwardShaded）：顶点已 CPU 预变换到世界空间（见 MeshRenderer），
// 仅需 PerFrame.vp 投影；方向光 Lambert + PerMaterial.albedo + albedo 纹理（slot0）。
// 进阶特征（CSM/SSS/clearcoat/clustered/DDGI/...）由进阶前向 WGSL 承载；BGL 含全部 8 UBO/20 纹理槽，
// 本着色器仅取其用到的子集（WebGPU 允许）。
const char* kWgslForward = R"WGSL(// dse-wgsl
struct PerFrame {
  vp : mat4x4<f32>,
  view : mat4x4<f32>,
  camera_pos : vec4<f32>,
  foliage_wind : vec4<f32>,
  foliage_push : vec4<f32>,
};
struct PerScene {
  light_dir_and_enabled : vec4<f32>,
  light_color_and_ambient : vec4<f32>,
  light_params : vec4<f32>,
};
struct PerMaterial {
  albedo : vec4<f32>,
  roughness_ao : vec4<f32>,
  emissive : vec4<f32>,
  flags : vec4<f32>,
};
@group(1) @binding(0) var<uniform> per_frame : PerFrame;
@group(1) @binding(1) var<uniform> per_scene : PerScene;
@group(1) @binding(2) var<uniform> per_material : PerMaterial;
@group(2) @binding(0) var albedo_tex : texture_2d<f32>;
@group(2) @binding(1) var albedo_smp : sampler;

struct VsOut {
  @builtin(position) clip : vec4<f32>,
  @location(0) nrm : vec3<f32>,
  @location(1) uv : vec2<f32>,
  @location(2) col : vec4<f32>,
};
@vertex fn vs_main(
  @location(0) pos : vec3<f32>,
  @location(1) color : vec4<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) normal : vec3<f32>,
  @location(4) tangent : vec3<f32>,
) -> VsOut {
  var o : VsOut;
  o.clip = per_frame.vp * vec4<f32>(pos, 1.0);
  o.nrm = normal;
  o.uv = uv;
  o.col = color;
  return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let tex = textureSample(albedo_tex, albedo_smp, i.uv);
  let base = tex.rgb * per_material.albedo.rgb * i.col.rgb;
  let n = normalize(i.nrm);
  let l = normalize(per_scene.light_dir_and_enabled.xyz);
  let ndl = max(dot(n, l), 0.0);
  let enabled = per_scene.light_dir_and_enabled.w;
  let ambient = per_scene.light_color_and_ambient.w;
  let light_col = per_scene.light_color_and_ambient.rgb;
  let intensity = per_scene.light_params.x;
  let diffuse = light_col * (ndl * intensity * enabled);
  let lit = base * (vec3<f32>(ambient) + diffuse);
  let emissive = per_material.emissive.rgb;
  return vec4<f32>(lit + emissive, 1.0);
}
)WGSL";

// 进阶前向着色（ForwardShaded / DrawShaded）：移植 forward_shaded.frag 的特性子集——
//   shading_mode（0 PBR Cook-Torrance / 2 半兰伯特皮肤 / 3 半兰伯特静态 / 4 Toon / Unlit）、
//   SSS、clearcoat、clustered 点光（set3 PointLightUBO ≤64）、CSM 方向光阴影（set1 light_space_matrices
//   + shadow atlas，flat unit11 → group2 binding22/23）。UBO 逐字段对齐 mesh_renderer.cpp 的
//   FwdShadedMaterialUBO(160B)/FwdPerSceneUBO(560B)/PointLightsUBO std140 布局。
// 未纳入子集（白默认纹理/关闭 → 与引擎同结果，留后续）：法线贴图/POM、splatmap/积雪、MR/自发光/AO
//   贴图、DDGI/SH 间接光、聚光灯、anisotropy、watercolor(5)/faceSDF(6)。
// 顶点已 CPU 预变换到世界空间（位置/法线/切线，见 BuildShadedWorldVertexBuffer）。前向输出线性 HDR
//   （不在此 tonemap），由 composite（kWgslComposite）统一 Reinhard + sRGB。
const char* kWgslForwardShaded = R"WGSL(// dse-wgsl
const PI : f32 = 3.14159265359;
struct PerFrame {
  vp : mat4x4<f32>,
  view : mat4x4<f32>,
  camera_pos : vec4<f32>,
  foliage_wind : vec4<f32>,
  foliage_push : vec4<f32>,
};
struct PerScene {
  light_dir_and_enabled : vec4<f32>,
  light_color_and_ambient : vec4<f32>,
  light_params : vec4<f32>,             // x=intensity, y=shadow_strength, z=receive_shadow
  cascade_splits : vec4<f32>,
  light_space_matrices : array<mat4x4<f32>, 3>,
  shadow_atlas_regions : array<vec4<f32>, 3>,
  spot_light_space_matrices : array<mat4x4<f32>, 4>,
};
struct PerMaterial {
  albedo : vec4<f32>,        // xyz=base, w=metallic
  roughness_ao : vec4<f32>,  // x=rough, y=ao, z=normal_strength, w=alpha_cutoff
  emissive : vec4<f32>,      // xyz=emissive, w=alpha_test_on
  flags : vec4<f32>,         // x=normal_map, y=mr_map, z=emissive_map, w=occlusion_map
  mode_params : vec4<f32>,   // x=shading_mode, y=double_sided, z=anisotropy, w=pom
  sss : vec4<f32>,           // xyz=tint, w=strength
  clearcoat : vec4<f32>,     // x=coat, y=coat_roughness
  toon_shadow : vec4<f32>,   // xyz=shadow_color, w=threshold
  toon_params : vec4<f32>,   // x=softness, y=spec_size, z=spec_strength, w=rim
  watercolor : vec4<f32>,
};
struct PointLight {
  color : vec3<f32>, intensity : f32,
  position : vec3<f32>, radius : f32,
  cast_shadow : i32, shadow_index : i32, pad : vec2<f32>,
};
struct PointLights {
  count : i32, p0 : i32, p1 : i32, p2 : i32,
  lights : array<PointLight, 64>,
};
// Final-Feat-8：聚光灯（std140 64B/项，与 ubo_types.h SpotLightEntry 同布局）。
struct SpotLight {
  color : vec3<f32>, intensity : f32,
  position : vec3<f32>, radius : f32,
  direction : vec3<f32>, inner_cone : f32,
  outer_cone : f32, cast_shadow : i32, shadow_index : i32, pad : f32,
};
struct SpotLights {
  count : i32, p0 : i32, p1 : i32, p2 : i32,
  lights : array<SpotLight, 64>,
};
@group(1) @binding(0) var<uniform> per_frame : PerFrame;
@group(1) @binding(1) var<uniform> per_scene : PerScene;
@group(1) @binding(2) var<uniform> per_material : PerMaterial;
@group(1) @binding(3) var<uniform> point_lights : PointLights;
@group(1) @binding(7) var<uniform> spot_lights : SpotLights;
@group(2) @binding(0) var albedo_tex : texture_2d<f32>;
@group(2) @binding(1) var albedo_smp : sampler;
// 5.1b：CSM shadow atlas（flat unit11 → group2 binding22）以 texture_depth_2d 采样（textureLoad 手动
//   3×3 PCF，无需采样器）。消费方 mesh_renderer 在无 shadow map 时给 slot11 绑「白 RGBA8」回退（Float），
//   WebGPU 后端在 CollectGroupBindings 处把该回退替换为恒亮 Depth32 回退纹理（深度=1→无遮挡），使本
//   binding 的 sampleType 在所有 draw 上恒为 Depth、与此处声明一致（消除 BGL sampleType 冲突）。真实
//   遮挡逻辑见下方 DirectionalShadow / SampleCascadeShadow（移植 forward_shaded.frag，离屏自检 T5-1 已验证）。
@group(2) @binding(22) var shadow_atlas : texture_depth_2d;
// Final-Feat-8：聚光灯 2D 阴影（flat slot12-15 → group2 binding24/26/28/30，texture_depth_2d，textureLoad
//   手动 3×3 PCF，无需采样器）。点光源 cube 阴影（flat slot16-19 → binding32/34/36/38，texture_depth_cube
//   + 非过滤采样器 binding33/35/37/39）。消费方 mesh_renderer 无 shadow map 时分别绑「白 RGBA8/白 cube」
//   回退（Float），后端 CollectGroupBindings 把这些回退替换为恒亮 Depth32（2D / cube）回退纹理（深度=1→
//   无遮挡），使各 binding 的 sampleType 在所有 draw 上恒为 Depth、与此处声明一致（消除 BGL sampleType 冲突）。
@group(2) @binding(24) var spot_shadow_0 : texture_depth_2d;
@group(2) @binding(26) var spot_shadow_1 : texture_depth_2d;
@group(2) @binding(28) var spot_shadow_2 : texture_depth_2d;
@group(2) @binding(30) var spot_shadow_3 : texture_depth_2d;
@group(2) @binding(32) var point_shadow_0 : texture_depth_cube;
@group(2) @binding(33) var point_shadow_smp_0 : sampler;
@group(2) @binding(34) var point_shadow_1 : texture_depth_cube;
@group(2) @binding(35) var point_shadow_smp_1 : sampler;
@group(2) @binding(36) var point_shadow_2 : texture_depth_cube;
@group(2) @binding(37) var point_shadow_smp_2 : sampler;
@group(2) @binding(38) var point_shadow_3 : texture_depth_cube;
@group(2) @binding(39) var point_shadow_smp_3 : sampler;

fn DistributionGGX(N : vec3<f32>, H : vec3<f32>, roughness : f32) -> f32 {
  let a = roughness * roughness;
  let a2 = a * a;
  let NdotH = max(dot(N, H), 0.0);
  let denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
  return a2 / max(PI * denom * denom, 1e-7);
}
fn GeometrySchlickGGX(NdotV : f32, roughness : f32) -> f32 {
  let r = roughness + 1.0;
  let k = (r * r) / 8.0;
  return NdotV / (NdotV * (1.0 - k) + k);
}
fn GeometrySmith(N : vec3<f32>, V : vec3<f32>, L : vec3<f32>, roughness : f32) -> f32 {
  return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
       * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}
fn FresnelSchlick(cosTheta : f32, F0 : vec3<f32>) -> vec3<f32> {
  return F0 + (vec3<f32>(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// CSM 方向光阴影：移植 forward_shaded.frag 的 SampleCascadeShadow + DirectionalShadow（textureLoad 版
//   手动 3×3 PCF，逻辑同离屏自检 T5-1）。slot11 由后端保证恒为 Depth32（真 atlas 或恒亮回退纹理）。
fn SampleCascadeShadow(idx : i32, worldPos : vec3<f32>, bias : f32) -> f32 {
  let lp = per_scene.light_space_matrices[idx] * vec4<f32>(worldPos, 1.0);
  if (lp.w <= 1e-5) { return 0.0; }
  var pc = lp.xyz / lp.w;
  pc = pc * 0.5 + 0.5;
  if (pc.z > 1.0) { return 0.0; }
  if (pc.x < 0.0 || pc.x > 1.0 || pc.y < 0.0 || pc.y > 1.0) { return 0.0; }
  let region = per_scene.shadow_atlas_regions[idx];
  let uv = pc.xy * region.xy + region.zw;
  let dims = vec2<i32>(textureDimensions(shadow_atlas, 0));
  let base = vec2<i32>(uv * vec2<f32>(dims));
  var occ = 0.0;
  for (var x = -1; x <= 1; x = x + 1) {
    for (var y = -1; y <= 1; y = y + 1) {
      let c = clamp(base + vec2<i32>(x, y), vec2<i32>(0, 0), dims - vec2<i32>(1, 1));
      let d = textureLoad(shadow_atlas, c, 0);
      occ = occ + select(0.0, 1.0, (pc.z - bias) > d);
    }
  }
  return occ / 9.0;
}
fn DirectionalShadow(worldPos : vec3<f32>, N : vec3<f32>, L : vec3<f32>) -> f32 {
  if (per_scene.light_params.z < 0.5) { return 0.0; }   // receive_shadow 关
  let viewDepth = abs((per_frame.view * vec4<f32>(worldPos, 1.0)).z);
  var idx : i32 = 2;                                     // CSM_CASCADES - 1
  if (viewDepth < per_scene.cascade_splits.x) { idx = 0; }
  else if (viewDepth < per_scene.cascade_splits.y) { idx = 1; }
  let bias = max(0.0025 * (1.0 - dot(N, L)), 0.0004);
  let shadow = SampleCascadeShadow(idx, worldPos, bias);
  return clamp(shadow * per_scene.light_params.y, 0.0, 1.0);  // y = shadow_strength
}
// Final-Feat-8：聚光灯 2D 阴影（移植 forward_shaded.frag SpotShadow）。light-space 投影 → 手动 3×3 PCF
//   深度比较，返 0（无遮挡）~1（全遮挡）×shadow_strength。slot12-15 由后端保证恒为 Depth32。
fn SpotShadowDims(idx : i32) -> vec2<i32> {
  if (idx == 0) { return vec2<i32>(textureDimensions(spot_shadow_0, 0)); }
  if (idx == 1) { return vec2<i32>(textureDimensions(spot_shadow_1, 0)); }
  if (idx == 2) { return vec2<i32>(textureDimensions(spot_shadow_2, 0)); }
  return vec2<i32>(textureDimensions(spot_shadow_3, 0));
}
fn SpotShadowTexel(idx : i32, c : vec2<i32>) -> f32 {
  if (idx == 0) { return textureLoad(spot_shadow_0, c, 0); }
  if (idx == 1) { return textureLoad(spot_shadow_1, c, 0); }
  if (idx == 2) { return textureLoad(spot_shadow_2, c, 0); }
  return textureLoad(spot_shadow_3, c, 0);
}
fn SpotShadow(idx : i32, worldPos : vec3<f32>, N : vec3<f32>, L : vec3<f32>) -> f32 {
  if (idx < 0 || idx >= 4) { return 0.0; }
  let lp = per_scene.spot_light_space_matrices[idx] * vec4<f32>(worldPos, 1.0);
  if (lp.w <= 1e-5) { return 0.0; }
  var pc = lp.xyz / lp.w;
  pc = pc * 0.5 + 0.5;
  if (pc.z > 1.0) { return 0.0; }
  if (pc.x < 0.0 || pc.x > 1.0 || pc.y < 0.0 || pc.y > 1.0) { return 0.0; }
  let bias = max(0.003 * (1.0 - dot(N, L)), 0.0005);
  let dims = SpotShadowDims(idx);
  let base = vec2<i32>(pc.xy * vec2<f32>(dims));
  var occ = 0.0;
  for (var x = -1; x <= 1; x = x + 1) {
    for (var y = -1; y <= 1; y = y + 1) {
      let c = clamp(base + vec2<i32>(x, y), vec2<i32>(0, 0), dims - vec2<i32>(1, 1));
      let d = SpotShadowTexel(idx, c);
      occ = occ + select(0.0, 1.0, (pc.z - bias) > d);
    }
  }
  return clamp(occ / 9.0 * per_scene.light_params.y, 0.0, 1.0);
}
// Final-Feat-8：点光源 cube 阴影（移植 forward_shaded.frag PointShadow）。cube 存归一化距离
//   distance/radius；采样取回乘 radius 与当前片到灯距离比较。返 0（无遮挡）或 shadow_strength。
fn PointShadowDepth(idx : i32, dir : vec3<f32>) -> f32 {
  if (idx == 0) { return textureSampleLevel(point_shadow_0, point_shadow_smp_0, dir, 0); }
  if (idx == 1) { return textureSampleLevel(point_shadow_1, point_shadow_smp_1, dir, 0); }
  if (idx == 2) { return textureSampleLevel(point_shadow_2, point_shadow_smp_2, dir, 0); }
  return textureSampleLevel(point_shadow_3, point_shadow_smp_3, dir, 0);
}
fn PointShadow(idx : i32, worldPos : vec3<f32>, lightPos : vec3<f32>, radius : f32) -> f32 {
  if (idx < 0 || idx >= 4) { return 0.0; }
  let toFrag = worldPos - lightPos;
  let cur = length(toFrag);
  if (cur >= radius) { return 0.0; }
  let closest = PointShadowDepth(idx, toFrag) * radius;
  let bias = 0.05;
  return select(0.0, per_scene.light_params.y, (cur - bias) > closest);
}
fn PointLightsLo(N : vec3<f32>, V : vec3<f32>, world_pos : vec3<f32>,
                 surface_albedo : vec3<f32>, roughness : f32, metallic : f32, F0 : vec3<f32>) -> vec3<f32> {
  var sum = vec3<f32>(0.0);
  let n = point_lights.count;
  for (var i = 0; i < n; i = i + 1) {
    let pl = point_lights.lights[i];
    let d = pl.position - world_pos;
    let dist = length(d);
    let L = d / max(dist, 1e-4);
    let atten = clamp(1.0 - (dist * dist) / (pl.radius * pl.radius + 1e-4), 0.0, 1.0);
    var psh = 0.0;
    if (pl.cast_shadow != 0) { psh = PointShadow(pl.shadow_index, world_pos, pl.position, pl.radius); }
    let radiance = pl.color * pl.intensity * atten * (1.0 - psh);
    let H = normalize(V + L);
    let NDF = DistributionGGX(N, H, roughness);
    let G = GeometrySmith(N, V, L, roughness);
    let F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    let spec = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
    sum = sum + (kD * surface_albedo / PI + spec) * radiance * max(dot(N, L), 0.0);
  }
  return sum;
}
// Final-Feat-8：聚光灯光照（移植 forward_shaded.frag）。距离衰减×内/外锥角平滑×(1-聚光阴影)。
//   count=0 时不进循环（无贡献，与原输出一致）。
fn SpotLightsLo(N : vec3<f32>, V : vec3<f32>, world_pos : vec3<f32>,
                surface_albedo : vec3<f32>, roughness : f32, metallic : f32, F0 : vec3<f32>) -> vec3<f32> {
  var sum = vec3<f32>(0.0);
  let n = spot_lights.count;
  for (var i = 0; i < n; i = i + 1) {
    let sl = spot_lights.lights[i];
    let d = sl.position - world_pos;
    let dist = length(d);
    let L = d / max(dist, 1e-4);
    let atten0 = clamp(1.0 - (dist * dist) / (sl.radius * sl.radius + 1e-4), 0.0, 1.0);
    let atten = atten0 * atten0;
    let spotDir = normalize(-sl.direction);
    let theta = dot(L, spotDir);
    let innerCos = cos(radians(sl.inner_cone));
    let outerCos = cos(radians(sl.outer_cone));
    let cone = clamp((theta - outerCos) / max(innerCos - outerCos, 1e-4), 0.0, 1.0);
    if (cone <= 0.0) { continue; }
    var ssh = 0.0;
    if (sl.cast_shadow != 0) { ssh = SpotShadow(sl.shadow_index, world_pos, N, L); }
    let radiance = sl.color * sl.intensity * atten * cone * (1.0 - ssh);
    let H = normalize(V + L);
    let NDF = DistributionGGX(N, H, roughness);
    let G = GeometrySmith(N, V, L, roughness);
    let F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    let spec = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
    sum = sum + (kD * surface_albedo / PI + spec) * radiance * max(dot(N, L), 0.0);
  }
  return sum;
}
fn SubsurfaceScattering(N : vec3<f32>, L : vec3<f32>, alb : vec3<f32>, sss_s : f32,
                        light_col : vec3<f32>, li : f32, tint : vec3<f32>) -> vec3<f32> {
  let wrap = 0.5 * sss_s;
  let wrapped = max(0.0, (dot(N, L) + wrap) / (1.0 + wrap));
  let diff = wrapped - max(dot(N, L), 0.0);
  var sss_tint = tint;
  if (dot(tint, tint) <= 0.0) { sss_tint = vec3<f32>(1.0, 0.35, 0.2); }
  return alb * sss_tint * diff * light_col * li;
}

struct VsOut {
  @builtin(position) clip : vec4<f32>,
  @location(0) nrm : vec3<f32>,
  @location(1) uv : vec2<f32>,
  @location(2) col : vec4<f32>,
  @location(3) wpos : vec3<f32>,
};
@vertex fn vs_main(
  @location(0) pos : vec3<f32>,
  @location(1) color : vec4<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) normal : vec3<f32>,
  @location(4) tangent : vec3<f32>,
) -> VsOut {
  var o : VsOut;
  o.clip = per_frame.vp * vec4<f32>(pos, 1.0);
  o.nrm = normal;
  o.uv = uv;
  o.col = color;
  o.wpos = pos;
  return o;
}
@fragment fn fs_main(i : VsOut, @builtin(front_facing) ff : bool) -> @location(0) vec4<f32> {
  let shading_mode = i32(per_material.mode_params.x + 0.5);
  let double_sided = per_material.mode_params.y > 0.5;
  var N = normalize(i.nrm);
  if (double_sided && !ff) { N = -N; }
  let V = normalize(per_frame.camera_pos.xyz - i.wpos);

  let texColor = textureSample(albedo_tex, albedo_smp, i.uv) * i.col;
  // alpha-test（emissive.w = 开关，roughness_ao.w = cutoff）。
  if (per_material.emissive.w > 0.5 && texColor.a < clamp(per_material.roughness_ao.w, 0.0, 1.0)) { discard; }

  let light_color = per_scene.light_color_and_ambient.xyz;
  let ambient = per_scene.light_color_and_ambient.w;
  let light_intensity = per_scene.light_params.x;
  let lighting_enabled = per_scene.light_dir_and_enabled.w > 0.5;
  let L = normalize(per_scene.light_dir_and_enabled.xyz);
  let shadow = DirectionalShadow(i.wpos, N, L);

  var color = vec3<f32>(0.0);
  var out_alpha = texColor.a;

  if (!lighting_enabled) {
    color = texColor.rgb * per_material.albedo.xyz;
  } else if (shading_mode == 2) {
    let hl = dot(N, L) * 0.5 + 0.5;
    let base_color = texColor.rgb * per_material.albedo.xyz;
    color = base_color * light_color * (hl * light_intensity * (1.0 - shadow) + ambient * 0.5);
    out_alpha = 1.0;
  } else if (shading_mode == 3) {
    let R = reflect(-L, N);
    let hl = dot(N, L) * 0.5 + 0.5;
    let diffuse = per_material.albedo.xyz * hl * light_color * light_intensity * (1.0 - shadow);
    let spec_power = max(per_material.roughness_ao.x, 1.0);
    let specular = vec3<f32>(per_material.albedo.w) * pow(max(dot(R, V), 0.0), spec_power);
    color = (diffuse + specular + per_material.emissive.xyz) * texColor.rgb;
  } else if (shading_mode == 4) {
    let H = normalize(L + V);
    let NdotL = dot(N, L) * 0.5 + 0.5;
    let soft = per_material.toon_params.x;
    let band1 = smoothstep(per_material.toon_shadow.w - soft, per_material.toon_shadow.w + soft, NdotL);
    let band2 = smoothstep(0.7 - soft, 0.7 + soft, NdotL);
    let cel = band1 * 0.7 + band2 * 0.3;
    let baseColor = texColor.rgb * per_material.albedo.xyz;
    let shadowColor = baseColor * per_material.toon_shadow.xyz;
    var diffuse = mix(shadowColor, baseColor * light_color, cel);
    diffuse = mix(shadowColor, diffuse, 1.0 - shadow);
    let spec = step(per_material.toon_params.y, max(dot(N, H), 0.0)) * per_material.toon_params.z;
    let rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * per_material.toon_params.w;
    color = diffuse + light_color * spec * (1.0 - shadow) + vec3<f32>(rim);
  } else {
    // 默认 PBR（Cook-Torrance）+ SSS / clearcoat / clustered 点光。
    let surface_albedo = pow(texColor.rgb * per_material.albedo.xyz, vec3<f32>(2.2));
    let metallic = clamp(per_material.albedo.w, 0.0, 1.0);
    let roughness = clamp(per_material.roughness_ao.x, 0.04, 1.0);
    let ao = max(per_material.roughness_ao.y, 0.0);
    let F0 = mix(vec3<f32>(0.04), surface_albedo, vec3<f32>(metallic));
    let H = normalize(V + L);
    let NDF = DistributionGGX(N, H, roughness);
    let G = GeometrySmith(N, V, L, roughness);
    let F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    let specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
    let NdotL = max(dot(N, L), 0.0);
    var Lo = (kD * surface_albedo / PI + specular) * light_color * light_intensity * NdotL;
    if (per_material.sss.w > 0.0) {
      Lo = Lo + SubsurfaceScattering(N, L, surface_albedo, per_material.sss.w,
                                     light_color, light_intensity, per_material.sss.xyz);
    }
    if (per_material.clearcoat.x > 0.0) {
      let cc_r = max(per_material.clearcoat.y, 0.04);
      let NDF_cc = DistributionGGX(N, H, cc_r);
      let G_cc = GeometrySmith(N, V, L, cc_r);
      let F_cc = FresnelSchlick(max(dot(H, V), 0.0), vec3<f32>(0.04));
      let spec_cc = (NDF_cc * G_cc * F_cc) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
      Lo = Lo + spec_cc * per_material.clearcoat.x * NdotL * light_color * light_intensity;
    }
    Lo = Lo * (1.0 - shadow);
    Lo = Lo + PointLightsLo(N, V, i.wpos, surface_albedo, roughness, metallic, F0);
    Lo = Lo + SpotLightsLo(N, V, i.wpos, surface_albedo, roughness, metallic, F0);
    color = vec3<f32>(ambient) * surface_albedo * ao + Lo + per_material.emissive.xyz;
  }
  // 与 forward_shaded.frag 末尾一致：Reinhard tonemap + gamma（保证三后端 LDR 输出一致）。
  // 后续 composite（kWgslComposite）再做一次 Reinhard+sRGB，与 WebGL2 参考帧（同样双重处理）对齐。
  color = color / (color + vec3<f32>(1.0));
  color = pow(max(color, vec3<f32>(0.0)), vec3<f32>(1.0 / 2.2));
  return vec4<f32>(color, out_alpha);
}
)WGSL";

// 天空盒：vp 推送常量（VS push, group0 binding0）；cubemap 在 slot0 → group2 binding0/1。
// 顶点为 36 顶点立方体（vec3 pos）。z=w 使深度落远平面（配 LEQUAL 深度测试）。
const char* kWgslSkybox = R"WGSL(// dse-wgsl
struct VsPush { vp : mat4x4<f32> };
@group(0) @binding(0) var<uniform> vs_push : VsPush;
@group(2) @binding(0) var sky_tex : texture_cube<f32>;
@group(2) @binding(1) var sky_smp : sampler;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) dir : vec3<f32> };
@vertex fn vs_main(@location(0) in_pos : vec3<f32>) -> VsOut {
  var o : VsOut;
  let p = vs_push.vp * vec4<f32>(in_pos, 1.0);
  o.pos = p.xyww;
  o.dir = in_pos;
  return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  return textureSample(sky_tex, sky_smp, normalize(i.dir));
}
)WGSL";

// 内建天空盒立方体（36 顶点，每顶点 vec3，逆时针外向；与桌面后端一致）。
const float kSkyboxCubeVerts[] = {
  -1,  1, -1,  -1, -1, -1,   1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
  -1, -1,  1,  -1, -1, -1,  -1,  1, -1,  -1,  1, -1,  -1,  1,  1,  -1, -1,  1,
   1, -1, -1,   1, -1,  1,   1,  1,  1,   1,  1,  1,   1,  1, -1,   1, -1, -1,
  -1, -1,  1,  -1,  1,  1,   1,  1,  1,   1,  1,  1,   1, -1,  1,  -1, -1,  1,
  -1,  1, -1,   1,  1, -1,   1,  1,  1,   1,  1,  1,  -1,  1,  1,  -1,  1, -1,
  -1, -1, -1,  -1, -1,  1,   1, -1, -1,   1, -1, -1,  -1, -1,  1,   1, -1,  1,
};

}  // namespace
namespace {
const char* kGpuDrivenPBRWGSL = R"WGSL(// dse-wgsl
struct PerFrame {
  vp : mat4x4<f32>,
  view : mat4x4<f32>,
  camera_pos : vec4<f32>,
};
struct PerScene {
  light_dir : vec4<f32>,      // xyz=光行进方向, w=enabled
  light_color : vec4<f32>,    // rgb=颜色, w=ambient 强度
  light_params : vec4<f32>,   // x=intensity, y=shadow_strength, z=receive_shadow, w=0
};
struct Inst {
  model : mat4x4<f32>,
  material_id : u32,
  draw_cmd_id : u32,
  pad0 : u32,
  pad1 : u32,
};
struct Mat {
  albedo : vec4<f32>,         // rgb + metallic
  roughness_ao : vec4<f32>,   // roughness, ao, normal_strength, alpha_cutoff
  emissive : vec4<f32>,       // rgb + alpha_test
  flags : vec4<f32>,
  extra : vec4<f32>,
  extra2 : vec4<f32>,
  toon_shadow : vec4<f32>,
  toon_params : vec4<f32>,
};
@group(1) @binding(0) var<uniform> per_frame : PerFrame;
@group(1) @binding(1) var<uniform> per_scene : PerScene;
@group(3) @binding(5) var<storage, read> instances : array<Inst>;
@group(3) @binding(9) var<storage, read> materials : array<Mat>;
@group(2) @binding(0) var albedo_tex : texture_2d<f32>;
@group(2) @binding(1) var albedo_smp : sampler;

struct VsIn {
  @location(0) pos : vec3<f32>,
  @location(1) color : vec4<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) normal : vec3<f32>,
  @location(4) tangent : vec3<f32>,
  @location(5) weights : vec4<f32>,
  @location(6) joints : vec4<f32>,
};
struct VsOut {
  @builtin(position) clip : vec4<f32>,
  @location(0) world_pos : vec3<f32>,
  @location(1) world_normal : vec3<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) @interpolate(flat) material_id : u32,
  @location(4) vcolor : vec4<f32>,
};

@vertex fn vs_main(i : VsIn, @builtin(instance_index) inst : u32) -> VsOut {
  let model = instances[inst].model;
  let wp = model * vec4<f32>(i.pos, 1.0);
  var o : VsOut;
  o.clip = per_frame.vp * wp;
  o.world_pos = wp.xyz;
  o.world_normal = normalize((model * vec4<f32>(i.normal, 0.0)).xyz);
  o.uv = i.uv;
  o.material_id = instances[inst].material_id;
  o.vcolor = i.color;
  return o;
}

const PI = 3.14159265359;
fn distributionGGX(ndh : f32, rough : f32) -> f32 {
  let a = rough * rough;
  let a2 = a * a;
  let d = ndh * ndh * (a2 - 1.0) + 1.0;
  return a2 / max(PI * d * d, 1e-4);
}
fn geometrySchlickGGX(ndv : f32, rough : f32) -> f32 {
  let r = rough + 1.0;
  let k = (r * r) / 8.0;
  return ndv / (ndv * (1.0 - k) + k);
}
fn fresnelSchlick(ct : f32, f0 : vec3<f32>) -> vec3<f32> {
  return f0 + (vec3<f32>(1.0) - f0) * pow(clamp(1.0 - ct, 0.0, 1.0), 5.0);
}

@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let m = materials[i.material_id];
  let tex = textureSample(albedo_tex, albedo_smp, i.uv);
  // inline mesh 把实色烘进顶点色（材质 albedo=白）；file mesh 反之（顶点色=白）。两路均为 albedo×tex×vcolor。
  let base = m.albedo.rgb * tex.rgb * i.vcolor.rgb;
  let metallic = m.albedo.a;
  let rough = max(m.roughness_ao.x, 0.04);
  let ao = m.roughness_ao.y;

  let n = normalize(i.world_normal);
  let v = normalize(per_frame.camera_pos.xyz - i.world_pos);
  let l = normalize(-per_scene.light_dir.xyz);
  let h = normalize(v + l);
  let ndl = max(dot(n, l), 0.0);
  let ndv = max(dot(n, v), 0.0);
  let ndh = max(dot(n, h), 0.0);
  let hdv = max(dot(h, v), 0.0);

  let f0 = mix(vec3<f32>(0.04), base, metallic);
  let ndf = distributionGGX(ndh, rough);
  let g = geometrySchlickGGX(ndv, rough) * geometrySchlickGGX(ndl, rough);
  let f = fresnelSchlick(hdv, f0);
  let spec = (ndf * g * f) / max(4.0 * ndv * ndl, 1e-4);
  let kd = (vec3<f32>(1.0) - f) * (1.0 - metallic);
  let intensity = per_scene.light_params.x;
  let radiance = per_scene.light_color.rgb * intensity;
  let diffuse = kd * base / PI;
  var color = (diffuse + spec) * radiance * ndl;
  color += per_scene.light_color.a * base * ao;   // 环境项
  color += m.emissive.rgb;
  color = color / (color + vec3<f32>(1.0));        // Reinhard
  color = pow(color, vec3<f32>(1.0 / 2.2));        // gamma
  return vec4<f32>(color, 1.0);
}
)WGSL";
}  // namespace
unsigned int WebGPUShaderManager::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
    // 以 sentinel 行 `// dse-wgsl`（允许前导空白）区分两类着色器：
    //   - WGSL（内建/自检程序）：vert_src 即整段 WGSL module（含 vs_main/fs_main），编译出 module。
    //   - 引擎 GLSL：无离线 GLSL→WGSL 工具，故不转译、module 留空，其绘制在录制期被优雅跳过。
    ShaderEntry e;
    e.vert_src = vert_src;
    e.frag_src = frag_src;

    const char* kSentinel = "// dse-wgsl";
    const size_t first = vert_src.find_first_not_of(" \t\r\n");
    const bool is_wgsl = first != std::string::npos &&
                         vert_src.compare(first, std::strlen(kSentinel), kSentinel) == 0;
    if (is_wgsl) {
        e.module = CompileWGSL(vert_src, "dse-wgsl-program");
        // 单一 module 同时承载 vs/fs 入口；无 fs_main 视为仅深度 pass（无片元阶段）。
        e.has_fragment = vert_src.find("fn fs_main") != std::string::npos ||
                         vert_src.find("fn " + e.fs_entry) != std::string::npos;
        // 解析 WGSL 实际声明的 `@group(N) @binding(M)`，供 explicit layout/BindGroup 过滤
        //（仅纳入着色器真正使用的绑定，避免引擎多绑资源超 per-stage 上限 / 与着色器用量不符）。
        ParseWgslBindings(vert_src, e.wgsl_bindings);
        if (!e.module) {
            DEBUG_LOG_ERROR("WebGPU: WGSL 着色器编译失败（module 为空），该程序绘制将被跳过");
        }
    }

    const unsigned int h = NextHandle();
    shaders_[h] = std::move(e);
    return h;
}

void WebGPUShaderManager::DeleteShaderProgram(unsigned int program_handle) {
    auto it = shaders_.find(program_handle);
    if (it == shaders_.end()) return;
    if (it->second.module) wgpuShaderModuleRelease(it->second.module);
    shaders_.erase(it);
}

unsigned int WebGPUShaderManager::GetOrCreateWgslProgram(const std::string& key, const std::string& wgsl) {
    auto it = wgsl_program_cache_.find(key);
    if (it != wgsl_program_cache_.end()) return it->second;
    const unsigned int h = CreateShaderProgram(wgsl, wgsl);
    wgsl_program_cache_[key] = h;
    DEBUG_LOG_INFO("WebGPU: 内建 WGSL 程序 '{}' -> handle {}", key, h);
    return h;
}

unsigned int WebGPUShaderManager::GetBuiltinProgram(BuiltinProgram program) {
    switch (program) {
        case BuiltinProgram::Skybox:
            return GetOrCreateWgslProgram("builtin.skybox", kWgslSkybox);
        // 静态前向 PBR（最小前向 WGSL，64B PerMaterial）。
        case BuiltinProgram::ForwardPbr:
            return GetOrCreateWgslProgram("builtin.forward", kWgslForward);
        // 进阶：高级 shading（shading_mode/SSS/clearcoat/点光/CSM，160B PerMaterial）。
        case BuiltinProgram::ForwardShaded:
            return GetOrCreateWgslProgram("builtin.forward.shaded", kWgslForwardShaded);
        // 仅深度 pass（阴影贴图 / pre-Z；GPU-driven 路径）。
        case BuiltinProgram::ForwardPbrDepth:
        case BuiltinProgram::ForwardInstancedDepth:
            return GetOrCreateWgslProgram("builtin.shadow_depth", wgsl::kWgslShadowDepth);
        default:
            // 蒙皮/实例化/morph/粒子/毛发/GBuffer 等需 SSBO 或专用布局，留后续阶段。
            return 0;
    }
}

unsigned int WebGPUShaderManager::GetGenPPShaderProgram(const std::string& effect_name) {
    // 合成族（HDR→LDR tonemap）与直拷族分流；未迁移效果返回 0（其 pass 优雅跳过）。
    // B-2：bloom 合成走 ACES + bloom/vignette（kWgslBloomComposite），独立于无 bloom 时的
    //   tonemapping/ssao_apply 直合成（kWgslComposite，Reinhard）。
    if (effect_name == "bloom_composite") {
        return GetOrCreateWgslProgram("genpp.bloom_composite", kWgslBloomComposite);
    }
    if (effect_name == "tonemapping" || effect_name == "ssao_apply") {
        return GetOrCreateWgslProgram("genpp.composite", kWgslComposite);
    }
    // B-2：bloom 链各趟（亮度提取 / 13-tap 降采样 / 3x3 帐篷升采样），全屏 quad WGSL。
    if (effect_name == "bloom_extract") {
        return GetOrCreateWgslProgram("genpp.bloom_extract", kWgslBloomExtract);
    }
    if (effect_name == "bloom_downsample") {
        return GetOrCreateWgslProgram("genpp.bloom_downsample", kWgslBloomDownsample);
    }
    if (effect_name == "bloom_upsample") {
        return GetOrCreateWgslProgram("genpp.bloom_upsample", kWgslBloomUpsample);
    }
    if (effect_name == "copy" || effect_name == "passthrough") {
        return GetOrCreateWgslProgram("genpp.blit", kWgslFullscreenBlit);
    }
    if (effect_name == "fxaa") {
        return GetOrCreateWgslProgram("genpp.fxaa", wgsl::kWgslFxaa);
    }
    if (effect_name == "ssao") {
        return GetOrCreateWgslProgram("genpp.ssao", wgsl::kWgslSsao);
    }
    if (effect_name == "taa_resolve" || effect_name == "taa") {
        return GetOrCreateWgslProgram("genpp.taa", wgsl::kWgslTaaResolve);
    }
    if (effect_name == "motion_blur") {
        return GetOrCreateWgslProgram("genpp.motion_blur", wgsl::kWgslMotionBlur);
    }
    if (effect_name == "dof" || effect_name == "depth_of_field") {
        return GetOrCreateWgslProgram("genpp.dof", wgsl::kWgslDof);
    }
    if (effect_name == "ssr" || effect_name == "screen_space_reflections") {
        return GetOrCreateWgslProgram("genpp.ssr", wgsl::kWgslSsr);
    }
    if (effect_name == "contact_shadow" || effect_name == "contact_shadows") {
        return GetOrCreateWgslProgram("genpp.contact_shadow", wgsl::kWgslContactShadow);
    }
    return 0;
}

unsigned int WebGPUShaderManager::GetSkyboxCubeVertexBuffer() {
    if (skybox_cube_vbo_) return skybox_cube_vbo_;
    skybox_cube_vbo_ = res_->CreateBuffer(sizeof(kSkyboxCubeVerts), kSkyboxCubeVerts,
                                    /*is_dynamic=*/false, /*is_index=*/false);
    return skybox_cube_vbo_;
}

WGPUShaderModule WebGPUShaderManager::CompileWGSL(const std::string& code, const char* label) {
    if (!device_) return nullptr;
    WGPUShaderModuleWGSLDescriptor wgsl{};
    wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl.code = code.c_str();
    WGPUShaderModuleDescriptor sd{};
    sd.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&wgsl);
    sd.label = label;
    return wgpuDeviceCreateShaderModule(device_, &sd);
}

bool WebGPUShaderManager::EnsureGpuDrivenPBRShader() {
    if (gpu_driven_pbr_failed_) return false;
    if (gpu_driven_pbr_program_ && gpu_driven_pbr_pso_ && white_texture_ &&
        gpu_driven_perframe_ubo_ && gpu_driven_perscene_ubo_) {
        return true;
    }
    if (!EnsureInitialized() || !device_) return false;
    if (!gpu_driven_pbr_program_) {
        gpu_driven_pbr_program_ = CreateShaderProgram(kGpuDrivenPBRWGSL, "");
        if (!gpu_driven_pbr_program_) {
            gpu_driven_pbr_failed_ = true;
            DEBUG_LOG_ERROR("WebGPU: GPU-driven PBR WGSL 编译失败，HasGPUDrivenPBRShader 将返回 false");
            return false;
        }
    }
    if (!gpu_driven_pbr_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = true;
        d.depth_write_enabled = true;
        d.depth_func = CompareFunc::Less;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        gpu_driven_pbr_pso_ = pso_->CreatePipelineState(d);
    }
    if (!white_texture_) {
        const unsigned char white[4] = {255, 255, 255, 255};
        white_texture_ = res_->CreateTexture2D(1, 1, white, /*linear_filter=*/true);
    }
    if (!gpu_driven_perframe_ubo_) {
        GpuBufferDesc d; d.size = sizeof(PerFrameUBO); d.usage = GpuBufferUsage::kUniform;
        gpu_driven_perframe_ubo_ = res_->CreateGpuBuffer(d, nullptr);
    }
    if (!gpu_driven_perscene_ubo_) {
        GpuBufferDesc d; d.size = sizeof(PerSceneUBO); d.usage = GpuBufferUsage::kUniform;
        gpu_driven_perscene_ubo_ = res_->CreateGpuBuffer(d, nullptr);
    }
    return gpu_driven_pbr_program_ && gpu_driven_pbr_pso_ && white_texture_ &&
           gpu_driven_perframe_ubo_ && gpu_driven_perscene_ubo_;
}

bool WebGPUShaderManager::HasGPUDrivenPBRShader() const {
    // 惰性编译缓存查询（与 GL「init 时编译，运行时查句柄」语义对齐）。const 方法经 const_cast
    // 触发一次性惰性编译，后续直接返回缓存句柄状态。
    return const_cast<WebGPUShaderManager*>(this)->EnsureGpuDrivenPBRShader();
}

unsigned int WebGPUShaderManager::CreateComputeShader(const std::string& source) {
    if (!EnsureInitialized() || !device_) return 0;
    // 仅接受 WGSL（首非空行 `// dse-wgsl`）：引擎 GLSL/SPIR-V compute 无离线转译，返回 0 跳过。
    const char* kSentinel = "// dse-wgsl";
    const size_t first = source.find_first_not_of(" \t\r\n");
    const bool is_wgsl = first != std::string::npos &&
                         source.compare(first, std::strlen(kSentinel), kSentinel) == 0;
    if (!is_wgsl) {
        DEBUG_LOG_WARN("WebGPU: CreateComputeShader 收到非 WGSL 源（无 // dse-wgsl 标记），跳过（返回 0）");
        return 0;
    }
    ComputeShaderEntry e;
    e.module = CompileWGSL(source, "dse-wgsl-compute");
    if (!e.module) {
        DEBUG_LOG_ERROR("WebGPU: compute WGSL 编译失败（module 为空）");
        return 0;
    }
    // 入口名：默认 cs_main，允许 `fn main` 兜底。
    if (source.find("fn cs_main") == std::string::npos &&
        source.find("fn main") != std::string::npos) {
        e.entry = "main";
    }
    ParseWgslBindings(source, e.wgsl_bindings);
    const unsigned int h = NextHandle();
    compute_shaders_[h] = std::move(e);
    return h;
}

unsigned int WebGPUShaderManager::CreateComputeShaderEx(
    const std::string& /*gl_src*/, const std::string& /*vk_src*/, const std::string& /*hlsl_src*/,
    uint32_t /*ssbo_count*/, uint32_t /*storage_image_count*/, uint32_t /*sampler_count*/,
    uint32_t /*push_constant_bytes*/, const std::string& wgsl_src) {
    // WebGPU 仅消费手写 WGSL 源槽。空槽表示该 compute 特性尚未手译 WGSL（如 GPU-driven
    // 剔除 / HiZ / skinning / hair / grass）——返回 0，调用方按句柄 0 优雅回退到 CPU/无该特性。
    // 布局计数（ssbo/img/smp/pc）不需要：compute 管线 layout 由 WGSL @group/@binding 解析驱动。
    if (wgsl_src.empty()) return 0;
    return CreateComputeShader(wgsl_src);
}

void WebGPUShaderManager::DeleteComputeShader(unsigned int handle) {
    auto it = compute_shaders_.find(handle);
    if (it == compute_shaders_.end()) return;
    if (it->second.module) wgpuShaderModuleRelease(it->second.module);
    compute_shaders_.erase(it);
}

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
