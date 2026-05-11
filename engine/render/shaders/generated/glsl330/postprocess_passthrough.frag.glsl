#version 430

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 vTexCoords;

void main()
{
    FragColor = texture(screenTexture, vTexCoords);
}

