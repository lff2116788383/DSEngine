#version 430

struct EdgeDetectParams
{
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

uniform EdgeDetectParams _28;

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

float linearize_depth(float d)
{
    float ndc = (d * 2.0) - 1.0;
    return ((2.0 * _28.u_near) * _28.u_far) / ((_28.u_far + _28.u_near) - (ndc * (_28.u_far - _28.u_near)));
}

vec3 reconstruct_normal(vec2 uv, vec2 texel_size)
{
    float param = texture(screenTexture, uv).x;
    float dc = linearize_depth(param);
    float param_1 = texture(screenTexture, uv - vec2(texel_size.x, 0.0)).x;
    float dl = linearize_depth(param_1);
    float param_2 = texture(screenTexture, uv + vec2(texel_size.x, 0.0)).x;
    float dr = linearize_depth(param_2);
    float param_3 = texture(screenTexture, uv - vec2(0.0, texel_size.y)).x;
    float db = linearize_depth(param_3);
    float param_4 = texture(screenTexture, uv + vec2(0.0, texel_size.y)).x;
    float dt = linearize_depth(param_4);
    return normalize(vec3(dl - dr, db - dt, (2.0 * texel_size.x) * dc));
}

void main()
{
    vec2 base_texel = vec2(1.0 / _28.u_screen_w, 1.0 / _28.u_screen_h);
    vec2 texel = base_texel * _28.u_thickness;
    float param = texture(screenTexture, vTexCoords).x;
    float d_c = linearize_depth(param);
    float param_1 = texture(screenTexture, vTexCoords + vec2(-texel.x, 0.0)).x;
    float d_l = linearize_depth(param_1);
    float param_2 = texture(screenTexture, vTexCoords + vec2(texel.x, 0.0)).x;
    float d_r = linearize_depth(param_2);
    float param_3 = texture(screenTexture, vTexCoords + vec2(0.0, texel.y)).x;
    float d_t = linearize_depth(param_3);
    float param_4 = texture(screenTexture, vTexCoords + vec2(0.0, -texel.y)).x;
    float d_b = linearize_depth(param_4);
    float depth_diff = abs(d_l - d_r) + abs(d_t - d_b);
    float depth_edge = smoothstep(0.0, _28.u_depth_threshold * d_c, depth_diff);
    vec2 param_5 = vTexCoords;
    vec2 param_6 = base_texel;
    vec3 n_c = reconstruct_normal(param_5, param_6);
    vec2 param_7 = vTexCoords + vec2(-texel.x, 0.0);
    vec2 param_8 = base_texel;
    vec3 n_l = reconstruct_normal(param_7, param_8);
    vec2 param_9 = vTexCoords + vec2(texel.x, 0.0);
    vec2 param_10 = base_texel;
    vec3 n_r = reconstruct_normal(param_9, param_10);
    vec2 param_11 = vTexCoords + vec2(0.0, texel.y);
    vec2 param_12 = base_texel;
    vec3 n_t = reconstruct_normal(param_11, param_12);
    vec2 param_13 = vTexCoords + vec2(0.0, -texel.y);
    vec2 param_14 = base_texel;
    vec3 n_b = reconstruct_normal(param_13, param_14);
    float normal_diff = length(n_l - n_r) + length(n_t - n_b);
    float normal_edge = smoothstep(0.0, _28.u_normal_threshold, normal_diff);
    float edge = clamp(max(depth_edge, normal_edge), 0.0, 1.0);
    FragColor = vec4(_28.u_outline_r, _28.u_outline_g, _28.u_outline_b, edge);
}

