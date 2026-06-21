#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_reveal_tex;

void main() {
    vec4 accum = texture(screenTexture, vTexCoords);
    float revealage = texture(u_reveal_tex, vTexCoords).r;

    // No transparent fragments: early out
    if (accum.a < 1e-4) discard;

    vec3 avg_color = accum.rgb / max(accum.a, 1e-5);
    FragColor = vec4(avg_color, 1.0 - revealage);
}
