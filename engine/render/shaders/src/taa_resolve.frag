#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 5) uniform sampler2D u_history;
layout(set = 2, binding = 2) uniform sampler2D u_motion_vector;

layout(push_constant) uniform TaaParams {
    float u_blend_factor;
    float u_jitter_x;
    float u_jitter_y;
    int   u_frame_index;
    float u_screen_w;
    float u_screen_h;
};

void main() {
    vec3 current = texture(screenTexture, vTexCoords).rgb;
    vec2 mv = texture(u_motion_vector, vTexCoords).rg;
    vec2 history_uv = vTexCoords - mv - vec2(u_jitter_x, u_jitter_y);
    history_uv = clamp(history_uv, vec2(0.0), vec2(1.0));
    vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
    vec3 m1 = vec3(0.0), m2 = vec3(0.0);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            vec3 s = texture(screenTexture, vTexCoords + vec2(dx, dy) * texel).rgb;
            m1 += s;
            m2 += s * s;
        }
    }
    m1 /= 9.0;
    vec3 sigma = sqrt(max(m2 / 9.0 - m1 * m1, vec3(0.0)));
    vec3 aabb_min = m1 - 1.25 * sigma;
    vec3 aabb_max = m1 + 1.25 * sigma;
    vec3 history = texture(u_history, history_uv).rgb;
    history = clamp(history, aabb_min, aabb_max);
    float velocity_len = length(mv * vec2(u_screen_w, u_screen_h));
    float vel_weight = clamp(velocity_len * 0.5, 0.0, 0.5);
    float alpha = u_frame_index < 2 ? 1.0 : clamp(u_blend_factor + vel_weight, u_blend_factor, 1.0);
    FragColor = vec4(mix(history, current, alpha), 1.0);
}
