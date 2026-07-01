/**
 * @file postprocess_wgsl.h
 * @brief WebGPU WGSL 后处理着色器集合（补全桌面后端已有的 GLSL/HLSL 版本）。
 *
 * 所有后处理共用全屏 quad 顶点着色器（PPVertex: pos@0, uv@1），
 * 布局遵循 PostProcessRenderer 约定：
 *   group(2) binding(0) = uniform params
 *   group(2) binding(1) = source texture + sampler
 *   group(2) binding(2) = secondary texture (motion vector / color)
 *   group(2) binding(5) = history texture (TAA)
 */

#pragma once

namespace dse::render::wgsl {

// ── FXAA ─────────────────────────────────────────────────────────────────────
inline const char* kWgslFxaa = R"WGSL(// dse-wgsl
struct Params { resolution : vec2<f32> };
@group(2) @binding(0) var<uniform> params : Params;
@group(2) @binding(1) var src_tex : texture_2d<f32>;
@group(2) @binding(2) var src_smp : sampler;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = vec2<f32>(uv.x, 1.0 - uv.y); return o;
}
fn luma(c : vec3<f32>) -> f32 { return dot(c, vec3<f32>(0.299, 0.587, 0.114)); }
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let texel = vec2<f32>(1.0) / params.resolution;
  let lumaM  = luma(textureSample(src_tex, src_smp, i.uv).rgb);
  let lumaNW = luma(textureSample(src_tex, src_smp, i.uv + vec2<f32>(-1.0,-1.0) * texel).rgb);
  let lumaNE = luma(textureSample(src_tex, src_smp, i.uv + vec2<f32>( 1.0,-1.0) * texel).rgb);
  let lumaSW = luma(textureSample(src_tex, src_smp, i.uv + vec2<f32>(-1.0, 1.0) * texel).rgb);
  let lumaSE = luma(textureSample(src_tex, src_smp, i.uv + vec2<f32>( 1.0, 1.0) * texel).rgb);
  let lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
  let lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
  let lumaRange = lumaMax - lumaMin;
  if (lumaRange < max(0.0312, lumaMax * 0.125)) {
    return textureSample(src_tex, src_smp, i.uv);
  }
  var dir : vec2<f32>;
  dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
  dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
  let dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.0625, 1.0 / 128.0);
  let rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
  dir = clamp(dir * rcpDirMin, vec2<f32>(-8.0), vec2<f32>(8.0)) * texel;
  let rgbA = 0.5 * (
    textureSample(src_tex, src_smp, i.uv + dir * (1.0/3.0 - 0.5)).rgb +
    textureSample(src_tex, src_smp, i.uv + dir * (2.0/3.0 - 0.5)).rgb);
  let rgbB = rgbA * 0.5 + 0.25 * (
    textureSample(src_tex, src_smp, i.uv + dir * -0.5).rgb +
    textureSample(src_tex, src_smp, i.uv + dir *  0.5).rgb);
  let lumaB = luma(rgbB);
  if (lumaB < lumaMin || lumaB > lumaMax) { return vec4<f32>(rgbA, 1.0); }
  return vec4<f32>(rgbB, 1.0);
}
)WGSL";

