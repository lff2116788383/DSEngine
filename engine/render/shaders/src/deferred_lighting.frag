#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_gbuf_normal;
layout(set = 2, binding = 3) uniform sampler2D u_gbuf_position;

layout(push_constant) uniform DeferredLightParams {
    vec3 u_light_dir;
    float u_light_intensity;
    vec3 u_light_color;
    float u_ambient;
};

void main() {
    vec3 albedo   = texture(screenTexture, vTexCoords).rgb;
    vec3 normal   = texture(u_gbuf_normal, vTexCoords).rgb * 2.0 - 1.0;
    vec3 position = texture(u_gbuf_position, vTexCoords).rgb;
    if (length(normal) < 0.01) { FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    normal = normalize(normal);
    float NdotL = max(dot(normal, -normalize(u_light_dir)), 0.0);
    vec3 diffuse = albedo * u_light_color * u_light_intensity * NdotL;
    vec3 ambient_color = albedo * u_ambient;
    FragColor = vec4(diffuse + ambient_color, 1.0);
}
