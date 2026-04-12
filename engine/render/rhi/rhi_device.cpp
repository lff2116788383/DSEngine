/**
 * @file rhi_device.cpp
 * @brief 渲染硬件接口(RHI)抽象层，提供跨图形API的底层渲染命令封装
 */

#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include "engine/platform/screen.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstddef>
#include <algorithm>
#include <functional>
#include <string>

constexpr size_t MAX_SPRITES = 10000;
constexpr size_t MAX_VERTICES = MAX_SPRITES * 4;
constexpr size_t MAX_INDICES = MAX_SPRITES * 6;

struct DrawPostProcessCmd {
    unsigned int source_texture;
    std::string effect_name;
    std::vector<float> params;
};


namespace {
unsigned int CompileShaderProgram(const char* vertex_shader_source, const char* fragment_shader_source) {
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(vertex_shader);
    int vertex_compiled = 0;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_compiled);
    if (vertex_compiled == GL_FALSE) {
        char log_buffer[1024];
        glGetShaderInfoLog(vertex_shader, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL vertex shader compile failed: {}", log_buffer);
    }

    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    glCompileShader(fragment_shader);
    int fragment_compiled = 0;
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_compiled);
    if (fragment_compiled == GL_FALSE) {
        char log_buffer[1024];
        glGetShaderInfoLog(fragment_shader, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL fragment shader compile failed: {}", log_buffer);
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        char log_buffer[1024];
        glGetProgramInfoLog(program, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL shader link failed: {}", log_buffer);
    }
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}
}

unsigned int OpenGLRhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    unsigned int handle = 0;
    glGenBuffers(1, &handle);
    resource_ledger_.buffers_created += 1;
    unsigned int target = is_index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, handle);
    glBufferData(target, size, data, is_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    return handle;
}

void OpenGLRhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    unsigned int target = is_index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, handle);
    glBufferSubData(target, offset, size, data);
}

void OpenGLRhiDevice::DeleteBuffer(unsigned int handle) {
    glDeleteBuffers(1, &handle);
    resource_ledger_.buffers_destroyed += 1;
}

unsigned int OpenGLRhiDevice::CreateVertexArray() {
    unsigned int handle = 0;
    glGenVertexArrays(1, &handle);
    resource_ledger_.vertex_arrays_created += 1;
    return handle;
}

void OpenGLRhiDevice::DeleteVertexArray(unsigned int handle) {
    glDeleteVertexArrays(1, &handle);
    resource_ledger_.vertex_arrays_destroyed += 1;
}

