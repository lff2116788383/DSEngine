#version 450
#extension GL_ARB_separate_shader_objects : enable
// @SSBO_LOW_REGISTERS
// B3: SSBO 驱动的 3D 粒子广告牌顶点着色器（取代 per-instance 顶点属性的 particle.vert）。
//
// 与 forward_shaded_instanced.vert 同约定：每实例数据走 set7.binding0 readonly SSBO
// （通用原语 BindStorageBuffer(0) → GL binding0 / VK slot0(set7 为首个空 set) / DX11 register(t0)），
// 按 gl_InstanceIndex 取每实例 world pos + size + color；view 矩阵提取相机右/上轴做广告牌。
// 配 particle.frag（set2.binding1 sampler2D → 通用原语 BindTexture(slot 0)）。
//
// 契约（RHI_PRIMITIVE_CONTRACT §6）：DX11 SV_InstanceID 从 0 起，base-instance 偏移须经
// SSBO 偏移表达；本着色器恒 0 基引 SSBO，配 DrawIndexedInstanced(first_instance=0)。

layout(location = 0) in vec3 aPos;       // 单位 quad 局部坐标（x,y ∈ [-0.5,0.5]，z=0）
layout(location = 1) in vec2 aTexCoord;

layout(location = 0) out vec4 vParticleColor;
layout(location = 1) out vec2 vTexCoord;

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
    vec4 foliage_wind;
    vec4 foliage_push;
};

// 每实例：xyz = world position，w = size；color = 顶点色（与纹理相乘）。
struct ParticleInstance {
    vec4 pos_size;
    vec4 color;
};
layout(std430, set = 7, binding = 0) readonly buffer ParticleInstances {
    ParticleInstance u_instances[];
};

void main() {
    ParticleInstance inst = u_instances[gl_InstanceIndex];

    // 从 view 矩阵列取相机右/上轴（view 正交基的转置即相机轴），做朝相机广告牌。
    vec3 camera_right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camera_up    = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 world = inst.pos_size.xyz
        + camera_right * aPos.x * inst.pos_size.w
        + camera_up    * aPos.y * inst.pos_size.w;

    gl_Position = vp * vec4(world, 1.0);
    vParticleColor = inst.color;
    vTexCoord = aTexCoord;
}
