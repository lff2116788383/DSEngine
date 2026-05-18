#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

void main() {
    float logSum = 0.0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            vec2 uv = (vec2(float(i), float(j)) + 0.5) / 8.0;
            vec3 c = texture(screenTexture, uv).rgb;
            float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
            logSum += log(max(lum, 0.0001));
        }
    }
    float avgLogLum = logSum / 64.0;
    FragColor = vec4(avgLogLum, 0.0, 0.0, 1.0);
}