void OpenGLRhiDevice::EnsureInitialized() {
    if (initialized_) {
        return;
    }
    // ... vertex shader ...
    const char* vertex_shader = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec4 aColor;
        layout (location = 2) in vec2 aTexCoord;
        layout (location = 3) in vec3 aNormal;
        layout (location = 4) in vec3 aTangent;
        layout (location = 5) in vec4 aBoneWeights;
        layout (location = 6) in vec4 aBoneIndices;

        out vec4 ourColor;
        out vec2 TexCoord;
        out vec3 FragPos;
        out vec3 Normal;
        out mat3 TBN;
        out vec3 FragPosViewSpace;

        uniform mat4 u_model;
        uniform mat4 u_vp;
        uniform mat4 u_view;
        uniform bool u_skinned;
        const int MAX_BONES = 100;
        uniform mat4 u_bone_matrices[MAX_BONES];

        uniform bool u_morph_enabled;
        const int MAX_MORPH_TARGETS = 4;
        uniform float u_morph_weights[MAX_MORPH_TARGETS];

        void main() {
            mat4 boneTransform = mat4(1.0);
            if (u_skinned) {
                boneTransform = u_bone_matrices[int(aBoneIndices[0])] * aBoneWeights[0] +
                                u_bone_matrices[int(aBoneIndices[1])] * aBoneWeights[1] +
                                u_bone_matrices[int(aBoneIndices[2])] * aBoneWeights[2] +
                                u_bone_matrices[int(aBoneIndices[3])] * aBoneWeights[3];
            }
            
            vec3 morphedPos = aPos;
            vec3 morphedNormal = aNormal;
            if (u_morph_enabled) {
                morphedPos += vec3(0.01) * u_morph_weights[0]; 
            }
            
            vec4 localPos = boneTransform * vec4(morphedPos, 1.0);
            vec4 worldPos = u_model * localPos;
            gl_Position = u_vp * worldPos;
            
            FragPos = worldPos.xyz;
            FragPosViewSpace = (u_view * worldPos).xyz;
            ourColor = aColor;
            TexCoord = aTexCoord;
            
            mat3 normalMatrix = transpose(inverse(mat3(u_model * boneTransform)));
            vec3 T = normalize(normalMatrix * aTangent);
            vec3 N = normalize(normalMatrix * morphedNormal);
            T = normalize(T - dot(T, N) * N);
            vec3 B = cross(N, T);
            TBN = mat3(T, B, N);
            Normal = N;
        }
    )";

    const char* fragment_shader = R"(
        #version 330 core
        out vec4 FragColor;

        in vec4 ourColor;
        in vec2 TexCoord;
        in vec3 FragPos;
        in vec3 Normal;
        in mat3 TBN;
        in vec3 FragPosViewSpace;

        uniform sampler2D u_texture;
        uniform sampler2D u_normal_map;
        uniform bool u_has_normal_map;
        
        #define CSM_CASCADES 3
        uniform sampler2D u_shadow_maps[CSM_CASCADES];
        uniform mat4 u_light_space_matrices[CSM_CASCADES];
        uniform float u_cascade_splits[CSM_CASCADES];
        uniform sampler2D u_spot_shadow_maps[4];
        uniform mat4 u_spot_light_space_matrices[4];

        uniform vec3 u_camera_pos;
        uniform bool u_lighting_enabled;
        uniform vec3 u_light_direction;
        uniform vec3 u_light_color;
        uniform float u_light_intensity;
        uniform float u_ambient_intensity;
        uniform float u_shadow_strength;
        uniform bool u_receive_shadow;

        struct PointLight {
            vec3 color;
            vec3 position;
            float intensity;
            float radius;
        };
        #define MAX_POINT_LIGHTS 4
        uniform int u_point_light_count;
        uniform PointLight u_point_lights[MAX_POINT_LIGHTS];

        struct SpotLight {
            vec3 color;
            vec3 position;
            vec3 direction;
            float intensity;
            float radius;
            float inner_cone;
            float outer_cone;
            bool cast_shadow;
            int shadow_index;
        };
        #define MAX_SPOT_LIGHTS 4
        uniform int u_spot_light_count;
        uniform SpotLight u_spot_lights[MAX_SPOT_LIGHTS];

        uniform vec3 u_material_albedo;
        uniform float u_material_metallic;
        uniform float u_material_roughness;
        uniform float u_material_ao;
        uniform vec3 u_material_emissive;
        uniform float u_material_normal_strength;
        uniform float u_material_alpha_cutoff;

        const float PI = 3.14159265359;

        float DistributionGGX(vec3 N, vec3 H, float roughness) {
            float a = roughness*roughness;
            float a2 = a*a;
            float NdotH = max(dot(N, H), 0.0);
            float NdotH2 = NdotH*NdotH;
            float nom   = a2;
            float denom = (NdotH2 * (a2 - 1.0) + 1.0);
            denom = PI * denom * denom;
            return nom / max(denom, 0.0000001);
        }

        float GeometrySchlickGGX(float NdotV, float roughness) {
            float r = (roughness + 1.0);
            float k = (r*r) / 8.0;
            float nom   = NdotV;
            float denom = NdotV * (1.0 - k) + k;
            return nom / denom;
        }

        float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
            float NdotV = max(dot(N, V), 0.0);
            float NdotL = max(dot(N, L), 0.0);
            float ggx2 = GeometrySchlickGGX(NdotV, roughness);
            float ggx1 = GeometrySchlickGGX(NdotL, roughness);
            return ggx1 * ggx2;
        }

        vec3 fresnelSchlick(float cosTheta, vec3 F0) {
            return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
        }

        float ShadowCalculation(vec3 fragPosWorldSpace, vec3 fragPosViewSpace, vec3 normal, vec3 lightDir) {
            if (!u_receive_shadow) return 0.0;
            
            int cascadeIndex = CSM_CASCADES - 1;
            for (int i = 0; i < CSM_CASCADES - 1; ++i) {
                if (abs(fragPosViewSpace.z) < u_cascade_splits[i]) {
                    cascadeIndex = i;
                    break;
                }
            }
            
            vec4 fragPosLightSpace = u_light_space_matrices[cascadeIndex] * vec4(fragPosWorldSpace, 1.0);
            vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
            projCoords = projCoords * 0.5 + 0.5;
            
            if(projCoords.z > 1.0) return 0.0;
            if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;

            float currentDepth = projCoords.z;
            float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
            
            float shadow = 0.0;
            vec2 texelSize = 1.0 / vec2(textureSize(u_shadow_maps[cascadeIndex], 0));
            for (int x = -1; x <= 1; ++x) {
                for (int y = -1; y <= 1; ++y) {
                    float pcfDepth = texture(u_shadow_maps[cascadeIndex], projCoords.xy + vec2(x, y) * texelSize).r;
                    shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
                }
            }
            shadow /= 9.0;
            return clamp(shadow * u_shadow_strength, 0.0, 1.0);
        }

        float SpotShadowCalculation(int shadowIndex, vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir) {
            if (shadowIndex < 0 || shadowIndex >= 4) return 0.0;
            vec4 fragPosLightSpace = u_spot_light_space_matrices[shadowIndex] * vec4(fragPosWorldSpace, 1.0);
            vec3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 0.0001);
            projCoords = projCoords * 0.5 + 0.5;
            if (projCoords.z > 1.0) return 0.0;
            if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;
            float currentDepth = projCoords.z;
            float bias = max(0.003 * (1.0 - dot(normal, lightDir)), 0.0005);
            float shadow = 0.0;
            vec2 texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[shadowIndex], 0));
            for (int x = -1; x <= 1; ++x) {
                for (int y = -1; y <= 1; ++y) {
                    float pcfDepth = texture(u_spot_shadow_maps[shadowIndex], projCoords.xy + vec2(x, y) * texelSize).r;
                    shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
                }
            }
            shadow /= 9.0;
            return clamp(shadow * u_shadow_strength, 0.0, 1.0);
        }

        void main() {
            vec4 texColor = texture(u_texture, TexCoord);
            if (texColor.a < clamp(u_material_alpha_cutoff, 0.0, 1.0)) discard;

            vec3 N = Normal;
            if (u_has_normal_map) {
                vec3 normalMap = texture(u_normal_map, TexCoord).rgb;
                normalMap = normalMap * 2.0 - 1.0;
                normalMap.xy *= u_material_normal_strength;
                N = normalize(TBN * normalMap);
            }

            if (!u_lighting_enabled) {
                vec3 result = texColor.rgb * ourColor.rgb * u_material_albedo;
                // Tonemapping & Gamma correction
                result = result / (result + vec3(1.0));
                result = pow(result, vec3(1.0/2.2));
                FragColor = vec4(result, texColor.a * ourColor.a);
                return;
            }

            vec3 albedo = pow(texColor.rgb * ourColor.rgb * u_material_albedo, vec3(2.2));
            vec3 V = normalize(u_camera_pos - FragPos);
            vec3 F0 = vec3(0.04);
            F0 = mix(F0, albedo, u_material_metallic);

            vec3 Lo = vec3(0.0);
            
            // Directional Light
            {
                vec3 L = normalize(-u_light_direction);
                vec3 H = normalize(V + L);
                float NDF = DistributionGGX(N, H, u_material_roughness);
                float G   = GeometrySmith(N, V, L, u_material_roughness);
                vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 numerator    = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
                vec3 specular     = numerator / denominator;

                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - u_material_metallic;

                float NdotL = max(dot(N, L), 0.0);
                float shadow = ShadowCalculation(FragPos, FragPosViewSpace, N, L);
                Lo += (kD * albedo / PI + specular) * u_light_color * u_light_intensity * NdotL * (1.0 - shadow);
            }
            
            // Point Lights
            for(int i = 0; i < u_point_light_count; ++i) {
                vec3 L = normalize(u_point_lights[i].position - FragPos);
                vec3 H = normalize(V + L);
                float distance = length(u_point_lights[i].position - FragPos);
                float attenuation = clamp(1.0 - (distance*distance)/(u_point_lights[i].radius*u_point_lights[i].radius), 0.0, 1.0);
                attenuation *= attenuation;
                vec3 radiance = u_point_lights[i].color * u_point_lights[i].intensity * attenuation;

                float NDF = DistributionGGX(N, H, u_material_roughness);
                float G   = GeometrySmith(N, V, L, u_material_roughness);
                vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 numerator    = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
                vec3 specular     = numerator / denominator;

                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - u_material_metallic;

                float NdotL = max(dot(N, L), 0.0);
                Lo += (kD * albedo / PI + specular) * radiance * NdotL;
            }

            // Spot Lights
            for(int i = 0; i < u_spot_light_count; ++i) {
                vec3 L = normalize(u_spot_lights[i].position - FragPos);
                vec3 H = normalize(V + L);
                float distance = length(u_spot_lights[i].position - FragPos);
                float attenuation = clamp(1.0 - (distance * distance) / (u_spot_lights[i].radius * u_spot_lights[i].radius), 0.0, 1.0);
                attenuation *= attenuation;

                vec3 spotDir = normalize(-u_spot_lights[i].direction);
                float theta = dot(L, spotDir);
                float outerCos = cos(radians(u_spot_lights[i].outer_cone));
                float innerCos = cos(radians(u_spot_lights[i].inner_cone));
                float epsilon = max(innerCos - outerCos, 0.0001);
                float cone = clamp((theta - outerCos) / epsilon, 0.0, 1.0);
                if (cone <= 0.0) {
                    continue;
                }

                vec3 radiance = u_spot_lights[i].color * u_spot_lights[i].intensity * attenuation * cone;
                float NDF = DistributionGGX(N, H, u_material_roughness);
                float G   = GeometrySmith(N, V, L, u_material_roughness);
                vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 numerator    = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
                vec3 specular     = numerator / denominator;

                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - u_material_metallic;

                float NdotL = max(dot(N, L), 0.0);
                float spot_shadow = 0.0;
                if (u_spot_lights[i].cast_shadow) {
                    spot_shadow = SpotShadowCalculation(u_spot_lights[i].shadow_index, FragPos, N, L);
                }
                Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - spot_shadow);
            }
            
            // Ambient lighting (Placeholder for IBL Irradiance/Specular)
            // To prevent pure metals from being completely black when not directly lit,
            // we use a simple ambient approximation until true IBL cubemap sampling is integrated.
            vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);
            vec3 kS_ambient = F;
            vec3 kD_ambient = 1.0 - kS_ambient;
            kD_ambient *= 1.0 - u_material_metallic;
            
            // Simple constant ambient light for now
            vec3 irradiance = vec3(u_ambient_intensity);
            vec3 diffuse_ambient = irradiance * albedo;
            // Fake specular ambient based on roughness
            vec3 specular_ambient = irradiance * F0 * (1.0 - u_material_roughness);
            
            vec3 ambient = (kD_ambient * diffuse_ambient + specular_ambient) * u_material_ao;
            vec3 color = ambient + Lo + u_material_emissive;

            color = color / (color + vec3(1.0));
            color = pow(color, vec3(1.0/2.2));

            FragColor = vec4(color, texColor.a * ourColor.a);
        }
    )";
    shader_handle_ = CompileShaderProgram(vertex_shader, fragment_shader);
    resource_ledger_.shader_programs_created += 1;
    uniform_texture_loc_ = glGetUniformLocation(shader_handle_, "u_texture");
    uniform_tint_loc_ = glGetUniformLocation(shader_handle_, "u_tint");
    uniform_vp_loc_ = glGetUniformLocation(shader_handle_, "u_vp");
    uniform_cascade_splits_loc_ = glGetUniformLocation(shader_handle_, "u_cascade_splits");
    for (int i = 0; i < 3; ++i) {
        std::string ls_name = "u_light_space_matrices[" + std::to_string(i) + "]";
        uniform_light_space_matrix_loc_[i] = glGetUniformLocation(shader_handle_, ls_name.c_str());
        std::string sm_name = "u_shadow_maps[" + std::to_string(i) + "]";
        uniform_shadow_map_loc_[i] = glGetUniformLocation(shader_handle_, sm_name.c_str());
    }
    
    uniform_normal_map_loc_ = glGetUniformLocation(shader_handle_, "u_normal_map");
    uniform_has_normal_map_loc_ = glGetUniformLocation(shader_handle_, "u_has_normal_map");
    uniform_camera_pos_loc_ = glGetUniformLocation(shader_handle_, "u_camera_pos");
    uniform_lighting_enabled_loc_ = glGetUniformLocation(shader_handle_, "u_lighting_enabled");
    uniform_light_direction_loc_ = glGetUniformLocation(shader_handle_, "u_light_direction");
    uniform_light_color_loc_ = glGetUniformLocation(shader_handle_, "u_light_color");
    uniform_light_intensity_loc_ = glGetUniformLocation(shader_handle_, "u_light_intensity");
    uniform_ambient_intensity_loc_ = glGetUniformLocation(shader_handle_, "u_ambient_intensity");
    uniform_shadow_strength_loc_ = glGetUniformLocation(shader_handle_, "u_shadow_strength");
    uniform_material_albedo_loc_ = glGetUniformLocation(shader_handle_, "u_material_albedo");
    uniform_material_metallic_loc_ = glGetUniformLocation(shader_handle_, "u_material_metallic");
    uniform_material_roughness_loc_ = glGetUniformLocation(shader_handle_, "u_material_roughness");
    uniform_material_ao_loc_ = glGetUniformLocation(shader_handle_, "u_material_ao");
    uniform_material_emissive_loc_ = glGetUniformLocation(shader_handle_, "u_material_emissive");
    uniform_material_normal_strength_loc_ = glGetUniformLocation(shader_handle_, "u_material_normal_strength");
    uniform_material_alpha_cutoff_loc_ = glGetUniformLocation(shader_handle_, "u_material_alpha_cutoff");
    uniform_receive_shadow_loc_ = glGetUniformLocation(shader_handle_, "u_receive_shadow");
    uniform_skinned_loc_ = glGetUniformLocation(shader_handle_, "u_skinned");
    uniform_bone_matrices_loc_ = glGetUniformLocation(shader_handle_, "u_bone_matrices");

    uniform_morph_enabled_loc_ = glGetUniformLocation(shader_handle_, "u_morph_enabled");
    uniform_morph_weights_loc_ = glGetUniformLocation(shader_handle_, "u_morph_weights");

    uniform_point_light_count_loc_ = glGetUniformLocation(shader_handle_, "u_point_light_count");
    for (int i = 0; i < 4; ++i) {
        std::string base = "u_point_lights[" + std::to_string(i) + "].";
        uniform_point_lights_loc_[i].color = glGetUniformLocation(shader_handle_, (base + "color").c_str());
        uniform_point_lights_loc_[i].position = glGetUniformLocation(shader_handle_, (base + "position").c_str());
        uniform_point_lights_loc_[i].intensity = glGetUniformLocation(shader_handle_, (base + "intensity").c_str());
        uniform_point_lights_loc_[i].radius = glGetUniformLocation(shader_handle_, (base + "radius").c_str());
    }

    uniform_spot_light_count_loc_ = glGetUniformLocation(shader_handle_, "u_spot_light_count");
    for (int i = 0; i < 4; ++i) {
        std::string shadow_name = "u_spot_shadow_maps[" + std::to_string(i) + "]";
        uniform_spot_shadow_map_loc_[i] = glGetUniformLocation(shader_handle_, shadow_name.c_str());
        std::string matrix_name = "u_spot_light_space_matrices[" + std::to_string(i) + "]";
        uniform_spot_light_space_matrix_loc_[i] = glGetUniformLocation(shader_handle_, matrix_name.c_str());
        std::string base = "u_spot_lights[" + std::to_string(i) + "].";
        uniform_spot_lights_loc_[i].color = glGetUniformLocation(shader_handle_, (base + "color").c_str());
        uniform_spot_lights_loc_[i].position = glGetUniformLocation(shader_handle_, (base + "position").c_str());
        uniform_spot_lights_loc_[i].direction = glGetUniformLocation(shader_handle_, (base + "direction").c_str());
        uniform_spot_lights_loc_[i].intensity = glGetUniformLocation(shader_handle_, (base + "intensity").c_str());
        uniform_spot_lights_loc_[i].radius = glGetUniformLocation(shader_handle_, (base + "radius").c_str());
        uniform_spot_lights_loc_[i].inner_cone = glGetUniformLocation(shader_handle_, (base + "inner_cone").c_str());
        uniform_spot_lights_loc_[i].outer_cone = glGetUniformLocation(shader_handle_, (base + "outer_cone").c_str());
        uniform_spot_lights_loc_[i].cast_shadow = glGetUniformLocation(shader_handle_, (base + "cast_shadow").c_str());
        uniform_spot_lights_loc_[i].shadow_index = glGetUniformLocation(shader_handle_, (base + "shadow_index").c_str());
    }

    std::vector<BatchVertex> vertices(MAX_VERTICES);
    std::vector<unsigned short> indices(MAX_INDICES);
    
    for (size_t i = 0, j = 0; i < MAX_SPRITES; ++i) {
        indices[j++] = i * 4 + 0;
        indices[j++] = i * 4 + 1;
        indices[j++] = i * 4 + 2;
        indices[j++] = i * 4 + 2;
        indices[j++] = i * 4 + 3;
        indices[j++] = i * 4 + 0;
    }

    vao_handle_ = CreateVertexArray();
    glBindVertexArray(vao_handle_);
    vbo_handle_ = CreateBuffer(vertices.size() * sizeof(BatchVertex), vertices.data(), true, false);
    ebo_handle_ = CreateBuffer(indices.size() * sizeof(unsigned short), indices.data(), false, true);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_handle_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, uv)));
    glBindVertexArray(0);

    mesh_vbo_handle_ = CreateBuffer(MAX_VERTICES * sizeof(BatchVertex), nullptr, true, false);
    mesh_ibo_handle_ = CreateBuffer(MAX_INDICES * sizeof(unsigned short), nullptr, true, true);
    mesh_vao_handle_ = CreateVertexArray();
    glBindVertexArray(mesh_vao_handle_);
    glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo_handle_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ibo_handle_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, uv)));
    glBindVertexArray(0);

    unsigned char white_texture[] = {255, 255, 255, 255};
    glGenTextures(1, &white_texture_handle_);
    resource_ledger_.textures_created += 1;
    glBindTexture(GL_TEXTURE_2D, white_texture_handle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_texture);
    glBindTexture(GL_TEXTURE_2D, 0);
    initialized_ = true;
}

