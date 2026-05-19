#version 430

struct FxaaParams
{
    vec2 u_resolution;
};

uniform FxaaParams _27;

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

float luma(vec3 c)
{
    return dot(c, vec3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625));
}

void main()
{
    vec2 texel = vec2(1.0) / _27.u_resolution;
    vec3 param = texture(screenTexture, vTexCoords).xyz;
    float lumaM = luma(param);
    vec3 param_1 = texture(screenTexture, vTexCoords + (vec2(-1.0) * texel)).xyz;
    float lumaNW = luma(param_1);
    vec3 param_2 = texture(screenTexture, vTexCoords + (vec2(1.0, -1.0) * texel)).xyz;
    float lumaNE = luma(param_2);
    vec3 param_3 = texture(screenTexture, vTexCoords + (vec2(-1.0, 1.0) * texel)).xyz;
    float lumaSW = luma(param_3);
    vec3 param_4 = texture(screenTexture, vTexCoords + (vec2(1.0) * texel)).xyz;
    float lumaSE = luma(param_4);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    if (lumaRange < max(0.031199999153614044189453125, lumaMax * 0.125))
    {
        FragColor = texture(screenTexture, vTexCoords);
        return;
    }
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = (lumaNW + lumaSW) - (lumaNE + lumaSE);
    float dirReduce = max(((((lumaNW + lumaNE) + lumaSW) + lumaSE) * 0.25) * 0.25, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * texel;
    vec3 rgbA = (texture(screenTexture, vTexCoords + (dir * (-0.16666667163372039794921875))).xyz + texture(screenTexture, vTexCoords + (dir * 0.16666667163372039794921875)).xyz) * 0.5;
    vec3 rgbB = (rgbA * 0.5) + ((texture(screenTexture, vTexCoords + (dir * (-0.5))).xyz + texture(screenTexture, vTexCoords + (dir * 0.5)).xyz) * 0.25);
    vec3 param_5 = rgbB;
    float lumaB = luma(param_5);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        FragColor = vec4(rgbA, 1.0);
    }
    else
    {
        FragColor = vec4(rgbB, 1.0);
    }
}

