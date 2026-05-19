cbuffer ContactShadowParams
{
    float3 _23_u_light_dir : packoffset(c0);
    float _23_u_near : packoffset(c0.w);
    float _23_u_far : packoffset(c1);
    float2 _23_u_screen_size : packoffset(c1.z);
    float _23_u_strength : packoffset(c2);
    float _23_u_step_size : packoffset(c2.y);
    int _23_u_num_steps : packoffset(c2.z);
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
    return ((2.0f * _23_u_near) * _23_u_far) / ((_23_u_far + _23_u_near) - (z * (_23_u_far - _23_u_near)));
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
    float3 lightDir = normalize(_23_u_light_dir);
    float2 texelSize = 1.0f.xx / _23_u_screen_size;
    float occlusion = 0.0f;
    int validSteps = 0;
    for (int i = 1; i <= _23_u_num_steps; i++)
    {
        float dist = _23_u_step_size * float(i);
        float2 sampleUV = vTexCoords + (((lightDir.xy * texelSize) * dist) * 50.0f);
        bool _127 = sampleUV.x < 0.0f;
        bool _135;
        if (!_127)
        {
            _135 = sampleUV.y < 0.0f;
        }
        else
        {
            _135 = _127;
        }
        bool _142;
        if (!_135)
        {
            _142 = sampleUV.x > 1.0f;
        }
        else
        {
            _142 = _135;
        }
        bool _149;
        if (!_142)
        {
            _149 = sampleUV.y > 1.0f;
        }
        else
        {
            _149 = _142;
        }
        if (_149)
        {
            break;
        }
        float sampleDepth = screenTexture.SampleLevel(_screenTexture_sampler, sampleUV, 0.0f).x;
        if (sampleDepth >= 1.0f)
        {
            continue;
        }
        float param_1 = sampleDepth;
        float sampleLin = linearizeDepth(param_1);
        float diff = sampleLin - linDepth;
        bool _172 = diff > 0.0f;
        bool _179;
        if (_172)
        {
            _179 = diff < _23_u_step_size;
        }
        else
        {
            _179 = _172;
        }
        if (_179)
        {
            float k = 1.0f - (diff / _23_u_step_size);
            occlusion += (k * k);
        }
        validSteps++;
    }
    float _200;
    if (validSteps > 0)
    {
        _200 = 1.0f - clamp((occlusion / float(validSteps)) * _23_u_strength, 0.0f, 1.0f);
    }
    else
    {
        _200 = 1.0f;
    }
    float shadow = _200;
    FragColor = float4(shadow.xxx, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
