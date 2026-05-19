#version 430

struct DecalParams
{
    float u_depth_handle;
    float u_decal_handle;
    float m00;
    float m01;
    float m02;
    float m03;
    float m10;
    float m11;
    float m12;
    float m13;
    float m20;
    float m21;
    float m22;
    float m23;
    float m30;
    float m31;
    float m32;
    float m33;
    float u_color_r;
    float u_color_g;
    float u_color_b;
    float u_color_a;
    float u_angle_fade;
    float u_decal_up_x;
    float u_decal_up_y;
    float u_decal_up_z;
};

uniform DecalParams _35;

layout(binding = 2) uniform sampler2D u_depth_tex;
layout(binding = 3) uniform sampler2D u_decal_tex;
layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    float depth = texture(u_depth_tex, vTexCoords).x;
    if (depth >= 0.99989998340606689453125)
    {
        discard;
    }
    mat4 inv_mvp = mat4(vec4(vec4(_35.m00, _35.m01, _35.m02, _35.m03)), vec4(vec4(_35.m10, _35.m11, _35.m12, _35.m13)), vec4(vec4(_35.m20, _35.m21, _35.m22, _35.m23)), vec4(vec4(_35.m30, _35.m31, _35.m32, _35.m33)));
    vec4 clip = vec4((vTexCoords * 2.0) - vec2(1.0), (depth * 2.0) - 1.0, 1.0);
    vec4 local4 = inv_mvp * clip;
    vec3 local = local4.xyz / vec3(local4.w);
    bool _144 = abs(local.x) > 0.5;
    bool _153;
    if (!_144)
    {
        _153 = abs(local.y) > 0.5;
    }
    else
    {
        _153 = _144;
    }
    bool _162;
    if (!_153)
    {
        _162 = abs(local.z) > 0.5;
    }
    else
    {
        _162 = _153;
    }
    if (_162)
    {
        discard;
    }
    vec2 decal_uv = local.xz + vec2(0.5);
    vec4 color = vec4(_35.u_color_r, _35.u_color_g, _35.u_color_b, _35.u_color_a);
    vec4 decal = texture(u_decal_tex, decal_uv) * color;
    float angle_factor = 1.0;
    if (_35.u_angle_fade > 0.0)
    {
        vec2 texel = vec2(1.0) / vec2(textureSize(u_depth_tex, 0));
        float dl = texture(u_depth_tex, vTexCoords + vec2(-texel.x, 0.0)).x;
        float dr = texture(u_depth_tex, vTexCoords + vec2(texel.x, 0.0)).x;
        float dt = texture(u_depth_tex, vTexCoords + vec2(0.0, texel.y)).x;
        float db = texture(u_depth_tex, vTexCoords + vec2(0.0, -texel.y)).x;
        vec3 normal = normalize(vec3(dl - dr, dt - db, 2.0 * texel.x));
        vec3 decal_up = vec3(_35.u_decal_up_x, _35.u_decal_up_y, _35.u_decal_up_z);
        float facing = abs(dot(normal, decal_up));
        angle_factor = smoothstep(0.0, 1.0 - _35.u_angle_fade, facing);
    }
    FragColor = vec4(decal.xyz, decal.w * angle_factor);
}