void OpenGLRhiDevice::Shutdown() {
    for (auto& [handle, target] : render_targets_) {
        (void)handle;
        if (target.fbo_handle != 0) {
            glDeleteFramebuffers(1, &target.fbo_handle);
            resource_ledger_.framebuffers_destroyed += 1;
            target.fbo_handle = 0;
        }
        if (target.color_texture_handle != 0) {
            glDeleteTextures(1, &target.color_texture_handle);
            resource_ledger_.textures_destroyed += 1;
            target.color_texture_handle = 0;
        }
        if (target.depth_texture_handle != 0) {
            glDeleteTextures(1, &target.depth_texture_handle);
            resource_ledger_.textures_destroyed += 1;
            target.depth_texture_handle = 0;
        }
        resource_ledger_.render_targets_destroyed += 1;
    }
    render_targets_.clear();
    resource_ledger_.pipeline_states_destroyed += pipeline_states_.size();
    pipeline_states_.clear();
    active_pipeline_state_ = 0;
    active_render_target_ = 0;

    if (white_texture_handle_ != 0) {
        glDeleteTextures(1, &white_texture_handle_);
        resource_ledger_.textures_destroyed += 1;
        white_texture_handle_ = 0;
    }
    if (mesh_vbo_handle_ != 0) {
        DeleteBuffer(mesh_vbo_handle_);
        mesh_vbo_handle_ = 0;
    }
    if (mesh_ibo_handle_ != 0) {
        DeleteBuffer(mesh_ibo_handle_);
        mesh_ibo_handle_ = 0;
    }
    if (mesh_vao_handle_ != 0) {
        DeleteVertexArray(mesh_vao_handle_);
        mesh_vao_handle_ = 0;
    }
    if (vbo_handle_ != 0) {
        DeleteBuffer(vbo_handle_);
        vbo_handle_ = 0;
    }
    if (ebo_handle_ != 0) {
        DeleteBuffer(ebo_handle_);
        ebo_handle_ = 0;
    }
    if (vao_handle_ != 0) {
        DeleteVertexArray(vao_handle_);
        vao_handle_ = 0;
    }
    if (shader_handle_ != 0) {
        glDeleteProgram(shader_handle_);
        resource_ledger_.shader_programs_destroyed += 1;
        shader_handle_ = 0;
    }
    if (skybox_vbo_handle_ != 0) {
        DeleteBuffer(skybox_vbo_handle_);
        skybox_vbo_handle_ = 0;
    }
    if (skybox_vao_handle_ != 0) {
        DeleteVertexArray(skybox_vao_handle_);
        skybox_vao_handle_ = 0;
    }
    if (skybox_shader_handle_ != 0) {
        DeleteShaderProgram(skybox_shader_handle_);
        skybox_shader_handle_ = 0;
    }
    skybox_view_loc_ = -1;
    skybox_proj_loc_ = -1;
    skybox_tex_loc_ = -1;
    uniform_texture_loc_ = -1;
    uniform_tint_loc_ = -1;
    uniform_vp_loc_ = -1;
    uniform_light_intensity_loc_ = -1;
    uniform_ambient_intensity_loc_ = -1;
    uniform_shadow_strength_loc_ = -1;
    LogResourceLedger();
    initialized_ = false;
}

void OpenGLCommandBuffer::SetCamera(const glm::mat4& view, const glm::mat4& projection) {
    view_ = view;
    projection_ = projection;
}

void OpenGLCommandBuffer::BeginRenderPass(const RenderPassDesc& render_pass) {
    begin_render_pass_cmds_.push_back({next_cmd_order_++, render_pass});
}

void OpenGLCommandBuffer::EndRenderPass() {
    end_render_pass_cmds_.push_back({next_cmd_order_++});
}

void OpenGLCommandBuffer::SetPipelineState(unsigned int pipeline_state_handle) {
    set_pipeline_state_cmds_.push_back({next_cmd_order_++, pipeline_state_handle});
}

void OpenGLCommandBuffer::SetGlobalMat4(const std::string& name, const glm::mat4& value) {
    set_global_mat4_cmds_.push_back({next_cmd_order_++, name, value});
}

void OpenGLCommandBuffer::SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) {
    set_global_mat4_array_cmds_.push_back({next_cmd_order_++, name, values});
}

void OpenGLCommandBuffer::SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) {
    set_global_float_array_cmds_.push_back({next_cmd_order_++, name, values});
}

