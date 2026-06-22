#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

layout(push_constant) uniform MvParams {
    float screen_w;
    float screen_h;
    float m0;  float m1;  float m2;  float m3;
    float m4;  float m5;  float m6;  float m7;
    float m8;  float m9;  float m10; float m11;
    float m12; float m13; float m14; float m15;
};

void main() {
    mat4 reproj = mat4(
        vec4(m0,  m1,  m2,  m3),
        vec4(m4,  m5,  m6,  m7),
        vec4(m8,  m9,  m10, m11),
        vec4(m12, m13, m14, m15));
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
