#version 430

struct LumAdaptParams
{
    float u_dt;
    float u_speed_up;
    float u_speed_down;
    float u_min_exposure;
    float u_max_exposure;
    float u_compensation;
};

uniform LumAdaptParams _34;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D prevAdaptedTex;

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 vTexCoords;

void main()
{
    float avgLogLum = texture(screenTexture, vec2(0.5)).x;
    float avgLum = exp(avgLogLum);
    float targetExposure = 0.180000007152557373046875 / max(avgLum, 0.001000000047497451305389404296875);
    targetExposure = clamp(targetExposure * exp2(_34.u_compensation), _34.u_min_exposure, _34.u_max_exposure);
    float prevExposure = texture(prevAdaptedTex, vec2(0.5)).x;
    if (prevExposure <= 0.0)
    {
        prevExposure = targetExposure;
    }
    float _65;
    if (targetExposure > prevExposure)
    {
        _65 = _34.u_speed_up;
    }
    else
    {
        _65 = _34.u_speed_down;
    }
    float speed = _65;
    float adapted = prevExposure + ((targetExposure - prevExposure) * (1.0 - exp((-_34.u_dt) * speed)));
    FragColor = vec4(adapted, 0.0, 0.0, 1.0);
}

