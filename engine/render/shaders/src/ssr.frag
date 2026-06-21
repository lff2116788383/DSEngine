#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_color_texture;

layout(std140, set = 2, binding = 0) uniform SsrParams {
    float max_distance;
    float thickness;
    float step_size;
    float max_steps;
    float near_plane;
    float far_plane;
    float screen_w;
    float screen_h;
    float fade_distance;
    float max_roughness;
};

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

vec3 reconstructNormal(vec2 uv) {
    vec2 texel = 1.0 / vec2(screen_w, screen_h);
    float dc = linearizeDepth(texture(screenTexture, uv).r);
    float dl = linearizeDepth(texture(screenTexture, uv - vec2(texel.x, 0.0)).r);
    float dr = linearizeDepth(texture(screenTexture, uv + vec2(texel.x, 0.0)).r);
    float db = linearizeDepth(texture(screenTexture, uv - vec2(0.0, texel.y)).r);
    float dt = linearizeDepth(texture(screenTexture, uv + vec2(0.0, texel.y)).r);
    return normalize(vec3(dl - dr, db - dt, 2.0 * texel.x * dc));
}

float edgeFade(vec2 uv) {
    vec2 edge = smoothstep(vec2(0.0), vec2(fade_distance), uv)
              * (1.0 - smoothstep(vec2(1.0 - fade_distance), vec2(1.0), uv));
    return edge.x * edge.y;
}

void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    if (depth >= 1.0) { FragColor = vec4(0.0); return; }
    float lin_depth = linearizeDepth(depth);
    vec3 normal = reconstructNormal(vTexCoords);
    vec3 view_dir = vec3(vTexCoords * 2.0 - 1.0, 1.0);
    view_dir = normalize(view_dir);
    vec3 reflect_dir = reflect(view_dir, normal);
    vec2 texel = 1.0 / vec2(screen_w, screen_h);
    vec2 ray_uv = vTexCoords;
    float ray_depth = lin_depth;
    int num_steps = int(max_steps);
    for (int i = 0; i < num_steps; ++i) {
        ray_uv += reflect_dir.xy * texel * step_size;
        if (ray_uv.x < 0.0 || ray_uv.x > 1.0 || ray_uv.y < 0.0 || ray_uv.y > 1.0) break;
        float sample_depth = linearizeDepth(textureLod(screenTexture, ray_uv, 0.0).r);
        ray_depth += reflect_dir.z * step_size;
        float depth_diff = ray_depth - sample_depth;
        if (depth_diff > 0.0 && depth_diff < thickness) {
            float step_fade = 1.0 - float(i) / max_steps;
            float screen_fade = edgeFade(ray_uv);
            float hit_dc = linearizeDepth(textureLod(screenTexture, ray_uv, 0.0).r);
            float hit_dl = linearizeDepth(textureLod(screenTexture, ray_uv - vec2(texel.x, 0.0), 0.0).r);
            float hit_dr = linearizeDepth(textureLod(screenTexture, ray_uv + vec2(texel.x, 0.0), 0.0).r);
            float roughness_est = abs(hit_dl - hit_dr) / max(hit_dc, 0.01);
            float roughness_fade = 1.0 - smoothstep(0.0, max_roughness, roughness_est);
            float fade = step_fade * screen_fade * roughness_fade;
            vec3 hit_color = textureLod(u_color_texture, ray_uv, 0.0).rgb;
            FragColor = vec4(hit_color * fade, fade);
            return;
        }
    }
    FragColor = vec4(0.0);
}
