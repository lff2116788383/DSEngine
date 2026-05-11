#version 430

layout(binding = 2, std140) uniform BloomParams
{
    float filterRadius;
} _11;

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    float x = _11.filterRadius;
    float y = _11.filterRadius;
    vec3 a = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y + y)).xyz;
    vec3 b = texture(screenTexture, vec2(vTexCoords.x, vTexCoords.y + y)).xyz;
    vec3 c = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y + y)).xyz;
    vec3 d = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y)).xyz;
    vec3 e = texture(screenTexture, vec2(vTexCoords.x, vTexCoords.y)).xyz;
    vec3 f = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y)).xyz;
    vec3 g = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y - y)).xyz;
    vec3 h = texture(screenTexture, vec2(vTexCoords.x, vTexCoords.y - y)).xyz;
    vec3 i = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y - y)).xyz;
    vec3 upsample = e * 4.0;
    upsample += ((((b + d) + f) + h) * 2.0);
    upsample += (((a + c) + g) + i);
    upsample *= 0.0625;
    FragColor = vec4(upsample, 1.0);
}

