#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_depth_tex;
layout(set = 2, binding = 3) uniform sampler2D u_transmittance_lut;

layout(push_constant) uniform AtmosphereSkyParams {
    float u_sun_dir_x;  float u_sun_dir_y;  float u_sun_dir_z;
    float u_rayleigh_r; float u_rayleigh_g; float u_rayleigh_b;
    float u_rayleigh_scale_h;
    float u_mie_coeff;
    float u_mie_scale_h;
    float u_mie_g;
    float u_planet_radius;
    float u_atmosphere_height;
    float u_sun_intensity_r; float u_sun_intensity_g; float u_sun_intensity_b;
    float u_sun_disk_angle;
    float u_near;        float u_far;
    float u_tan_fov_y;   float u_aspect;
    float u_right_x;     float u_right_y;   float u_right_z;
    float u_up_x;        float u_up_y;      float u_up_z;
    float u_fwd_x;       float u_fwd_y;     float u_fwd_z;
    float u_ozone_r;     float u_ozone_g;   float u_ozone_b;
    float u_ozone_center_h;
    float u_ozone_width;
    float u_sky_view_steps;
    float u_reserved;
};

const float PI = 3.14159265358979;

// Rayleigh 相函数
float RayleighPhase(float cos_theta) {
    return (3.0 / (16.0 * PI)) * (1.0 + cos_theta * cos_theta);
}

// Henyey-Greenstein 相函数
float MiePhase(float cos_theta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 / (4.0 * PI)) * (1.0 - g2) / (denom * sqrt(denom));
}

// 查询 transmittance LUT
vec3 SampleTransmittance(float h, float cos_theta) {
    float u = cos_theta * 0.5 + 0.5;
    float v = h / u_atmosphere_height;
    return textureLod(u_transmittance_lut, vec2(u, v), 0.0).rgb;
}

// 大气密度 at height
float RayleighDensity(float h) { return exp(-h / u_rayleigh_scale_h); }
float MieDensity(float h) { return exp(-h / u_mie_scale_h); }
float OzoneDensity(float h) {
    return max(0.0, 1.0 - abs(h - u_ozone_center_h) / (u_ozone_width * 0.5));
}

void main() {
    vec4 scene = texture(screenTexture, vTexCoords);
    float depth = texture(u_depth_tex, vTexCoords).r;

    // 仅渲染天空像素（depth ≈ 1.0）
    if (depth < 0.9999) {
        FragColor = scene;
        return;
    }

    // 重建视方向
    vec2 ndc = vTexCoords * 2.0 - 1.0;
    vec3 camFwd   = vec3(u_fwd_x, u_fwd_y, u_fwd_z);
    vec3 camRight = vec3(u_right_x, u_right_y, u_right_z);
    vec3 camUp    = vec3(u_up_x, u_up_y, u_up_z);
    vec3 viewDir = normalize(camFwd
        + ndc.x * camRight * u_tan_fov_y * u_aspect
        + ndc.y * camUp    * u_tan_fov_y);

    vec3 sunDir = normalize(vec3(u_sun_dir_x, u_sun_dir_y, u_sun_dir_z));
    vec3 rayleigh_coeff = vec3(u_rayleigh_r, u_rayleigh_g, u_rayleigh_b);
    vec3 ozone_coeff = vec3(u_ozone_r, u_ozone_g, u_ozone_b);
    vec3 sun_intensity = vec3(u_sun_intensity_r, u_sun_intensity_g, u_sun_intensity_b);

    // 观察者在地表（Camera-Relative: 近似地表高度 = 0）
    float R = u_planet_radius;
    float R_top = R + u_atmosphere_height;
    float observer_h = 1.0; // 1m above ground

    // Ray-sphere intersection with atmosphere top
    float r = R + observer_h;
    float cos_view_zenith = viewDir.y; // Y-up
    float discriminant = r * r * (cos_view_zenith * cos_view_zenith - 1.0) + R_top * R_top;
    if (discriminant < 0.0) { FragColor = scene; return; }
    float ray_length = -r * cos_view_zenith + sqrt(discriminant);

    // 检查是否命中地面
    float disc_ground = r * r * (cos_view_zenith * cos_view_zenith - 1.0) + R * R;
    if (disc_ground >= 0.0) {
        float t_ground = -r * cos_view_zenith - sqrt(disc_ground);
        if (t_ground > 0.0) ray_length = t_ground;
    }

    int steps = int(u_sky_view_steps);
    float step_size = ray_length / float(steps);

    // In-scattering 积分
    vec3 in_scatter_rayleigh = vec3(0.0);
    vec3 in_scatter_mie = vec3(0.0);
    vec3 optical_depth = vec3(0.0);

    float cos_sun_view = dot(viewDir, sunDir);
    float rayleigh_phase = RayleighPhase(cos_sun_view);
    float mie_phase = MiePhase(cos_sun_view, u_mie_g);

    for (int i = 0; i < steps; ++i) {
        float t = (float(i) + 0.5) * step_size;

        // 采样点位置（球面坐标）
        vec3 sample_pos = vec3(0.0, r, 0.0) + viewDir * t;
        float sample_r = length(sample_pos);
        float sample_h = sample_r - R;
        if (sample_h < 0.0) break;

        // 密度
        float rho_r = RayleighDensity(sample_h);
        float rho_m = MieDensity(sample_h);
        float rho_o = OzoneDensity(sample_h);

        // 当前步光学深度
        vec3 step_extinction = rayleigh_coeff * rho_r + vec3(u_mie_coeff) * rho_m + ozone_coeff * rho_o;
        optical_depth += step_extinction * step_size;
        vec3 transmittance_to_sample = exp(-optical_depth);

        // 从采样点到太阳的 transmittance（查 LUT）
        float cos_sun_zenith = dot(normalize(sample_pos), sunDir);
        vec3 sun_transmittance = SampleTransmittance(sample_h, cos_sun_zenith);

        // 累积 in-scattering
        in_scatter_rayleigh += rho_r * transmittance_to_sample * sun_transmittance * step_size;
        in_scatter_mie += rho_m * transmittance_to_sample * sun_transmittance * step_size;
    }

    vec3 sky_color = sun_intensity * (
        in_scatter_rayleigh * rayleigh_coeff * rayleigh_phase +
        in_scatter_mie * vec3(u_mie_coeff) * mie_phase
    );

    // 太阳圆盘
    float sun_cos_angle = cos(u_sun_disk_angle * 0.5);
    if (cos_sun_view > sun_cos_angle) {
        vec3 sun_transmittance = SampleTransmittance(observer_h, sunDir.y);
        float limb_darkening = 1.0 - pow(1.0 - (cos_sun_view - sun_cos_angle) / (1.0 - sun_cos_angle), 0.5);
        sky_color += sun_intensity * sun_transmittance * limb_darkening * 50.0;
    }

    FragColor = vec4(sky_color, 1.0);
}