// ── SSAO ─────────────────────────────────────────────────────────────────────
inline const char* kWgslSsao = R"WGSL(// dse-wgsl
struct Params {
  radius : f32, bias : f32, near_p : f32, far_p : f32,
  screen_w : f32, screen_h : f32, sample_count : f32, power_val : f32,
  intensity : f32,
};
@group(2) @binding(0) var<uniform> params : Params;
@group(2) @binding(1) var depth_tex : texture_2d<f32>;
@group(2) @binding(2) var depth_smp : sampler;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = vec2<f32>(uv.x, 1.0 - uv.y); return o;
}
fn linearize(d : f32) -> f32 {
  let z = d * 2.0 - 1.0;
  return (2.0 * params.near_p * params.far_p) / (params.far_p + params.near_p - z * (params.far_p - params.near_p));
}
fn reconstructNormal(uv : vec2<f32>) -> vec3<f32> {
  let texel = vec2<f32>(1.0 / params.screen_w, 1.0 / params.screen_h);
  let dc = linearize(textureSample(depth_tex, depth_smp, uv).r);
  let dl = linearize(textureSample(depth_tex, depth_smp, uv - vec2<f32>(texel.x, 0.0)).r);
  let dr = linearize(textureSample(depth_tex, depth_smp, uv + vec2<f32>(texel.x, 0.0)).r);
  let db = linearize(textureSample(depth_tex, depth_smp, uv - vec2<f32>(0.0, texel.y)).r);
  let dt = linearize(textureSample(depth_tex, depth_smp, uv + vec2<f32>(0.0, texel.y)).r);
  return normalize(vec3<f32>(dl - dr, db - dt, 2.0 * texel.x * dc));
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let depth = textureSample(depth_tex, depth_smp, i.uv).r;
  if (depth >= 1.0) { return vec4<f32>(1.0); }
  let linDepth = linearize(depth);
  let normal = reconstructNormal(i.uv);
  var occlusion = 0.0;
  let rScale = params.radius / linDepth;
  let sc = clamp(i32(params.sample_count), 4, 16);
  let kernel = array<vec3<f32>, 16>(
    vec3<f32>( 0.5381, 0.1856,-0.4319), vec3<f32>( 0.1379, 0.2486, 0.4430),
    vec3<f32>( 0.3371, 0.5679,-0.0057), vec3<f32>(-0.6999,-0.0451,-0.0019),
    vec3<f32>( 0.0689,-0.1598,-0.8547), vec3<f32>( 0.0560, 0.0069,-0.1843),
    vec3<f32>(-0.0146, 0.1402, 0.0762), vec3<f32>( 0.0100,-0.1924,-0.0344),
    vec3<f32>(-0.3577,-0.5301,-0.4358), vec3<f32>(-0.3169, 0.1063, 0.0158),
    vec3<f32>( 0.0103,-0.5869, 0.0046), vec3<f32>(-0.0897,-0.4940, 0.3287),
    vec3<f32>( 0.7119,-0.0154,-0.0918), vec3<f32>(-0.0533, 0.0596,-0.5411),
    vec3<f32>( 0.0352,-0.0631, 0.5460), vec3<f32>(-0.4776, 0.2847,-0.0271),
  );
  let screen_size = vec2<f32>(params.screen_w, params.screen_h);
  for (var idx = 0; idx < sc; idx++) {
    var sampleDir = kernel[idx];
    if (dot(sampleDir, normal) < 0.0) { sampleDir = -sampleDir; }
    let sampleUV = i.uv + sampleDir.xy * rScale * (1.0 / screen_size);
    let sampleDepth = linearize(textureSample(depth_tex, depth_smp, sampleUV).r);
    let rangeCheck = smoothstep(0.0, 1.0, params.radius / abs(linDepth - sampleDepth));
    if (sampleDepth < linDepth - params.bias) { occlusion += rangeCheck; }
  }
  occlusion = 1.0 - (occlusion / f32(sc));
  occlusion = pow(occlusion, params.power_val) * params.intensity;
  return vec4<f32>(vec3<f32>(clamp(occlusion, 0.0, 1.0)), 1.0);
}
)WGSL";

// ── TAA Resolve ──────────────────────────────────────────────────────────────
inline const char* kWgslTaaResolve = R"WGSL(// dse-wgsl
struct Params {
  blend_factor : f32, jitter_x : f32, jitter_y : f32, frame_index : f32,
  screen_w : f32, screen_h : f32,
};
@group(2) @binding(0) var<uniform> params : Params;
@group(2) @binding(1) var cur_tex : texture_2d<f32>;
@group(2) @binding(2) var mv_tex : texture_2d<f32>;
@group(2) @binding(3) var tex_smp : sampler;
@group(2) @binding(5) var hist_tex : texture_2d<f32>;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = vec2<f32>(uv.x, 1.0 - uv.y); return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let current = textureSample(cur_tex, tex_smp, i.uv).rgb;
  let mv = textureSample(mv_tex, tex_smp, i.uv).rg;
  var history_uv = i.uv - mv - vec2<f32>(params.jitter_x, params.jitter_y);
  history_uv = clamp(history_uv, vec2<f32>(0.0), vec2<f32>(1.0));
  let texel = vec2<f32>(1.0 / params.screen_w, 1.0 / params.screen_h);
  var m1 = vec3<f32>(0.0);
  var m2 = vec3<f32>(0.0);
  for (var dx = -1; dx <= 1; dx++) {
    for (var dy = -1; dy <= 1; dy++) {
      let s = textureSample(cur_tex, tex_smp, i.uv + vec2<f32>(f32(dx), f32(dy)) * texel).rgb;
      m1 += s; m2 += s * s;
    }
  }
  m1 /= 9.0;
  let sigma = sqrt(max(m2 / 9.0 - m1 * m1, vec3<f32>(0.0)));
  let aabb_min = m1 - 1.25 * sigma;
  let aabb_max = m1 + 1.25 * sigma;
  var history = textureSample(hist_tex, tex_smp, history_uv).rgb;
  history = clamp(history, aabb_min, aabb_max);
  let velocity_len = length(mv * vec2<f32>(params.screen_w, params.screen_h));
  let vel_weight = clamp(velocity_len * 0.5, 0.0, 0.5);
  var alpha = params.blend_factor;
  if (params.frame_index < 2.0) { alpha = 1.0; }
  else { alpha = clamp(params.blend_factor + vel_weight, params.blend_factor, 1.0); }
  return vec4<f32>(mix(history, current, alpha), 1.0);
}
)WGSL";

