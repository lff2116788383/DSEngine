cbuffer DeferredLightParams
{
    float3 _58_u_light_dir : packoffset(c0);
    float _58_u_light_intensity : packoffset(c0.w);
    float3 _58_u_light_color : packoffset(c1);
    float _58_u_ambient : packoffset(c1.w);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);
Texture2D<float4> u_gbuf_normal : register(t1);
SamplerState _u_gbuf_normal_sampler : register(s1);
Texture2D<float4> u_gbuf_position : register(t2);
SamplerState _u_gbuf_position_sampler : register(s2);

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
    float3 albedo = screenTexture.Sample(_screenTexture_sampler, vTexCoords).xyz;
    float3 normal = (u_gbuf_normal.Sample(_u_gbuf_normal_sampler, vTexCoords).xyz * 2.0f) - 1.0f.xxx;
    float3 position = u_gbuf_position.Sample(_u_gbuf_position_sampler, vTexCoords).xyz;
    if (length(normal) < 0.00999999977648258209228515625f)
    {
        FragColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }
    normal = normalize(normal);
    float NdotL = max(dot(normal, -normalize(_58_u_light_dir)), 0.0f);
    float3 diffuse = ((albedo * _58_u_light_color) * _58_u_light_intensity) * NdotL;
    float3 ambient_color = albedo * _58_u_ambient;
    FragColor = float4(diffuse + ambient_color, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
