#version 430

struct TaaParams
{
    float u_blend_factor;
    float u_jitter_x;
    float u_jitter_y;
    int u_frame_index;
    float u_screen_w;
    float u_screen_h;
};

uniform TaaParams _36;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D u_motion_vector;
layout(binding = 5) uniform sampler2D u_history;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec3 current = texture(screenTexture, vTexCoords).xyz;
    vec2 mv = texture(u_motion_vector, vTexCoords).xy;
    vec2 history_uv = (vTexCoords - mv) - vec2(_36.u_jitter_x, _36.u_jitter_y);
    history_uv = clamp(history_uv, vec2(0.0), vec2(1.0));
    vec2 texel = vec2(1.0) / vec2(_36.u_screen_w, _36.u_screen_h);
    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            vec3 s = texture(screenTexture, vTexCoords + (vec2(float(dx), float(dy)) * texel)).xyz;
            m1 += s;
            m2 += (s * s);
        }
    }
    m1 /= vec3(9.0);
    vec3 sigma = sqrt(max((m2 / vec3(9.0)) - (m1 * m1), vec3(0.0)));
    vec3 aabb_min = m1 - (sigma * 1.25);
    vec3 aabb_max = m1 + (sigma * 1.25);
    vec3 history = texture(u_history, history_uv).xyz;
    history = clamp(history, aabb_min, aabb_max);
    float velocity_len = length(mv * vec2(_36.u_screen_w, _36.u_screen_h));
    float vel_weight = clamp(velocity_len * 0.5, 0.0, 0.5);
    float _165;
    if (_36.u_frame_index < 2)
    {
        _165 = 1.0;
    }
    else
    {
        _165 = clamp(_36.u_blend_factor + vel_weight, _36.u_blend_factor, 1.0);
    }
    float alpha = _165;
    FragColor = vec4(mix(history, current, vec3(alpha)), 1.0);
}

