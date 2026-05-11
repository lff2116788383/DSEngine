#version 430

layout(binding = 1) uniform sampler2D u_texture;

layout(location = 1) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec4 vColor;

void main()
{
    vec4 texColor = texture(u_texture, vTexCoord);
    FragColor = texColor * vColor;
}

