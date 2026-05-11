#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 u_model;
    mat4 u_vp;
} pc;

void main() {
    gl_Position = pc.u_vp * pc.u_model * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
    vTexCoord = aTexCoord;
}
