#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

layout(std140, set = 2, binding = 0) uniform TransmittanceLutParams {
    float u_planet_radius;
    float u_atmosphere_height;
    float u_rayleigh_r; float u_rayleigh_g; float u_rayleigh_b;
    float u_rayleigh_scale_h;
    float u_mie_coeff;
    float u_mie_scale_h;
};

const int NUM_STEPS = 40;
const float PI = 3.14159265358979;

// 计算从高度 h、天顶角 cos_theta 出发到大气顶部的光学深度
vec3 ComputeTransmittance(float h, float cos_theta) {
    float R = u_planet_radius;
    float H = u_atmosphere_height;
    float R_top = R + H;

    // 射线起点
    float r = R + h;
    // 射线长度（到大气顶部交点）
    float discriminant = r * r * (cos_theta * cos_theta - 1.0) + R_top * R_top;
    if (discriminant < 0.0) return vec3(0.0);
    float ray_length = -r * cos_theta + sqrt(discriminant);
    if (ray_length < 0.0) return vec3(0.0);

    float step_size = ray_length / float(NUM_STEPS);
    vec3 rayleigh_coeff = vec3(u_rayleigh_r, u_rayleigh_g, u_rayleigh_b);

    vec3 optical_depth = vec3(0.0);
    for (int i = 0; i < NUM_STEPS; ++i) {
        float t = (float(i) + 0.5) * step_size;
        // 当前采样点的高度
        float sample_r = sqrt(r * r + 2.0 * r * t * cos_theta + t * t);
        float sample_h = sample_r - R;
        if (sample_h < 0.0) break; // 击中地面

        float rayleigh_density = exp(-sample_h / u_rayleigh_scale_h);
        float mie_density = exp(-sample_h / u_mie_scale_h);

        optical_depth += rayleigh_coeff * rayleigh_density * step_size;
        optical_depth += vec3(u_mie_coeff) * mie_density * step_size;
    }

    return exp(-optical_depth);
}

void main() {
    // UV 映射: x → cos_theta [−1, 1], y → height [0, H]
    float cos_theta = vTexCoords.x * 2.0 - 1.0;
    float h = vTexCoords.y * u_atmosphere_height;

    vec3 transmittance = ComputeTransmittance(h, cos_theta);
    FragColor = vec4(transmittance, 1.0);
}
