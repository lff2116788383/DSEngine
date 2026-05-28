#version 450
#extension GL_ARB_separate_shader_objects : enable

// Separable Subsurface Scattering (SSSS) blur pass
// Based on Jorge Jimenez's "Separable SSS" technique.
// Two-pass Gaussian blur weighted by depth difference to prevent bleeding across edges.

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_scene_color;  // alpha = SSS strength mask from PBR output
layout(set = 2, binding = 2) uniform sampler2D u_depth_texture;

layout(push_constant) uniform SSSBlurParams {
    vec2 u_direction;          // (1,0) for horizontal, (0,1) for vertical
    vec2 u_screen_size;        // screen width, height
    float u_sss_width;         // SSS kernel width in screen pixels (default ~11)
    float u_depth_falloff;     // depth sensitivity (higher = stricter edge detection)
};

// Burley's normalized diffusion profile approximation (skin)
// Weights for 7-tap kernel at fixed offsets
const int KERNEL_SIZE = 7;
const float kOffsets[KERNEL_SIZE] = float[](0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
const vec4 kKernel[KERNEL_SIZE] = vec4[](
    vec4(0.560, 0.560, 0.560, 1.0),   // center
    vec4(0.220, 0.160, 0.120, 1.0),   // skin red diffusion
    vec4(0.100, 0.070, 0.040, 1.0),
    vec4(0.040, 0.020, 0.010, 1.0),
    vec4(0.015, 0.006, 0.003, 1.0),
    vec4(0.005, 0.002, 0.001, 1.0),
    vec4(0.002, 0.001, 0.000, 1.0)
);

float LinearizeDepth(float d) {
    // Assume reversed-Z or standard [0,1] mapping; adjust as needed
    return d;
}

void main() {
    vec4 center_color = texture(u_scene_color, vTexCoord);
    float sss_mask = center_color.a;

    // Early out for non-skin pixels
    if (sss_mask < 0.01) {
        FragColor = center_color;
        return;
    }

    float center_depth = LinearizeDepth(texture(u_depth_texture, vTexCoord).r);
    vec2 step = u_direction / u_screen_size * u_sss_width * sss_mask;

    vec3 color_sum = center_color.rgb * kKernel[0].rgb;
    vec3 weight_sum = kKernel[0].rgb;

    for (int i = 1; i < KERNEL_SIZE; ++i) {
        vec2 offset = step * kOffsets[i];

        // Positive direction
        vec2 uv_pos = vTexCoord + offset;
        vec4 sample_pos = texture(u_scene_color, uv_pos);
        float depth_pos = LinearizeDepth(texture(u_depth_texture, uv_pos).r);
        float mask_pos = sample_pos.a;

        // Depth-aware weight: reject samples across depth discontinuities
        float depth_diff_pos = abs(center_depth - depth_pos) * u_depth_falloff;
        float depth_weight_pos = max(0.0, 1.0 - depth_diff_pos) * mask_pos;
        vec3 w_pos = kKernel[i].rgb * depth_weight_pos;
        color_sum += sample_pos.rgb * w_pos;
        weight_sum += w_pos;

        // Negative direction
        vec2 uv_neg = vTexCoord - offset;
        vec4 sample_neg = texture(u_scene_color, uv_neg);
        float depth_neg = LinearizeDepth(texture(u_depth_texture, uv_neg).r);
        float mask_neg = sample_neg.a;

        float depth_diff_neg = abs(center_depth - depth_neg) * u_depth_falloff;
        float depth_weight_neg = max(0.0, 1.0 - depth_diff_neg) * mask_neg;
        vec3 w_neg = kKernel[i].rgb * depth_weight_neg;
        color_sum += sample_neg.rgb * w_neg;
        weight_sum += w_neg;
    }

    vec3 result = color_sum / max(weight_sum, vec3(0.001));
    FragColor = vec4(result, center_color.a);
}
