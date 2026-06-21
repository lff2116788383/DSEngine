#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_depth_tex;

layout(std140, set = 2, binding = 0) uniform WaterParams {
    float u_water_level;
    float u_deep_r;    float u_deep_g;    float u_deep_b;
    float u_shallow_r; float u_shallow_g; float u_shallow_b;
    float u_max_depth;
    float u_transparency;
    float u_wave_amplitude; float u_wave_frequency; float u_wave_speed;
    float u_wave_dir_x; float u_wave_dir_y;
    float u_refraction_strength;
    float u_specular_power;
    float u_reflection_strength;
    float u_time;
    float u_sun_dir_x; float u_sun_dir_y; float u_sun_dir_z;
    float u_cam_pos_x; float u_cam_pos_y; float u_cam_pos_z;
    float u_near; float u_far;
    float u_fwd_x; float u_fwd_y; float u_fwd_z;
    float u_tan_fov_y;
    float u_aspect;
    float u_caustic_intensity; float u_caustic_scale;
    float u_foam_intensity; float u_foam_depth_threshold;
    float u_uw_fog_density;
    float u_uw_fog_r; float u_uw_fog_g; float u_uw_fog_b;
};

float WaterLinZ(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

vec3 GerstnerNormal(vec2 xz, float t, vec2 d1) {
    float k = u_wave_frequency;
    float a = u_wave_amplitude;
    float sp = u_wave_speed;
    vec2 d2 = vec2(-d1.y, d1.x);
    float p1 = dot(d1, xz) * k - t * sp;
    float p2 = dot(d2, xz) * k * 1.3 - t * sp * 0.7;
    float dx = -k * a * (d1.x * cos(p1) + d2.x * 0.5 * cos(p2));
    float dz = -k * a * (d1.y * cos(p1) + d2.y * 0.5 * cos(p2));
    return normalize(vec3(-dx, 1.0, -dz));
}

void main() {
    vec4 scene = texture(screenTexture, vTexCoords);
    float depth_raw = texture(u_depth_tex, vTexCoords).r;

    vec3 camFwd = vec3(u_fwd_x, u_fwd_y, u_fwd_z);
    vec3 camPos = vec3(u_cam_pos_x, u_cam_pos_y, u_cam_pos_z);
    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 camRight = normalize(cross(worldUp, camFwd));
    vec3 camUp = cross(camFwd, camRight);
    vec2 ndc = vTexCoords * 2.0 - 1.0;
    vec3 rayDir = normalize(camFwd
        + ndc.x * camRight * u_tan_fov_y * u_aspect
        + ndc.y * camUp    * u_tan_fov_y);

    float denom = rayDir.y;
    if (abs(denom) < 1e-6) { FragColor = scene; return; }
    float t_hit = (u_water_level - camPos.y) / denom;
    if (t_hit < 0.0) { FragColor = scene; return; }

    float scene_linear = (depth_raw < 0.9999)
        ? WaterLinZ(depth_raw) / max(dot(rayDir, camFwd), 0.0001)
        : 1e6;
    if (t_hit > scene_linear) { FragColor = scene; return; }

    vec3 water_world = camPos + rayDir * t_hit;
    vec2 waveDir = vec2(u_wave_dir_x, u_wave_dir_y);

    float underwater_depth = max(scene_linear - t_hit, 0.0);
    float depth_factor = clamp(underwater_depth / max(u_max_depth, 0.01), 0.0, 1.0);
    vec3 deepC = vec3(u_deep_r, u_deep_g, u_deep_b);
    vec3 shallowC = vec3(u_shallow_r, u_shallow_g, u_shallow_b);
    vec3 water_color = mix(shallowC, deepC, depth_factor);

    vec3 wave_normal = GerstnerNormal(water_world.xz, u_time, waveDir);

    // ── Rain ripples: procedural concentric rings on the water surface ──
    {
        vec2 ripple_offset = vec2(0.0);
        for (int ri = 0; ri < 8; ++ri) {
            float fi = float(ri);
            vec2 center = vec2(
                fract(sin(fi * 127.1) * 43758.5) * 20.0 - 10.0,
                fract(sin(fi * 311.7) * 43758.5) * 20.0 - 10.0
            ) + floor(water_world.xz / 20.0) * 20.0;
            float phase = fract(sin(fi * 74.7) * 43758.5);
            float age = fract(u_time * 0.4 + phase);
            float ring_r = age * 1.5;
            vec2 delta = water_world.xz - center;
            float dist = length(delta);
            float ring = exp(-(dist - ring_r) * (dist - ring_r) * 40.0) * (1.0 - age);
            vec2 dir = dist > 0.001 ? delta / dist : vec2(0.0);
            ripple_offset += dir * ring * 0.03;
        }
        wave_normal.xz += ripple_offset;
        wave_normal = normalize(wave_normal);
    }

    vec2 refract_offset = wave_normal.xz * u_refraction_strength;
    vec2 refract_uv = clamp(vTexCoords + refract_offset, 0.0, 1.0);
    vec3 refracted = texture(screenTexture, refract_uv).rgb;

    float cos_theta = max(dot(-rayDir, wave_normal), 0.0);
    float fresnel = u_reflection_strength + (1.0 - u_reflection_strength) * pow(1.0 - cos_theta, 5.0);

    vec3 reflected_dir = reflect(rayDir, wave_normal);
    float sky_grad = clamp(reflected_dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky_color = mix(vec3(0.3, 0.4, 0.5), vec3(0.6, 0.75, 1.0), sky_grad);

    // SSR: screen-space ray march for water reflection
    {
        vec3 ray_pos = water_world;
        vec3 ray_step = reflected_dir * 0.5;
        float ssr_weight = 0.0;
        for (int i = 0; i < 32; ++i) {
            ray_pos += ray_step;
            vec3 delta = ray_pos - camPos;
            float ray_z = dot(delta, camFwd);
            if (ray_z <= u_near) continue;
            vec2 proj_uv = vec2(
                dot(delta, camRight) / (ray_z * u_tan_fov_y * u_aspect),
                dot(delta, camUp)    / (ray_z * u_tan_fov_y)
            );
            proj_uv = proj_uv * 0.5 + 0.5;
            if (proj_uv.x < 0.0 || proj_uv.x > 1.0 || proj_uv.y < 0.0 || proj_uv.y > 1.0) break;
            float sampled_depth = texture(u_depth_tex, proj_uv).r;
            float sampled_linear = (sampled_depth < 0.9999)
                ? WaterLinZ(sampled_depth)
                : 1e6;
            if (ray_z > sampled_linear && ray_z - sampled_linear < 2.0) {
                sky_color = texture(screenTexture, proj_uv).rgb;
                ssr_weight = 1.0 - float(i) / 32.0;
                break;
            }
        }
        // 未命中时保留 sky_color 作为 fallback
    }

    vec3 sunDir = vec3(u_sun_dir_x, u_sun_dir_y, u_sun_dir_z);
    vec3 half_vec = normalize(-rayDir + (-sunDir));
    float spec = pow(max(dot(wave_normal, half_vec), 0.0), u_specular_power);

    // caustics: dual-layer Voronoi
    vec3 caustic = vec3(0.0);
    if (u_caustic_intensity > 0.001) {
        vec2 cUV = water_world.xz / u_caustic_scale;
        float v1 = 1.0, v2 = 1.0;
        for (int ci = 0; ci < 2; ci++) {
            float speed = (ci == 0) ? 0.4 : -0.3;
            vec2 uvc = cUV + vec2(u_time * speed, u_time * speed * 0.7);
            vec2 cell = floor(uvc);
            vec2 frac_uv = fract(uvc);
            float minD = 1.0;
            for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {
                    vec2 nb = vec2(float(x), float(y));
                    vec2 h = fract(sin(vec2(
                        dot(cell + nb, vec2(127.1, 311.7)),
                        dot(cell + nb, vec2(269.5, 183.3))
                    )) * 43758.5453);
                    vec2 diff = nb + h - frac_uv;
                    minD = min(minD, dot(diff, diff));
                }
            }
            if (ci == 0) v1 = minD; else v2 = minD;
        }
        float pattern = clamp(pow(min(v1, v2), 0.5) * 2.0, 0.0, 1.0);
        pattern = 1.0 - pattern;
        pattern = pow(pattern, 2.5);
        caustic = vec3(pattern) * u_caustic_intensity * (1.0 - depth_factor);
    }

    // foam
    float foam = 0.0;
    if (u_foam_intensity > 0.001) {
        foam = (1.0 - smoothstep(0.0, u_foam_depth_threshold, underwater_depth)) * u_foam_intensity;
        float foam_noise = fract(sin(dot(water_world.xz * 5.0 + u_time * 0.3, vec2(12.9898, 78.233))) * 43758.5453);
        foam *= (0.6 + 0.4 * foam_noise);
    }

    vec3 underwater = mix(refracted, water_color, depth_factor * u_transparency) + caustic;
    vec3 surface = mix(underwater, sky_color, fresnel) + vec3(spec) + vec3(foam);

    // underwater fog
    if (camPos.y < u_water_level && u_uw_fog_density > 0.001) {
        float fog_dist = length(water_world - camPos);
        float fog_factor = 1.0 - exp(-u_uw_fog_density * fog_dist);
        surface = mix(surface, vec3(u_uw_fog_r, u_uw_fog_g, u_uw_fog_b), clamp(fog_factor, 0.0, 1.0));
    }

    float edge_fade = smoothstep(0.0, 0.5, underwater_depth);
    float alpha = u_transparency * edge_fade;
    FragColor = vec4(mix(scene.rgb, surface, alpha), scene.a);
}
