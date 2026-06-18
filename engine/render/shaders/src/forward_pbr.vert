#version 450
#extension GL_ARB_separate_shader_objects : enable
// B2b-1: 自包含静态 forward PBR 顶点着色器。
// 顶点已在 CPU 侧变换到世界空间（pos/normal/tangent），故 VS 仅应用 vp。
// 与批渲染「CPU 预变换 + vp」约定一致，规避按后端语义不一的 push-constant 原语。

layout(location = 0) in vec3 aPos;       // world-space position
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aNormal;    // world-space normal
layout(location = 4) in vec3 aTangent;   // world-space tangent

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

void main() {
    vWorldPos = aPos;
    vNormal = aNormal;
    vTangent = aTangent;
    vColor = aColor;
    vTexCoord = aTexCoord;
    gl_Position = vp * vec4(aPos, 1.0);
}
