#version 450
#extension GL_ARB_separate_shader_objects : enable

// Eye rendering shader — multi-layer structure:
// Layer 1: Iris (with parallax offset for refraction depth)
// Layer 2: Cornea (clear coat GGX specular + environment reflection)
// Layer 3: Limbal darkening ring
// Layer 4: Sclera (standard diffuse)

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec3 vFragPos;
layout(location = 3) in vec3 vNormal;
layout(location = 4) in mat3 vTBN;
layout(location = 7) in vec3 vFragPosViewSpace;

layout(location = 0) out vec4 FragColor;

// PerFrame / PerScene UBOs (shared with pbr.frag)
layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
};

layout(std140, set = 1, binding = 0) uniform PerScene {
    vec4 light_dir_and_enabled;
    vec4 light_color_and_ambient;
    vec4 light_params;
};

layout(set = 2, binding = 0) uniform sampler2D u_iris_texture;
layout(set = 2, binding = 1) uniform sampler2D u_sclera_texture;
layout(set = 2, binding = 2) uniform sampler2D u_normal_map;
layout(set = 2, binding = 3) uniform samplerCube u_env_cubemap;

layout(push_constant) uniform EyeParams {
    float u_iris_depth;            // Parallax depth for iris refraction (default 0.3)
    float u_iris_radius;           // Normalized iris radius in UV space (default 0.3)
    float u_pupil_scale;           // Pupil dilation (0.1 - 0.8)
    float u_limbal_width;          // Limbal darkening ring width (default 0.05)
    float u_limbal_intensity;      // Limbal darkening intensity (0-1, default 0.6)
    float u_cornea_roughness;      // Cornea specular roughness (default 0.05)
    float u_cornea_ior;            // Index of refraction (default 1.376)
    float u_sclera_roughness;      // Sclera roughness (default 0.8)
    vec3 u_iris_tint;              // Iris color multiplier
    float _pad0;
    vec2 u_iris_center;            // Iris center in UV space (default 0.5, 0.5)
};

const float PI = 3.14159265359;

// GGX NDF
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 0.0001);
}

// Geometry Smith
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
    return ggx1 * ggx2;
}

// Fresnel Schlick
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Parallax offset for iris refraction
vec2 ParallaxIrisOffset(vec3 V_tangent, float depth) {
    // Simple single-layer parallax
    return V_tangent.xy / max(V_tangent.z, 0.01) * depth;
}

void main() {
    vec3 N = normalize(vNormal);
    if (textureSize(u_normal_map, 0).x > 1) {
        vec3 nm = texture(u_normal_map, vTexCoord).rgb * 2.0 - 1.0;
        N = normalize(vTBN * nm);
    }

    vec3 V = normalize(camera_pos.xyz - vFragPos);
    vec3 L = normalize(-light_dir_and_enabled.xyz);
    vec3 H = normalize(V + L);

    vec3 light_color = light_color_and_ambient.rgb;
    float light_intensity = light_params.x;
    float ambient = light_color_and_ambient.w;

    // Calculate distance from iris center in UV space
    vec2 uv_offset = vTexCoord - u_iris_center;
    float dist_from_center = length(uv_offset);

    // --- Iris with parallax refraction ---
    vec3 V_tangent = normalize(transpose(vTBN) * V);
    vec2 parallax_offset = ParallaxIrisOffset(V_tangent, u_iris_depth);

    // Pupil: darken center based on pupil_scale
    float pupil_radius = u_iris_radius * u_pupil_scale;
    vec2 iris_uv = vTexCoord + parallax_offset * step(dist_from_center, u_iris_radius);

    vec3 iris_color;
    if (dist_from_center < pupil_radius) {
        iris_color = vec3(0.01);  // Pupil is nearly black
    } else if (dist_from_center < u_iris_radius) {
        iris_color = texture(u_iris_texture, iris_uv).rgb * u_iris_tint;
    } else {
        iris_color = texture(u_sclera_texture, vTexCoord).rgb;
    }

    // --- Limbal darkening ---
    float limbal_dist = abs(dist_from_center - u_iris_radius);
    float limbal_factor = 1.0 - smoothstep(0.0, u_limbal_width, limbal_dist) * u_limbal_intensity;
    if (dist_from_center > u_iris_radius - u_limbal_width && dist_from_center < u_iris_radius + u_limbal_width) {
        iris_color *= limbal_factor;
    }

    // --- Sclera diffuse lighting ---
    float NdotL = max(dot(N, L), 0.0);
    float roughness = (dist_from_center < u_iris_radius) ? 0.9 : u_sclera_roughness;
    vec3 diffuse = iris_color * (NdotL * light_color * light_intensity + ambient * vec3(0.3));

    // --- Cornea specular (clear coat over entire eye) ---
    float F0_scalar = pow((u_cornea_ior - 1.0) / (u_cornea_ior + 1.0), 2.0);
    vec3 F0 = vec3(F0_scalar);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = DistributionGGX(N, H, u_cornea_roughness);
    float G = GeometrySmith(N, V, L, u_cornea_roughness);
    vec3 cornea_spec = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001);
    cornea_spec *= light_color * light_intensity * NdotL;

    // --- Environment reflection on cornea ---
    vec3 R = reflect(-V, N);
    vec3 env_reflection = texture(u_env_cubemap, R).rgb * F * 0.3;

    // --- Combine ---
    vec3 final_color = diffuse + cornea_spec + env_reflection;

    FragColor = vec4(final_color, 1.0);
}