void OpenGLCommandBuffer::DrawBatch(const std::vector<DrawBatchItem>& items) {
    DrawBatchCmd cmd;
    cmd.order = next_cmd_order_++;
    cmd.items = items;
    cmd.view = view_;
    cmd.projection = projection_;
    draw_batch_cmds_.push_back(std::move(cmd));
}

void OpenGLCommandBuffer::DrawMeshBatch(const std::vector<MeshDrawItem>& items) {
    if (items.empty()) return;
    draw_mesh_batch_cmds_.push_back({next_cmd_order_++, items, view_, projection_});
}

void OpenGLCommandBuffer::DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) {
    DrawBatch(items);
}

void OpenGLCommandBuffer::DrawSkybox(unsigned int cubemap_texture_handle) {
    draw_skybox_cmds_.push_back({next_cmd_order_++, cubemap_texture_handle, view_, projection_});
}

void OpenGLCommandBuffer::ClearColor(const glm::vec4& color) {
    clear_cmds_.push_back({next_cmd_order_++, color});
}

void OpenGLCommandBuffer::DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) {
    draw_post_process_cmds_.push_back({next_cmd_order_++, source_texture, effect_name, params});
}

void OpenGLCommandBuffer::DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (items.empty()) return;
    draw_particles3d_cmds_.push_back({next_cmd_order_++, items, view, projection});
}

void OpenGLCommandBuffer::Reset() {
    begin_render_pass_cmds_.clear();
    end_render_pass_cmds_.clear();
    set_pipeline_state_cmds_.clear();
    set_global_mat4_cmds_.clear();
    set_global_mat4_array_cmds_.clear();
    set_global_float_array_cmds_.clear();
    clear_cmds_.clear();
    draw_batch_cmds_.clear();
    draw_mesh_batch_cmds_.clear();
    draw_skybox_cmds_.clear();
    draw_post_process_cmds_.clear();
    draw_particles3d_cmds_.clear();
    next_cmd_order_ = 0;
}

void OpenGLCommandBuffer::Execute(OpenGLRhiDevice* device) {
    if (!device) {
        return;
    }

    std::vector<CommandRef> commands;
    commands.reserve(begin_render_pass_cmds_.size() + set_pipeline_state_cmds_.size() + clear_cmds_.size() + draw_batch_cmds_.size() + draw_mesh_batch_cmds_.size() + end_render_pass_cmds_.size());
    for (size_t i = 0; i < begin_render_pass_cmds_.size(); ++i) {
        commands.push_back({begin_render_pass_cmds_[i].order, 0, i});
    }
    for (size_t i = 0; i < set_pipeline_state_cmds_.size(); ++i) {
        commands.push_back({set_pipeline_state_cmds_[i].order, 1, i});
    }
    for (size_t i = 0; i < set_global_mat4_cmds_.size(); ++i) {
        commands.push_back({set_global_mat4_cmds_[i].order, 8, i});
    }
    for (size_t i = 0; i < clear_cmds_.size(); ++i) {
        commands.push_back({clear_cmds_[i].order, 2, i});
    }
    for (size_t i = 0; i < draw_batch_cmds_.size(); ++i) {
        commands.push_back({draw_batch_cmds_[i].order, 3, i});
    }
    for (size_t i = 0; i < end_render_pass_cmds_.size(); ++i) {
        commands.push_back({end_render_pass_cmds_[i].order, 4, i});
    }
    for (size_t i = 0; i < draw_mesh_batch_cmds_.size(); ++i) {
        commands.push_back({draw_mesh_batch_cmds_[i].order, 5, i});
    }
    for (size_t i = 0; i < draw_skybox_cmds_.size(); ++i) {
        commands.push_back({draw_skybox_cmds_[i].order, 7, i});
    }
    for (size_t i = 0; i < set_global_mat4_array_cmds_.size(); ++i) {
        commands.push_back({set_global_mat4_array_cmds_[i].order, 9, i});
    }
    for (size_t i = 0; i < set_global_float_array_cmds_.size(); ++i) {
        commands.push_back({set_global_float_array_cmds_[i].order, 10, i});
    }
    for (size_t i = 0; i < draw_post_process_cmds_.size(); ++i) {
        commands.push_back({draw_post_process_cmds_[i].order, 11, i});
    }
    for (size_t i = 0; i < draw_particles3d_cmds_.size(); ++i) {
        commands.push_back({draw_particles3d_cmds_[i].order, 12, i});
    }
    std::sort(commands.begin(), commands.end(), [](const CommandRef& a, const CommandRef& b) {
        return a.order < b.order;
    });

    for (const auto& cmd : commands) {
        if (cmd.type == 0) {
            device->RealBeginRenderPass(begin_render_pass_cmds_[cmd.index].render_pass);
        } else if (cmd.type == 1) {
            device->RealSetPipelineState(set_pipeline_state_cmds_[cmd.index].pipeline_state_handle);
        } else if (cmd.type == 8) {
            const auto& mat_cmd = set_global_mat4_cmds_[cmd.index];
            if (mat_cmd.name == "u_spot_light_space_matrix") {
                device->SetGlobalSpotLightSpaceMatrix(mat_cmd.value);
            } else if (mat_cmd.name == "u_light_space_matrix") {
                device->SetGlobalLightSpaceMatrix(0, mat_cmd.value);
            }
        } else if (cmd.type == 9) {
            const auto& mat_cmd = set_global_mat4_array_cmds_[cmd.index];
            if (mat_cmd.name == "u_light_space_matrices") {
                for(size_t j=0; j<3 && j<mat_cmd.values.size(); ++j) {
                    device->SetGlobalLightSpaceMatrix(static_cast<unsigned int>(j), mat_cmd.values[j]);
                }
            } else if (mat_cmd.name == "u_spot_light_space_matrices") {
                for(size_t j=0; j<4 && j<mat_cmd.values.size(); ++j) {
                    device->SetGlobalSpotLightSpaceMatrix(static_cast<unsigned int>(j), mat_cmd.values[j]);
                }
            }
        } else if (cmd.type == 10) {
            const auto& mat_cmd = set_global_float_array_cmds_[cmd.index];
            if (mat_cmd.name == "u_cascade_splits") {
                for(size_t j=0; j<3 && j<mat_cmd.values.size(); ++j) {
                    device->SetGlobalCascadeSplit(static_cast<unsigned int>(j), mat_cmd.values[j]);
                }
            }
        } else if (cmd.type == 2) {
            device->RealClearColor(clear_cmds_[cmd.index].color);
        } else if (cmd.type == 3) {
            device->RealSubmitDrawBatch(draw_batch_cmds_[cmd.index].items, draw_batch_cmds_[cmd.index].view, draw_batch_cmds_[cmd.index].projection);
        } else if (cmd.type == 4) {
            device->RealEndRenderPass();
        } else if (cmd.type == 5) {
            device->RealSubmitDrawMeshBatch(draw_mesh_batch_cmds_[cmd.index].items, draw_mesh_batch_cmds_[cmd.index].view, draw_mesh_batch_cmds_[cmd.index].projection);
        } else if (cmd.type == 7) {
            device->RealSubmitDrawSkybox(draw_skybox_cmds_[cmd.index].cubemap_texture_handle, draw_skybox_cmds_[cmd.index].view, draw_skybox_cmds_[cmd.index].projection);
        } else if (cmd.type == 11) {
            device->RealSubmitDrawPostProcess(draw_post_process_cmds_[cmd.index].source_texture, draw_post_process_cmds_[cmd.index].effect_name, draw_post_process_cmds_[cmd.index].params);
        } else if (cmd.type == 12) {
            device->RealSubmitDrawParticles3D(draw_particles3d_cmds_[cmd.index].items, draw_particles3d_cmds_[cmd.index].view, draw_particles3d_cmds_[cmd.index].projection);
        }
    }
    Reset();
}

void OpenGLRhiDevice::BeginFrame() {
    EnsureInitialized();
    current_frame_stats_ = {};
}

