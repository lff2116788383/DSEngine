#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

layout(set = 2, binding = 2) uniform sampler2D bloomBlur;
layout(std140, set = 2, binding = 3) uniform BloomParams {
    float exposure;
    float bloomIntensity;
};

vec3 AcesFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    vec3 bloomColor = texture(bloomBlur, vTexCoords).rgb;
    color += bloomColor * bloomIntensity;
    color = AcesFilmic(color * exposure);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
