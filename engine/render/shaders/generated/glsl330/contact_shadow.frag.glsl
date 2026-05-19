#version 430

struct ContactShadowParams
{
    vec3 u_light_dir;
    float u_near;
    float u_far;
    vec2 u_screen_size;
    float u_strength;
    float u_step_size;
    int u_num_steps;
};

uniform ContactShadowParams _23;

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

float linearizeDepth(float d)
{
    float z = (d * 2.0) - 1.0;
    return ((2.0 * _23.u_near) * _23.u_far) / ((_23.u_far + _23.u_near) - (z * (_23.u_far - _23.u_near)));
}

void main()
{
    float depth = texture(screenTexture, vTexCoords).x;
    if (depth >= 1.0)
    {
        FragColor = vec4(1.0);
        return;
    }
    float param = depth;
    float linDepth = linearizeDepth(param);
    vec3 lightDir = normalize(_23.u_light_dir);
    vec2 texelSize = vec2(1.0) / _23.u_screen_size;
    float occlusion = 0.0;
    int validSteps = 0;
    for (int i = 1; i <= _23.u_num_steps; i++)
    {
        float dist = _23.u_step_size * float(i);
        vec2 sampleUV = vTexCoords + (((lightDir.xy * texelSize) * dist) * 50.0);
        bool _127 = sampleUV.x < 0.0;
        bool _135;
        if (!_127)
        {
            _135 = sampleUV.y < 0.0;
        }
        else
        {
            _135 = _127;
        }
        bool _142;
        if (!_135)
        {
            _142 = sampleUV.x > 1.0;
        }
        else
        {
            _142 = _135;
        }
        bool _149;
        if (!_142)
        {
            _149 = sampleUV.y > 1.0;
        }
        else
        {
            _149 = _142;
        }
        if (_149)
        {
            break;
        }
        float sampleDepth = textureLod(screenTexture, sampleUV, 0.0).x;
        if (sampleDepth >= 1.0)
        {
            continue;
        }
        float param_1 = sampleDepth;
        float sampleLin = linearizeDepth(param_1);
        float diff = sampleLin - linDepth;
        bool _172 = diff > 0.0;
        bool _179;
        if (_172)
        {
            _179 = diff < _23.u_step_size;
        }
        else
        {
            _179 = _172;
        }
        if (_179)
        {
            float k = 1.0 - (diff / _23.u_step_size);
            occlusion += (k * k);
        }
        validSteps++;
    }
    float _200;
    if (validSteps > 0)
    {
        _200 = 1.0 - clamp((occlusion / float(validSteps)) * _23.u_strength, 0.0, 1.0);
    }
    else
    {
        _200 = 1.0;
    }
    float shadow = _200;
    FragColor = vec4(vec3(shadow), 1.0);
}

