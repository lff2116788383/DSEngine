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
// Pass 1: Verlet Integration + Gravity + Wind
// ============================================================
static const char* kHairIntegrateSource = R"(
#version 430 core
layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer PositionCurrent { vec4 pos_cur[]; };
layout(std430, binding = 1) buffer PositionPrev    { vec4 pos_prev[]; };
layout(std430, binding = 2) readonly buffer PositionRest { vec4 pos_rest[]; };
layout(std430, binding = 3) readonly buffer StrandInfo   { uvec2 strand_info[]; };

uniform int   u_num_vertices;
uniform float u_dt;
uniform float u_damping;
uniform float u_gx, u_gy, u_gz, u_gw;  // gravity dir(xyz) + magnitude(w)
uniform float u_wx, u_wy, u_wz, u_ww;  // wind vec(xyz) + turbulence(w)
uniform float u_time;

// Simple hash for per-vertex wind variation
float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main() {
    uint vid = gl_GlobalInvocationID.x;
    if (int(vid) >= u_num_vertices) return;

    vec4 cur = pos_cur[vid];
    vec4 prev = pos_prev[vid];
    vec4 rest = pos_rest[vid];

    // Determine if this is a root vertex (first vertex of its strand)
    // w component of position stores rest_length to previous vertex:
    // root vertex has w=0.0, non-root has w=seg_length>0
    bool is_root = (rest.w < 0.001);

    if (is_root) {
        // Root vertices are fixed to their rest position
        // (world transform is applied on CPU side before upload)
        pos_prev[vid] = cur;
        return;
    }

    // Verlet integration
    vec3 velocity = (cur.xyz - prev.xyz) * (1.0 - u_damping);

    // Gravity
    vec3 gravity_force = vec3(u_gx, u_gy, u_gz) * u_gw * u_dt * u_dt;

    // Wind with turbulence
    float wind_var = hash11(float(vid) * 0.37 + u_time * 1.7) * 2.0 - 1.0;
    vec3 wind_force = vec3(u_wx, u_wy, u_wz) * u_dt * u_dt * (1.0 + wind_var * u_ww);

    vec3 new_pos = cur.xyz + velocity + gravity_force + wind_force;

    pos_prev[vid] = cur;
    pos_cur[vid] = vec4(new_pos, cur.w);
}
)";

// ============================================================
// Pass 2: Edge Length Constraints (1 thread per strand)
// ============================================================
static const char* kHairLengthConstraintSource = R"(
#version 430 core
layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer PositionCurrent { vec4 pos_cur[]; };
layout(std430, binding = 1) readonly buffer PositionRest { vec4 pos_rest[]; };
layout(std430, binding = 2) readonly buffer StrandInfo   { uvec2 strand_info[]; };

uniform int u_num_strands;

void main() {
    uint sid = gl_GlobalInvocationID.x;
    if (int(sid) >= u_num_strands) return;

    uint offset = strand_info[sid].x;
    uint count  = strand_info[sid].y;
    if (count < 2) return;

    // Forward pass: enforce edge lengths from root to tip
    for (uint i = 0; i < count - 1; ++i) {
        uint i0 = offset + i;
        uint i1 = offset + i + 1;

        vec3 p0 = pos_cur[i0].xyz;
        vec3 p1 = pos_cur[i1].xyz;

        float rest_length = length(pos_rest[i1].xyz - pos_rest[i0].xyz);
        vec3 delta = p1 - p0;
        float cur_length = length(delta);

        if (cur_length < 1e-7) continue;

        // Move child vertex to maintain rest length
        vec3 dir = delta / cur_length;
        vec3 target = p0 + dir * rest_length;

        // Root is fixed, only move child
        if (i == 0) {
            pos_cur[i1] = vec4(target, pos_cur[i1].w);
        } else {
            // Split correction between parent and child
            vec3 correction = (target - p1) * 0.5;
            pos_cur[i0] = vec4(p0 - correction, pos_cur[i0].w);
            pos_cur[i1] = vec4(p1 + correction, pos_cur[i1].w);
        }
    }
}
)";

// ============================================================
// Pass 3: Local Shape Constraints (1 thread per strand)
// ============================================================
static const char* kHairLocalShapeSource = R"(
#version 430 core
layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer PositionCurrent { vec4 pos_cur[]; };
layout(std430, binding = 1) readonly buffer PositionRest { vec4 pos_rest[]; };
layout(std430, binding = 2) readonly buffer StrandInfo   { uvec2 strand_info[]; };

uniform int   u_num_strands;
uniform float u_stiffness_local;
uniform float u_stiffness_global;

