/**
 * @file gl_draw_executor_fx.cpp
 * @brief GLDrawExecutor - 3D particles & hair strand rendering (split from gl_draw_executor.cpp)
 */

#include "engine/render/rhi/opengl/gl_draw_executor.h"
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/base/debug.h"
#include "engine/render/rhi/opengl/gl_loader.h"
#include "engine/render/shaders/generated/embed/hair_vert.gen.h"
#include "engine/render/shaders/generated/embed/hair_frag.gen.h"

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
// Hair Strand 渲染
// ============================================================
using namespace dse::render::generated_shaders;

void GLDrawExecutor::DrawHairStrands(const std::vector<HairDrawItem>& items,
                                      const glm::mat4& view,
                                      const glm::mat4& projection,
                                      GLShaderManager& shader_mgr) {
    if (items.empty()) return;
    (void)shader_mgr;

    // 懒初始化 hair shader
    if (hair_shader_handle_ == 0) {
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        const char* hair_vs_src = khair_vert_glsl430;
        glShaderSource(vs, 1, &hair_vs_src, nullptr);
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
        const char* hair_fs_src = khair_frag_glsl430;
        glShaderSource(fs, 1, &hair_fs_src, nullptr);
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
    if (!hair_vao_handle_) {
        if (create_vao_fn_) {
            hair_vao_handle_ = create_vao_fn_();
        } else {
            unsigned int hv = 0; glGenVertexArrays(1, &hv); hair_vao_handle_ = VertexArrayHandle{hv};
        }
    }

    glUseProgram(hair_shader_handle_);
    glBindVertexArray(hair_vao_handle_.raw());

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
#if !DSE_GL_ES_RUNTIME
    glEnable(GL_LINE_SMOOTH);  // GLES 无线条抗锯齿，移动端忽略
#endif

    for (const auto& item : items) {
        if (item.strand_count == 0 || item.total_vertex_count == 0) continue;
        if (!item.strand_firsts || !item.strand_counts) continue;

        // 发丝宽度（clamped to driver 支持范围）
        float line_w = (std::max)(1.0f, item.fiber_radius * 40.0f);
        glLineWidth(line_w);

        // 绑定 SSBO
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, item.position_ssbo.raw());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, item.tangent_ssbo.raw());

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

        // 每个 strand 是一个 GL_LINE_STRIP
#if DSE_GL_ES_RUNTIME
        // GLES 无 glMultiDrawArrays：逐 strand 调用 glDrawArrays 回退。
        for (GLsizei s = 0; s < static_cast<GLsizei>(item.strand_count); ++s)
            glDrawArrays(GL_LINE_STRIP, item.strand_firsts[s], item.strand_counts[s]);
#else
        glMultiDrawArrays(GL_LINE_STRIP,
                          item.strand_firsts,
                          item.strand_counts,
                          static_cast<GLsizei>(item.strand_count));
#endif

        global_state_.current_frame_stats.draw_calls += 1;
    }

    // 恢复状态
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
#if !DSE_GL_ES_RUNTIME
    glDisable(GL_LINE_SMOOTH);
#endif
    glBindVertexArray(0);
    glUseProgram(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
}
} // namespace render
} // namespace dse