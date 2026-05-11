#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
void main() {
    vec2 tex_offset = 1.0 / textureSize(screenTexture, 0);
    vec3 result = texture(screenTexture, vTexCoords).rgb * weight[0];
    for(int i = 1; i < 5; ++i) {
        result += texture(screenTexture, vTexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        result += texture(screenTexture, vTexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
    }
    FragColor = vec4(result, 1.0);
}
