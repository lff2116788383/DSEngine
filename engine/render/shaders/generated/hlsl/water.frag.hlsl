cbuffer WaterParams
{
    float _29_u_depth_handle : packoffset(c0);
    float _29_u_water_level : packoffset(c0.y);
    float _29_u_deep_r : packoffset(c0.z);
    float _29_u_deep_g : packoffset(c0.w);
    float _29_u_deep_b : packoffset(c1);
    float _29_u_shallow_r : packoffset(c1.y);
    float _29_u_shallow_g : packoffset(c1.z);
    float _29_u_shallow_b : packoffset(c1.w);
    float _29_u_max_depth : packoffset(c2);
    float _29_u_transparency : packoffset(c2.y);
    float _29_u_wave_amplitude : packoffset(c2.z);
    float _29_u_wave_frequency : packoffset(c2.w);
    float _29_u_wave_speed : packoffset(c3);
    float _29_u_wave_dir_x : packoffset(c3.y);
    float _29_u_wave_dir_y : packoffset(c3.z);
    float _29_u_refraction_strength : packoffset(c3.w);
    float _29_u_specular_power : packoffset(c4);
    float _29_u_reflection_strength : packoffset(c4.y);
    float _29_u_time : packoffset(c4.z);
    float _29_u_sun_dir_x : packoffset(c4.w);
    float _29_u_sun_dir_y : packoffset(c5);
    float _29_u_sun_dir_z : packoffset(c5.y);
    float _29_u_cam_pos_x : packoffset(c5.z);
    float _29_u_cam_pos_y : packoffset(c5.w);
    float _29_u_cam_pos_z : packoffset(c6);
    float _29_u_near : packoffset(c6.y);
    float _29_u_far : packoffset(c6.z);
    float _29_u_fwd_x : packoffset(c6.w);
    float _29_u_fwd_y : packoffset(c7);
    float _29_u_fwd_z : packoffset(c7.y);
    float _29_u_tan_fov_y : packoffset(c7.z);
    float _29_u_aspect : packoffset(c7.w);
    float _29_u_caustic_intensity : packoffset(c8);
    float _29_u_caustic_scale : packoffset(c8.y);
    float _29_u_foam_intensity : packoffset(c8.z);
    float _29_u_foam_depth_threshold : packoffset(c8.w);
    float _29_u_uw_fog_density : packoffset(c9);
    float _29_u_uw_fog_r : packoffset(c9.y);
    float _29_u_uw_fog_g : packoffset(c9.z);
    float _29_u_uw_fog_b : packoffset(c9.w);
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

float WaterLinZ(float d)
{
    float z = (d * 2.0f) - 1.0f;
    return ((2.0f * _29_u_near) * _29_u_far) / ((_29_u_far + _29_u_near) - (z * (_29_u_far - _29_u_near)));
}

float3 GerstnerNormal(float2 xz, float t, float2 d1)
{
    float k = _29_u_wave_frequency;
    float a = _29_u_wave_amplitude;
    float sp = _29_u_wave_speed;
    float2 d2 = float2(-d1.y, d1.x);
    float p1 = (dot(d1, xz) * k) - (t * sp);
    float p2 = ((dot(d2, xz) * k) * 1.2999999523162841796875f) - ((t * sp) * 0.699999988079071044921875f);
    float dx = ((-k) * a) * ((d1.x * cos(p1)) + ((d2.x * 0.5f) * cos(p2)));
    float dz = ((-k) * a) * ((d1.y * cos(p1)) + ((d2.y * 0.5f) * cos(p2)));
    return normalize(float3(-dx, 1.0f, -dz));
}

void frag_main()
{
    float4 scene = screenTexture.Sample(_screenTexture_sampler, vTexCoords);
    float depth_raw = u_depth_tex.Sample(_u_depth_tex_sampler, vTexCoords).x;
    float3 camFwd = float3(_29_u_fwd_x, _29_u_fwd_y, _29_u_fwd_z);
    float3 camPos = float3(_29_u_cam_pos_x, _29_u_cam_pos_y, _29_u_cam_pos_z);
    float3 worldUp = float3(0.0f, 1.0f, 0.0f);
    float3 camRight = normalize(cross(worldUp, camFwd));
    float3 camUp = cross(camFwd, camRight);
    float2 ndc = (vTexCoords * 2.0f) - 1.0f.xx;
    float3 rayDir = normalize((camFwd + (((camRight * ndc.x) * _29_u_tan_fov_y) * _29_u_aspect)) + ((camUp * ndc.y) * _29_u_tan_fov_y));
    float denom = rayDir.y;
    if (abs(denom) < 9.9999999747524270787835121154785e-07f)
    {
        FragColor = scene;
        return;
    }
    float t_hit = (_29_u_water_level - camPos.y) / denom;
    if (t_hit < 0.0f)
    {
        FragColor = scene;
        return;
    }
    float _262;
    if (depth_raw < 0.99989998340606689453125f)
    {
        float param = depth_raw;
        _262 = WaterLinZ(param) / max(dot(rayDir, camFwd), 9.9999997473787516355514526367188e-05f);
    }
    else
    {
        _262 = 1000000.0f;
    }
    float scene_linear = _262;
    if (t_hit > scene_linear)
    {
        FragColor = scene;
        return;
    }
    float3 water_world = camPos + (rayDir * t_hit);
    float2 waveDir = float2(_29_u_wave_dir_x, _29_u_wave_dir_y);
    float underwater_depth = max(scene_linear - t_hit, 0.0f);
    float depth_factor = clamp(underwater_depth / max(_29_u_max_depth, 0.00999999977648258209228515625f), 0.0f, 1.0f);
    float3 deepC = float3(_29_u_deep_r, _29_u_deep_g, _29_u_deep_b);
    float3 shallowC = float3(_29_u_shallow_r, _29_u_shallow_g, _29_u_shallow_b);
    float3 water_color = lerp(shallowC, deepC, depth_factor.xxx);
    float2 param_1 = water_world.xz;
    float param_2 = _29_u_time;
    float2 param_3 = waveDir;
    float3 wave_normal = GerstnerNormal(param_1, param_2, param_3);
    float2 refract_offset = wave_normal.xz * _29_u_refraction_strength;
    float2 refract_uv = clamp(vTexCoords + refract_offset, 0.0f.xx, 1.0f.xx);
    float3 refracted = screenTexture.Sample(_screenTexture_sampler, refract_uv).xyz;
    float cos_theta = max(dot(-rayDir, wave_normal), 0.0f);
    float fresnel = _29_u_reflection_strength + ((1.0f - _29_u_reflection_strength) * pow(1.0f - cos_theta, 5.0f));
    float3 reflected_dir = reflect(rayDir, wave_normal);
    float sky_grad = clamp((reflected_dir.y * 0.5f) + 0.5f, 0.0f, 1.0f);
    float3 sky_color = lerp(float3(0.300000011920928955078125f, 0.4000000059604644775390625f, 0.5f), float3(0.60000002384185791015625f, 0.75f, 1.0f), sky_grad.xxx);
    float3 sunDir = float3(_29_u_sun_dir_x, _29_u_sun_dir_y, _29_u_sun_dir_z);
    float3 half_vec = normalize((-rayDir) + (-sunDir));
    float spec = pow(max(dot(wave_normal, half_vec), 0.0f), _29_u_specular_power);
    float3 caustic = 0.0f.xxx;
    if (_29_u_caustic_intensity > 0.001000000047497451305389404296875f)
    {
        float2 cUV = water_world.xz / _29_u_caustic_scale.xx;
        float v1 = 1.0f;
        float v2 = 1.0f;
        for (int ci = 0; ci < 2; ci++)
        {
            float speed = (ci == 0) ? 0.4000000059604644775390625f : (-0.300000011920928955078125f);
            float2 uvc = cUV + float2(_29_u_time * speed, (_29_u_time * speed) * 0.699999988079071044921875f);
            float2 cell = floor(uvc);
            float2 frac_uv = frac(uvc);
            float minD = 1.0f;
            for (int y = -1; y <= 1; y++)
            {
                for (int x = -1; x <= 1; x++)
                {
                    float2 nb = float2(float(x), float(y));
                    float2 h = frac(sin(float2(dot(cell + nb, float2(127.09999847412109375f, 311.70001220703125f)), dot(cell + nb, float2(269.5f, 183.3000030517578125f)))) * 43758.546875f);
                    float2 diff = (nb + h) - frac_uv;
                    minD = min(minD, dot(diff, diff));
                }
            }
            if (ci == 0)
            {
                v1 = minD;
            }
            else
            {
                v2 = minD;
            }
        }
        float pattern = clamp(pow(min(v1, v2), 0.5f) * 2.0f, 0.0f, 1.0f);
        pattern = 1.0f - pattern;
        pattern = pow(pattern, 2.5f);
        caustic = (pattern.xxx * _29_u_caustic_intensity) * (1.0f - depth_factor);
    }
    float foam = 0.0f;
    if (_29_u_foam_intensity > 0.001000000047497451305389404296875f)
    {
        foam = (1.0f - smoothstep(0.0f, _29_u_foam_depth_threshold, underwater_depth)) * _29_u_foam_intensity;
        float foam_noise = frac(sin(dot((water_world.xz * 5.0f) + (_29_u_time * 0.300000011920928955078125f).xx, float2(12.98980045318603515625f, 78.233001708984375f))) * 43758.546875f);
        foam *= (0.60000002384185791015625f + (0.4000000059604644775390625f * foam_noise));
    }
    float3 underwater = lerp(refracted, water_color, (depth_factor * _29_u_transparency).xxx) + caustic;
    float3 surface = (lerp(underwater, sky_color, fresnel.xxx) + spec.xxx) + foam.xxx;
    bool _642 = camPos.y < _29_u_water_level;
    bool _649;
    if (_642)
    {
        _649 = _29_u_uw_fog_density > 0.001000000047497451305389404296875f;
    }
    else
    {
        _649 = _642;
    }
    if (_649)
    {
        float fog_dist = length(water_world - camPos);
        float fog_factor = 1.0f - exp((-_29_u_uw_fog_density) * fog_dist);
        surface = lerp(surface, float3(_29_u_uw_fog_r, _29_u_uw_fog_g, _29_u_uw_fog_b), clamp(fog_factor, 0.0f, 1.0f).xxx);
    }
    float edge_fade = smoothstep(0.0f, 0.5f, underwater_depth);
    float alpha = _29_u_transparency * edge_fade;
    FragColor = float4(lerp(scene.xyz, surface, alpha.xxx), scene.w);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
