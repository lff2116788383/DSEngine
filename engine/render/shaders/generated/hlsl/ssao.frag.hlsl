static const float3 _245[16] = { float3(0.5381000041961669921875f, 0.18559999763965606689453125f, -0.4318999946117401123046875f), float3(0.13789999485015869140625f, 0.248600006103515625f, 0.4429999887943267822265625f), float3(0.3370999991893768310546875f, 0.567900002002716064453125f, -0.0057000000961124897003173828125f), float3(-0.699899971485137939453125f, -0.0450999997556209564208984375f, -0.0019000000320374965667724609375f), float3(0.068899996578693389892578125f, -0.159799993038177490234375f, -0.854700028896331787109375f), float3(0.056000001728534698486328125f, 0.0068999999202787876129150390625f, -0.184300005435943603515625f), float3(-0.014600000344216823577880859375f, 0.14020000398159027099609375f, 0.076200000941753387451171875f), float3(0.00999999977648258209228515625f, -0.19239999353885650634765625f, -0.03440000116825103759765625f), float3(-0.35769999027252197265625f, -0.53009998798370361328125f, -0.4357999861240386962890625f), float3(-0.3169000148773193359375f, 0.10629999637603759765625f, 0.015799999237060546875f), float3(0.010300000198185443878173828125f, -0.5868999958038330078125f, 0.0046000001020729541778564453125f), float3(-0.08969999849796295166015625f, -0.4939999878406524658203125f, 0.328700006008148193359375f), float3(0.7118999958038330078125f, -0.015399999916553497314453125f, -0.091799996793270111083984375f), float3(-0.053300000727176666259765625f, 0.0595999993383884429931640625f, -0.541100025177001953125f), float3(0.03519999980926513671875f, -0.063100002706050872802734375f, 0.546000003814697265625f), float3(-0.4776000082492828369140625f, 0.2847000062465667724609375f, -0.0271000005304813385009765625f) };

cbuffer SsaoParams
{
    float _27_u_radius : packoffset(c0);
    float _27_u_bias : packoffset(c0.y);
    float _27_u_near : packoffset(c0.z);
    float _27_u_far : packoffset(c0.w);
    float2 _27_u_screen_size : packoffset(c1);
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

float linearizeDepth(float d)
{
    float z = (d * 2.0f) - 1.0f;
    return ((2.0f * _27_u_near) * _27_u_far) / ((_27_u_far + _27_u_near) - (z * (_27_u_far - _27_u_near)));
}

float3 reconstructNormal(float2 uv)
{
    float2 texel = 1.0f.xx / _27_u_screen_size;
    float param = screenTexture.Sample(_screenTexture_sampler, uv).x;
    float dc = linearizeDepth(param);
    float param_1 = screenTexture.Sample(_screenTexture_sampler, uv - float2(texel.x, 0.0f)).x;
    float dl = linearizeDepth(param_1);
    float param_2 = screenTexture.Sample(_screenTexture_sampler, uv + float2(texel.x, 0.0f)).x;
    float dr = linearizeDepth(param_2);
    float param_3 = screenTexture.Sample(_screenTexture_sampler, uv - float2(0.0f, texel.y)).x;
    float db = linearizeDepth(param_3);
    float param_4 = screenTexture.Sample(_screenTexture_sampler, uv + float2(0.0f, texel.y)).x;
    float dt = linearizeDepth(param_4);
    return normalize(float3(dl - dr, db - dt, (2.0f * texel.x) * dc));
}

void frag_main()
{
    float depth = screenTexture.Sample(_screenTexture_sampler, vTexCoords).x;
    if (depth >= 1.0f)
    {
        FragColor = 1.0f.xxxx;
        return;
    }
    float param = depth;
    float linDepth = linearizeDepth(param);
    float2 param_1 = vTexCoords;
    float3 normal = reconstructNormal(param_1);
    float occlusion = 0.0f;
    float rScale = _27_u_radius / linDepth;
    for (int i = 0; i < 16; i++)
    {
        float3 sampleDir = _245[i];
        if (dot(sampleDir, normal) < 0.0f)
        {
            sampleDir = -sampleDir;
        }
        float2 sampleUV = vTexCoords + ((sampleDir.xy * rScale) * (1.0f.xx / _27_u_screen_size));
        float param_2 = screenTexture.Sample(_screenTexture_sampler, sampleUV).x;
        float sampleDepth = linearizeDepth(param_2);
        float rangeCheck = smoothstep(0.0f, 1.0f, _27_u_radius / abs(linDepth - sampleDepth));
        if (sampleDepth < (linDepth - _27_u_bias))
        {
            occlusion += rangeCheck;
        }
    }
    occlusion = 1.0f - (occlusion / 16.0f);
    FragColor = float4(occlusion.xxx, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
