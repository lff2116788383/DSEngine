#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_depth_tex;

layout(push_constant) uniform VolumetricCloudParams {
    float u_depth_handle;
    float u_cloud_bottom;
    float u_cloud_top;
    float u_coverage;
    float u_density;
    float u_shape_scale;
    float u_detail_scale;
    float u_detail_strength;
    float u_erosion;
    float u_wind_offset_x;
    float u_wind_offset_z;
    float u_silver_intensity;
    float u_powder_strength;
    float u_ambient_strength;
    float u_sun_dir_x;
    float u_sun_dir_y;
    float u_sun_dir_z;
    float u_cam_pos_x;
    float u_cam_pos_y;
    float u_cam_pos_z;
    float u_near;
    float u_far;
    float u_right_x;
    float u_right_y;
    float u_right_z;
    float u_up_x;
    float u_up_y;
    float u_up_z;
    float u_fwd_x;
    float u_fwd_y;
    float u_fwd_z;
};

// ============================================================
// Procedural Noise (hash-based value noise + FBM)
// ============================================================

float hash3to1(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep

    float a = hash3to1(i);
    float b = hash3to1(i + vec3(1, 0, 0));
    float c = hash3to1(i + vec3(0, 1, 0));
    float d = hash3to1(i + vec3(1, 1, 0));
    float e = hash3to1(i + vec3(0, 0, 1));
    float g = hash3to1(i + vec3(1, 0, 1));
    float h = hash3to1(i + vec3(0, 1, 1));
    float k = hash3to1(i + vec3(1, 1, 1));

    return mix(mix(mix(a, b, f.x), mix(c, d, f.x), f.y),
               mix(mix(e, g, f.x), mix(h, k, f.x), f.y), f.z);
}

float fbm4(vec3 p) {
    float v = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 4; i++) {
        v += amp * valueNoise3D(p * freq);
        freq *= 2.0;
        amp *= 0.5;
    }
    return v;
}

float fbm3(vec3 p) {
    float v = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 3; i++) {
        v += amp * valueNoise3D(p * freq);
        freq *= 2.0;
        amp *= 0.5;
    }
    return v;
}

// ============================================================
// Cloud Density Sampling
// ============================================================

float remap(float v, float lo, float hi, float new_lo, float new_hi) {
    return new_lo + (v - lo) / (hi - lo) * (new_hi - new_lo);
}

float heightGradient(float h_frac) {
    // Anvil-shaped height profile
    float bottom = smoothstep(0.0, 0.15, h_frac);
    float top = smoothstep(1.0, 0.6, h_frac);
    return bottom * top;
}

float sampleCloudDensity(vec3 world_pos) {
    float h_frac = clamp((world_pos.y - u_cloud_bottom) / (u_cloud_top - u_cloud_bottom), 0.0, 1.0);
    float h_grad = heightGradient(h_frac);

    vec3 wind = vec3(u_wind_offset_x, 0.0, u_wind_offset_z);
    vec3 shape_pos = world_pos * u_shape_scale + wind * u_shape_scale;

    float shape_noise = fbm4(shape_pos);
    // Remap noise with coverage
    float base_cloud = remap(shape_noise, 1.0 - u_coverage, 1.0, 0.0, 1.0);
    base_cloud = max(base_cloud, 0.0) * h_grad;

    if (base_cloud <= 0.0) return 0.0;

    // Detail erosion
    vec3 detail_pos = world_pos * u_detail_scale + wind * u_detail_scale * 0.3;
    float detail_noise = fbm3(detail_pos);
    float detail_modifier = mix(detail_noise, 1.0 - detail_noise, clamp(h_frac * 2.0, 0.0, 1.0));
    base_cloud -= detail_modifier * u_detail_strength * u_erosion;

    return max(base_cloud, 0.0) * u_density;
}

// ============================================================
// Lighting
// ============================================================

float henyeyGreenstein(float cos_theta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(max(1.0 + g2 - 2.0 * g * cos_theta, 0.001), 1.5));
}

float dualLobePhase(float cos_theta) {
    float forward = henyeyGreenstein(cos_theta, 0.8);
    float backward = henyeyGreenstein(cos_theta, -0.3);
    float silver = henyeyGreenstein(cos_theta, 0.99 - u_silver_intensity * 0.2);
    return mix(mix(forward, backward, 0.3), silver, 0.15);
}

float lightMarch(vec3 pos, vec3 sun_dir) {
    const int LIGHT_STEPS = 6;
    float cloud_thickness = u_cloud_top - u_cloud_bottom;
    float step_size = cloud_thickness * 0.5 / float(LIGHT_STEPS);

    float total_density = 0.0;
    for (int i = 0; i < LIGHT_STEPS; i++) {
        pos += sun_dir * step_size;
        float h_frac = (pos.y - u_cloud_bottom) / (u_cloud_top - u_cloud_bottom);
        if (h_frac < 0.0 || h_frac > 1.0) break;
        total_density += sampleCloudDensity(pos) * step_size;
    }
    return total_density;
}

