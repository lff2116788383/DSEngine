/**
 * @file hair_compute_shaders.h
 * @brief TressFX 风格毛发模拟 Compute Shader 源码 (GLSL 430)
 *
 * 4 个 Compute Pass:
 * 1. Integration — Verlet 积分 + 重力 + 风力
 * 2. Length Constraint — 边长保持（每 strand 一个线程，迭代）
 * 3. Local Shape Constraint — 局部形状保持（向旋转后的静止姿态拉回）
 * 4. Update Tangent — 从位置计算切线
 *
 * SSBO 布局:
 *   binding 0: vec4 position_current[num_vertices]
 *   binding 1: vec4 position_prev[num_vertices]
 *   binding 2: vec4 position_rest[num_vertices]  (readonly)
 *   binding 3: vec4 tangent[num_vertices]
 *   binding 4: uvec2 strand_info[num_strands]    (readonly, .x=vertex_offset, .y=vertex_count)
 */

#ifndef DSE_RENDER_HAIR_COMPUTE_SHADERS_H
#define DSE_RENDER_HAIR_COMPUTE_SHADERS_H

namespace dse {
namespace render {

// ============================================================
// ===  WebGPU WGSL 变体（手译，无离线 GLSL→WGSL 工具） =========
// 与 src/hair_*.comp（GL430/VK450/HLSL 单源）逐句一致。SSBO 走 group3 b0.. （WebGPU RHI
// compute 统一 BufferBindingType_Storage → 全部声明 read_write，含只读输入与
// pass1 未用但绑定齐全的 strand_info）；命名 uniform 走 group1 b8 保留 binding，
// 各成员 @align(16)、声明同名同序，对齐 SetComputeUniform* 调用序的 16B 定位
// （见 webgpu_rhi_device.cpp kComputeNamedUboBinding=8）。保留字 target→tgt。
// ============================================================

// Pass 1 WGSL: SSBO {0=pos_cur,1=pos_prev,2=pos_rest,3=strand_info}  uniform 12 成员
static const char* kHairIntegrateSourceWGSL = R"WGSL(// dse-wgsl
@group(3) @binding(0) var<storage, read_write> pos_cur : array<vec4<f32>>;
@group(3) @binding(1) var<storage, read_write> pos_prev : array<vec4<f32>>;
@group(3) @binding(2) var<storage, read_write> pos_rest : array<vec4<f32>>;
@group(3) @binding(3) var<storage, read_write> strand_info : array<vec2<u32>>;
struct PC {
  @align(16) u_num_vertices : i32,
  @align(16) u_dt : f32,
  @align(16) u_damping : f32,
  @align(16) u_gx : f32,
  @align(16) u_gy : f32,
  @align(16) u_gz : f32,
  @align(16) u_gw : f32,
  @align(16) u_wx : f32,
  @align(16) u_wy : f32,
  @align(16) u_wz : f32,
  @align(16) u_ww : f32,
  @align(16) u_time : f32,
};
@group(1) @binding(8) var<uniform> pc : PC;
fn hash11(p_in : f32) -> f32 {
  var p = fract(p_in * 0.1031);
  p = p * (p + 33.33);
  p = p * (p + p);
  return fract(p);
}
@compute @workgroup_size(64, 1, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let vid = gid.x;
  if (i32(vid) >= pc.u_num_vertices) { return; }
  let cur = pos_cur[vid];
  let prev = pos_prev[vid];
  let rest = pos_rest[vid];
  if (rest.w < 0.001) {
    pos_prev[vid] = cur;
    return;
  }
  let velocity = (cur.xyz - prev.xyz) * (1.0 - pc.u_damping);
  let gravity_force = vec3<f32>(pc.u_gx, pc.u_gy, pc.u_gz) * pc.u_gw * pc.u_dt * pc.u_dt;
  let wind_var = hash11(f32(vid) * 0.37 + pc.u_time * 1.7) * 2.0 - 1.0;
  let wind_force = vec3<f32>(pc.u_wx, pc.u_wy, pc.u_wz) * pc.u_dt * pc.u_dt * (1.0 + wind_var * pc.u_ww);
  let new_pos = cur.xyz + velocity + gravity_force + wind_force;
  pos_prev[vid] = cur;
  pos_cur[vid] = vec4<f32>(new_pos, cur.w);
}
)WGSL";

// Pass 2 WGSL: SSBO {0=pos_cur,1=pos_rest,2=strand_info}  uniform 1 成员
static const char* kHairLengthConstraintSourceWGSL = R"WGSL(// dse-wgsl
@group(3) @binding(0) var<storage, read_write> pos_cur : array<vec4<f32>>;
@group(3) @binding(1) var<storage, read_write> pos_rest : array<vec4<f32>>;
@group(3) @binding(2) var<storage, read_write> strand_info : array<vec2<u32>>;
struct PC { @align(16) u_num_strands : i32, };
@group(1) @binding(8) var<uniform> pc : PC;
@compute @workgroup_size(64, 1, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let sid = gid.x;
  if (i32(sid) >= pc.u_num_strands) { return; }
  let off = strand_info[sid].x;
  let cnt = strand_info[sid].y;
  if (cnt < 2u) { return; }
  for (var i : u32 = 0u; i < cnt - 1u; i = i + 1u) {
    let i0 = off + i;
    let i1 = off + i + 1u;
    let p0 = pos_cur[i0].xyz;
    let p1 = pos_cur[i1].xyz;
    let rl = length(pos_rest[i1].xyz - pos_rest[i0].xyz);
    let d = p1 - p0;
    let cl = length(d);
    if (cl < 1e-7) { continue; }
    let tgt = p0 + d / cl * rl;
    if (i == 0u) {
      pos_cur[i1] = vec4<f32>(tgt, pos_cur[i1].w);
    } else {
      let c = (tgt - p1) * 0.5;
      pos_cur[i0] = vec4<f32>(p0 - c, pos_cur[i0].w);
      pos_cur[i1] = vec4<f32>(p1 + c, pos_cur[i1].w);
    }
  }
}
)WGSL";

