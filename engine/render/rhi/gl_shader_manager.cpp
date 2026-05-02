/**
 * @file gl_shader_manager.cpp
 * @brief GLShaderManager 实现 - 着色器管理器
 */

#include "engine/render/rhi/gl_shader_manager.h"
#include "engine/base/debug.h"
#include <glad/gl.h>
#include <cstdio>

namespace dse {
namespace render {

// ============================================================
// 通用着色器编译/链接
// ============================================================

unsigned int GLShaderManager::CompileProgram(const char* vertex_src, const char* fragment_src) {
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_src, nullptr);
    glCompileShader(vertex_shader);
    int vertex_compiled = 0;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_compiled);
    if (vertex_compiled == GL_FALSE) {
        char log_buffer[1024];
        glGetShaderInfoLog(vertex_shader, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL vertex shader compile failed: {}", log_buffer);
    }

    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_src, nullptr);
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

void GLShaderManager::DeleteProgram(unsigned int handle) {
    if (handle == 0) return;
    glDeleteProgram(handle);
    programs_destroyed_ += 1;
}

// ============================================================
// 内置 PBR 着色器
// ============================================================

void GLShaderManager::InitBuiltinPBRShader() {
    // --- 顶点着色器 ---
    // UBO 化后：u_vp / u_view 从 PerFrame UBO (binding=0) 获取
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

        // PerFrame UBO (binding = 0)
        layout(std140) uniform PerFrame {
            mat4 vp;
            mat4 view;
            vec4 camera_pos;
        };

        uniform mat4 u_model;
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
            gl_Position = vp * worldPos;
            
            FragPos = worldPos.xyz;
            FragPosViewSpace = (view * worldPos).xyz;
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

    // --- 片段着色器（PBR + CSM + 点光源/聚光灯 + 阴影） ---
    // UBO 化后：PerFrame/PerScene/PerMaterial 从 UBO 获取，纹理/骨骼等保留独立 uniform
    const char* fragment_shader = R"(
        #version 330 core
        out vec4 FragColor;

        in vec4 ourColor;
        in vec2 TexCoord;
        in vec3 FragPos;
        in vec3 Normal;
        in mat3 TBN;
        in vec3 FragPosViewSpace;

        // PerFrame UBO (binding = 0)
        layout(std140) uniform PerFrame {
            mat4 vp;
            mat4 view;
            vec4 camera_pos;
        };

        // PerScene UBO (binding = 1)
        layout(std140) uniform PerScene {
            vec4 light_dir_and_enabled;     // xyz = light_direction, w = lighting_enabled
            vec4 light_color_and_ambient;   // xyz = light_color, w = ambient_intensity
            vec4 light_params;              // x = intensity, y = shadow_strength, z = receive_shadow
            vec4 cascade_splits;            // xyz = cascade splits
            mat4 light_space_matrices[3];
        };

        // PerMaterial UBO (binding = 2)
        layout(std140) uniform PerMaterial {
            vec4 albedo;             // xyz = albedo, w = metallic
            vec4 roughness_ao;       // x = roughness, y = ao, z = normal_strength, w = alpha_cutoff
            vec4 emissive;           // xyz = emissive, w = alpha_test
            vec4 flags;              // x = has_normal_map, y = has_mr_map, z = has_emissive_map, w = has_occlusion_map
        };

        uniform sampler2D u_texture;
        uniform sampler2D u_normal_map;
        uniform sampler2D u_metallic_roughness_map;
        uniform sampler2D u_emissive_map;
        uniform sampler2D u_occlusion_map;
        
        #define CSM_CASCADES 3
        uniform sampler2D u_shadow_maps[CSM_CASCADES];
        uniform sampler2D u_spot_shadow_maps[4];
        uniform mat4 u_spot_light_space_matrices[4];
        uniform samplerCube u_point_shadow_maps[4];

        struct PointLight {
            vec3 color;
            vec3 position;
            float intensity;
            float radius;
            bool cast_shadow;
            int shadow_index;
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

        const float PI = 3.14159265359;

        // UBO 字段便捷访问别名
        #define u_lighting_enabled    (light_dir_and_enabled.w != 0.0)
        #define u_light_direction     light_dir_and_enabled.xyz
        #define u_light_color         light_color_and_ambient.xyz
        #define u_light_intensity     light_params.x
        #define u_ambient_intensity   light_color_and_ambient.w
        #define u_shadow_strength     light_params.y
        #define u_receive_shadow      (light_params.z != 0.0)
        #define u_cascade_splits      cascade_splits.xyz
        #define u_light_space_matrices light_space_matrices

        #define u_material_albedo           albedo.xyz
        #define u_material_metallic         albedo.w
        #define u_material_roughness        roughness_ao.x
        #define u_material_ao               roughness_ao.y
        #define u_material_normal_strength  roughness_ao.z
        #define u_material_alpha_cutoff     roughness_ao.w
        #define u_material_emissive         emissive.xyz
        #define u_material_alpha_test       (emissive.w != 0.0)
        #define u_has_normal_map            (flags.x != 0.0)
        #define u_has_metallic_roughness_map (flags.y != 0.0)
        #define u_has_emissive_map          (flags.z != 0.0)
        #define u_has_occlusion_map         (flags.w != 0.0)
        #define u_camera_pos                camera_pos.xyz

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

        float PointShadowCalculation(int shadowIndex, vec3 fragPosWorldSpace, vec3 lightPos, float lightRadius) {
            if (shadowIndex < 0 || shadowIndex >= 4) return 0.0;
            vec3 fragToLight = fragPosWorldSpace - lightPos;
            float currentDepth = length(fragToLight);
            if (currentDepth >= lightRadius) return 0.0;
            float closestDepth = texture(u_point_shadow_maps[shadowIndex], fragToLight).r * lightRadius;
            float bias = 0.05;
            return (currentDepth - bias) > closestDepth ? u_shadow_strength : 0.0;
        }

        void main() {
            vec4 texColor = texture(u_texture, TexCoord);
            if (u_material_alpha_test && texColor.a < clamp(u_material_alpha_cutoff, 0.0, 1.0)) discard;

            vec3 N = Normal;
            if (u_has_normal_map) {
                vec3 normalMap = texture(u_normal_map, TexCoord).rgb;
                normalMap = normalMap * 2.0 - 1.0;
                normalMap.xy *= u_material_normal_strength;
                N = normalize(TBN * normalMap);
            }

            if (!u_lighting_enabled) {
                vec3 result = texColor.rgb * ourColor.rgb * u_material_albedo;
                if (u_has_emissive_map) {
                    result += texture(u_emissive_map, TexCoord).rgb * u_material_emissive;
                }
                result = result / (result + vec3(1.0));
                result = pow(result, vec3(1.0/2.2));
                FragColor = vec4(result, texColor.a * ourColor.a);
                return;
            }

            vec3 surface_albedo = pow(texColor.rgb * ourColor.rgb * u_material_albedo, vec3(2.2));
            float metallic = clamp(u_material_metallic, 0.0, 1.0);
            float roughness = clamp(u_material_roughness, 0.04, 1.0);
            float ao = max(u_material_ao, 0.0);
            vec3 surface_emissive = u_material_emissive;
            if (u_has_metallic_roughness_map) {
                vec4 mrSample = texture(u_metallic_roughness_map, TexCoord);
                roughness = clamp(mrSample.g * u_material_roughness, 0.04, 1.0);
                metallic = clamp(mrSample.b * u_material_metallic, 0.0, 1.0);
            }
            if (u_has_occlusion_map) {
                ao *= texture(u_occlusion_map, TexCoord).r;
            }
            if (u_has_emissive_map) {
                surface_emissive *= texture(u_emissive_map, TexCoord).rgb;
            }
            vec3 V = normalize(u_camera_pos - FragPos);
            vec3 F0 = vec3(0.04);
            F0 = mix(F0, surface_albedo, metallic);

            vec3 Lo = vec3(0.0);
            
            // 方向光
            {
                vec3 L = normalize(-u_light_direction);
                vec3 H = normalize(V + L);
                float NDF = DistributionGGX(N, H, roughness);
                float G   = GeometrySmith(N, V, L, roughness);
                vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 numerator    = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
                vec3 specular     = numerator / denominator;

                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - metallic;

                float NdotL = max(dot(N, L), 0.0);
                float shadow = ShadowCalculation(FragPos, FragPosViewSpace, N, L);
                Lo += (kD * surface_albedo / PI + specular) * u_light_color * u_light_intensity * NdotL * (1.0 - shadow);
            }
            
            // 点光源
            for(int i = 0; i < u_point_light_count; ++i) {
                vec3 L = normalize(u_point_lights[i].position - FragPos);
                vec3 H = normalize(V + L);
                float distance = length(u_point_lights[i].position - FragPos);
                float attenuation = clamp(1.0 - (distance*distance)/(u_point_lights[i].radius*u_point_lights[i].radius), 0.0, 1.0);
                attenuation *= attenuation;
                vec3 radiance = u_point_lights[i].color * u_point_lights[i].intensity * attenuation;

                float NDF = DistributionGGX(N, H, roughness);
                float G   = GeometrySmith(N, V, L, roughness);
                vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 numerator    = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
                vec3 specular     = numerator / denominator;

                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - metallic;

                float NdotL = max(dot(N, L), 0.0);
                float point_shadow = 0.0;
                bool point_cast_shadow = u_point_lights[i].cast_shadow;
                int point_shadow_index = u_point_lights[i].shadow_index;
                if (point_cast_shadow) {
                    point_shadow = PointShadowCalculation(point_shadow_index, FragPos, u_point_lights[i].position, u_point_lights[i].radius);
                }
                Lo += (kD * surface_albedo / PI + specular) * radiance * NdotL * (1.0 - point_shadow);
            }

            // 聚光灯
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
                float NDF = DistributionGGX(N, H, roughness);
                float G   = GeometrySmith(N, V, L, roughness);
                vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 numerator    = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
                vec3 specular     = numerator / denominator;

                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - metallic;

                float NdotL = max(dot(N, L), 0.0);
                float spot_shadow = 0.0;
                bool spot_cast_shadow = u_spot_lights[i].cast_shadow;
                int spot_shadow_index = u_spot_lights[i].shadow_index;
                if (spot_cast_shadow) {
                    spot_shadow = SpotShadowCalculation(spot_shadow_index, FragPos, N, L);
                }
                Lo += (kD * surface_albedo / PI + specular) * radiance * NdotL * (1.0 - spot_shadow);
            }
            
            // 环境光
            vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);
            vec3 kS_ambient = F;
            vec3 kD_ambient = 1.0 - kS_ambient;
            kD_ambient *= 1.0 - metallic;
            
            vec3 irradiance = vec3(u_ambient_intensity);
            vec3 diffuse_ambient = irradiance * surface_albedo;
            vec3 specular_ambient = irradiance * F0 * (1.0 - roughness);
            
            vec3 ambient = (kD_ambient * diffuse_ambient + specular_ambient) * ao;
            vec3 color = ambient + Lo + surface_emissive;

            color = color / (color + vec3(1.0));
            color = pow(color, vec3(1.0/2.2));

            FragColor = vec4(color, texColor.a * ourColor.a);
        }
    )";

    pbr_shader_handle_ = CompileProgram(vertex_shader, fragment_shader);
    programs_created_ += 1;
    CachePBRLocations();
}