unsigned int OpenGLRhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    unsigned int handle = ++next_render_target_handle_;
    unsigned int color_texture_handle = 0;
    unsigned int depth_texture_handle = 0;
    unsigned int fbo_handle = 0;

    auto cleanup_failed_rt = [&]() {
        if (fbo_handle != 0) {
            glDeleteFramebuffers(1, &fbo_handle);
            resource_ledger_.framebuffers_destroyed += 1;
            fbo_handle = 0;
        }
        if (depth_texture_handle != 0) {
            glDeleteTextures(1, &depth_texture_handle);
            resource_ledger_.textures_destroyed += 1;
            depth_texture_handle = 0;
        }
        if (color_texture_handle != 0) {
            glDeleteTextures(1, &color_texture_handle);
            resource_ledger_.textures_destroyed += 1;
            color_texture_handle = 0;
        }
    };

    glGenTextures(1, &color_texture_handle);
    resource_ledger_.textures_created += 1;
    if (color_texture_handle == 0) {
        DEBUG_LOG_ERROR("OpenGL CreateRenderTarget failed: glGenTextures returned 0 for color attachment ({}x{}, color={}, depth={}, mipmaps={})",
            desc.width, desc.height, desc.has_color, desc.has_depth, desc.generate_mipmaps);
        return 0;
    }

    if (desc.has_depth) {
        glGenTextures(1, &depth_texture_handle);
        resource_ledger_.textures_created += 1;
        if (depth_texture_handle == 0) {
            DEBUG_LOG_ERROR("OpenGL CreateRenderTarget failed: glGenTextures returned 0 for depth attachment ({}x{}, color={}, depth={}, mipmaps={})",
                desc.width, desc.height, desc.has_color, desc.has_depth, desc.generate_mipmaps);
            cleanup_failed_rt();
            return 0;
        }
    }

    glGenFramebuffers(1, &fbo_handle);
    resource_ledger_.framebuffers_created += 1;
    if (fbo_handle == 0) {
        DEBUG_LOG_ERROR("OpenGL CreateRenderTarget failed: glGenFramebuffers returned 0 ({}x{}, color={}, depth={}, mipmaps={})",
            desc.width, desc.height, desc.has_color, desc.has_depth, desc.generate_mipmaps);
        cleanup_failed_rt();
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, color_texture_handle);

    // Support Phase 2 HDR and mipmaps
    GLint internal_format = desc.has_color ? GL_RGBA16F : GL_RGBA;
    GLenum type = desc.has_color ? GL_FLOAT : GL_UNSIGNED_BYTE;

    if (desc.generate_mipmaps) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, desc.width, desc.height, 0, GL_RGBA, type, nullptr);

    if (desc.generate_mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    if (desc.has_depth) {
        glBindTexture(GL_TEXTURE_2D, depth_texture_handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, desc.width, desc.height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_handle);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture_handle, 0);
    if (desc.has_depth) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_handle, 0);
    }

    const GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
        DEBUG_LOG_ERROR("OpenGL CreateRenderTarget failed: framebuffer incomplete, status=0x{:X} ({}x{}, color={}, depth={}, mipmaps={})",
            static_cast<unsigned int>(framebuffer_status), desc.width, desc.height, desc.has_color, desc.has_depth, desc.generate_mipmaps);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        cleanup_failed_rt();
        return 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    render_targets_[handle] = {desc, fbo_handle, color_texture_handle, depth_texture_handle};
    resource_ledger_.render_targets_created += 1;
    return handle;
}

unsigned int OpenGLRhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    auto it = render_targets_.find(render_target_handle);
    if (it == render_targets_.end()) {
        return 0;
    }
    return it->second.color_texture_handle;
}

unsigned int OpenGLRhiDevice::GetRenderTargetDepthTexture(unsigned int render_target_handle) const {
    auto it = render_targets_.find(render_target_handle);
    if (it == render_targets_.end()) {
        return 0;
    }
    return it->second.depth_texture_handle;
}


unsigned int OpenGLRhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    unsigned int texture_handle = 0;
    glGenTextures(1, &texture_handle);
    resource_ledger_.textures_created += 1;
    glBindTexture(GL_TEXTURE_2D, texture_handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture_handle;
}

void OpenGLRhiDevice::DeleteTexture(unsigned int texture_handle) {
    if (texture_handle == 0) {
        return;
    }
    glDeleteTextures(1, &texture_handle);
    resource_ledger_.textures_destroyed += 1;
}

unsigned int OpenGLRhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
    unsigned int shader_program = CompileShaderProgram(vert_src.c_str(), frag_src.c_str());
    resource_ledger_.shader_programs_created += 1;
    return shader_program;
}

void OpenGLRhiDevice::DeleteShaderProgram(unsigned int program_handle) {
    if (program_handle == 0) {
        return;
    }
    glDeleteProgram(program_handle);
    resource_ledger_.shader_programs_destroyed += 1;
}

unsigned int OpenGLRhiDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    unsigned int handle = ++next_pipeline_state_handle_;
    pipeline_states_[handle] = desc;
    resource_ledger_.pipeline_states_created += 1;
    return handle;
}

std::shared_ptr<CommandBuffer> OpenGLRhiDevice::CreateCommandBuffer() {
    return std::make_shared<OpenGLCommandBuffer>();
}

void OpenGLRhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    auto gl_cmd = std::dynamic_pointer_cast<OpenGLCommandBuffer>(cmd_buffer);
    if (gl_cmd) {
        gl_cmd->Execute(this);
    }
}

