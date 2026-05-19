cbuffer MvParams
{
    float _46_screen_w : packoffset(c0);
    float _46_screen_h : packoffset(c0.y);
    row_major float4x4 _46_reproj : packoffset(c1);
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

void frag_main()
{
    float depth = screenTexture.Sample(_screenTexture_sampler, vTexCoords).x;
    float2 ndc = (vTexCoords * 2.0f) - 1.0f.xx;
    float z_ndc = (depth * 2.0f) - 1.0f;
    float4 clip_pos = float4(ndc, z_ndc, 1.0f);
    float4 prev_clip = mul(clip_pos, _46_reproj);
    float _56 = prev_clip.w;
    float4 _57 = prev_clip;
    float2 _60 = _57.xy / _56.xx;
    prev_clip.x = _60.x;
    prev_clip.y = _60.y;
    float2 prev_uv = (prev_clip.xy * 0.5f) + 0.5f.xx;
    float2 velocity = vTexCoords - prev_uv;
    FragColor = float4(velocity, 0.0f, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
