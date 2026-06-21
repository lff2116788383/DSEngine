#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D bloomBlur;
layout(set = 2, binding = 3) uniform sampler2D ssaoTexture;
layout(set = 2, binding = 4) uniform sampler2D autoExposureTex;
layout(set = 2, binding = 5) uniform sampler3D u_lut;
layout(set = 2, binding = 6) uniform sampler2D contactShadowTex;

layout(std140, set = 2, binding = 0) uniform BloomCompositeAeParams {
    float exposure;
    float bloomIntensity;
    float bloomEnabled;
    float ssaoEnabled;
    float autoExposureEnabled;
    float lutEnabled;
    float lutIntensity;
    float csEnabled;
    float csStrength;
    float vignetteEnabled;
    float vignetteIntensity;
    float vignetteRadius;
    float vignetteSoftness;
    float filmGrainEnabled;
    float filmGrainIntensity;
    float filmGrainTime;
};

vec3 AcesFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float GrainNoise(vec2 uv, float time_seed) {
    return fract(sin(dot(uv + vec2(time_seed, time_seed * 0.37), vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    if (ssaoEnabled != 0.0) {
        float ao = texture(ssaoTexture, vTexCoords).r;
        color *= ao;
    }
    if (bloomEnabled != 0.0) {
        vec3 bloomColor = texture(bloomBlur, vTexCoords).rgb;
        color += bloomColor * bloomIntensity;
    }
    if (csEnabled != 0.0) {
        float cs = texture(contactShadowTex, vTexCoords).r;
        color *= (1.0 - (1.0 - cs) * csStrength);
    }
    float finalExposure = exposure;
    if (autoExposureEnabled != 0.0) {
        finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
    }
    color = AcesFilmic(color * finalExposure);
    color = pow(color, vec3(1.0 / 2.2));
    if (lutEnabled != 0.0) {
        vec3 lutColor = texture(u_lut, clamp(color, 0.0, 1.0)).rgb;
        color = mix(color, lutColor, lutIntensity);
    }
    if (vignetteEnabled != 0.0) {
        float dist = length(vTexCoords - vec2(0.5));
        float radius = clamp(vignetteRadius, 0.001, 1.5);
        float softness = max(vignetteSoftness, 0.0001);
        float vignette = 1.0 - smoothstep(radius, radius + softness, dist);
        color *= mix(1.0, vignette, clamp(vignetteIntensity, 0.0, 1.0));
    }
    if (filmGrainEnabled != 0.0) {
        float grain = GrainNoise(vTexCoords * vec2(1280.0, 720.0), filmGrainTime) - 0.5;
        color = clamp(color + grain * filmGrainIntensity, 0.0, 1.0);
    }
    float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    color += (ign - 0.5) / 255.0;
    FragColor = vec4(color, 1.0);
}
