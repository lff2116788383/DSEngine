#version 430

struct TonemapParams
{
    float u_manual_exposure;
    int u_auto_exposure_enabled;
    int u_lut_enabled;
    float u_lut_intensity;
};

uniform TonemapParams _68;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D autoExposureTex;
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

void main()
{
    vec3 hdrColor = texture(screenTexture, vTexCoords).xyz;
    float finalExposure = _68.u_manual_exposure;
    if (_68.u_auto_exposure_enabled != 0)
    {
        finalExposure = texture(autoExposureTex, vec2(0.5)).x;
    }
    vec3 param = hdrColor * finalExposure;
    vec3 result = AcesFilmic(param);
    result = pow(result, vec3(0.4545454680919647216796875));
    if (_68.u_lut_enabled != 0)
    {
        vec3 lutColor = texture(u_lut, clamp(result, vec3(0.0), vec3(1.0))).xyz;
        result = mix(result, lutColor, vec3(_68.u_lut_intensity));
    }
    float ign = fract(52.98291778564453125 * fract((0.067110560834407806396484375 * gl_FragCoord.x) + (0.005837149918079376220703125 * gl_FragCoord.y)));
    result += vec3((ign - 0.5) / 255.0);
    FragColor = vec4(result, 1.0);
}

