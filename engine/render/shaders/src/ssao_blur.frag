#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(screenTexture, 0));
    float result = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(screenTexture, vTexCoords + offset).r;
        }
    }
    FragColor = vec4(vec3(result / 25.0), 1.0);
}
