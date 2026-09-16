#version 450
#extension GL_ARB_separate_shader_objects : enable
// HD-2D M3.1 lit billboard fragment shader.
// Minimal fixed-function style shading: directional + up to 8 point + 8 spot
// lights. The billboard normal faces the camera-relative origin, so a sprite
// reads as a lit card instead of a flat unlit texture.

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec3 vWorldPos;

layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_texture;

struct Sprite3DPointLightGPU {
    vec4 position_radius;  // xyz = world position, w = radius
    vec4 color_intensity;  // xyz = color, w = intensity
    vec4 pad0;
    vec4 pad1;
};

struct Sprite3DSpotLightGPU {
    vec4 position_radius;  // xyz = world position, w = radius
    vec4 direction_inner;  // xyz = normalized light direction, w = cos(inner)
    vec4 color_intensity;  // xyz = color, w = intensity
    vec4 outer_pad;        // x = cos(outer)
};

layout(std140, set = 0, binding = 1) uniform Sprite3DLight {
    vec4 u_dir_direction_enabled;  // xyz = direction TO light, w = enabled
    vec4 u_dir_color_ambient;      // xyz = color, w = ambient intensity
    vec4 u_dir_params;             // x = intensity
    ivec4 u_point_count;           // x = count
    Sprite3DPointLightGPU u_points[8];
    ivec4 u_spot_count;            // x = count
    Sprite3DSpotLightGPU u_spots[8];
};

void main() {
    vec4 albedo = texture(u_texture, vTexCoord);
    if (albedo.a * vColor.a < 0.1) {
        discard;
    }

    vec3 N = normalize(-vWorldPos);
    vec3 light = u_dir_color_ambient.rgb * u_dir_color_ambient.a;

    if (u_dir_direction_enabled.w > 0.5) {
        vec3 L = normalize(u_dir_direction_enabled.xyz);
        light += u_dir_color_ambient.rgb * u_dir_params.x * max(dot(N, L), 0.0);
    }

    for (int i = 0; i < u_point_count.x && i < 8; ++i) {
        vec3 to_light = u_points[i].position_radius.xyz - vWorldPos;
        float dist = length(to_light);
        float radius = max(u_points[i].position_radius.w, 1.0e-3);
        float atten = max(1.0 - dist / radius, 0.0);
        atten *= atten;
        vec3 L = to_light / max(dist, 1.0e-4);
        light += u_points[i].color_intensity.rgb *
                 u_points[i].color_intensity.w *
                 max(dot(N, L), 0.0) * atten;
    }

    for (int i = 0; i < u_spot_count.x && i < 8; ++i) {
        vec3 to_light = u_spots[i].position_radius.xyz - vWorldPos;
        float dist = length(to_light);
        float radius = max(u_spots[i].position_radius.w, 1.0e-3);
        float atten = max(1.0 - dist / radius, 0.0);
        atten *= atten;
        vec3 L = to_light / max(dist, 1.0e-4);
        vec3 spot_dir = normalize(u_spots[i].direction_inner.xyz);
        float cos_angle = dot(-L, spot_dir);
        float inner = u_spots[i].direction_inner.w;
        float outer = u_spots[i].outer_pad.x;
        float cone = smoothstep(outer, inner, cos_angle);
        light += u_spots[i].color_intensity.rgb *
                 u_spots[i].color_intensity.w *
                 max(dot(N, L), 0.0) * atten * cone;
    }

    vec3 color = albedo.rgb * vColor.rgb * light;
    FragColor = vec4(color, albedo.a * vColor.a);
}