// ============================================================
// Ray-Sphere Intersection (flat earth approximation with height layers)
// ============================================================

vec2 rayCloudLayerIntersection(vec3 ray_origin, vec3 ray_dir) {
    // For flat cloud layers (good approximation for typical cloud altitudes)
    float ray_y = ray_dir.y + sign(ray_dir.y) * 1e-6; // avoid div-by-zero
    float t_bottom = (u_cloud_bottom - ray_origin.y) / ray_y;
    float t_top = (u_cloud_top - ray_origin.y) / ray_y;

    float t_near = min(t_bottom, t_top);
    float t_far = max(t_bottom, t_top);

    // Camera inside cloud layer
    if (ray_origin.y >= u_cloud_bottom && ray_origin.y <= u_cloud_top) {
        t_near = 0.0;
    }

    return vec2(max(t_near, 0.0), max(t_far, 0.0));
}

// ============================================================
// Depth linearization
// ============================================================

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

// ============================================================
// Main
// ============================================================

void main() {
    vec4 scene = texture(screenTexture, vTexCoords);
    float depth = texture(u_depth_tex, vTexCoords).r;

    // Reconstruct view ray
    vec2 ndc = vTexCoords * 2.0 - 1.0;
    vec3 camFwd = vec3(u_fwd_x, u_fwd_y, u_fwd_z);
    vec3 camRight = vec3(u_right_x, u_right_y, u_right_z);
    vec3 camUp = vec3(u_up_x, u_up_y, u_up_z);
    vec3 camPos = vec3(u_cam_pos_x, u_cam_pos_y, u_cam_pos_z);

    // right/up are pre-scaled by tan_fov_y * aspect / tan_fov_y respectively
    vec3 ray_dir = normalize(camFwd + ndc.x * camRight + ndc.y * camUp);

    // Compute max ray distance from depth
    float max_dist = 1e10;
    if (depth < 0.9999) {
        float view_z = linearizeDepth(depth);
        float cos_angle = max(dot(ray_dir, camFwd), 0.0001);
        max_dist = view_z / cos_angle;
    }

    // Find cloud layer intersection
    vec2 cloud_t = rayCloudLayerIntersection(camPos, ray_dir);
    float t_enter = cloud_t.x;
    float t_exit = min(cloud_t.y, max_dist);

    if (t_enter >= t_exit || t_exit <= 0.0) {
        FragColor = scene;
        return;
    }

    // Raymarching setup
    vec3 sunDir = normalize(vec3(u_sun_dir_x, u_sun_dir_y, u_sun_dir_z));
    float cos_theta = dot(ray_dir, -sunDir);
    float phase = dualLobePhase(cos_theta);

    const int MAX_STEPS = 64;
    float march_length = t_exit - t_enter;
    float step_size = march_length / float(MAX_STEPS);

    float transmittance = 1.0;
    vec3 light_energy = vec3(0.0);

    // Blue noise offset to reduce banding
    float dither = fract(sin(dot(vTexCoords * 1000.0, vec2(12.9898, 78.233))) * 43758.5453);
    float t = t_enter + step_size * dither;

    for (int i = 0; i < MAX_STEPS; i++) {
        if (t >= t_exit) break;

        vec3 pos = camPos + ray_dir * t;
        float density = sampleCloudDensity(pos);

        if (density > 0.001) {
            // Beer-Lambert extinction
            float sample_transmittance = exp(-density * step_size);

            // Light marching
            float light_density = lightMarch(pos, -sunDir);
            float beer = exp(-light_density);
            float powder = 1.0 - exp(-light_density * 2.0);
            float beer_powder = beer * mix(1.0, powder, u_powder_strength * 0.5);

            // Inscattering
            float light_contribution = beer_powder * phase;
            float ambient = u_ambient_strength;

            // Height-based ambient (brighter near top)
            float h_frac = clamp((pos.y - u_cloud_bottom) / (u_cloud_top - u_cloud_bottom), 0.0, 1.0);
            ambient += h_frac * 0.2;

            vec3 luminance = vec3(light_contribution + ambient);

            // Energy-conserving integration
            vec3 integScatter = luminance * (1.0 - sample_transmittance);
            light_energy += transmittance * integScatter;
            transmittance *= sample_transmittance;

            // Early exit
            if (transmittance < 0.01) {
                transmittance = 0.0;
                break;
            }
        }

        t += step_size;
    }

    // Composite: clouds over scene
    vec3 final_color = scene.rgb * transmittance + light_energy;
    FragColor = vec4(final_color, scene.a);
}
