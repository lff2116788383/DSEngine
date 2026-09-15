#version 450
#extension GL_ARB_separate_shader_objects : enable
// HD-2D Sprite3D fragment shader: alpha-tested texture * tint.

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;

layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_texture;

void main() {
    vec4 albedo = texture(u_texture, vTexCoord);
    vec4 color = albedo * vColor;
    if (color.a < 0.1) {
        discard;   // keep transparent texels out of the depth buffer
    }
    FragColor = color;
}
