#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_color_texture;

layout(push_constant) uniform DofParams {
    float focus_distance;
    float focus_range;
    float bokeh_radius;
    float near_plane;
    float far_plane;
    float screen_w;
    float screen_h;
};

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    float lin_depth = linearizeDepth(depth);
    float coc = clamp(abs(lin_depth - focus_distance) / focus_range, 0.0, 1.0);
    vec2 texel = 1.0 / vec2(screen_w, screen_h);
    float radius = coc * bokeh_radius;
    vec3 color = vec3(0.0);
    float total_weight = 0.0;
    const int SAMPLES = 16;
    const float GOLDEN_ANGLE = 2.39996323;
    for (int i = 0; i < SAMPLES; ++i) {
        float r = sqrt(float(i) / float(SAMPLES)) * radius;
        float theta = float(i) * GOLDEN_ANGLE;
        vec2 offset = vec2(cos(theta), sin(theta)) * r * texel;
        float sample_depth = linearizeDepth(textureLod(screenTexture, vTexCoords + offset, 0.0).r);
        float sample_coc = clamp(abs(sample_depth - focus_distance) / focus_range, 0.0, 1.0);
        float w = max(sample_coc, coc);
        color += textureLod(u_color_texture, vTexCoords + offset, 0.0).rgb * w;
        total_weight += w;
    }
    if (total_weight > 0.0) color /= total_weight;
    else color = texture(u_color_texture, vTexCoords).rgb;
    FragColor = vec4(color, 1.0);
}
