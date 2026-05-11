static float4 gl_Position;
static float2 vTexCoords;
static float2 aTexCoords;
static float2 aPos;

struct SPIRV_Cross_Input
{
    float2 aPos : TEXCOORD0;
    float2 aTexCoords : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float2 vTexCoords : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    vTexCoords = aTexCoords;
    gl_Position = float4(aPos, 0.0f, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    aTexCoords = stage_input.aTexCoords;
    aPos = stage_input.aPos;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vTexCoords = vTexCoords;
    return stage_output;
}
