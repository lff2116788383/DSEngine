#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

layout(std140, set = 2, binding = 2) uniform BloomParams {
    float filterRadius;
};
void main() {
    float x = filterRadius;
    float y = filterRadius;
    vec3 a = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y + y)).rgb;
    vec3 b = texture(screenTexture, vec2(vTexCoords.x,     vTexCoords.y + y)).rgb;
    vec3 c = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y + y)).rgb;
    vec3 d = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y)).rgb;
    vec3 e = texture(screenTexture, vec2(vTexCoords.x,     vTexCoords.y)).rgb;
    vec3 f = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y)).rgb;
    vec3 g = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y - y)).rgb;
    vec3 h = texture(screenTexture, vec2(vTexCoords.x,     vTexCoords.y - y)).rgb;
    vec3 i = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y - y)).rgb;
    vec3 upsample = e*4.0;
    upsample += (b+d+f+h)*2.0;
    upsample += (a+c+g+i);
    upsample *= 1.0 / 16.0;
    FragColor = vec4(upsample, 1.0);
}
