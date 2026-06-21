#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_depth_tex;
layout(set = 2, binding = 3) uniform sampler2D u_decal_tex;

layout(std140, set = 2, binding = 0) uniform DecalParams {
    float m00; float m01; float m02; float m03;
    float m10; float m11; float m12; float m13;
    float m20; float m21; float m22; float m23;
    float m30; float m31; float m32; float m33;
    float u_color_r; float u_color_g; float u_color_b; float u_color_a;
    float u_angle_fade;
    float u_decal_up_x; float u_decal_up_y; float u_decal_up_z;
};

void main() {
    float depth = texture(u_depth_tex, vTexCoords).r;
    if (depth >= 0.9999) discard;

    mat4 inv_mvp = mat4(
        vec4(m00, m01, m02, m03),
        vec4(m10, m11, m12, m13),
        vec4(m20, m21, m22, m23),
        vec4(m30, m31, m32, m33));
    vec4 clip = vec4(vTexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 local4 = inv_mvp * clip;
    vec3 local = local4.xyz / local4.w;

    if (abs(local.x) > 0.5 || abs(local.y) > 0.5 || abs(local.z) > 0.5) discard;

    vec2 decal_uv = local.xz + 0.5;
    vec4 color = vec4(u_color_r, u_color_g, u_color_b, u_color_a);
    vec4 decal = texture(u_decal_tex, decal_uv) * color;

    float angle_factor = 1.0;
    if (u_angle_fade > 0.0) {
        vec2 texel = 1.0 / textureSize(u_depth_tex, 0);
        float dl = texture(u_depth_tex, vTexCoords + vec2(-texel.x, 0.0)).r;
        float dr = texture(u_depth_tex, vTexCoords + vec2( texel.x, 0.0)).r;
        float dt = texture(u_depth_tex, vTexCoords + vec2(0.0,  texel.y)).r;
        float db = texture(u_depth_tex, vTexCoords + vec2(0.0, -texel.y)).r;
        vec3 normal = normalize(vec3(dl - dr, dt - db, 2.0 * texel.x));
        vec3 decal_up = vec3(u_decal_up_x, u_decal_up_y, u_decal_up_z);
        float facing = abs(dot(normal, decal_up));
        angle_factor = smoothstep(0.0, 1.0 - u_angle_fade, facing);
    }
    FragColor = vec4(decal.rgb, decal.a * angle_factor);
}