void OpenGLRhiDevice::RealClearColor(const glm::vec4& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRhiDevice::RealBeginRenderPass(const RenderPassDesc& render_pass) {
    bool has_depth = false;
    if (render_pass.render_target == 0) {
        if (active_render_target_ != 0) {
            auto active_it = render_targets_.find(active_render_target_);
            if (active_it != render_targets_.end()) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }
        active_render_target_ = 0;
        glViewport(0, 0, Screen::width(), Screen::height());
    } else {
        auto it = render_targets_.find(render_pass.render_target);
        if (it != render_targets_.end()) {
            if (active_render_target_ != 0 && active_render_target_ != render_pass.render_target) {
                auto active_it = render_targets_.find(active_render_target_);
                if (active_it != render_targets_.end()) {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, it->second.fbo_handle);
            glViewport(0, 0, it->second.desc.width, it->second.desc.height);
            active_render_target_ = render_pass.render_target;
            has_depth = it->second.desc.has_depth;
        }
    }
    current_frame_stats_.render_passes += 1;
    if (render_pass.render_target != 0) {
        auto stat_it = render_targets_.find(render_pass.render_target);
        if (stat_it != render_targets_.end() && !stat_it->second.desc.has_color && stat_it->second.desc.has_depth) {
            current_frame_stats_.shadow_passes += 1;
        }
    }
    if (render_pass.clear_color_enabled) {
        glClearColor(render_pass.clear_color.r, render_pass.clear_color.g, render_pass.clear_color.b, render_pass.clear_color.a);
        glClearDepth(1.0);
        glClear(has_depth ? (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) : GL_COLOR_BUFFER_BIT);
    } else if (has_depth) {
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void OpenGLRhiDevice::RealEndRenderPass() {
    if (active_render_target_ != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        active_render_target_ = 0;
    }
}

void OpenGLRhiDevice::RealSetPipelineState(unsigned int pipeline_state_handle) {
    active_pipeline_state_ = pipeline_state_handle;
    auto it = pipeline_states_.find(pipeline_state_handle);
    if (it == pipeline_states_.end()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        return;
    }
    if (it->second.blend_enabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    if (it->second.blend_enabled) {
        glBlendFunc(it->second.blend_src, it->second.blend_dst);
    }
    if (it->second.depth_test_enabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(it->second.depth_write_enabled ? GL_TRUE : GL_FALSE);
    if (it->second.culling_enabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(it->second.cull_face);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

void OpenGLRhiDevice::RealSubmitDrawMeshBatch(const std::vector<MeshDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (items.empty()) return;
    current_frame_stats_.mesh_count += static_cast<int>(items.size());

    glm::mat4 vp = projection * view;
    
    glUseProgram(shader_handle_);
    if (active_pipeline_state_ != 0) {
        RealSetPipelineState(active_pipeline_state_);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glUniform1i(uniform_texture_loc_, 0);
    glUniform3f(uniform_tint_loc_, 1.0f, 1.0f, 1.0f);
    glUniformMatrix4fv(uniform_vp_loc_, 1, GL_FALSE, &vp[0][0]);
    // Extract camera pos from view matrix roughly for PBR
    glm::mat4 inv_view = glm::inverse(view);
    glUniform3f(uniform_camera_pos_loc_, inv_view[3][0], inv_view[3][1], inv_view[3][2]);
    
    // glUniformMatrix4fv(uniform_light_space_matrix_loc_, 1, GL_FALSE, &global_light_space_matrix_[0][0]);
    
    // For shadow mapping
    // We expect the FramePipeline to have bound the shadow map to texture unit 2
    // glUniform1i(uniform_shadow_map_loc_, 2);
    // unsigned int shadow_map = global_shadow_map_[0];
    // glActiveTexture(GL_TEXTURE2);
    // glBindTexture(GL_TEXTURE_2D, shadow_map);

    unsigned int last_texture_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_normal_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_blend_mode = std::numeric_limits<unsigned int>::max();

    for (const auto& item : items) {
        if (item.vertices.empty() || item.indices.empty()) continue;

        unsigned int tex = item.texture_handle == 0 ? white_texture_handle_ : item.texture_handle;
        if (last_texture_handle != tex) {
            if (last_texture_handle != std::numeric_limits<unsigned int>::max()) {
                current_frame_stats_.material_switches += 1;
            }
            last_texture_handle = tex;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        if (last_normal_map_handle != item.normal_map_handle) {
            if (last_normal_map_handle != std::numeric_limits<unsigned int>::max()) {
                current_frame_stats_.material_switches += 1;
            }
            last_normal_map_handle = item.normal_map_handle;
        }

        if (item.normal_map_handle != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, item.normal_map_handle);
            glUniform1i(uniform_normal_map_loc_, 1);
            glUniform1i(uniform_has_normal_map_loc_, 1);
        } else {
            glUniform1i(uniform_has_normal_map_loc_, 0);
        }

        glUniform1i(uniform_lighting_enabled_loc_, item.lighting_enabled ? 1 : 0);
        glUniform3f(uniform_light_direction_loc_, item.light_direction.x, item.light_direction.y, item.light_direction.z);
        glUniform3f(uniform_light_color_loc_, item.light_color.r, item.light_color.g, item.light_color.b);
        glUniform1f(uniform_light_intensity_loc_, item.light_intensity);
        glUniform1f(uniform_ambient_intensity_loc_, item.ambient_intensity);
        glUniform1f(uniform_shadow_strength_loc_, item.shadow_strength);

        // Point lights
        int point_count = std::min(static_cast<int>(item.point_lights.size()), 4);
        if (uniform_point_light_count_loc_ != -1) glUniform1i(uniform_point_light_count_loc_, point_count);
        for (int i = 0; i < point_count; ++i) {
            glUniform3f(uniform_point_lights_loc_[i].color, item.point_lights[i].color.r, item.point_lights[i].color.g, item.point_lights[i].color.b);
            glUniform3f(uniform_point_lights_loc_[i].position, item.point_lights[i].position.x, item.point_lights[i].position.y, item.point_lights[i].position.z);
            glUniform1f(uniform_point_lights_loc_[i].intensity, item.point_lights[i].intensity);
            glUniform1f(uniform_point_lights_loc_[i].radius, item.point_lights[i].radius);
        }

        // Spot lights
        int spot_count = std::min(static_cast<int>(item.spot_lights.size()), 4);
        if (uniform_spot_light_count_loc_ != -1) glUniform1i(uniform_spot_light_count_loc_, spot_count);
        for (int i = 0; i < spot_count; ++i) {
            glUniform3f(uniform_spot_lights_loc_[i].color, item.spot_lights[i].color.r, item.spot_lights[i].color.g, item.spot_lights[i].color.b);
            glUniform3f(uniform_spot_lights_loc_[i].position, item.spot_lights[i].position.x, item.spot_lights[i].position.y, item.spot_lights[i].position.z);
            glUniform3f(uniform_spot_lights_loc_[i].direction, item.spot_lights[i].direction.x, item.spot_lights[i].direction.y, item.spot_lights[i].direction.z);
            glUniform1f(uniform_spot_lights_loc_[i].intensity, item.spot_lights[i].intensity);
            glUniform1f(uniform_spot_lights_loc_[i].radius, item.spot_lights[i].radius);
            glUniform1f(uniform_spot_lights_loc_[i].inner_cone, item.spot_lights[i].inner_cone);
            glUniform1f(uniform_spot_lights_loc_[i].outer_cone, item.spot_lights[i].outer_cone);
            if (uniform_spot_lights_loc_[i].cast_shadow != -1) glUniform1i(uniform_spot_lights_loc_[i].cast_shadow, item.spot_lights[i].cast_shadow ? 1 : 0);
            if (uniform_spot_lights_loc_[i].shadow_index != -1) glUniform1i(uniform_spot_lights_loc_[i].shadow_index, item.spot_lights[i].shadow_index);
        }

        for (int i = 0; i < 3; ++i) {
            if (uniform_light_space_matrix_loc_[i] != -1) {
                glUniformMatrix4fv(uniform_light_space_matrix_loc_[i], 1, GL_FALSE, glm::value_ptr(global_light_space_matrix_[i]));
            }
            
            if (uniform_shadow_map_loc_[i] != -1) {
                glActiveTexture(GL_TEXTURE2 + i);
                glBindTexture(GL_TEXTURE_2D, global_shadow_map_[i]);
                glUniform1i(uniform_shadow_map_loc_[i], 2 + i);
            }
        }
        if (uniform_cascade_splits_loc_ != -1) {
            glUniform1fv(uniform_cascade_splits_loc_, 3, global_cascade_splits_);
        }
        for (int i = 0; i < 4; ++i) {
            if (uniform_spot_light_space_matrix_loc_[i] != -1) {
                glUniformMatrix4fv(uniform_spot_light_space_matrix_loc_[i], 1, GL_FALSE, glm::value_ptr(global_spot_light_space_matrix_[i]));
            }
            if (uniform_spot_shadow_map_loc_[i] != -1) {
                glActiveTexture(GL_TEXTURE5 + i);
                glBindTexture(GL_TEXTURE_2D, global_spot_shadow_map_[i]);
                glUniform1i(uniform_spot_shadow_map_loc_[i], 5 + i);
            }
        }

        if (last_blend_mode != item.blend_mode) {
            if (last_blend_mode != std::numeric_limits<unsigned int>::max()) {
                current_frame_stats_.material_switches += 1;
            }
            last_blend_mode = item.blend_mode;
        }

        glUniform3f(uniform_material_albedo_loc_, item.material_albedo.r, item.material_albedo.g, item.material_albedo.b);
        glUniform1f(uniform_material_metallic_loc_, item.material_metallic);
        glUniform1f(uniform_material_roughness_loc_, item.material_roughness);
        glUniform1f(uniform_material_ao_loc_, item.material_ao);
        glUniform3f(uniform_material_emissive_loc_, item.material_emissive.r, item.material_emissive.g, item.material_emissive.b);
        glUniform1f(uniform_material_normal_strength_loc_, item.material_normal_strength);
        glUniform1f(uniform_material_alpha_cutoff_loc_, item.material_alpha_cutoff);
        glUniform1i(uniform_receive_shadow_loc_, item.receive_shadow ? 1 : 0);

        if (uniform_skinned_loc_ != -1) {
            glUniform1i(uniform_skinned_loc_, item.skinned ? 1 : 0);
        }
        if (item.skinned && uniform_bone_matrices_loc_ != -1 && !item.bone_matrices.empty()) {
            glUniformMatrix4fv(uniform_bone_matrices_loc_, static_cast<GLsizei>(std::min(item.bone_matrices.size(), static_cast<size_t>(100))), GL_FALSE, glm::value_ptr(item.bone_matrices[0]));
        }

        if (uniform_morph_enabled_loc_ != -1) {
            glUniform1i(uniform_morph_enabled_loc_, item.morph_enabled ? 1 : 0);
        }
        if (item.morph_enabled && uniform_morph_weights_loc_ != -1 && !item.morph_weights.empty()) {
            glUniform1fv(uniform_morph_weights_loc_, static_cast<GLsizei>(std::min(item.morph_weights.size(), static_cast<size_t>(4))), item.morph_weights.data());
        }

        if (item.vao_override > 0) {
            glBindVertexArray(item.vao_override);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(item.index_count_override), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        } else {
            UpdateBuffer(mesh_vbo_handle_, 0, item.vertices.size() * sizeof(BatchVertex), item.vertices.data(), false);
            UpdateBuffer(mesh_ibo_handle_, 0, item.indices.size() * sizeof(unsigned short), item.indices.data(), true);

            glBindVertexArray(mesh_vao_handle_);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(item.indices.size()), GL_UNSIGNED_SHORT, nullptr);
            glBindVertexArray(0);
        }

        current_frame_stats_.draw_calls += 1;
    }
}

void OpenGLRhiDevice::RealSubmitDrawSkybox(unsigned int cubemap_texture_handle, const glm::mat4& view, const glm::mat4& projection) {
    if (cubemap_texture_handle == 0) return;

    if (skybox_shader_handle_ == 0) {
        const char* vs_src = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            out vec3 TexCoords;
            uniform mat4 projection;
            uniform mat4 view;
            void main() {
                TexCoords = aPos;
                vec4 pos = projection * view * vec4(aPos, 1.0);
                gl_Position = pos.xyww;
            }
        )";
        const char* fs_src = R"(
            #version 330 core
            out vec4 FragColor;
            in vec3 TexCoords;
            uniform samplerCube skybox;
            void main() {
                FragColor = texture(skybox, TexCoords);
            }
        )";
        unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vs_src, nullptr);
        glCompileShader(vs);
        unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fs_src, nullptr);
        glCompileShader(fs);
        skybox_shader_handle_ = glCreateProgram();
        resource_ledger_.shader_programs_created += 1;
        glAttachShader(skybox_shader_handle_, vs);
        glAttachShader(skybox_shader_handle_, fs);
        glLinkProgram(skybox_shader_handle_);
        glDeleteShader(vs);
        glDeleteShader(fs);
        
        skybox_view_loc_ = glGetUniformLocation(skybox_shader_handle_, "view");
        skybox_proj_loc_ = glGetUniformLocation(skybox_shader_handle_, "projection");
        skybox_tex_loc_ = glGetUniformLocation(skybox_shader_handle_, "skybox");
        
        float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };
        skybox_vao_handle_ = CreateVertexArray();
        skybox_vbo_handle_ = CreateBuffer(sizeof(skyboxVertices), skyboxVertices, false, false);
        glBindVertexArray(skybox_vao_handle_);
        glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo_handle_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }
    
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skybox_shader_handle_);
    glm::mat4 skybox_view = glm::mat4(glm::mat3(view));
    glUniformMatrix4fv(skybox_view_loc_, 1, GL_FALSE, glm::value_ptr(skybox_view));
    glUniformMatrix4fv(skybox_proj_loc_, 1, GL_FALSE, glm::value_ptr(projection));
    
    glBindVertexArray(skybox_vao_handle_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture_handle);
    glUniform1i(skybox_tex_loc_, 0);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
    
    current_frame_stats_.draw_calls += 1;
}

void OpenGLRhiDevice::RealSubmitDrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) {
    static unsigned int pp_vao = 0;
    static unsigned int pp_vbo = 0;
    static std::unordered_map<std::string, unsigned int> pp_shaders;

    if (pp_vao == 0) {
        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        glGenVertexArrays(1, &pp_vao);
        glGenBuffers(1, &pp_vbo);
        glBindVertexArray(pp_vao);
        glBindBuffer(GL_ARRAY_BUFFER, pp_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    if (pp_shaders.find(effect_name) == pp_shaders.end()) {
        const char* vs_src = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoords;
            out vec2 TexCoords;
            void main() {
                TexCoords = aTexCoords;
                gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
            }
        )";

        std::string fs_src = "#version 330 core\nout vec4 FragColor;\nin vec2 TexCoords;\nuniform sampler2D screenTexture;\n";
        
        if (effect_name == "bloom_downsample") {
            fs_src += R"(
                uniform vec2 srcResolution;
                void main() {
                    vec2 srcTexelSize = 1.0 / srcResolution;
                    float x = srcTexelSize.x;
                    float y = srcTexelSize.y;

                    // Take 13 samples around current texel:
                    // a - b - c
                    // - j - k -
                    // d - e - f
                    // - l - m -
                    // g - h - i
                    vec3 a = texture(screenTexture, vec2(TexCoords.x - 2*x, TexCoords.y + 2*y)).rgb;
                    vec3 b = texture(screenTexture, vec2(TexCoords.x,       TexCoords.y + 2*y)).rgb;
                    vec3 c = texture(screenTexture, vec2(TexCoords.x + 2*x, TexCoords.y + 2*y)).rgb;

                    vec3 d = texture(screenTexture, vec2(TexCoords.x - 2*x, TexCoords.y)).rgb;
                    vec3 e = texture(screenTexture, vec2(TexCoords.x,       TexCoords.y)).rgb;
                    vec3 f = texture(screenTexture, vec2(TexCoords.x + 2*x, TexCoords.y)).rgb;

                    vec3 g = texture(screenTexture, vec2(TexCoords.x - 2*x, TexCoords.y - 2*y)).rgb;
                    vec3 h = texture(screenTexture, vec2(TexCoords.x,       TexCoords.y - 2*y)).rgb;
                    vec3 i = texture(screenTexture, vec2(TexCoords.x + 2*x, TexCoords.y - 2*y)).rgb;

                    vec3 j = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
                    vec3 k = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;
                    vec3 l = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
                    vec3 m = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

                    // Apply weights
                    vec3 downsample = e*0.125;
                    downsample += (a+c+g+i)*0.03125;
                    downsample += (b+d+f+h)*0.0625;
                    downsample += (j+k+l+m)*0.125;
                    
                    // Anti-flicker: max clamp or Karis average can be applied here for the first pass if needed

                    FragColor = vec4(downsample, 1.0);
                }
            )";
        } else if (effect_name == "bloom_upsample") {
            fs_src += R"(
                uniform float filterRadius;
                void main() {
                    float x = filterRadius;
                    float y = filterRadius;

                    // Take 9 samples around current texel:
                    // a - b - c
                    // d - e - f
                    // g - h - i
                    // === and apply tent filter ===
                    vec3 a = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
                    vec3 b = texture(screenTexture, vec2(TexCoords.x,     TexCoords.y + y)).rgb;
                    vec3 c = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;

                    vec3 d = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y)).rgb;
                    vec3 e = texture(screenTexture, vec2(TexCoords.x,     TexCoords.y)).rgb;
                    vec3 f = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y)).rgb;

                    vec3 g = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
                    vec3 h = texture(screenTexture, vec2(TexCoords.x,     TexCoords.y - y)).rgb;
                    vec3 i = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

                    vec3 upsample = e*4.0;
                    upsample += (b+d+f+h)*2.0;
                    upsample += (a+c+g+i);
                    upsample *= 1.0 / 16.0;

                    FragColor = vec4(upsample, 1.0);
                }
            )";
        } else if (effect_name == "bloom_extract") {
            fs_src += R"(
                uniform float threshold;
                void main() {
                    vec3 color = texture(screenTexture, TexCoords).rgb;
                    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
                    if(brightness > threshold)
                        FragColor = vec4(color, 1.0);
                    else
                        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
                }
            )";
        } else if (effect_name == "bloom_blur_h") {
            fs_src += R"(
                uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
                void main() {
                    vec2 tex_offset = 1.0 / textureSize(screenTexture, 0); // gets size of single texel
                    vec3 result = texture(screenTexture, TexCoords).rgb * weight[0]; // current fragment's contribution
                    for(int i = 1; i < 5; ++i) {
                        result += texture(screenTexture, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                        result += texture(screenTexture, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                    }
                    FragColor = vec4(result, 1.0);
                }
            )";
        } else if (effect_name == "bloom_blur_v") {
            fs_src += R"(
                uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
                void main() {
                    vec2 tex_offset = 1.0 / textureSize(screenTexture, 0); // gets size of single texel
                    vec3 result = texture(screenTexture, TexCoords).rgb * weight[0]; // current fragment's contribution
                    for(int i = 1; i < 5; ++i) {
                        result += texture(screenTexture, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                        result += texture(screenTexture, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                    }
                    FragColor = vec4(result, 1.0);
                }
            )";
        } else if (effect_name == "bloom_composite") {
            fs_src += R"(
                uniform sampler2D bloomBlur;
                uniform float exposure;
                uniform float bloomIntensity;
                void main() {
                    vec3 hdrColor = texture(screenTexture, TexCoords).rgb;      
                    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
                    hdrColor += bloomColor * bloomIntensity; // additive blending
                    // tone mapping
                    vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
                    // also gamma correct while we're at it       
                    result = pow(result, vec3(1.0 / 2.2));
                    FragColor = vec4(result, 1.0);
                }
            )";
        } else {
            // Default copy
            fs_src += R"(
                void main() {
                    FragColor = texture(screenTexture, TexCoords);
                }
            )";
        }

        unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vs_src, nullptr);
        glCompileShader(vs);
        
        const char* fs_c_str = fs_src.c_str();
        unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fs_c_str, nullptr);
        glCompileShader(fs);
        
        unsigned int shader = glCreateProgram();
        glAttachShader(shader, vs);
        glAttachShader(shader, fs);
        glLinkProgram(shader);
        glDeleteShader(vs);
        glDeleteShader(fs);

        pp_shaders[effect_name] = shader;
    }

    unsigned int shader = pp_shaders[effect_name];
    glUseProgram(shader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source_texture);
    glUniform1i(glGetUniformLocation(shader, "screenTexture"), 0);

    if (effect_name == "bloom_extract" && params.size() >= 1) {
        glUniform1f(glGetUniformLocation(shader, "threshold"), params[0]);
    } else if (effect_name == "bloom_downsample" && params.size() >= 2) {
        glUniform2f(glGetUniformLocation(shader, "srcResolution"), params[0], params[1]);
    } else if (effect_name == "bloom_upsample" && params.size() >= 1) {
        glUniform1f(glGetUniformLocation(shader, "filterRadius"), params[0]);
    } else if (effect_name == "bloom_composite") {
        if (params.size() >= 1) {
            glActiveTexture(GL_TEXTURE1);
            // param[0] is interpreted as texture handle for bloomBlur
            glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[0]));
            glUniform1i(glGetUniformLocation(shader, "bloomBlur"), 1);
        }
        if (params.size() >= 2) {
            glUniform1f(glGetUniformLocation(shader, "exposure"), params[1]);
        }
        if (params.size() >= 3) {
            glUniform1f(glGetUniformLocation(shader, "bloomIntensity"), params[2]);
        }
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glBindVertexArray(pp_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void OpenGLRhiDevice::RealSubmitDrawBatch(const std::vector<DrawBatchItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (items.empty()) {
        return;
    }
    current_frame_stats_.sprite_count += static_cast<int>(items.size());

    glm::mat4 vp = projection * view;
    
    glUseProgram(shader_handle_);
    if (active_pipeline_state_ != 0) {
        RealSetPipelineState(active_pipeline_state_);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glUniform1i(uniform_texture_loc_, 0);
    glUniform3f(uniform_tint_loc_, 1.0f, 1.0f, 1.0f);
    glUniformMatrix4fv(uniform_vp_loc_, 1, GL_FALSE, &vp[0][0]);

    std::vector<BatchVertex> batch_vertices;
    batch_vertices.reserve(MAX_VERTICES);

    unsigned int current_texture = items[0].texture_handle == 0 ? white_texture_handle_ : items[0].texture_handle;
    unsigned int current_material_instance = items[0].material_instance_id;
    unsigned int current_shader_variant = items[0].shader_variant_key;
    unsigned int current_blend_mode = items[0].blend_mode;
    const unsigned int additive_variant_key = static_cast<unsigned int>(std::hash<std::string>{}("SPRITE_ADDITIVE"));
    auto apply_blend = [&](unsigned int blend_mode, unsigned int shader_variant_key) {
        glEnable(GL_BLEND);
        if (blend_mode == 1 || shader_variant_key == additive_variant_key) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            return;
        }
        if (blend_mode == 2) {
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            return;
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    };
    apply_blend(current_blend_mode, current_shader_variant);
    
    auto flush_batch = [&]() {
        if (batch_vertices.empty()) {
            return;
        }
        int batch_sprites = static_cast<int>(batch_vertices.size() / 4);
        current_frame_stats_.draw_calls += 1;
        current_frame_stats_.max_batch_sprites = std::max(current_frame_stats_.max_batch_sprites, batch_sprites);
        
        UpdateBuffer(vbo_handle_, 0, batch_vertices.size() * sizeof(BatchVertex), batch_vertices.data(), false);
        
        apply_blend(current_blend_mode, current_shader_variant);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_texture);
        glBindVertexArray(vao_handle_);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>((batch_vertices.size() / 4) * 6), GL_UNSIGNED_SHORT, nullptr);
        glBindVertexArray(0);
        
        batch_vertices.clear();
    };

    const glm::vec4 quad_positions[4] = {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 1.0f}
    };
    
    const glm::vec2 quad_uvs[4] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    };

    for (const auto& item : items) {
        unsigned int tex = item.texture_handle == 0 ? white_texture_handle_ : item.texture_handle;
        if (tex != current_texture ||
            item.material_instance_id != current_material_instance ||
            item.shader_variant_key != current_shader_variant ||
            item.blend_mode != current_blend_mode ||
            batch_vertices.size() + 4 > MAX_VERTICES) {
            flush_batch();
            current_texture = tex;
            current_material_instance = item.material_instance_id;
            current_shader_variant = item.shader_variant_key;
            current_blend_mode = item.blend_mode;
        }

        // Apply UV rect if provided, else use default quad UVs
        // Assuming item.uv is a rect (x, y, w, h)
        // If uv.z and uv.w are 0, we fallback to default [0,1]
        glm::vec2 uvs[4];
        if (item.uv.z > 0.0f && item.uv.w > 0.0f) {
            const bool use_max_uv = item.uv.z > item.uv.x && item.uv.w > item.uv.y;
            const float u1 = use_max_uv ? item.uv.z : (item.uv.x + item.uv.z);
            const float v1 = use_max_uv ? item.uv.w : (item.uv.y + item.uv.w);
            uvs[0] = {item.uv.x, item.uv.y};
            uvs[1] = {u1, item.uv.y};
            uvs[2] = {u1, v1};
            uvs[3] = {item.uv.x, v1};
        } else {
            for (int i = 0; i < 4; ++i) uvs[i] = quad_uvs[i];
        }

        for (int i = 0; i < 4; ++i) {
            BatchVertex vertex;
            glm::vec4 world_pos = item.model * quad_positions[i];
            vertex.pos = glm::vec3(world_pos.x, world_pos.y, world_pos.z);
            vertex.color = item.color;
            vertex.uv = uvs[i];
            batch_vertices.push_back(vertex);
        }
    }
    
    flush_batch();
}

