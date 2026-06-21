#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D prevAdaptedTex;

layout(std140, set = 2, binding = 0) uniform LumAdaptParams {
    float u_dt;
    float u_speed_up;
    float u_speed_down;
    float u_min_exposure;
    float u_max_exposure;
    float u_compensation;
};

void main() {
    float avgLogLum = texture(screenTexture, vec2(0.5, 0.5)).r;
    float avgLum = exp(avgLogLum);
    float targetExposure = 0.18 / max(avgLum, 0.001);
    targetExposure = clamp(targetExposure * exp2(u_compensation), u_min_exposure, u_max_exposure);
    float prevExposure = texture(prevAdaptedTex, vec2(0.5, 0.5)).r;
    if (prevExposure <= 0.0) prevExposure = targetExposure;
    float speed = (targetExposure > prevExposure) ? u_speed_up : u_speed_down;
    float adapted = prevExposure + (targetExposure - prevExposure) * (1.0 - exp(-u_dt * speed));
    FragColor = vec4(adapted, 0.0, 0.0, 1.0);
}
