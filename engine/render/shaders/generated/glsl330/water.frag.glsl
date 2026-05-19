#version 430

struct WaterParams
{
    float u_depth_handle;
    float u_water_level;
    float u_deep_r;
    float u_deep_g;
    float u_deep_b;
    float u_shallow_r;
    float u_shallow_g;
    float u_shallow_b;
    float u_max_depth;
    float u_transparency;
    float u_wave_amplitude;
    float u_wave_frequency;
    float u_wave_speed;
    float u_wave_dir_x;
    float u_wave_dir_y;
    float u_refraction_strength;
    float u_specular_power;
    float u_reflection_strength;
    float u_time;
    float u_sun_dir_x;
    float u_sun_dir_y;
    float u_sun_dir_z;
    float u_cam_pos_x;
    float u_cam_pos_y;
    float u_cam_pos_z;
    float u_near;
    float u_far;
    float u_fwd_x;
    float u_fwd_y;
    float u_fwd_z;
    float u_tan_fov_y;
    float u_aspect;
    float u_caustic_intensity;
    float u_caustic_scale;
    float u_foam_intensity;
    float u_foam_depth_threshold;
    float u_uw_fog_density;
    float u_uw_fog_r;
    float u_uw_fog_g;
    float u_uw_fog_b;
};

uniform WaterParams _29;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D u_depth_tex;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

float WaterLinZ(float d)
{
    float z = (d * 2.0) - 1.0;
    return ((2.0 * _29.u_near) * _29.u_far) / ((_29.u_far + _29.u_near) - (z * (_29.u_far - _29.u_near)));
}

vec3 GerstnerNormal(vec2 xz, float t, vec2 d1)
{
    float k = _29.u_wave_frequency;
    float a = _29.u_wave_amplitude;
    float sp = _29.u_wave_speed;
    vec2 d2 = vec2(-d1.y, d1.x);
    float p1 = (dot(d1, xz) * k) - (t * sp);
    float p2 = ((dot(d2, xz) * k) * 1.2999999523162841796875) - ((t * sp) * 0.699999988079071044921875);
    float dx = ((-k) * a) * ((d1.x * cos(p1)) + ((d2.x * 0.5) * cos(p2)));
    float dz = ((-k) * a) * ((d1.y * cos(p1)) + ((d2.y * 0.5) * cos(p2)));
    return normalize(vec3(-dx, 1.0, -dz));
}

