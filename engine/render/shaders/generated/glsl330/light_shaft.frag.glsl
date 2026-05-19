#version 430

struct LightShaftParams
{
    float u_depth_handle;
    float u_sun_x;
    float u_sun_y;
    float u_light_r;
    float u_light_g;
    float u_light_b;
    float u_density;
    float u_weight;
    float u_decay;
    float u_exposure;
    float u_num_samples;
    float u_intensity;
    float pad0;
    float pad1;
    float pad2;
    float pad3;
};

uniform LightShaftParams _25;

layout(binding = 0) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D u_depth_tex;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec4 scene = texture(screenTexture, vTexCoords);
    int samples = int(_25.u_num_samples);
    vec2 delta_uv = ((vec2(_25.u_sun_x, _25.u_sun_y) - vTexCoords) * _25.u_density) / vec2(float(samples));
    vec2 uv = vTexCoords;
    float illum_decay = 1.0;
    vec3 accumulated = vec3(0.0);
    for (int i = 0; i < samples; i++)
    {
        uv += delta_uv;
        vec2 suv = clamp(uv, vec2(0.001000000047497451305389404296875), vec2(0.999000012874603271484375));
        float d = texture(u_depth_tex, suv).x;
        vec3 s = texture(screenTexture, suv).xyz;
        float sky = step(0.99989998340606689453125, d);
        float lum = dot(s, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
        float bright = smoothstep(0.800000011920928955078125, 1.2000000476837158203125, lum);
        float mask = max(sky, bright);
        accumulated += (((s * mask) * illum_decay) * _25.u_weight);
        illum_decay *= _25.u_decay;
        if (illum_decay < 0.0030000000260770320892333984375)
        {
            break;
        }
    }
    vec3 result = scene.xyz + (((accumulated * _25.u_exposure) * vec3(_25.u_light_r, _25.u_light_g, _25.u_light_b)) * _25.u_intensity);
    FragColor = vec4(result, 1.0);
}