void GLShaderManager::CachePBRLocations() {
    auto& loc = pbr_locations_;

    // --- UBO block 绑定 ---
    // 获取 UBO block index 并绑定到固定 binding point
    loc.per_frame_block_index = glGetUniformBlockIndex(pbr_shader_handle_, "PerFrame");
    if (loc.per_frame_block_index != GL_INVALID_INDEX) {
        glUniformBlockBinding(pbr_shader_handle_, loc.per_frame_block_index, 0);
    }

    loc.per_scene_block_index = glGetUniformBlockIndex(pbr_shader_handle_, "PerScene");
    if (loc.per_scene_block_index != GL_INVALID_INDEX) {
        glUniformBlockBinding(pbr_shader_handle_, loc.per_scene_block_index, 1);
    }

    loc.per_material_block_index = glGetUniformBlockIndex(pbr_shader_handle_, "PerMaterial");
    if (loc.per_material_block_index != GL_INVALID_INDEX) {
        glUniformBlockBinding(pbr_shader_handle_, loc.per_material_block_index, 2);
    }

    // --- 纹理采样器 uniform location ---
    loc.texture = glGetUniformLocation(pbr_shader_handle_, "u_texture");
    loc.normal_map = glGetUniformLocation(pbr_shader_handle_, "u_normal_map");
    loc.metallic_roughness_map = glGetUniformLocation(pbr_shader_handle_, "u_metallic_roughness_map");
    loc.emissive_map = glGetUniformLocation(pbr_shader_handle_, "u_emissive_map");
    loc.occlusion_map = glGetUniformLocation(pbr_shader_handle_, "u_occlusion_map");
    for (int i = 0; i < 3; ++i) {
        std::string sm_name = "u_shadow_maps[" + std::to_string(i) + "]";
        loc.shadow_map[i] = glGetUniformLocation(pbr_shader_handle_, sm_name.c_str());
    }

    // --- CSM 点阴影贴图 ---
    for (int i = 0; i < 4; ++i) {
        std::string point_shadow_name = "u_point_shadow_maps[" + std::to_string(i) + "]";
        glGetUniformLocation(pbr_shader_handle_, point_shadow_name.c_str());
    }

    // --- 聚光灯阴影 ---
    for (int i = 0; i < 4; ++i) {
        std::string shadow_name = "u_spot_shadow_maps[" + std::to_string(i) + "]";
        loc.spot_shadow_map[i] = glGetUniformLocation(pbr_shader_handle_, shadow_name.c_str());
        std::string matrix_name = "u_spot_light_space_matrices[" + std::to_string(i) + "]";
        loc.spot_light_space_matrix[i] = glGetUniformLocation(pbr_shader_handle_, matrix_name.c_str());
    }

    // --- 逐对象 uniform（模型矩阵、骨骼、变形目标） ---
    loc.model = glGetUniformLocation(pbr_shader_handle_, "u_model");
    loc.skinned = glGetUniformLocation(pbr_shader_handle_, "u_skinned");
    loc.bone_matrices = glGetUniformLocation(pbr_shader_handle_, "u_bone_matrices");
    loc.morph_enabled = glGetUniformLocation(pbr_shader_handle_, "u_morph_enabled");
    loc.morph_weights = glGetUniformLocation(pbr_shader_handle_, "u_morph_weights");

    // --- 点光源（暂保留为独立 uniform，struct 数组 std140 布局复杂） ---
    loc.point_light_count = glGetUniformLocation(pbr_shader_handle_, "u_point_light_count");
    for (int i = 0; i < 4; ++i) {
        std::string base = "u_point_lights[" + std::to_string(i) + "].";
        loc.point_lights[i].color = glGetUniformLocation(pbr_shader_handle_, (base + "color").c_str());
        loc.point_lights[i].position = glGetUniformLocation(pbr_shader_handle_, (base + "position").c_str());
        loc.point_lights[i].intensity = glGetUniformLocation(pbr_shader_handle_, (base + "intensity").c_str());
        loc.point_lights[i].radius = glGetUniformLocation(pbr_shader_handle_, (base + "radius").c_str());
        loc.point_lights[i].cast_shadow = glGetUniformLocation(pbr_shader_handle_, (base + "cast_shadow").c_str());
        loc.point_lights[i].shadow_index = glGetUniformLocation(pbr_shader_handle_, (base + "shadow_index").c_str());
    }

    // --- 聚光灯 ---
    loc.spot_light_count = glGetUniformLocation(pbr_shader_handle_, "u_spot_light_count");
    for (int i = 0; i < 4; ++i) {
        std::string base = "u_spot_lights[" + std::to_string(i) + "].";
        loc.spot_lights[i].color = glGetUniformLocation(pbr_shader_handle_, (base + "color").c_str());
        loc.spot_lights[i].position = glGetUniformLocation(pbr_shader_handle_, (base + "position").c_str());
        loc.spot_lights[i].direction = glGetUniformLocation(pbr_shader_handle_, (base + "direction").c_str());
        loc.spot_lights[i].intensity = glGetUniformLocation(pbr_shader_handle_, (base + "intensity").c_str());
        loc.spot_lights[i].radius = glGetUniformLocation(pbr_shader_handle_, (base + "radius").c_str());
        loc.spot_lights[i].inner_cone = glGetUniformLocation(pbr_shader_handle_, (base + "inner_cone").c_str());
        loc.spot_lights[i].outer_cone = glGetUniformLocation(pbr_shader_handle_, (base + "outer_cone").c_str());
        loc.spot_lights[i].cast_shadow = glGetUniformLocation(pbr_shader_handle_, (base + "cast_shadow").c_str());
        loc.spot_lights[i].shadow_index = glGetUniformLocation(pbr_shader_handle_, (base + "shadow_index").c_str());
    }
}

