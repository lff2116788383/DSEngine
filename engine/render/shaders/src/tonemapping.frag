#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D autoExposureTex;
layout(set = 2, binding = 5) uniform sampler3D u_lut;

layout(push_constant) uniform TonemapParams {
    float u_manual_exposure;
    int u_auto_exposure_enabled;
    int u_lut_enabled;
    float u_lut_intensity;
};

vec3 AcesFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdrColor = texture(screenTexture, vTexCoords).rgb;
    float finalExposure = u_manual_exposure;
    if (u_auto_exposure_enabled != 0) {
        finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
    }
    vec3 result = AcesFilmic(hdrColor * finalExposure);
    result = pow(result, vec3(1.0 / 2.2));
    if (u_lut_enabled != 0) {
        vec3 lutColor = texture(u_lut, clamp(result, 0.0, 1.0)).rgb;
        result = mix(result, lutColor, u_lut_intensity);
    }
    float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    result += (ign - 0.5) / 255.0;
    FragColor = vec4(result, 1.0);
}
