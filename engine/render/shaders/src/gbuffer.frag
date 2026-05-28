#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec3 vFragPos;
layout(location = 3) in vec3 vNormal;

// Set 0: PerFrame (VS 使用，此处保持 layout 兼容)
layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
    vec4 foliage_wind;
    vec4 foliage_push;
};

// Set 1: PerScene (占位，保持与 PBR 管线 descriptor layout 兼容)
layout(std140, set = 1, binding = 0) uniform PerScene {
    vec4 _gbuf_dummy;
};

// Set 2: Samplers
layout(set = 2, binding = 1) uniform sampler2D u_texture;

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gPosition;

void main() {
    vec4 albedo = texture(u_texture, vTexCoord) * vColor;
    if (albedo.a < 0.01) discard;
    gAlbedo   = albedo;
    gNormal   = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    gPosition = vec4(vFragPos, 1.0);
}
