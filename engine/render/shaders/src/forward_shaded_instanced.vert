#version 450
#extension GL_ARB_separate_shader_objects : enable
// @SSBO_LOW_REGISTERS
// Final-Feat-3: 硬件实例化 + 高级 shading 组合顶点着色器（配 forward_shaded.frag）。
//
// 与 forward_pbr_instanced.vert 同算：顶点为局部空间，每实例 model 矩阵走 SSBO，
// 按 gl_InstanceIndex 取出后施 model + vp；varying 接口（location 0..4）与
// forward_shaded.frag 逐位匹配。
//
// 区别仅在实例 SSBO 的描述符 set：forward_shaded.frag 已占满 set0..set6（各 binding0，
// 其中 set3 为 PointLightUBO），故实例 SSBO 改置 set7.binding0 以避开 Vulkan 同 set+binding
// 冲突；binding 仍保持 0，使一次通用原语 BindStorageBuffer(0) 在三后端命中：
//   GL   spirv-cross 剥离 set、保留 binding=0 → glBindBufferBase(0)
//   VK   通用原语按 (set,binding) 升序映射，单 SSBO 即第 0 个 → slot 0（set7 为首个空闲 set）
//   DX11 经 @SSBO_LOW_REGISTERS → register(t0)（VS-only 绑定，不撞 PS 纹理）
//
// 契约（RHI_PRIMITIVE_CONTRACT §6）：DX11 SV_InstanceID 始终从 0 起，base-instance 偏移
// 须经 SSBO 偏移表达；故恒以 0 基索引 SSBO，调用方 DrawIndexedInstanced(first_instance=0)。

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

layout(std430, set = 7, binding = 0) readonly buffer InstanceModels {
    mat4 u_models[];   // 每实例 model 矩阵（世界空间）
};

void main() {
    mat4 model = u_models[gl_InstanceIndex];
    vec4 worldPos = model * vec4(aPos, 1.0);
    mat3 model3 = mat3(model);

    // 植被风弯曲 + 角色推力（与 pbr.vert:112-138 同算）。foliage_wind.y<=0 时整段跳过，
    // 故非植被绘制（MeshRenderer 喂零 wind）零位移、不回归；植被绘制由 CPU 侧喂入 grs 风参。
    if (foliage_wind.y > 0.001) {
        float height_factor = clamp(aPos.y, 0.0, 1.0);
        float hf2 = height_factor * height_factor;

        float t = foliage_wind.x;
        vec2 wind_dir = vec2(foliage_wind.z, foliage_wind.w);
        float wind_str = foliage_wind.y;
        float phase = dot(worldPos.xz, vec2(0.3, 0.7));
        float sway = sin(t * 2.0 + phase) * 0.5 + sin(t * 3.7 + phase * 1.3) * 0.3;
        worldPos.xz += wind_dir * sway * wind_str * hf2 * 0.15;
        worldPos.y -= abs(sway) * wind_str * hf2 * 0.02;

        if (foliage_push.w > 0.001) {
            vec3 push_delta = worldPos.xyz - foliage_push.xyz;
            float push_dist = length(push_delta.xz);
            float push_factor = 1.0 - clamp(push_dist / foliage_push.w, 0.0, 1.0);
            push_factor = push_factor * push_factor * hf2;
            if (push_dist > 0.001) {
                vec2 push_dir = push_delta.xz / push_dist;
                worldPos.xz += push_dir * push_factor * 0.5;
                worldPos.y -= push_factor * 0.15;
            }
        }
    }

    vWorldPos = worldPos.xyz;
    vNormal   = normalize(model3 * aNormal);
    vTangent  = model3 * aTangent;
    vColor    = aColor;
    vTexCoord = aTexCoord;
    gl_Position = vp * worldPos;
}
