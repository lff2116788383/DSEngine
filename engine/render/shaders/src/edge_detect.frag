#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

layout(std140, set = 2, binding = 0) uniform EdgeDetectParams {
    float u_thickness;
    float u_depth_threshold;
    float u_normal_threshold;
    float u_outline_r;
    float u_outline_g;
    float u_outline_b;
    float u_near;
    float u_far;
    float u_screen_w;
    float u_screen_h;
};

float linearize_depth(float d) {
    float ndc = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - ndc * (u_far - u_near));
}

vec3 reconstruct_normal(vec2 uv, vec2 texel_size) {
    float dc = linearize_depth(texture(screenTexture, uv).r);
    float dl = linearize_depth(texture(screenTexture, uv - vec2(texel_size.x, 0.0)).r);
    float dr = linearize_depth(texture(screenTexture, uv + vec2(texel_size.x, 0.0)).r);
    float db = linearize_depth(texture(screenTexture, uv - vec2(0.0, texel_size.y)).r);
    float dt = linearize_depth(texture(screenTexture, uv + vec2(0.0, texel_size.y)).r);
    return normalize(vec3(dl - dr, db - dt, 2.0 * texel_size.x * dc));
}

void main() {
    vec2 base_texel = vec2(1.0 / u_screen_w, 1.0 / u_screen_h);
    vec2 texel = base_texel * u_thickness;
    float d_c = linearize_depth(texture(screenTexture, vTexCoords).r);
    float d_l = linearize_depth(texture(screenTexture, vTexCoords + vec2(-texel.x, 0.0)).r);
    float d_r = linearize_depth(texture(screenTexture, vTexCoords + vec2( texel.x, 0.0)).r);
    float d_t = linearize_depth(texture(screenTexture, vTexCoords + vec2(0.0,  texel.y)).r);
    float d_b = linearize_depth(texture(screenTexture, vTexCoords + vec2(0.0, -texel.y)).r);
    float depth_diff = abs(d_l - d_r) + abs(d_t - d_b);
    float depth_edge = smoothstep(0.0, u_depth_threshold * d_c, depth_diff);
    vec3 n_c = reconstruct_normal(vTexCoords, base_texel);
    vec3 n_l = reconstruct_normal(vTexCoords + vec2(-texel.x, 0.0), base_texel);
    vec3 n_r = reconstruct_normal(vTexCoords + vec2( texel.x, 0.0), base_texel);
    vec3 n_t = reconstruct_normal(vTexCoords + vec2(0.0,  texel.y), base_texel);
    vec3 n_b = reconstruct_normal(vTexCoords + vec2(0.0, -texel.y), base_texel);
    float normal_diff = length(n_l - n_r) + length(n_t - n_b);
    float normal_edge = smoothstep(0.0, u_normal_threshold, normal_diff);
    float edge = clamp(max(depth_edge, normal_edge), 0.0, 1.0);
    FragColor = vec4(u_outline_r, u_outline_g, u_outline_b, edge);
}
