#version 430

struct MotionBlurParams
{
    float intensity;
    float num_samples;
    float screen_w;
    float screen_h;
};

uniform MotionBlurParams _23;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D u_color_texture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec2 velocity = texture(screenTexture, vTexCoords).xy * _23.intensity;
    int samples = max(int(_23.num_samples), 1);
    vec3 color = texture(u_color_texture, vTexCoords).xyz;
    float total = 1.0;
    for (int i = 1; i < samples; i++)
    {
        float t = float(i) / float(samples);
        vec2 sample_uv = vTexCoords + (velocity * t);
        bool _75 = sample_uv.x >= 0.0;
        bool _81;
        if (_75)
        {
            _81 = sample_uv.x <= 1.0;
        }
        else
        {
            _81 = _75;
        }
        bool _88;
        if (_81)
        {
            _88 = sample_uv.y >= 0.0;
        }
        else
        {
            _88 = _81;
        }
        bool _94;
        if (_88)
        {
            _94 = sample_uv.y <= 1.0;
        }
        else
        {
            _94 = _88;
        }
        if (_94)
        {
            color += texture(u_color_texture, sample_uv).xyz;
            total += 1.0;
        }
    }
    FragColor = vec4(color / vec3(total), 1.0);
}

