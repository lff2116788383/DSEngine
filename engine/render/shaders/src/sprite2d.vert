#version 450
#extension GL_ARB_separate_shader_objects : enable
// Dedicated 2D sprite-batch vertex shader.
// Matches the GL batch VAO layout (pos@0=vec3, color@1=vec4, uv@2=vec2) and the
// PerFrame UBO, so it is a drop-in for the PBR program on the 2D batch path
// while remaining trivially lowerable to GLES 3.0 / WebGL2 (no SSBO/compute).
// Batch vertices are already world-transformed on the CPU, so only vp is applied.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
    vec4 foliage_wind;
    vec4 foliage_push;
};

void main() {
    gl_Position = vp * vec4(aPos, 1.0);
    vColor = aColor;
    vTexCoord = aTexCoord;
}