// ============================================================
// 天空盒着色器
// ============================================================

void GLShaderManager::InitSkyboxShader() {
    if (skybox_shader_handle_ != 0) return;

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
    programs_created_ += 1;
    glAttachShader(skybox_shader_handle_, vs);
    glAttachShader(skybox_shader_handle_, fs);
    glLinkProgram(skybox_shader_handle_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    skybox_locations_.view = glGetUniformLocation(skybox_shader_handle_, "view");
    skybox_locations_.projection = glGetUniformLocation(skybox_shader_handle_, "projection");
    skybox_locations_.tex = glGetUniformLocation(skybox_shader_handle_, "skybox");
}

// ============================================================
// 粒子着色器
// ============================================================

void GLShaderManager::InitParticleShader() {
    if (particle_shader_handle_ != 0) return;

    const char* vs_src = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        layout (location = 2) in vec3 iPos;
        layout (location = 3) in vec4 iColor;
        layout (location = 4) in float iSize;

        out vec4 ParticleColor;
        out vec2 TexCoord;

        uniform mat4 u_vp;
        uniform vec3 u_camera_right;
        uniform vec3 u_camera_up;

        void main() {
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

    particle_shader_handle_ = CompileProgram(vs_src, fs_src);
    programs_created_ += 1;
    particle_locations_.vp = glGetUniformLocation(particle_shader_handle_, "u_vp");
    particle_locations_.texture = glGetUniformLocation(particle_shader_handle_, "u_texture");
}

// ============================================================
// 后处理着色器缓存
// ============================================================

unsigned int GLShaderManager::GetOrCreatePostProcessShader(const std::string& effect_name,
                                                            const char* vs_src,
                                                            const std::string& fs_src) {
    auto it = pp_shaders_.find(effect_name);
    if (it != pp_shaders_.end()) {
        return it->second;
    }

    const char* fs_c_str = fs_src.c_str();
    unsigned int shader = CompileProgram(vs_src, fs_c_str);
    programs_created_ += 1;
    pp_shaders_[effect_name] = shader;
    return shader;
}

bool GLShaderManager::HasPostProcessShader(const std::string& effect_name) const {
    return pp_shaders_.find(effect_name) != pp_shaders_.end();
}

// ============================================================
// 清理
// ============================================================

void GLShaderManager::Shutdown() {
    if (pbr_shader_handle_ != 0) {
        glDeleteProgram(pbr_shader_handle_);
        programs_destroyed_ += 1;
        pbr_shader_handle_ = 0;
    }
    if (skybox_shader_handle_ != 0) {
        glDeleteProgram(skybox_shader_handle_);
        programs_destroyed_ += 1;
        skybox_shader_handle_ = 0;
    }
    if (particle_shader_handle_ != 0) {
        glDeleteProgram(particle_shader_handle_);
        programs_destroyed_ += 1;
        particle_shader_handle_ = 0;
    }
    for (auto& [name, handle] : pp_shaders_) {
        if (handle != 0) {
            glDeleteProgram(handle);
            programs_destroyed_ += 1;
        }
    }
    pp_shaders_.clear();
}

} // namespace render
} // namespace dse
