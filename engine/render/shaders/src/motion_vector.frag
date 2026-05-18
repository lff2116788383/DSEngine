#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

layout(push_constant) uniform MvParams {
    float screen_w;
    float screen_h;
    mat4 reproj;
};

void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    vec2 ndc = vTexCoords * 2.0 - 1.0;
    float z_ndc = depth * 2.0 - 1.0;
    vec4 clip_pos = vec4(ndc, z_ndc, 1.0);
    vec4 prev_clip = reproj * clip_pos;
    prev_clip.xy /= prev_clip.w;
    vec2 prev_uv = prev_clip.xy * 0.5 + 0.5;
    vec2 velocity = vTexCoords - prev_uv;
    FragColor = vec4(velocity, 0.0, 1.0);
}
