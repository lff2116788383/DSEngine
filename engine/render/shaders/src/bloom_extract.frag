#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

layout(std140, set = 2, binding = 2) uniform BloomParams {
    float threshold;
    float knee;
};
void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float soft = brightness - (threshold - knee);
    soft = clamp(soft / (2.0 * knee + 0.0001), 0.0, 1.0);
    soft = soft * soft;
    float contribution = max(soft, step(threshold, brightness));
    FragColor = vec4(color * contribution, 1.0);
}
