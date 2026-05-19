cbuffer VolumetricFogParams
{
    float _20_u_depth_handle : packoffset(c0);
    float _20_u_fog_r : packoffset(c0.y);
    float _20_u_fog_g : packoffset(c0.z);
    float _20_u_fog_b : packoffset(c0.w);
    float _20_u_fog_density : packoffset(c1);
    float _20_u_height_falloff : packoffset(c1.y);
    float _20_u_height_offset : packoffset(c1.z);
    float _20_u_fog_start : packoffset(c1.w);
    float _20_u_fog_end : packoffset(c2);
    float _20_u_fog_steps : packoffset(c2.y);
    float _20_u_sun_scatter : packoffset(c2.z);
    float _20_u_sun_dir_x : packoffset(c2.w);
    float _20_u_sun_dir_y : packoffset(c3);
    float _20_u_sun_dir_z : packoffset(c3.y);
    float _20_u_cam_pos_x : packoffset(c3.z);
    float _20_u_cam_pos_y : packoffset(c3.w);
    float _20_u_cam_pos_z : packoffset(c4);
    float _20_u_near : packoffset(c4.y);
    float _20_u_far : packoffset(c4.z);
    float _20_u_right_x : packoffset(c4.w);
    float _20_u_right_y : packoffset(c5);
    float _20_u_right_z : packoffset(c5.y);
    float _20_u_up_x : packoffset(c5.z);
    float _20_u_up_y : packoffset(c5.w);
    float _20_u_up_z : packoffset(c6);
    float _20_u_fwd_x : packoffset(c6.y);
    float _20_u_fwd_y : packoffset(c6.z);
    float _20_u_fwd_z : packoffset(c6.w);
    float _20_u_tan_fov_y : packoffset(c7);
    float _20_u_aspect : packoffset(c7.y);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);
Texture2D<float4> u_depth_tex : register(t1);
SamplerState _u_depth_tex_sampler : register(s1);

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

float VFogLinZ(float d)
{
    float z = (d * 2.0f) - 1.0f;
    return ((2.0f * _20_u_near) * _20_u_far) / ((_20_u_far + _20_u_near) - (z * (_20_u_far - _20_u_near)));
}

void frag_main()
{
    float4 scene = screenTexture.Sample(_screenTexture_sampler, vTexCoords);
    float depth = u_depth_tex.Sample(_u_depth_tex_sampler, vTexCoords).x;
    if (depth >= 0.99989998340606689453125f)
    {
        FragColor = scene;
        return;
    }
    float param = depth;
    float viewZ = VFogLinZ(param);
    float2 ndc = (vTexCoords * 2.0f) - 1.0f.xx;
    float3 camFwd = float3(_20_u_fwd_x, _20_u_fwd_y, _20_u_fwd_z);
    float3 camRight = float3(_20_u_right_x, _20_u_right_y, _20_u_right_z);
    float3 camUp = float3(_20_u_up_x, _20_u_up_y, _20_u_up_z);
    float3 viewDir = normalize((camFwd + (((camRight * ndc.x) * _20_u_tan_fov_y) * _20_u_aspect)) + ((camUp * ndc.y) * _20_u_tan_fov_y));
    float cosAngle = max(dot(viewDir, camFwd), 9.9999997473787516355514526367188e-05f);
    float rayLen = viewZ / cosAngle;
    float marchStart = _20_u_fog_start;
    float marchEnd = min(rayLen, _20_u_fog_end);
    float steps = max(_20_u_fog_steps, 1.0f);
    if (marchEnd <= marchStart)
    {
        FragColor = scene;
        return;
    }
    float stepLen = (marchEnd - marchStart) / steps;
    float3 sunDir = float3(_20_u_sun_dir_x, _20_u_sun_dir_y, _20_u_sun_dir_z);
    float cosTheta = dot(viewDir, -sunDir);
    float g = 0.7599999904632568359375f;
    float g2 = g * g;
    float mie = (1.0f - g2) / (12.56637096405029296875f * pow(max((1.0f + g2) - ((2.0f * g) * cosTheta), 0.001000000047497451305389404296875f), 1.5f));
    float3 fogColor = float3(_20_u_fog_r, _20_u_fog_g, _20_u_fog_b);
    float3 camPos = float3(_20_u_cam_pos_x, _20_u_cam_pos_y, _20_u_cam_pos_z);
    float transmit = 1.0f;
    float3 inscatter = 0.0f.xxx;
    for (float i = 0.0f; i < steps; i += 1.0f)
    {
        float t = marchStart + ((i + 0.5f) * stepLen);
        float3 pos = camPos + (viewDir * t);
        float h = max(pos.y - _20_u_height_offset, 0.0f);
        float den = _20_u_fog_density * exp((-_20_u_height_falloff) * h);
        float sT = exp((-den) * stepLen);
        inscatter += ((fogColor + (1.0f.xxx * (mie * _20_u_sun_scatter))) * (transmit * (1.0f - sT)));
        transmit *= sT;
        if (transmit < 0.001000000047497451305389404296875f)
        {
            break;
        }
    }
    FragColor = float4((scene.xyz * transmit) + inscatter, scene.w);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
