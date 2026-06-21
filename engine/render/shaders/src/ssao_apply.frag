#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D ssaoTexture;
layout(set = 2, binding = 3) uniform sampler2D autoExposureTex;
layout(set = 2, binding = 5) uniform sampler3D lutTexture;

layout(std140, set = 2, binding = 0) uniform SsaoApplyParams {
    float exposure;
    float autoExposureEnabled;
    float lutEnabled;
    float lutIntensity;
};

vec3 AcesFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdrColor = texture(screenTexture, vTexCoords).rgb;
    float ao = texture(ssaoTexture, vTexCoords).r;
    hdrColor *= ao;
    float finalExposure = exposure;
    if (autoExposureEnabled != 0.0) {
        finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
    }
    vec3 result = AcesFilmic(hdrColor * finalExposure);
    result = pow(result, vec3(1.0 / 2.2));
    if (lutEnabled != 0.0) {
        vec3 lutColor = texture(lutTexture, clamp(result, 0.0, 1.0)).rgb;
        result = mix(result, lutColor, lutIntensity);
    }
    float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    result += (ign - 0.5) / 255.0;
    FragColor = vec4(result, 1.0);
}