void main() {
    uint sid = gl_GlobalInvocationID.x;
    if (int(sid) >= u_num_strands) return;

    uint offset = strand_info[sid].x;
    uint count  = strand_info[sid].y;
    if (count < 2) return;

    vec3 root_cur  = pos_cur[offset].xyz;
    vec3 root_rest = pos_rest[offset].xyz;
    vec3 root_offset = root_cur - root_rest;

    // Local shape: pull each vertex toward (rest_position + root_offset)
    // This approximates maintaining the local shape relative to the root
    for (uint i = 1; i < count; ++i) {
        uint idx = offset + i;
        vec3 cur = pos_cur[idx].xyz;
        vec3 rest = pos_rest[idx].xyz;

        // Local target: rest position translated by root movement
        vec3 local_target = rest + root_offset;

        // Global target: original rest position (keeps hair in place)
        vec3 global_target = rest;

        // Blend toward targets
        vec3 new_pos = cur;
        new_pos = mix(new_pos, local_target, u_stiffness_local * 0.1);
        new_pos = mix(new_pos, global_target, u_stiffness_global * 0.02);

        pos_cur[idx] = vec4(new_pos, pos_cur[idx].w);
    }
}
)";

// ============================================================
// Pass 4: Compute Tangents (1 thread per vertex)
// ============================================================
static const char* kHairUpdateTangentSource = R"(
#version 430 core
layout(local_size_x = 64) in;

layout(std430, binding = 0) readonly buffer PositionCurrent { vec4 pos_cur[]; };
layout(std430, binding = 1) buffer Tangent { vec4 tangent[]; };
layout(std430, binding = 2) readonly buffer StrandInfo { uvec2 strand_info[]; };

uniform int u_num_vertices;
uniform int u_num_strands;
uniform int u_verts_per_strand;  // TressFX: all strands have equal vertex count

void main() {
    uint vid = gl_GlobalInvocationID.x;
    if (int(vid) >= u_num_vertices) return;

    // O(1) strand lookup: all strands have equal vertex count
    uint strand_idx = vid / uint(u_verts_per_strand);
    uint local_idx  = vid % uint(u_verts_per_strand);
    if (int(strand_idx) >= u_num_strands) return;

    uint offset = strand_info[strand_idx].x;
    uint count  = strand_info[strand_idx].y;

    vec3 t;
    if (local_idx < count - 1) {
        // Forward difference
        t = normalize(pos_cur[vid + 1].xyz - pos_cur[vid].xyz);
    } else if (local_idx > 0) {
        // Backward difference for last vertex
        t = normalize(pos_cur[vid].xyz - pos_cur[vid - 1].xyz);
    } else {
        t = vec3(0.0, 1.0, 0.0);
    }

    float thickness = tangent[vid].w; // preserve thickness (root-to-tip interpolation)
    tangent[vid] = vec4(t, thickness);
}
)";

// ============================================================
// ===  Vulkan GLSL 450 变体  ===================================
// binding 重映射为 0-based 连续（避免 Vulkan descriptor gap）
// push constant 全部用标量，与 CPU SetComputeUniform* 顺序一致
// ============================================================

// Pass 1 VK: bindings {0=pos_cur,1=pos_prev,2=pos_rest,3=strand_info}  pc=48B
static const char* kHairIntegrateSourceVK = R"(
#version 450
layout(local_size_x = 64) in;
layout(set=0,binding=0,std430) buffer PosCur  { vec4 pos_cur[]; };
layout(set=0,binding=1,std430) buffer PosPrev { vec4 pos_prev[]; };
layout(set=0,binding=2,std430) readonly buffer PosRest  { vec4 pos_rest[]; };
layout(set=0,binding=3,std430) readonly buffer StrandInfo{ uvec2 strand_info[]; };
layout(push_constant) uniform PC {
    int   u_num_vertices; float u_dt; float u_damping;
    float u_gx; float u_gy; float u_gz; float u_gw;
    float u_wx; float u_wy; float u_wz; float u_ww;
    float u_time;
} pc;
float hash11(float p){p=fract(p*0.1031);p*=p+33.33;p*=p+p;return fract(p);}
void main(){
    uint vid=gl_GlobalInvocationID.x;
    if(int(vid)>=pc.u_num_vertices)return;
    vec4 cur=pos_cur[vid],prev=pos_prev[vid],rest=pos_rest[vid];
    if(rest.w<0.001){pos_prev[vid]=cur;return;}
    vec3 vel=(cur.xyz-prev.xyz)*(1.0-pc.u_damping);
    vec3 grav=vec3(pc.u_gx,pc.u_gy,pc.u_gz)*pc.u_gw*pc.u_dt*pc.u_dt;
    float wv=hash11(float(vid)*0.37+pc.u_time*1.7)*2.0-1.0;
    vec3 wind=vec3(pc.u_wx,pc.u_wy,pc.u_wz)*pc.u_dt*pc.u_dt*(1.0+wv*pc.u_ww);
    pos_prev[vid]=cur;
    pos_cur[vid]=vec4(cur.xyz+vel+grav+wind,cur.w);
}
)";

