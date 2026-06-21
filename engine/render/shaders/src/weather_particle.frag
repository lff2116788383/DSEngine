#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_depth_tex;

layout(std140, set = 2, binding = 0) uniform WeatherParams {
    float u_time;
    float u_intensity;        // [0,1]
    float u_wind_x;
    float u_wind_z;
    float u_type;             // 1=rain, 2=snow
    float u_color_r;
    float u_color_g;
    float u_color_b;
    float u_color_a;
    float u_cam_pos_x;
    float u_cam_pos_y;
    float u_cam_pos_z;
    float u_near;
    float u_far;
    float u_spawn_radius;
    float u_spawn_height;
    float u_screen_w;
    float u_screen_h;
    float u_fwd_x;
    float u_fwd_y;
    float u_fwd_z;
    float u_tan_fov_y;
    float u_aspect;
    float u_wetness;          // global wetness factor [0,1]
};

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float hash3(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

float WeatherLinZ(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

void main() {
    vec4 scene = texture(screenTexture, vTexCoords);
    float depth_raw = texture(u_depth_tex, vTexCoords).r;
    float scene_z = (depth_raw < 0.9999) ? WeatherLinZ(depth_raw) : 1e6;
    int weather_type = int(u_type + 0.5);
    if (weather_type == 0 || u_intensity < 0.001) {
        FragColor = scene;
        return;
    }

    vec3 camPos = vec3(u_cam_pos_x, u_cam_pos_y, u_cam_pos_z);
    vec3 camFwd = normalize(vec3(u_fwd_x, u_fwd_y, u_fwd_z));
    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 camRight = normalize(cross(worldUp, camFwd));
    vec3 camUp = cross(camFwd, camRight);

    int max_particles = int(u_intensity * 800.0);
    vec4 particle_color = vec4(u_color_r, u_color_g, u_color_b, u_color_a);
    vec3 accum = vec3(0.0);
    float accum_alpha = 0.0;

    for (int i = 0; i < max_particles; ++i) {
        float fi = float(i);
        float seed = hash(vec2(fi * 0.137, fi * 0.719));
        float seed2 = hash(vec2(fi * 0.413, fi * 0.257));
        float seed3 = hash(vec2(fi * 0.631, fi * 0.923));

        // particle world position: random within spawn volume around camera
        float angle = seed * 6.28318;
        float radius = sqrt(seed2) * u_spawn_radius;
        vec3 ppos;
        ppos.x = camPos.x + cos(angle) * radius + u_wind_x * u_time * 0.1;
        ppos.z = camPos.z + sin(angle) * radius + u_wind_z * u_time * 0.1;

        if (weather_type == 1) {
            // rain: fast falling
            float fall_speed = 12.0 + seed3 * 6.0;
            float y_cycle = mod(u_time * fall_speed + seed * 100.0, u_spawn_height);
            ppos.y = camPos.y + u_spawn_height * 0.5 - y_cycle;
        } else {
            // snow: slow falling with lateral drift
            float fall_speed = 1.5 + seed3 * 1.0;
            float y_cycle = mod(u_time * fall_speed + seed * 100.0, u_spawn_height);
            ppos.y = camPos.y + u_spawn_height * 0.5 - y_cycle;
            ppos.x += sin(u_time * 0.5 + fi) * 0.8;
            ppos.z += cos(u_time * 0.7 + fi * 1.3) * 0.6;
        }

        // project to screen
        vec3 delta = ppos - camPos;
        float pz = dot(delta, camFwd);
        if (pz < u_near || pz > min(scene_z, 80.0)) continue;

        vec2 proj_uv = vec2(
            dot(delta, camRight) / (pz * u_tan_fov_y * u_aspect),
            dot(delta, camUp)    / (pz * u_tan_fov_y)
        );
        proj_uv = proj_uv * 0.5 + 0.5;
        if (proj_uv.x < 0.0 || proj_uv.x > 1.0 || proj_uv.y < 0.0 || proj_uv.y > 1.0) continue;

        // particle screen-space size and shape
        float dist = length(delta);
        float pixel_size;
        vec2 diff;
        if (weather_type == 1) {
            // rain: elongated vertical streak
            pixel_size = 1.5 / (pz * 0.3 + 1.0);
            diff = vTexCoords - proj_uv;
            diff.x *= u_screen_w / u_screen_h;
            float streak = abs(diff.x) * 4.0 + abs(diff.y);
            float alpha = smoothstep(pixel_size, 0.0, streak) * particle_color.a;
            alpha *= (1.0 - dist / 80.0);
            accum += particle_color.rgb * alpha;
            accum_alpha += alpha;
        } else {
            // snow: round dot
            pixel_size = 2.5 / (pz * 0.3 + 1.0);
            diff = vTexCoords - proj_uv;
            diff.x *= u_screen_w / u_screen_h;
            float d2 = dot(diff, diff);
            float alpha = smoothstep(pixel_size * pixel_size, 0.0, d2) * particle_color.a;
            alpha *= (1.0 - dist / 80.0);
            accum += particle_color.rgb * alpha;
            accum_alpha += alpha;
        }
    }

    accum_alpha = clamp(accum_alpha, 0.0, 1.0);
    vec3 result = mix(scene.rgb, accum / max(accum_alpha, 0.001), accum_alpha);
    FragColor = vec4(result, scene.a);
}
