#version 430

struct SsrParams
{
    float max_distance;
    float thickness;
    float step_size;
    int max_steps;
    float near_plane;
    float far_plane;
    float screen_w;
    float screen_h;
};

uniform SsrParams _28;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D u_color_texture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

float linearizeDepth(float d)
{
    float z = (d * 2.0) - 1.0;
    return ((2.0 * _28.near_plane) * _28.far_plane) / ((_28.far_plane + _28.near_plane) - (z * (_28.far_plane - _28.near_plane)));
}

vec3 reconstructNormal(vec2 uv)
{
    vec2 texel = vec2(1.0) / vec2(_28.screen_w, _28.screen_h);
    float param = texture(screenTexture, uv).x;
    float dc = linearizeDepth(param);
    float param_1 = texture(screenTexture, uv - vec2(texel.x, 0.0)).x;
    float dl = linearizeDepth(param_1);
    float param_2 = texture(screenTexture, uv + vec2(texel.x, 0.0)).x;
    float dr = linearizeDepth(param_2);
    float param_3 = texture(screenTexture, uv - vec2(0.0, texel.y)).x;
    float db = linearizeDepth(param_3);
    float param_4 = texture(screenTexture, uv + vec2(0.0, texel.y)).x;
    float dt = linearizeDepth(param_4);
    return normalize(vec3(dl - dr, db - dt, (2.0 * texel.x) * dc));
}

void main()
{
    float depth = texture(screenTexture, vTexCoords).x;
    if (depth >= 1.0)
    {
        FragColor = vec4(0.0);
        return;
    }
    float param = depth;
    float lin_depth = linearizeDepth(param);
    vec2 param_1 = vTexCoords;
    vec3 normal = reconstructNormal(param_1);
    vec3 view_dir = vec3((vTexCoords * 2.0) - vec2(1.0), 1.0);
    view_dir = normalize(view_dir);
    vec3 reflect_dir = reflect(view_dir, normal);
    vec2 texel = vec2(1.0) / vec2(_28.screen_w, _28.screen_h);
    vec2 ray_uv = vTexCoords;
    float ray_depth = lin_depth;
    for (int i = 0; i < _28.max_steps; i++)
    {
        ray_uv += ((reflect_dir.xy * texel) * _28.step_size);
        bool _216 = ray_uv.x < 0.0;
        bool _223;
        if (!_216)
        {
            _223 = ray_uv.x > 1.0;
        }
        else
        {
            _223 = _216;
        }
        bool _230;
        if (!_223)
        {
            _230 = ray_uv.y < 0.0;
        }
        else
        {
            _230 = _223;
        }
        bool _237;
        if (!_230)
        {
            _237 = ray_uv.y > 1.0;
        }
        else
        {
            _237 = _230;
        }
        if (_237)
        {
            break;
        }
        float param_2 = textureLod(screenTexture, ray_uv, 0.0).x;
        float sample_depth = linearizeDepth(param_2);
        ray_depth += (reflect_dir.z * _28.step_size);
        float depth_diff = ray_depth - sample_depth;
        bool _261 = depth_diff > 0.0;
        bool _269;
        if (_261)
        {
            _269 = depth_diff < _28.thickness;
        }
        else
        {
            _269 = _261;
        }
        if (_269)
        {
            float fade = 1.0 - (float(i) / float(_28.max_steps));
            vec3 hit_color = textureLod(u_color_texture, ray_uv, 0.0).xyz;
            FragColor = vec4(hit_color * fade, fade);
            return;
        }
    }
    FragColor = vec4(0.0);
}

