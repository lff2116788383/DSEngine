#version 450
#extension GL_ARB_separate_shader_objects : enable
// @SSBO_LOW_REGISTERS
// B2b-3: 硬件实例化 forward PBR 顶点着色器（复用 forward_pbr.frag）。
//
// 顶点为局部空间；每实例 model 矩阵走 SSBO，按 gl_InstanceIndex 取出后施 model + vp。
// 契约（RHI_PRIMITIVE_CONTRACT §6）：DX11 SV_InstanceID 始终从 0 起，base-instance
// 偏移须经 SSBO 偏移表达，不能靠 first_instance 取数；故此处恒以 0 基索引 SSBO，
// 调用方 DrawIndexedInstanced(first_instance=0)。
//
// 实例 SSBO 置于 set3.binding0，使一次通用原语 BindStorageBuffer(0) 在三后端命中：
//   GL   spirv-cross 保留源 binding=0 → glBindBufferBase(0)
//   VK   通用原语按位置映射，单 SSBO 即第 0 个 → slot 0
//   DX11 经 @SSBO_LOW_REGISTERS → register(t0)（VS-only 绑定，不撞 PS 纹理 t0..t4）

layout(location = 0) in vec3 aPos;        // local-space position
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aNormal;     // local-space normal
layout(location = 4) in vec3 aTangent;    // local-space tangent

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;
layout(location = 2) out vec3 vWorldPos;
layout(location = 3) out vec3 vNormal;
layout(location = 4) out vec3 vTangent;

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
    vec4 foliage_wind;
    vec4 foliage_push;
};

layout(std430, set = 3, binding = 0) readonly buffer InstanceModels {
    mat4 u_models[];   // 每实例 model 矩阵（世界空间）
};

void main() {
    mat4 model = u_models[gl_InstanceIndex];
    vec4 worldPos = model * vec4(aPos, 1.0);
    mat3 model3 = mat3(model);

    vWorldPos = worldPos.xyz;
    vNormal   = normalize(model3 * aNormal);
    vTangent  = model3 * aTangent;
    vColor    = aColor;
    vTexCoord = aTexCoord;
    gl_Position = vp * worldPos;
}
