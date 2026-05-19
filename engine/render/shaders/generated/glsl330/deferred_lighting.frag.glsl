#version 430

struct DeferredLightParams
{
    vec3 u_light_dir;
    float u_light_intensity;
    vec3 u_light_color;
    float u_ambient;
};

uniform DeferredLightParams _58;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D u_gbuf_normal;
layout(binding = 3) uniform sampler2D u_gbuf_position;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec3 albedo = texture(screenTexture, vTexCoords).xyz;
    vec3 normal = (texture(u_gbuf_normal, vTexCoords).xyz * 2.0) - vec3(1.0);
    vec3 position = texture(u_gbuf_position, vTexCoords).xyz;
    if (length(normal) < 0.00999999977648258209228515625)
    {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    normal = normalize(normal);
    float NdotL = max(dot(normal, -normalize(_58.u_light_dir)), 0.0);
    vec3 diffuse = ((albedo * _58.u_light_color) * _58.u_light_intensity) * NdotL;
    vec3 ambient_color = albedo * _58.u_ambient;
    FragColor = vec4(diffuse + ambient_color, 1.0);
}