void OpenGLRhiDevice::RealSubmitDrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (items.empty()) return;

    if (particle_shader_handle_ == 0) {
        const char* vs_src = R"(
            #version 330 core
            // Per-vertex attributes (Quad)
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec2 aTexCoord;

            // Per-instance attributes (from SSBO or Instanced VBO)
            // For OpenGL 3.3, we use Instanced VBO. Layout 2: pos, 3: color, 4: size
            layout (location = 2) in vec3 iPos;
            layout (location = 3) in vec4 iColor;
            layout (location = 4) in float iSize;

            out vec4 ParticleColor;
            out vec2 TexCoord;

            uniform mat4 u_vp;
            uniform vec3 u_camera_right;
            uniform vec3 u_camera_up;

            void main() {
                // Billboard calculation
                vec3 vertexPosition_worldspace = iPos 
                    + u_camera_right * aPos.x * iSize 
                    + u_camera_up * aPos.y * iSize;

                gl_Position = u_vp * vec4(vertexPosition_worldspace, 1.0);
                
                ParticleColor = iColor;
                TexCoord = aTexCoord;
            }
        )";
        const char* fs_src = R"(
            #version 330 core
            out vec4 FragColor;
            in vec4 ParticleColor;
            in vec2 TexCoord;
            uniform sampler2D u_texture;

            void main() {
                vec4 texColor = texture(u_texture, TexCoord);
                FragColor = texColor * ParticleColor;
            }
        )";
        particle_shader_handle_ = CompileShaderProgram(vs_src, fs_src);
        resource_ledger_.shader_programs_created += 1;
        particle_uniform_vp_loc_ = glGetUniformLocation(particle_shader_handle_, "u_vp");
        particle_uniform_texture_loc_ = glGetUniformLocation(particle_shader_handle_, "u_texture");
    }

    // Shared Quad VAO
    static unsigned int quad_vao = 0;
    static unsigned int quad_vbo = 0;
    if (quad_vao == 0) {
        float quad_vertices[] = {
             -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
              0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
              0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
             -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
              0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
             -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
        };
        glGenVertexArrays(1, &quad_vao);
        glGenBuffers(1, &quad_vbo);
        glBindVertexArray(quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    glUseProgram(particle_shader_handle_);
    glm::mat4 vp = projection * view;
    glUniformMatrix4fv(particle_uniform_vp_loc_, 1, GL_FALSE, glm::value_ptr(vp));
    glUniform1i(particle_uniform_texture_loc_, 0);

    // Extract camera right and up vectors for billboarding
    glm::vec3 camera_right = glm::vec3(view[0][0], view[1][0], view[2][0]);
    glm::vec3 camera_up = glm::vec3(view[0][1], view[1][1], view[2][1]);
    glUniform3fv(glGetUniformLocation(particle_shader_handle_, "u_camera_right"), 1, glm::value_ptr(camera_right));
    glUniform3fv(glGetUniformLocation(particle_shader_handle_, "u_camera_up"), 1, glm::value_ptr(camera_up));

    // Disable depth write for particles, enable additive blending
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive

    for (const auto& item : items) {
        if (item.particle_count == 0 || item.instance_vbo == 0) continue;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, item.texture_handle == 0 ? white_texture_handle_ : item.texture_handle);

        glBindVertexArray(quad_vao);
        
        // Bind instance VBO
        glBindBuffer(GL_ARRAY_BUFFER, item.instance_vbo);
        
        // Layout: pos(vec3), color(vec4), size(float) -> total 8 floats = 32 bytes
        size_t stride = 8 * sizeof(float);
        
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glVertexAttribDivisor(2, 1); // Tell OpenGL this is an instanced attribute

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glVertexAttribDivisor(3, 1);

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
        glVertexAttribDivisor(4, 1);

        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, item.particle_count);
        current_frame_stats_.draw_calls += 1;

        // Reset divisors
        glVertexAttribDivisor(2, 0);
        glVertexAttribDivisor(3, 0);
        glVertexAttribDivisor(4, 0);
        glBindVertexArray(0);
    }

    // Restore state
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void OpenGLRhiDevice::EndFrame() {
    last_frame_stats_ = current_frame_stats_;
    glFlush();
}