// ── Motion Blur ──────────────────────────────────────────────────────────────
inline const char* kWgslMotionBlur = R"WGSL(// dse-wgsl
struct Params { intensity : f32, num_samples : f32, screen_w : f32, screen_h : f32 };
@group(2) @binding(0) var<uniform> params : Params;
@group(2) @binding(1) var mv_tex : texture_2d<f32>;
@group(2) @binding(2) var color_tex : texture_2d<f32>;
@group(2) @binding(3) var tex_smp : sampler;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = vec2<f32>(uv.x, 1.0 - uv.y); return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let velocity = textureSample(mv_tex, tex_smp, i.uv).rg * params.intensity;
  let samples = max(i32(params.num_samples), 1);
  var color = textureSample(color_tex, tex_smp, i.uv).rgb;
  var total = 1.0;
  for (var s = 1; s < samples; s++) {
    let t = f32(s) / f32(samples);
    let sample_uv = i.uv + velocity * t;
    if (sample_uv.x >= 0.0 && sample_uv.x <= 1.0 && sample_uv.y >= 0.0 && sample_uv.y <= 1.0) {
      color += textureSample(color_tex, tex_smp, sample_uv).rgb;
      total += 1.0;
    }
  }
  return vec4<f32>(color / total, 1.0);
}
)WGSL";

// ── Depth of Field ───────────────────────────────────────────────────────────
inline const char* kWgslDof = R"WGSL(// dse-wgsl
struct Params {
  focus_distance : f32, focus_range : f32, bokeh_radius : f32,
  near_plane : f32, far_plane : f32, screen_w : f32, screen_h : f32,
};
@group(2) @binding(0) var<uniform> params : Params;
@group(2) @binding(1) var depth_tex : texture_2d<f32>;
@group(2) @binding(2) var color_tex : texture_2d<f32>;
@group(2) @binding(3) var tex_smp : sampler;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = vec2<f32>(uv.x, 1.0 - uv.y); return o;
}
fn linearize(d : f32) -> f32 {
  let z = d * 2.0 - 1.0;
  return (2.0 * params.near_plane * params.far_plane) / (params.far_plane + params.near_plane - z * (params.far_plane - params.near_plane));
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let depth = textureSample(depth_tex, tex_smp, i.uv).r;
  let lin_depth = linearize(depth);
  let coc = clamp(abs(lin_depth - params.focus_distance) / params.focus_range, 0.0, 1.0);
  let texel = vec2<f32>(1.0 / params.screen_w, 1.0 / params.screen_h);
  let radius = coc * params.bokeh_radius;
  var color = vec3<f32>(0.0);
  var total_weight = 0.0;
  let GOLDEN_ANGLE = 2.39996323;
  for (var idx = 0; idx < 16; idx++) {
    let r = sqrt(f32(idx) / 16.0) * radius;
    let theta = f32(idx) * GOLDEN_ANGLE;
    let offset = vec2<f32>(cos(theta), sin(theta)) * r * texel;
    let sample_depth = linearize(textureSample(depth_tex, tex_smp, i.uv + offset).r);
    let sample_coc = clamp(abs(sample_depth - params.focus_distance) / params.focus_range, 0.0, 1.0);
    let w = max(sample_coc, coc);
    color += textureSample(color_tex, tex_smp, i.uv + offset).rgb * w;
    total_weight += w;
  }
  if (total_weight > 0.0) { color /= total_weight; }
  else { color = textureSample(color_tex, tex_smp, i.uv).rgb; }
  return vec4<f32>(color, 1.0);
}
)WGSL";

