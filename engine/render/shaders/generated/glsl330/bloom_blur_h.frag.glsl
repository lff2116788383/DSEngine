#version 430

const float _69[5] = float[](0.227026998996734619140625, 0.19459460675716400146484375, 0.121621601283550262451171875, 0.054053999483585357666015625, 0.01621600054204463958740234375);

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec2 tex_offset = vec2(1.0) / vec2(textureSize(screenTexture, 0));
    vec3 result = texture(screenTexture, vTexCoords).xyz * 0.227026998996734619140625;
    for (int i = 1; i < 5; i++)
    {
        result += (texture(screenTexture, vTexCoords + vec2(tex_offset.x * float(i), 0.0)).xyz * _69[i]);
        result += (texture(screenTexture, vTexCoords - vec2(tex_offset.x * float(i), 0.0)).xyz * _69[i]);
    }
    FragColor = vec4(result, 1.0);
}