const RenderStats& OpenGLRhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

void OpenGLRhiDevice::LogResourceLedger() const {
    const std::size_t live_textures = resource_ledger_.textures_created - resource_ledger_.textures_destroyed;
    const std::size_t live_fbos = resource_ledger_.framebuffers_created - resource_ledger_.framebuffers_destroyed;
    const std::size_t live_programs = resource_ledger_.shader_programs_created - resource_ledger_.shader_programs_destroyed;
    const std::size_t live_vaos = resource_ledger_.vertex_arrays_created - resource_ledger_.vertex_arrays_destroyed;
    const std::size_t live_buffers = resource_ledger_.buffers_created - resource_ledger_.buffers_destroyed;
    const std::size_t live_targets = resource_ledger_.render_targets_created - resource_ledger_.render_targets_destroyed;
    const std::size_t live_pipelines = resource_ledger_.pipeline_states_created - resource_ledger_.pipeline_states_destroyed;
    DEBUG_LOG_INFO("RHI resource ledger: textures={}/{}, framebuffers={}/{}, shader_programs={}/{}, vaos={}/{}, buffers={}/{}, render_targets={}/{}, pipeline_states={}/{}",
                   resource_ledger_.textures_destroyed, resource_ledger_.textures_created,
                   resource_ledger_.framebuffers_destroyed, resource_ledger_.framebuffers_created,
                   resource_ledger_.shader_programs_destroyed, resource_ledger_.shader_programs_created,
                   resource_ledger_.vertex_arrays_destroyed, resource_ledger_.vertex_arrays_created,
                   resource_ledger_.buffers_destroyed, resource_ledger_.buffers_created,
                   resource_ledger_.render_targets_destroyed, resource_ledger_.render_targets_created,
                   resource_ledger_.pipeline_states_destroyed, resource_ledger_.pipeline_states_created);
    if (live_textures != 0 || live_fbos != 0 || live_programs != 0 || live_vaos != 0 || live_buffers != 0 || live_targets != 0 || live_pipelines != 0) {
        DEBUG_LOG_WARN("RHI resource ledger leak suspect: live_textures={}, live_fbos={}, live_programs={}, live_vaos={}, live_buffers={}, live_targets={}, live_pipelines={}",
                       live_textures, live_fbos, live_programs, live_vaos, live_buffers, live_targets, live_pipelines);
    }
}
