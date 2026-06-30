#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_tex_y;
layout(set = 2, binding = 2) uniform sampler2D u_tex_u;
layout(set = 2, binding = 3) uniform sampler2D u_tex_v;

void main() {
    float y = texture(u_tex_y, vTexCoords).r;
    float u = texture(u_tex_u, vTexCoords).r - 0.5;
    float v = texture(u_tex_v, vTexCoords).r - 0.5;

    // BT.709 YUV to RGB conversion matrix
    float r = y + 1.5748 * v;
    float g = y - 0.1873 * u - 0.4681 * v;
    float b = y + 1.8556 * u;

    FragColor = vec4(clamp(r, 0.0, 1.0),
                     clamp(g, 0.0, 1.0),
                     clamp(b, 0.0, 1.0),
                     1.0);
}