// ── SSR (Screen-Space Reflections) ───────────────────────────────────────────
inline const char* kWgslSsr = R"WGSL(// dse-wgsl
struct Params {
  max_distance : f32, thickness : f32, step_size : f32, max_steps : f32,
  near_plane : f32, far_plane : f32, screen_w : f32, screen_h : f32,
  fade_distance : f32, max_roughness : f32,
};
@group(2) @binding(0) var<uniform> params : Params;
@group(2) @binding(1) var depth_tex : texture_2d<f32>;
@group(2) @binding(2) var color_tex : texture_2d<f32>;
@group(2) @binding(3) var tex_smp : sampler;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = vec2<f32>(uv.x, 1.0 - uv.y); return o;
}
fn linearize(d : f32) -> f32 {
  let z = d * 2.0 - 1.0;
  return (2.0 * params.near_plane * params.far_plane) / (params.far_plane + params.near_plane - z * (params.far_plane - params.near_plane));
}
fn reconstructNormal(uv : vec2<f32>) -> vec3<f32> {
  let texel = vec2<f32>(1.0 / params.screen_w, 1.0 / params.screen_h);
  let dc = linearize(textureSample(depth_tex, tex_smp, uv).r);
  let dl = linearize(textureSample(depth_tex, tex_smp, uv - vec2<f32>(texel.x, 0.0)).r);
  let dr = linearize(textureSample(depth_tex, tex_smp, uv + vec2<f32>(texel.x, 0.0)).r);
  let db = linearize(textureSample(depth_tex, tex_smp, uv - vec2<f32>(0.0, texel.y)).r);
  let dt = linearize(textureSample(depth_tex, tex_smp, uv + vec2<f32>(0.0, texel.y)).r);
  return normalize(vec3<f32>(dl - dr, db - dt, 2.0 * texel.x * dc));
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let depth = textureSample(depth_tex, tex_smp, i.uv).r;
  if (depth >= 1.0) { return vec4<f32>(0.0); }
  let lin_depth = linearize(depth);
  let normal = reconstructNormal(i.uv);
  let view_dir = normalize(vec3<f32>(i.uv * 2.0 - 1.0, 1.0));
  let reflect_dir = reflect(view_dir, normal);
  let texel = vec2<f32>(1.0 / params.screen_w, 1.0 / params.screen_h);
  var ray_uv = i.uv;
  var ray_depth = lin_depth;
  let num_steps = i32(params.max_steps);
  for (var step = 0; step < num_steps; step++) {
    ray_uv += reflect_dir.xy * texel * params.step_size;
    if (ray_uv.x < 0.0 || ray_uv.x > 1.0 || ray_uv.y < 0.0 || ray_uv.y > 1.0) { break; }
    let sample_depth = linearize(textureSample(depth_tex, tex_smp, ray_uv).r);
    ray_depth += reflect_dir.z * params.step_size;
    let depth_diff = ray_depth - sample_depth;
    if (depth_diff > 0.0 && depth_diff < params.thickness) {
      let step_fade = 1.0 - f32(step) / params.max_steps;
      let edge = smoothstep(vec2<f32>(0.0), vec2<f32>(params.fade_distance), ray_uv)
               * (vec2<f32>(1.0) - smoothstep(vec2<f32>(1.0 - params.fade_distance), vec2<f32>(1.0), ray_uv));
      let screen_fade = edge.x * edge.y;
      let fade = step_fade * screen_fade;
      let hit_color = textureSample(color_tex, tex_smp, ray_uv).rgb;
      return vec4<f32>(hit_color * fade, fade);
    }
  }
  return vec4<f32>(0.0);
}
)WGSL";

// ── Contact Shadows ──────────────────────────────────────────────────────────
inline const char* kWgslContactShadow = R"WGSL(// dse-wgsl
struct Params {
  light_dir_x : f32, light_dir_y : f32, light_dir_z : f32,
  near_p : f32, far_p : f32, screen_w : f32, screen_h : f32,
  strength : f32, num_steps : f32, step_size : f32,
};
@group(2) @binding(0) var<uniform> params : Params;
@group(2) @binding(1) var depth_tex : texture_2d<f32>;
@group(2) @binding(2) var depth_smp : sampler;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = vec2<f32>(uv.x, 1.0 - uv.y); return o;
}
fn linearize(d : f32) -> f32 {
  let z = d * 2.0 - 1.0;
  return (2.0 * params.near_p * params.far_p) / (params.far_p + params.near_p - z * (params.far_p - params.near_p));
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let depth = textureSample(depth_tex, depth_smp, i.uv).r;
  if (depth >= 1.0) { return vec4<f32>(1.0); }
  let linDepth = linearize(depth);
  let lightDir = normalize(vec3<f32>(params.light_dir_x, params.light_dir_y, params.light_dir_z));
  let texelSize = vec2<f32>(1.0 / params.screen_w, 1.0 / params.screen_h);
  var occlusion = 0.0;
  var validSteps = 0;
  let steps = i32(params.num_steps);
  for (var idx = 1; idx <= steps; idx++) {
    let dist = params.step_size * f32(idx);
    let sampleUV = i.uv + lightDir.xy * texelSize * dist * 50.0;
    if (sampleUV.x < 0.0 || sampleUV.y < 0.0 || sampleUV.x > 1.0 || sampleUV.y > 1.0) { break; }
    let sampleDepth = textureSample(depth_tex, depth_smp, sampleUV).r;
    if (sampleDepth >= 1.0) { continue; }
    let sampleLin = linearize(sampleDepth);
    let diff = sampleLin - linDepth;
    if (diff > 0.0 && diff < params.step_size) {
      let k = 1.0 - (diff / params.step_size);
      occlusion += k * k;
    }
    validSteps++;
  }
  var shadow = 1.0;
  if (validSteps > 0) {
    shadow = 1.0 - clamp(occlusion / f32(validSteps) * params.strength, 0.0, 1.0);
  }
  return vec4<f32>(vec3<f32>(shadow), 1.0);
}
)WGSL";