// Pass 2 VK: bindings {0=pos_cur,1=pos_rest,2=strand_info}  pc=4B
static const char* kHairLengthConstraintSourceVK = R"(
#version 450
layout(local_size_x = 64) in;
layout(set=0,binding=0,std430) buffer PosCur { vec4 pos_cur[]; };
layout(set=0,binding=1,std430) readonly buffer PosRest   { vec4 pos_rest[]; };
layout(set=0,binding=2,std430) readonly buffer StrandInfo{ uvec2 strand_info[]; };
layout(push_constant) uniform PC { int u_num_strands; } pc;
void main(){
    uint sid=gl_GlobalInvocationID.x;
    if(int(sid)>=pc.u_num_strands)return;
    uint off=strand_info[sid].x, cnt=strand_info[sid].y;
    if(cnt<2)return;
    for(uint i=0;i<cnt-1;++i){
        uint i0=off+i,i1=off+i+1;
        vec3 p0=pos_cur[i0].xyz,p1=pos_cur[i1].xyz;
        float rl=length(pos_rest[i1].xyz-pos_rest[i0].xyz);
        vec3 d=p1-p0;float cl=length(d);
        if(cl<1e-7)continue;
        vec3 tgt=p0+d/cl*rl;
        if(i==0){pos_cur[i1]=vec4(tgt,pos_cur[i1].w);}
        else{vec3 c=(tgt-p1)*0.5;pos_cur[i0]=vec4(p0-c,pos_cur[i0].w);pos_cur[i1]=vec4(p1+c,pos_cur[i1].w);}
    }
}
)";

// Pass 3 VK: bindings {0=pos_cur,1=pos_rest,2=strand_info}  pc=12B
static const char* kHairLocalShapeSourceVK = R"(
#version 450
layout(local_size_x = 64) in;
layout(set=0,binding=0,std430) buffer PosCur { vec4 pos_cur[]; };
layout(set=0,binding=1,std430) readonly buffer PosRest   { vec4 pos_rest[]; };
layout(set=0,binding=2,std430) readonly buffer StrandInfo{ uvec2 strand_info[]; };
layout(push_constant) uniform PC { int u_num_strands; float u_sl; float u_sg; } pc;
void main(){
    uint sid=gl_GlobalInvocationID.x;
    if(int(sid)>=pc.u_num_strands)return;
    uint off=strand_info[sid].x, cnt=strand_info[sid].y;
    if(cnt<2)return;
    vec3 rc=pos_cur[off].xyz, rr=pos_rest[off].xyz, ro=rc-rr;
    for(uint i=1;i<cnt;++i){
        uint idx=off+i;
        vec3 c=pos_cur[idx].xyz, r=pos_rest[idx].xyz;
        vec3 n=c;
        n=mix(n,r+ro,pc.u_sl*0.1);
        n=mix(n,r,pc.u_sg*0.02);
        pos_cur[idx]=vec4(n,pos_cur[idx].w);
    }
}
)";

// Pass 4 VK: bindings {0=pos_cur,1=tangent,2=strand_info}  pc=12B
static const char* kHairUpdateTangentSourceVK = R"(
#version 450
layout(local_size_x = 64) in;
layout(set=0,binding=0,std430) readonly buffer PosCur    { vec4 pos_cur[]; };
layout(set=0,binding=1,std430) buffer Tangent            { vec4 tangent[]; };
layout(set=0,binding=2,std430) readonly buffer StrandInfo{ uvec2 strand_info[]; };
layout(push_constant) uniform PC { int u_num_vertices; int u_num_strands; int u_vps; } pc;
void main(){
    uint vid=gl_GlobalInvocationID.x;
    if(int(vid)>=pc.u_num_vertices)return;
    uint si=vid/uint(pc.u_vps), li=vid%uint(pc.u_vps);
    if(int(si)>=pc.u_num_strands)return;
    uint cnt=strand_info[si].y;
    vec3 t;
    if(li<cnt-1)      t=normalize(pos_cur[vid+1].xyz-pos_cur[vid].xyz);
    else if(li>0)     t=normalize(pos_cur[vid].xyz-pos_cur[vid-1].xyz);
    else              t=vec3(0,1,0);
    tangent[vid]=vec4(t,tangent[vid].w);
}
)";

// ============================================================
// ===  HLSL CS 5.0 变体（D3D11 cbuffer + StructuredBuffer） ===
// SSBOs 均以 StructuredBuffer<float4> 绑定（t16-t20），
// 写入操作需要 DX11 RWStructuredBuffer UAV 扩展（pending）。
// ============================================================

