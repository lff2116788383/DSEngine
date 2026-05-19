#version 430

layout(binding = 0, std140) uniform PerFrame
{
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
} _63;

layout(binding = 0, std140) uniform PerScene
{
    vec4 _gbuf_dummy;
} _66;

layout(binding = 1) uniform sampler2D u_texture;

layout(location = 1) in vec2 vTexCoord;
layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec4 gNormal;
layout(location = 3) in vec3 vNormal;
layout(location = 2) out vec4 gPosition;
layout(location = 2) in vec3 vFragPos;

void main()
{
    vec4 albedo = texture(u_texture, vTexCoord) * vColor;
    if (albedo.w < 0.00999999977648258209228515625)
    {
        discard;
    }
    gAlbedo = albedo;
    gNormal = vec4((normalize(vNormal) * 0.5) + vec3(0.5), 1.0);
    gPosition = vec4(vFragPos, 1.0);
}