// ── Shadow depth-only (GPU-driven path) ──────────────────────────────────────
inline const char* kWgslShadowDepth = R"WGSL(// dse-wgsl
struct PerFrame { vp : mat4x4<f32>, view : mat4x4<f32>, camera_pos : vec4<f32>, foliage_wind : vec4<f32>, foliage_push : vec4<f32> };
struct Inst { model : mat4x4<f32>, material_id : u32, cmd_id : u32, pad0 : u32, pad1 : u32 };
@group(0) @binding(0) var<uniform> per_frame : PerFrame;
@group(3) @binding(5) var<storage, read> instances : array<Inst>;
@vertex fn vs_main(@location(0) pos : vec3<f32>, @builtin(instance_index) inst : u32) -> @builtin(position) vec4<f32> {
  let world_pos = instances[inst].model * vec4<f32>(pos, 1.0);
  return per_frame.vp * world_pos;
}
)WGSL";

// ── DDGI Probe Update Compute ────────────────────────────────────────────────
inline const char* kWgslDdgiProbeUpdate = R"WGSL(// dse-wgsl
struct Params {
  probe_count_x : u32, probe_count_y : u32, probe_count_z : u32, rays_per_probe : u32,
  grid_origin_x : f32, grid_origin_y : f32, grid_origin_z : f32, grid_spacing : f32,
  hysteresis : f32, depth_sharpness : f32, irradiance_texels : u32, depth_texels : u32,
  normal_bias : f32, view_bias : f32,
};
@group(0) @binding(0) var<uniform> params : Params;
@group(0) @binding(1) var<storage, read> probe_states : array<u32>;
@group(0) @binding(2) var irradiance_out : texture_storage_2d<rgba16float, write>;
@group(0) @binding(3) var depth_out : texture_storage_2d<rg16float, write>;
@group(0) @binding(4) var<storage, read> ray_data : array<vec4<f32>>;

@compute @workgroup_size(8, 8, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let probe_idx = gid.z;
  let total_probes = params.probe_count_x * params.probe_count_y * params.probe_count_z;
  if (probe_idx >= total_probes) { return; }
  let texel = vec2<u32>(gid.x, gid.y);
  let irr_size = params.irradiance_texels;
  if (texel.x >= irr_size || texel.y >= irr_size) { return; }
  let tc = (vec2<f32>(texel) + vec2<f32>(0.5)) / f32(irr_size) * 2.0 - 1.0;
  let oct_dir = normalize(vec3<f32>(tc.x, tc.y, 1.0 - abs(tc.x) - abs(tc.y)));
  var irradiance = vec3<f32>(0.0);
  var weight_sum = 0.0;
  for (var r = 0u; r < params.rays_per_probe; r++) {
    let ray_idx = probe_idx * params.rays_per_probe + r;
    let ray_result = ray_data[ray_idx];
    let ray_dir = normalize(ray_result.xyz);
    let w = max(dot(oct_dir, ray_dir), 0.0);
    if (w > 0.0) {
      irradiance += ray_result.xyz * w;
      weight_sum += w;
    }
  }
  if (weight_sum > 0.0) { irradiance /= weight_sum; }
  let out_coord = vec2<i32>(i32(probe_idx * irr_size + texel.x), i32(texel.y));
  textureStore(irradiance_out, out_coord, vec4<f32>(irradiance, 1.0));
}
)WGSL";

}  // namespace dse::render::wgsl
