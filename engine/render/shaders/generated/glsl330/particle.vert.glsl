#version 430

layout(binding = 0, std140) uniform PerFrame
{
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
} _14;

layout(location = 2) in vec3 iPos;
layout(location = 0) in vec3 aPos;
layout(location = 4) in float iSize;
layout(location = 0) out vec4 vParticleColor;
layout(location = 3) in vec4 iColor;
layout(location = 1) out vec2 vTexCoord;
layout(location = 1) in vec2 aTexCoord;

void main()
{
    vec3 camera_right = vec3(_14.view[0].x, _14.view[1].x, _14.view[2].x);
    vec3 camera_up = vec3(_14.view[0].y, _14.view[1].y, _14.view[2].y);
    vec3 vertexPosition_worldspace = (iPos + ((camera_right * aPos.x) * iSize)) + ((camera_up * aPos.y) * iSize);
    gl_Position = _14.vp * vec4(vertexPosition_worldspace, 1.0);
    vParticleColor = iColor;
    vTexCoord = aTexCoord;
}

