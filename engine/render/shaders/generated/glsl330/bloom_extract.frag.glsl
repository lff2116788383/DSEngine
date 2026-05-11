#version 430

layout(binding = 2, std140) uniform BloomParams
{
    float threshold;
} _33;

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec3 color = texture(screenTexture, vTexCoords).xyz;
    float brightness = dot(color, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    if (brightness > _33.threshold)
    {
        FragColor = vec4(color, 1.0);
    }
    else
    {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}

