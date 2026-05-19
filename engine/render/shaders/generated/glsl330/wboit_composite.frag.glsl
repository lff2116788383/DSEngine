#version 430

struct WboitParams
{
    float u_reveal_handle;
};

uniform WboitParams _61;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D u_reveal_tex;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec4 accum = texture(screenTexture, vTexCoords);
    float revealage = texture(u_reveal_tex, vTexCoords).x;
    if (accum.w < 9.9999997473787516355514526367188e-05)
    {
        discard;
    }
    vec3 avg_color = accum.xyz / vec3(max(accum.w, 9.9999997473787516355514526367188e-06));
    FragColor = vec4(avg_color, 1.0 - revealage);
}

