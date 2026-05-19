#version 430

struct BloomCompositeAeParams
{
    float exposure;
    float bloomIntensity;
    int bloomEnabled;
    int ssaoEnabled;
    int autoExposureEnabled;
    int lutEnabled;
    float lutIntensity;
    int csEnabled;
    float csStrength;
    int vignetteEnabled;
    float vignetteIntensity;
    float vignetteRadius;
    float vignetteSoftness;
    int filmGrainEnabled;
    float filmGrainIntensity;
    float filmGrainTime;
};

uniform BloomCompositeAeParams _90;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 3) uniform sampler2D ssaoTexture;
layout(binding = 2) uniform sampler2D bloomBlur;
layout(binding = 6) uniform sampler2D contactShadowTex;
layout(binding = 4) uniform sampler2D autoExposureTex;
layout(binding = 5) uniform sampler3D u_lut;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

vec3 AcesFilmic(vec3 x)
{
    float a = 2.5099999904632568359375;
    float b = 0.02999999932944774627685546875;
    float c = 2.4300000667572021484375;
    float d = 0.589999973773956298828125;
    float e = 0.14000000059604644775390625;
    return clamp((x * ((x * a) + vec3(b))) / ((x * ((x * c) + vec3(d))) + vec3(e)), vec3(0.0), vec3(1.0));
}

float GrainNoise(vec2 uv, float time_seed)
{
    return fract(sin(dot(uv + vec2(time_seed, time_seed * 0.37000000476837158203125), vec2(12.98980045318603515625, 78.233001708984375))) * 43758.546875);
}

void main()
{
    vec3 color = texture(screenTexture, vTexCoords).xyz;
    if (_90.ssaoEnabled != 0)
    {
        float ao = texture(ssaoTexture, vTexCoords).x;
        color *= ao;
    }
    if (_90.bloomEnabled != 0)
    {
        vec3 bloomColor = texture(bloomBlur, vTexCoords).xyz;
        color += (bloomColor * _90.bloomIntensity);
    }
    if (_90.csEnabled != 0)
    {
        float cs = texture(contactShadowTex, vTexCoords).x;
        color *= (1.0 - ((1.0 - cs) * _90.csStrength));
    }
    float finalExposure = _90.exposure;
    if (_90.autoExposureEnabled != 0)
    {
        finalExposure = texture(autoExposureTex, vec2(0.5)).x;
    }
    vec3 param = color * finalExposure;
    color = AcesFilmic(param);
    color = pow(color, vec3(0.4545454680919647216796875));
    if (_90.lutEnabled != 0)
    {
        vec3 lutColor = texture(u_lut, clamp(color, vec3(0.0), vec3(1.0))).xyz;
        color = mix(color, lutColor, vec3(_90.lutIntensity));
    }
    if (_90.vignetteEnabled != 0)
    {
        float dist = length(vTexCoords - vec2(0.5));
        float radius = clamp(_90.vignetteRadius, 0.001000000047497451305389404296875, 1.5);
        float softness = max(_90.vignetteSoftness, 9.9999997473787516355514526367188e-05);
        float vignette = 1.0 - smoothstep(radius, radius + softness, dist);
        color *= mix(1.0, vignette, clamp(_90.vignetteIntensity, 0.0, 1.0));
    }
    if (_90.filmGrainEnabled != 0)
    {
        vec2 param_1 = vTexCoords * vec2(1280.0, 720.0);
        float param_2 = _90.filmGrainTime;
        float grain = GrainNoise(param_1, param_2) - 0.5;
        color = clamp(color + vec3(grain * _90.filmGrainIntensity), vec3(0.0), vec3(1.0));
    }
    float ign = fract(52.98291778564453125 * fract((0.067110560834407806396484375 * gl_FragCoord.x) + (0.005837149918079376220703125 * gl_FragCoord.y)));
    color += vec3((ign - 0.5) / 255.0);
    FragColor = vec4(color, 1.0);
}

