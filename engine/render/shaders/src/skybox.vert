#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 aPos;
layout(location = 0) out vec3 vTexCoords;

layout(push_constant) uniform PushConstants {
    mat4 u_vp;
} pc;

void main() {
    vTexCoords = aPos;
    vec4 pos = pc.u_vp * vec4(aPos * 10000.0, 1.0);
    // xyww trick: z=w → 透视除法后 z/w=1.0（远平面），OpenGL/Vulkan 通用
    gl_Position = pos.xyww;
}