void main()
{
    vec4 scene = texture(screenTexture, vTexCoords);
    float depth_raw = texture(u_depth_tex, vTexCoords).x;
    vec3 camFwd = vec3(_29.u_fwd_x, _29.u_fwd_y, _29.u_fwd_z);
    vec3 camPos = vec3(_29.u_cam_pos_x, _29.u_cam_pos_y, _29.u_cam_pos_z);
    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 camRight = normalize(cross(worldUp, camFwd));
    vec3 camUp = cross(camFwd, camRight);
    vec2 ndc = (vTexCoords * 2.0) - vec2(1.0);
    vec3 rayDir = normalize((camFwd + (((camRight * ndc.x) * _29.u_tan_fov_y) * _29.u_aspect)) + ((camUp * ndc.y) * _29.u_tan_fov_y));
    float denom = rayDir.y;
    if (abs(denom) < 9.9999999747524270787835121154785e-07)
    {
        FragColor = scene;
        return;
    }
    float t_hit = (_29.u_water_level - camPos.y) / denom;
    if (t_hit < 0.0)
    {
        FragColor = scene;
        return;
    }
    float _262;
    if (depth_raw < 0.99989998340606689453125)
    {
        float param = depth_raw;
        _262 = WaterLinZ(param) / max(dot(rayDir, camFwd), 9.9999997473787516355514526367188e-05);
    }
    else
    {
        _262 = 1000000.0;
    }
    float scene_linear = _262;
    if (t_hit > scene_linear)
    {
        FragColor = scene;
        return;
    }
    vec3 water_world = camPos + (rayDir * t_hit);
    vec2 waveDir = vec2(_29.u_wave_dir_x, _29.u_wave_dir_y);
    float underwater_depth = max(scene_linear - t_hit, 0.0);
    float depth_factor = clamp(underwater_depth / max(_29.u_max_depth, 0.00999999977648258209228515625), 0.0, 1.0);
    vec3 deepC = vec3(_29.u_deep_r, _29.u_deep_g, _29.u_deep_b);
    vec3 shallowC = vec3(_29.u_shallow_r, _29.u_shallow_g, _29.u_shallow_b);
    vec3 water_color = mix(shallowC, deepC, vec3(depth_factor));
    vec2 param_1 = water_world.xz;
    float param_2 = _29.u_time;
    vec2 param_3 = waveDir;
    vec3 wave_normal = GerstnerNormal(param_1, param_2, param_3);
    vec2 refract_offset = wave_normal.xz * _29.u_refraction_strength;
    vec2 refract_uv = clamp(vTexCoords + refract_offset, vec2(0.0), vec2(1.0));
    vec3 refracted = texture(screenTexture, refract_uv).xyz;
    float cos_theta = max(dot(-rayDir, wave_normal), 0.0);
    float fresnel = _29.u_reflection_strength + ((1.0 - _29.u_reflection_strength) * pow(1.0 - cos_theta, 5.0));
    vec3 reflected_dir = reflect(rayDir, wave_normal);
    float sky_grad = clamp((reflected_dir.y * 0.5) + 0.5, 0.0, 1.0);
    vec3 sky_color = mix(vec3(0.300000011920928955078125, 0.4000000059604644775390625, 0.5), vec3(0.60000002384185791015625, 0.75, 1.0), vec3(sky_grad));
    vec3 sunDir = vec3(_29.u_sun_dir_x, _29.u_sun_dir_y, _29.u_sun_dir_z);
    vec3 half_vec = normalize((-rayDir) + (-sunDir));
    float spec = pow(max(dot(wave_normal, half_vec), 0.0), _29.u_specular_power);
    vec3 caustic = vec3(0.0);
    if (_29.u_caustic_intensity > 0.001000000047497451305389404296875)
    {
        vec2 cUV = water_world.xz / vec2(_29.u_caustic_scale);
        float v1 = 1.0;
        float v2 = 1.0;
        for (int ci = 0; ci < 2; ci++)
        {
            float speed = (ci == 0) ? 0.4000000059604644775390625 : (-0.300000011920928955078125);
            vec2 uvc = cUV + vec2(_29.u_time * speed, (_29.u_time * speed) * 0.699999988079071044921875);
            vec2 cell = floor(uvc);
            vec2 frac_uv = fract(uvc);
            float minD = 1.0;
            for (int y = -1; y <= 1; y++)
            {
                for (int x = -1; x <= 1; x++)
                {
                    vec2 nb = vec2(float(x), float(y));
                    vec2 h = fract(sin(vec2(dot(cell + nb, vec2(127.09999847412109375, 311.70001220703125)), dot(cell + nb, vec2(269.5, 183.3000030517578125)))) * 43758.546875);
                    vec2 diff = (nb + h) - frac_uv;
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
        float pattern = clamp(pow(min(v1, v2), 0.5) * 2.0, 0.0, 1.0);
        pattern = 1.0 - pattern;
        pattern = pow(pattern, 2.5);
        caustic = (vec3(pattern) * _29.u_caustic_intensity) * (1.0 - depth_factor);
    }
    float foam = 0.0;
    if (_29.u_foam_intensity > 0.001000000047497451305389404296875)
    {
        foam = (1.0 - smoothstep(0.0, _29.u_foam_depth_threshold, underwater_depth)) * _29.u_foam_intensity;
        float foam_noise = fract(sin(dot((water_world.xz * 5.0) + vec2(_29.u_time * 0.300000011920928955078125), vec2(12.98980045318603515625, 78.233001708984375))) * 43758.546875);
        foam *= (0.60000002384185791015625 + (0.4000000059604644775390625 * foam_noise));
    }
    vec3 underwater = mix(refracted, water_color, vec3(depth_factor * _29.u_transparency)) + caustic;
    vec3 surface = (mix(underwater, sky_color, vec3(fresnel)) + vec3(spec)) + vec3(foam);
    bool _642 = camPos.y < _29.u_water_level;
    bool _649;
    if (_642)
    {
        _649 = _29.u_uw_fog_density > 0.001000000047497451305389404296875;
    }
    else
    {
        _649 = _642;
    }
    if (_649)
    {
        float fog_dist = length(water_world - camPos);
        float fog_factor = 1.0 - exp((-_29.u_uw_fog_density) * fog_dist);
        surface = mix(surface, vec3(_29.u_uw_fog_r, _29.u_uw_fog_g, _29.u_uw_fog_b), vec3(clamp(fog_factor, 0.0, 1.0)));
    }
    float edge_fade = smoothstep(0.0, 0.5, underwater_depth);
    float alpha = _29.u_transparency * edge_fade;
    FragColor = vec4(mix(scene.xyz, surface, vec3(alpha)), scene.w);
}