// Pass 3 WGSL: SSBO {0=pos_cur,1=pos_rest,2=strand_info}  uniform 3 成员
static const char* kHairLocalShapeSourceWGSL = R"WGSL(// dse-wgsl
@group(3) @binding(0) var<storage, read_write> pos_cur : array<vec4<f32>>;
@group(3) @binding(1) var<storage, read_write> pos_rest : array<vec4<f32>>;
@group(3) @binding(2) var<storage, read_write> strand_info : array<vec2<u32>>;
struct PC {
  @align(16) u_num_strands : i32,
  @align(16) u_stiffness_local : f32,
  @align(16) u_stiffness_global : f32,
};
@group(1) @binding(8) var<uniform> pc : PC;
@compute @workgroup_size(64, 1, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let sid = gid.x;
  if (i32(sid) >= pc.u_num_strands) { return; }
  let off = strand_info[sid].x;
  let cnt = strand_info[sid].y;
  if (cnt < 2u) { return; }
  let rc = pos_cur[off].xyz;
  let rr = pos_rest[off].xyz;
  let ro = rc - rr;
  for (var i : u32 = 1u; i < cnt; i = i + 1u) {
    let idx = off + i;
    let c = pos_cur[idx].xyz;
    let r = pos_rest[idx].xyz;
    var n = c;
    n = mix(n, r + ro, pc.u_stiffness_local * 0.1);
    n = mix(n, r, pc.u_stiffness_global * 0.02);
    pos_cur[idx] = vec4<f32>(n, pos_cur[idx].w);
  }
}
)WGSL";

// Pass 4 WGSL: SSBO {0=pos_cur,1=tangent,2=strand_info}  uniform 3 成员
static const char* kHairUpdateTangentSourceWGSL = R"WGSL(// dse-wgsl
@group(3) @binding(0) var<storage, read_write> pos_cur : array<vec4<f32>>;
@group(3) @binding(1) var<storage, read_write> tangent : array<vec4<f32>>;
@group(3) @binding(2) var<storage, read_write> strand_info : array<vec2<u32>>;
struct PC {
  @align(16) u_num_vertices : i32,
  @align(16) u_num_strands : i32,
  @align(16) u_verts_per_strand : i32,
};
@group(1) @binding(8) var<uniform> pc : PC;
@compute @workgroup_size(64, 1, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let vid = gid.x;
  if (i32(vid) >= pc.u_num_vertices) { return; }
  let si = vid / u32(pc.u_verts_per_strand);
  let li = vid % u32(pc.u_verts_per_strand);
  if (i32(si) >= pc.u_num_strands) { return; }
  let cnt = strand_info[si].y;
  var t : vec3<f32>;
  if (li < cnt - 1u) {
    t = normalize(pos_cur[vid + 1u].xyz - pos_cur[vid].xyz);
  } else if (li > 0u) {
    t = normalize(pos_cur[vid].xyz - pos_cur[vid - 1u].xyz);
  } else {
    t = vec3<f32>(0.0, 1.0, 0.0);
  }
  let thickness = tangent[vid].w;
  tangent[vid] = vec4<f32>(t, thickness);
}
)WGSL";

} // namespace render
} // namespace dse

#endif // DSE_RENDER_HAIR_COMPUTE_SHADERS_H
