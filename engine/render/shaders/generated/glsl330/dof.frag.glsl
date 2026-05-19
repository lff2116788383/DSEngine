#version 430

struct DofParams
{
    float focus_distance;
    float focus_range;
    float bokeh_radius;
    float near_plane;
    float far_plane;
    float screen_w;
    float screen_h;
};

uniform DofParams _20;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D u_color_texture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

float linearizeDepth(float d)
{
    float z = (d * 2.0) - 1.0;
    return ((2.0 * _20.near_plane) * _20.far_plane) / ((_20.far_plane + _20.near_plane) - (z * (_20.far_plane - _20.near_plane)));
}

void main()
{
    float depth = texture(screenTexture, vTexCoords).x;
    float param = depth;
    float lin_depth = linearizeDepth(param);
    float coc = clamp(abs(lin_depth - _20.focus_distance) / _20.focus_range, 0.0, 1.0);
    vec2 texel = vec2(1.0) / vec2(_20.screen_w, _20.screen_h);
    float radius = coc * _20.bokeh_radius;
    vec3 color = vec3(0.0);
    float total_weight = 0.0;
    for (int i = 0; i < 16; i++)
    {
        float r = sqrt(float(i) / 16.0) * radius;
        float theta = float(i) * 2.3999631404876708984375;
        vec2 offset = (vec2(cos(theta), sin(theta)) * r) * texel;
        float param_1 = texture(screenTexture, vTexCoords + offset).x;
        float sample_depth = linearizeDepth(param_1);
        float sample_coc = clamp(abs(sample_depth - _20.focus_distance) / _20.focus_range, 0.0, 1.0);
        float w = max(sample_coc, coc);
        color += (texture(u_color_texture, vTexCoords + offset).xyz * w);
        total_weight += w;
    }
    if (total_weight > 0.0)
    {
        color /= vec3(total_weight);
    }
    else
    {
        color = texture(u_color_texture, vTexCoords).xyz;
    }
    FragColor = vec4(color, 1.0);
}

