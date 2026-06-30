#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;
layout(location = 0) out vec2 vTexCoords;

void main() {
    vTexCoords = aTexCoords;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
