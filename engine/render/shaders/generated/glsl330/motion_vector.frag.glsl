#version 430

struct MvParams
{
    float screen_w;
    float screen_h;
    mat4 reproj;
};

uniform MvParams _46;

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    float depth = texture(screenTexture, vTexCoords).x;
    vec2 ndc = (vTexCoords * 2.0) - vec2(1.0);
    float z_ndc = (depth * 2.0) - 1.0;
    vec4 clip_pos = vec4(ndc, z_ndc, 1.0);
    vec4 prev_clip = _46.reproj * clip_pos;
    float _56 = prev_clip.w;
    vec4 _57 = prev_clip;
    vec2 _60 = _57.xy / vec2(_56);
    prev_clip.x = _60.x;
    prev_clip.y = _60.y;
    vec2 prev_uv = (prev_clip.xy * 0.5) + vec2(0.5);
    vec2 velocity = vTexCoords - prev_uv;
    FragColor = vec4(velocity, 0.0, 1.0);
}

