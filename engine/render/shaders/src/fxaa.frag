#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

layout(set = 2, binding = 0) uniform FxaaParams {
    vec2 u_resolution;
};

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec2 texel = 1.0 / u_resolution;
    float lumaM  = luma(texture(screenTexture, vTexCoords).rgb);
    float lumaNW = luma(texture(screenTexture, vTexCoords + vec2(-1.0,-1.0) * texel).rgb);
    float lumaNE = luma(texture(screenTexture, vTexCoords + vec2( 1.0,-1.0) * texel).rgb);
    float lumaSW = luma(texture(screenTexture, vTexCoords + vec2(-1.0, 1.0) * texel).rgb);
    float lumaSE = luma(texture(screenTexture, vTexCoords + vec2( 1.0, 1.0) * texel).rgb);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    if (lumaRange < max(0.0312, lumaMax * 0.125)) {
        FragColor = texture(screenTexture, vTexCoords);
        return;
    }
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * 0.25, 1.0/128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * texel;
    vec3 rgbA = 0.5 * (
        texture(screenTexture, vTexCoords + dir * (1.0/3.0 - 0.5)).rgb +
        texture(screenTexture, vTexCoords + dir * (2.0/3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(screenTexture, vTexCoords + dir * -0.5).rgb +
        texture(screenTexture, vTexCoords + dir *  0.5).rgb);
    float lumaB = luma(rgbB);
    if (lumaB < lumaMin || lumaB > lumaMax)
        FragColor = vec4(rgbA, 1.0);
    else
        FragColor = vec4(rgbB, 1.0);
}
