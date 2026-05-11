#version 430

uniform mat4 u_vp;
layout(location = 0) out vec3 vTexCoords;
layout(location = 0) in vec3 aPos;

void main()
{
    vTexCoords = aPos;
    vec4 pos = u_vp * vec4(aPos * 10000.0, 1.0);
    gl_Position = pos.xyww;
}

