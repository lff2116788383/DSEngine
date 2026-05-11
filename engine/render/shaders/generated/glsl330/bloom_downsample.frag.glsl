#version 430

layout(binding = 2, std140) uniform BloomParams
{
    vec2 srcResolution;
} _13;

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec2 srcTexelSize = vec2(1.0) / _13.srcResolution;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;
    vec3 a = texture(screenTexture, vec2(vTexCoords.x - (2.0 * x), vTexCoords.y + (2.0 * y))).xyz;
    vec3 b = texture(screenTexture, vec2(vTexCoords.x, vTexCoords.y + (2.0 * y))).xyz;
    vec3 c = texture(screenTexture, vec2(vTexCoords.x + (2.0 * x), vTexCoords.y + (2.0 * y))).xyz;
    vec3 d = texture(screenTexture, vec2(vTexCoords.x - (2.0 * x), vTexCoords.y)).xyz;
    vec3 e = texture(screenTexture, vec2(vTexCoords.x, vTexCoords.y)).xyz;
    vec3 f = texture(screenTexture, vec2(vTexCoords.x + (2.0 * x), vTexCoords.y)).xyz;
    vec3 g = texture(screenTexture, vec2(vTexCoords.x - (2.0 * x), vTexCoords.y - (2.0 * y))).xyz;
    vec3 h = texture(screenTexture, vec2(vTexCoords.x, vTexCoords.y - (2.0 * y))).xyz;
    vec3 i = texture(screenTexture, vec2(vTexCoords.x + (2.0 * x), vTexCoords.y - (2.0 * y))).xyz;
    vec3 j = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y + y)).xyz;
    vec3 k = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y + y)).xyz;
    vec3 l = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y - y)).xyz;
    vec3 m = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y - y)).xyz;
    vec3 downsample = e * 0.125;
    downsample += ((((a + c) + g) + i) * 0.03125);
    downsample += ((((b + d) + f) + h) * 0.0625);
    downsample += ((((j + k) + l) + m) * 0.125);
    FragColor = vec4(downsample, 1.0);
}

