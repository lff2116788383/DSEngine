#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec3 vFragPos;
layout(location = 3) in vec3 vNormal;
layout(location = 4) in mat3 vTBN;
layout(location = 7) in vec3 vFragPosViewSpace;

layout(location = 0) out vec4 FragColor;

// Set 0: PerFrame
layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
};

// Set 1: PerScene
layout(std140, set = 1, binding = 0) uniform PerScene {
    vec4 light_dir_and_enabled;
    vec4 light_color_and_ambient;
    vec4 light_params;
    vec4 cascade_splits;
    mat4 light_space_matrices[3];
};

// DSSL PerMaterial UBO (auto-generated)
layout(std140, set = 2, binding = 0) uniform PerMaterial {
    vec4 _mat_albedo_color;
};
layout(set = 2, binding = 1) uniform sampler2D albedo_tex;
#define albedo_color _mat_albedo_color

const float PI = 3.14159265359;

#define u_lighting_enabled    (light_dir_and_enabled.w != 0.0)
#define u_light_direction     light_dir_and_enabled.xyz
#define u_light_color         light_color_and_ambient.xyz
#define u_light_intensity     light_params.x
#define u_ambient_intensity   light_color_and_ambient.w
#define u_shadow_strength     light_params.y
#define u_receive_shadow      (light_params.z != 0.0)
#define u_cascade_splits      cascade_splits.xyz
#define u_camera_pos          camera_pos.xyz

// DSSL 内置变量 (可读输入)
#define UV          vTexCoord
#define UV2         vTexCoord
#define COLOR       vColor
#define VERTEX      vFragPos
#define NORMAL      vNormal
#define WORLD_POSITION vFragPos
#define WORLD_NORMAL   vNormal
#define VIEW_DIR    _dssl_view_dir
#define SCREEN_UV   _dssl_screen_uv
#define TIME        light_params.w

vec4 dssl_sample(sampler2D tex, vec2 uv) { return texture(tex, uv); }
vec4 dssl_sample_cube(samplerCube tex, vec3 dir) { return texture(tex, dir); }

void main() {
    vec3  ALBEDO = vec3(1.0);
    float ALPHA = 1.0;
    vec3  EMISSION = vec3(0.0);
    float ALPHA_SCISSOR = 0.5;

    vec3 _dssl_view_dir = normalize(u_camera_pos - vFragPos);
    vec2 _dssl_screen_uv = gl_FragCoord.xy / vec2(1280.0, 720.0);

    {
        
    }

    vec3 color = ALBEDO + EMISSION;
    FragColor = vec4(color, ALPHA);
}
