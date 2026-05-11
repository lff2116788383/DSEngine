#version 430

layout(binding = 1) uniform samplerCube skybox;

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec3 vTexCoords;

void main()
{
    FragColor = texture(skybox, vTexCoords);
}

