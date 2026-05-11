#version 430

uniform mat4 u_model;
uniform mat4 u_vp;
layout(location = 0) in vec2 aPos;
layout(location = 0) out vec4 vColor;
layout(location = 2) in vec4 aColor;
layout(location = 1) out vec2 vTexCoord;
layout(location = 1) in vec2 aTexCoord;

void main()
{
    gl_Position = (u_vp * u_model) * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
    vTexCoord = aTexCoord;
}

