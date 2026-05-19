cbuffer FxaaParams
{
    float2 _27_u_resolution : packoffset(c0);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);

static float2 vTexCoords;
static float4 FragColor;

struct SPIRV_Cross_Input
{
    float2 vTexCoords : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 FragColor : SV_Target0;
};

float luma(float3 c)
{
    return dot(c, float3(0.2989999949932098388671875f, 0.58700001239776611328125f, 0.114000000059604644775390625f));
}

void frag_main()
{
    float2 texel = 1.0f.xx / _27_u_resolution;
    float3 param = screenTexture.Sample(_screenTexture_sampler, vTexCoords).xyz;
    float lumaM = luma(param);
    float3 param_1 = screenTexture.Sample(_screenTexture_sampler, vTexCoords + ((-1.0f).xx * texel)).xyz;
    float lumaNW = luma(param_1);
    float3 param_2 = screenTexture.Sample(_screenTexture_sampler, vTexCoords + (float2(1.0f, -1.0f) * texel)).xyz;
    float lumaNE = luma(param_2);
    float3 param_3 = screenTexture.Sample(_screenTexture_sampler, vTexCoords + (float2(-1.0f, 1.0f) * texel)).xyz;
    float lumaSW = luma(param_3);
    float3 param_4 = screenTexture.Sample(_screenTexture_sampler, vTexCoords + (1.0f.xx * texel)).xyz;
    float lumaSE = luma(param_4);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    if (lumaRange < max(0.031199999153614044189453125f, lumaMax * 0.125f))
    {
        FragColor = screenTexture.Sample(_screenTexture_sampler, vTexCoords);
        return;
    }
    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = (lumaNW + lumaSW) - (lumaNE + lumaSE);
    float dirReduce = max(((((lumaNW + lumaNE) + lumaSW) + lumaSE) * 0.25f) * 0.25f, 0.0078125f);
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(8.0f.xx, max((-8.0f).xx, dir * rcpDirMin)) * texel;
    float3 rgbA = (screenTexture.Sample(_screenTexture_sampler, vTexCoords + (dir * (-0.16666667163372039794921875f))).xyz + screenTexture.Sample(_screenTexture_sampler, vTexCoords + (dir * 0.16666667163372039794921875f)).xyz) * 0.5f;
    float3 rgbB = (rgbA * 0.5f) + ((screenTexture.Sample(_screenTexture_sampler, vTexCoords + (dir * (-0.5f))).xyz + screenTexture.Sample(_screenTexture_sampler, vTexCoords + (dir * 0.5f)).xyz) * 0.25f);
    float3 param_5 = rgbB;
    float lumaB = luma(param_5);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        FragColor = float4(rgbA, 1.0f);
    }
    else
    {
        FragColor = float4(rgbB, 1.0f);
    }
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
