#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D bloomBlur;
layout(set = 2, binding = 3) uniform sampler2D ssaoTexture;

layout(push_constant) uniform BloomCompositeSsaoParams {
    float exposure;
    float bloomIntensity;
    int ssaoEnabled;
    float _pad;
};

vec3 AcesFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    if (ssaoEnabled != 0) {
        float ao = texture(ssaoTexture, vTexCoords).r;
        color *= ao;
    }
    vec3 bloom = texture(bloomBlur, vTexCoords).rgb;
    color += bloom * bloomIntensity;
    color = AcesFilmic(color * exposure);
    color = pow(color, vec3(1.0 / 2.2));
    float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    color += (ign - 0.5) / 255.0;
    FragColor = vec4(color, 1.0);
}
