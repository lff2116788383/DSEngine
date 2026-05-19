/**
 * @file gl_draw_executor_fx.cpp
 * @brief GLDrawExecutor - 3D particles & hair strand rendering (split from gl_draw_executor.cpp)
 */

#include "engine/render/rhi/opengl/gl_draw_executor.h"
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/base/debug.h"
#include "engine/render/rhi/opengl/gl_loader.h"

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace dse {
namespace render {
// ============================================================
// 3D 粒子绘制
// ============================================================

void GLDrawExecutor::DrawParticles3D(const std::vector<Particle3DDrawItem>& items,
                                       const glm::mat4& view,
                                       const glm::mat4& projection,
                                       GLShaderManager& shader_mgr) {
    if (items.empty()) return;

    // 懒初始化粒子着色器
    if (shader_mgr.particle_shader_handle() == 0) {
        shader_mgr.InitParticleShader();
    }

    // 粒子四边形 VAO/VBO
    if (particle_quad_vao_handle_ == 0) {
        float quad_vertices[] = {
             -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
              0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
              0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
             -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
              0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
             -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
        };
        if (create_vao_fn_ && create_buffer_fn_) {
            particle_quad_vao_handle_ = create_vao_fn_();
            particle_quad_vbo_handle_ = create_buffer_fn_(sizeof(quad_vertices), quad_vertices, false, false);
        } else {
            glGenVertexArrays(1, &particle_quad_vao_handle_);
            glGenBuffers(1, &particle_quad_vbo_handle_);
        }
        glBindVertexArray(particle_quad_vao_handle_);
        glBindBuffer(GL_ARRAY_BUFFER, particle_quad_vbo_handle_);
        if (!create_buffer_fn_) {
            glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
        }
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    const auto& p_loc = shader_mgr.particle_locations();
    glUseProgram(shader_mgr.particle_shader_handle());
    // 粒子着色器使用 PerFrame UBO（vp + view），相机方向由着色器从 view 矩阵提取
    glUniform1i(p_loc.texture, 0);

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (const auto& item : items) {
        if (item.particle_count == 0 || item.instance_vbo == 0) continue;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, item.texture_handle == 0 ? white_texture_handle_ : item.texture_handle);

        glBindVertexArray(particle_quad_vao_handle_);
        glBindBuffer(GL_ARRAY_BUFFER, item.instance_vbo);

        size_t stride = 8 * sizeof(float);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glVertexAttribDivisor(2, 1);

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glVertexAttribDivisor(3, 1);

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
        glVertexAttribDivisor(4, 1);

        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, item.particle_count);
        global_state_.current_frame_stats.draw_calls += 1;

        glVertexAttribDivisor(2, 0);
        glVertexAttribDivisor(3, 0);
        glVertexAttribDivisor(4, 0);
        glBindVertexArray(0);
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ============================================================
// Hair Strand 渲染
// ============================================================

static const char* kHairVertSource = R"(
#version 430 core

layout(std430, binding = 0) readonly buffer PositionBuf { vec4 positions[]; };
layout(std430, binding = 3) readonly buffer TangentBuf  { vec4 tangents[]; };

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_world_pos;
out vec3 v_tangent;
out float v_t; // 0=root, 1=tip

void main() {
    vec4 pos = positions[gl_VertexID];
    vec4 tan = tangents[gl_VertexID];

    vec4 world_pos = u_model * vec4(pos.xyz, 1.0);
    v_world_pos = world_pos.xyz;
    v_tangent = normalize(mat3(u_model) * tan.xyz);
    v_t = 1.0 - tan.w; // tangent.w = thickness: 1 at root, 0 at tip 鈫?v_t: 0 at root, 1 at tip

    gl_Position = u_projection * u_view * world_pos;
}
)";

static const char* kHairFragSource = R"(
#version 430 core

in vec3 v_world_pos;
in vec3 v_tangent;
in float v_t;

uniform vec3 u_camera_pos;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform float u_light_intensity;
uniform float u_ambient_intensity;
uniform vec4 u_root_color;
uniform vec4 u_tip_color;
uniform float u_opacity;
uniform float u_spec_primary;
uniform float u_spec_secondary;
uniform float u_spec_strength1;
uniform float u_spec_strength2;
uniform vec3 u_spec_color;

out vec4 FragColor;

// Kajiya-Kay diffuse: sin(T, L)
float KajiyaDiffuse(vec3 T, vec3 L) {
    float TdotL = dot(T, L);
    return sqrt(max(0.0, 1.0 - TdotL * TdotL));
}

// Kajiya-Kay specular: sin(T, L) * sin(T, V) - cos(T, L) * cos(T, V)
float KajiyaSpecular(vec3 T, vec3 L, vec3 V, float power) {
    float TdotL = dot(T, L);
    float TdotV = dot(T, V);
    float sinTL = sqrt(max(0.0, 1.0 - TdotL * TdotL));
    float sinTV = sqrt(max(0.0, 1.0 - TdotV * TdotV));
    float cosAngle = sinTL * sinTV - TdotL * TdotV;
    return pow(max(0.0, cosAngle), power);
}

void main() {
    vec3 T = normalize(v_tangent);
    vec3 L = normalize(-u_light_dir);
    vec3 V = normalize(u_camera_pos - v_world_pos);

    // Interpolate color from root to tip
    float t = clamp(v_t, 0.0, 1.0);
    vec4 hair_color = mix(u_root_color, u_tip_color, t);

    // Kajiya-Kay lighting
    float diffuse = KajiyaDiffuse(T, L);
    float spec1 = KajiyaSpecular(T, L, V, u_spec_primary) * u_spec_strength1;
    float spec2 = KajiyaSpecular(T, L, V, u_spec_secondary) * u_spec_strength2;

    vec3 lit = hair_color.rgb * (u_ambient_intensity + diffuse * u_light_intensity) * u_light_color
             + (spec1 + spec2) * u_spec_color * u_light_color * u_light_intensity;

    FragColor = vec4(lit, hair_color.a * u_opacity);
}
)";

void GLDrawExecutor::DrawHairStrands(const std::vector<HairDrawItem>& items,
                                      const glm::mat4& view,
                                      const glm::mat4& projection,
                                      GLShaderManager& shader_mgr) {
    if (items.empty()) return;
    (void)shader_mgr;

    // 懒初始化 hair shader
    if (hair_shader_handle_ == 0) {
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &kHairVertSource, nullptr);
        glCompileShader(vs);
        GLint ok = 0;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(vs, sizeof(log), nullptr, log);
            DEBUG_LOG_ERROR("[HairDraw] VS compile error: {}", log);
            glDeleteShader(vs);
            return;
        }
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &kHairFragSource, nullptr);
        glCompileShader(fs);
        glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(fs, sizeof(log), nullptr, log);
            DEBUG_LOG_ERROR("[HairDraw] FS compile error: {}", log);
            glDeleteShader(vs);
            glDeleteShader(fs);
            return;
        }
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            DEBUG_LOG_ERROR("[HairDraw] link error: {}", log);
            glDeleteProgram(prog);
            glDeleteShader(vs);
            glDeleteShader(fs);
            return;
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        hair_shader_handle_ = prog;

        // 缓存 uniform locations
        hair_loc_model_   = glGetUniformLocation(prog, "u_model");
        hair_loc_view_    = glGetUniformLocation(prog, "u_view");
        hair_loc_proj_    = glGetUniformLocation(prog, "u_projection");
        hair_loc_cam_     = glGetUniformLocation(prog, "u_camera_pos");
        hair_loc_ldir_    = glGetUniformLocation(prog, "u_light_dir");
        hair_loc_lcol_    = glGetUniformLocation(prog, "u_light_color");
        hair_loc_lint_    = glGetUniformLocation(prog, "u_light_intensity");
        hair_loc_ambient_ = glGetUniformLocation(prog, "u_ambient_intensity");
        hair_loc_root_    = glGetUniformLocation(prog, "u_root_color");
        hair_loc_tip_     = glGetUniformLocation(prog, "u_tip_color");
        hair_loc_opacity_ = glGetUniformLocation(prog, "u_opacity");
        hair_loc_spec1_   = glGetUniformLocation(prog, "u_spec_primary");
        hair_loc_spec2_   = glGetUniformLocation(prog, "u_spec_secondary");
        hair_loc_sstr1_   = glGetUniformLocation(prog, "u_spec_strength1");
        hair_loc_sstr2_   = glGetUniformLocation(prog, "u_spec_strength2");
        hair_loc_scol_    = glGetUniformLocation(prog, "u_spec_color");
    }

    // 懒初始化空 VAO
    if (hair_vao_handle_ == 0) {
        if (create_vao_fn_) {
            hair_vao_handle_ = create_vao_fn_();
        } else {
            glGenVertexArrays(1, &hair_vao_handle_);
        }
    }

    glUseProgram(hair_shader_handle_);
    glBindVertexArray(hair_vao_handle_);

    // 缓存 uniform locations
    glUniformMatrix4fv(hair_loc_view_, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(hair_loc_proj_, 1, GL_FALSE, &projection[0][0]);

    glm::mat4 inv_view = glm::inverse(view);
    glm::vec3 cam_pos(inv_view[3]);
    glUniform3fv(hair_loc_cam_, 1, &cam_pos[0]);

    // 半透明混合 + 混却测试（只读）
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_LINE_SMOOTH);

    for (const auto& item : items) {
        if (item.strand_count == 0 || item.total_vertex_count == 0) continue;
        if (!item.strand_firsts || !item.strand_counts) continue;

        // 发丝宽度（clamped to driver 支持范围）
        float line_w = (std::max)(1.0f, item.fiber_radius * 40.0f);
        glLineWidth(line_w);

        // 绑定 SSBO
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, item.position_ssbo);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, item.tangent_ssbo);

        // Per-instance uniform
        glUniformMatrix4fv(hair_loc_model_, 1, GL_FALSE, &item.world_transform[0][0]);
        glUniform3fv(hair_loc_ldir_,   1, &item.light_direction[0]);
        glUniform3fv(hair_loc_lcol_,   1, &item.light_color[0]);
        glUniform1f(hair_loc_lint_,    item.light_intensity);
        glUniform1f(hair_loc_ambient_, item.ambient_intensity);
        glUniform4fv(hair_loc_root_,   1, &item.root_color[0]);
        glUniform4fv(hair_loc_tip_,    1, &item.tip_color[0]);
        glUniform1f(hair_loc_opacity_, item.opacity);
        glUniform1f(hair_loc_spec1_,   item.specular_primary);
        glUniform1f(hair_loc_spec2_,   item.specular_secondary);
        glUniform1f(hair_loc_sstr1_,   item.specular_strength_primary);
        glUniform1f(hair_loc_sstr2_,   item.specular_strength_secondary);
        glUniform3fv(hair_loc_scol_,   1, &item.specular_color[0]);

        // glMultiDrawArrays: 每个 strand 是一个 GL_LINE_STRIP
        glMultiDrawArrays(GL_LINE_STRIP,
                          item.strand_firsts,
                          item.strand_counts,
                          static_cast<GLsizei>(item.strand_count));

        global_state_.current_frame_stats.draw_calls += 1;
    }

    // 恢复状态
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_LINE_SMOOTH);
    glBindVertexArray(0);
    glUseProgram(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
}
} // namespace render
} // namespace dse