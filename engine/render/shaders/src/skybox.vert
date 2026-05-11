#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 aPos;
layout(location = 0) out vec3 vTexCoords;

layout(push_constant) uniform PushConstants {
    mat4 vp;
} pc;

void main() {
    vTexCoords = aPos;
    // 放大立方体使其大于近平面（near_clip=10 时顶点距离必须 > 10）
    vec4 pos = pc.vp * vec4(aPos * 10000.0, 1.0);
    // Vulkan NDC z∈[0,1]: z=w*0.999 保持在最远深度但不被裁剪
    gl_Position = vec4(pos.xy, pos.w * 0.999, pos.w);
}