// Pass 1 HLSL
static const char* kHairIntegrateSourceHLSL = R"(
cbuffer Params : register(b0) {
    int u_num_vertices; float u_dt; float u_damping;
    float u_gx,u_gy,u_gz,u_gw, u_wx,u_wy,u_wz,u_ww, u_time;
};
StructuredBuffer<float4> pos_rest   : register(t18);
StructuredBuffer<uint2>  strand_info: register(t20);
RWStructuredBuffer<float4> pos_cur  : register(u0);
RWStructuredBuffer<float4> pos_prev : register(u1);
float hash11(float p){p=frac(p*0.1031f);p*=p+33.33f;p*=p+p;return frac(p);}
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID){
    int vid=(int)id.x;
    if(vid>=u_num_vertices)return;
    float4 cur=pos_cur[vid],prev=pos_prev[vid],rest=pos_rest[vid];
    if(rest.w<0.001f){pos_prev[vid]=cur;return;}
    float3 vel=(cur.xyz-prev.xyz)*(1-u_damping);
    float3 grav=float3(u_gx,u_gy,u_gz)*u_gw*u_dt*u_dt;
    float wv=hash11((float)vid*0.37f+u_time*1.7f)*2-1;
    float3 wind=float3(u_wx,u_wy,u_wz)*u_dt*u_dt*(1+wv*u_ww);
    pos_prev[vid]=cur;
    pos_cur[vid]=float4(cur.xyz+vel+grav+wind,cur.w);
}
)";

// Pass 2 HLSL
static const char* kHairLengthConstraintSourceHLSL = R"(
cbuffer Params : register(b0) { int u_num_strands; int _p0; int _p1; int _p2; };
StructuredBuffer<float4> pos_rest   : register(t17);
StructuredBuffer<uint2>  strand_info: register(t20);
RWStructuredBuffer<float4> pos_cur  : register(u0);
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID){
    int sid=(int)id.x;
    if(sid>=u_num_strands)return;
    uint off=strand_info[sid].x, cnt=strand_info[sid].y;
    if(cnt<2)return;
    for(uint i=0;i<cnt-1;++i){
        uint i0=off+i,i1=off+i+1;
        float3 p0=pos_cur[i0].xyz,p1=pos_cur[i1].xyz;
        float rl=length(pos_rest[i1].xyz-pos_rest[i0].xyz);
        float3 d=p1-p0;float cl=length(d);
        if(cl<1e-7f)continue;
        float3 tgt=p0+d/cl*rl;
        if(i==0)pos_cur[i1]=float4(tgt,pos_cur[i1].w);
        else{float3 c=(tgt-p1)*0.5f;pos_cur[i0]=float4(p0-c,pos_cur[i0].w);pos_cur[i1]=float4(p1+c,pos_cur[i1].w);}
    }
}
)";

// Pass 3 HLSL
static const char* kHairLocalShapeSourceHLSL = R"(
cbuffer Params : register(b0) { int u_num_strands; float u_sl; float u_sg; int _p; };
StructuredBuffer<float4> pos_rest   : register(t17);
StructuredBuffer<uint2>  strand_info: register(t20);
RWStructuredBuffer<float4> pos_cur  : register(u0);
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID){
    int sid=(int)id.x;
    if(sid>=u_num_strands)return;
    uint off=strand_info[sid].x,cnt=strand_info[sid].y;
    if(cnt<2)return;
    float3 rc=pos_cur[off].xyz,rr=pos_rest[off].xyz,ro=rc-rr;
    for(uint i=1;i<cnt;++i){
        uint idx=off+i;
        float3 c=pos_cur[idx].xyz,r=pos_rest[idx].xyz;
        float3 n=c;
        n=lerp(n,r+ro,u_sl*0.1f);
        n=lerp(n,r,u_sg*0.02f);
        pos_cur[idx]=float4(n,pos_cur[idx].w);
    }
}
)";

// Pass 4 HLSL
static const char* kHairUpdateTangentSourceHLSL = R"(
cbuffer Params : register(b0) { int u_num_vertices; int u_num_strands; int u_vps; int _p; };
StructuredBuffer<float4>   pos_cur    : register(t16);
StructuredBuffer<uint2>    strand_info: register(t20);
RWStructuredBuffer<float4> tangent    : register(u0);
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID){
    int vid=(int)id.x;
    if(vid>=u_num_vertices)return;
    uint si=id.x/(uint)u_vps,li=id.x%(uint)u_vps;
    if((int)si>=u_num_strands)return;
    uint cnt=strand_info[si].y;
    float3 t;
    if(li<cnt-1)    t=normalize(pos_cur[vid+1].xyz-pos_cur[vid].xyz);
    else if(li>0)   t=normalize(pos_cur[vid].xyz-pos_cur[vid-1].xyz);
    else            t=float3(0,1,0);
    tangent[vid]=float4(t,tangent[vid].w);
}
)";

} // namespace render
} // namespace dse

#endif // DSE_RENDER_HAIR_COMPUTE_SHADERS_H
