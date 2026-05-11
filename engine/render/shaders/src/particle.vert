#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 iPos;
layout(location = 3) in vec4 iColor;
layout(location = 4) in float iSize;

layout(location = 0) out vec4 vParticleColor;
layout(location = 1) out vec2 vTexCoord;

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
};

void main() {
    vec3 camera_right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camera_up = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 vertexPosition_worldspace = iPos
        + camera_right * aPos.x * iSize
        + camera_up * aPos.y * iSize;

    gl_Position = vp * vec4(vertexPosition_worldspace, 1.0);
    vParticleColor = iColor;
    vTexCoord = aTexCoord;
}
