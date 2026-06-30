#version 450
#extension GL_ARB_separate_shader_objects : enable
// Impostor LOD billboard 片元着色器。
//
// 从 atlas 采样 albedo（rgba）+ 可选法线（rgb），alpha test 丢弃空区域。
// 支持简单方向光照明（法线重建）以避免 billboard 看起来太"平"。

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vViewDir;
layout(location = 2) in float vFade;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 1) uniform sampler2D u_atlas;         // albedo atlas (RGBA)
layout(set = 2, binding = 2) uniform sampler2D u_normal_atlas;  // normal atlas (RGB, 0=不使用)

layout(std140, set = 1, binding = 0) uniform ImpostorParams {
    vec4 u_light_dir;        // xyz = 主方向光方向（world space），w = normal_strength
    vec4 u_ambient_color;    // xyz = 环境光色，w = alpha_cutoff
};

void main() {
    vec4 albedo = texture(u_atlas, vTexCoord);

    // Alpha test：丢弃 atlas 空白区域
    float alpha_cutoff = u_ambient_color.w;
    if (albedo.a < alpha_cutoff) discard;

    vec3 color = albedo.rgb;

    // 可选法线重建光照
    float normal_strength = u_light_dir.w;
    if (normal_strength > 0.0) {
        vec3 normal_sample = texture(u_normal_atlas, vTexCoord).rgb;
        // atlas 法线 [0,1] → [-1,1]
        vec3 N = normalize(normal_sample * 2.0 - 1.0);
        N = mix(vec3(0.0, 0.0, 1.0), N, normal_strength);

        // 简单半兰伯特光照
        vec3 L = normalize(-u_light_dir.xyz);
        float NdotL = dot(N, L) * 0.5 + 0.5;
        vec3 ambient = u_ambient_color.rgb;
        color = albedo.rgb * (ambient + (1.0 - ambient) * NdotL);
    }

    // 距离渐变
    fragColor = vec4(color, albedo.a * vFade);
}
