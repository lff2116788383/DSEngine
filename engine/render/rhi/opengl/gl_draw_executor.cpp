/**
 * @file gl_draw_executor.cpp
 * @brief GLDrawExecutor 实现 - 绘制执行器
 */

#include "engine/render/rhi/opengl/gl_draw_executor.h"
#include "engine/render/rhi/opengl/gl_pipeline_state_manager.h"
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/opengl/gl_resource_manager.h"
#include "engine/render/rhi/opengl/ubo_manager.h"
#include "engine/render/rhi/opengl/gl_enum_convert.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/platform/screen.h"
#include "engine/base/debug.h"
#include "engine/render/rhi/opengl/gl_loader.h"

// GL 4.3 SSBO 常量 — glad/gl.h 仅包含 GL 3.3 定义
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <functional>
#include <cstddef>
#include <cmath>
#include <cstring>

constexpr size_t MAX_SPRITES = 10000;
constexpr size_t MAX_SPRITE_VERTICES = MAX_SPRITES * 4;
constexpr size_t MAX_SPRITE_INDICES = MAX_SPRITES * 6;
constexpr size_t MAX_MESH_VERTICES = 131072;
constexpr size_t MAX_MESH_INDICES = 262144;

namespace dse {
namespace render {

// ============================================================
// 几何缓冲区初始化
// ============================================================

void GLDrawExecutor::InitGeometryBuffers(InitCreateVaoFn create_vao_fn,
                                          InitCreateVboFn create_vbo_fn,
                                          InitUpdateVboFn update_vbo_fn) {
    // 构建精灵索引模板
    std::vector<unsigned short> indices(MAX_SPRITE_INDICES);
    for (size_t i = 0, j = 0; i < MAX_SPRITES; ++i) {
        indices[j++] = static_cast<unsigned short>(i * 4 + 0);
        indices[j++] = static_cast<unsigned short>(i * 4 + 1);
        indices[j++] = static_cast<unsigned short>(i * 4 + 2);
        indices[j++] = static_cast<unsigned short>(i * 4 + 2);
        indices[j++] = static_cast<unsigned short>(i * 4 + 3);
        indices[j++] = static_cast<unsigned short>(i * 4 + 0);
    }

    std::vector<BatchVertex> vertices(MAX_SPRITE_VERTICES);

    // 2D 精灵批处理 VAO
    vao_handle_ = create_vao_fn();
    glBindVertexArray(vao_handle_.raw());
    vbo_handle_ = create_vbo_fn(vertices.size() * sizeof(BatchVertex), vertices.data(), true, false);
    ebo_handle_ = create_vbo_fn(indices.size() * sizeof(unsigned short), indices.data(), false, true);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_handle_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, uv)));
    glBindVertexArray(0);
    // ebo 生命周期随 vao，这里不需要单独存储

    // 3D 网格 VAO
    mesh_vbo_handle_ = create_vbo_fn(MAX_MESH_VERTICES * sizeof(BatchVertex), nullptr, true, false);
    mesh_ibo_handle_ = create_vbo_fn(MAX_MESH_INDICES * sizeof(unsigned short), nullptr, true, true);
    mesh_vao_handle_ = create_vao_fn();
    glBindVertexArray(mesh_vao_handle_.raw());
    glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo_handle_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ibo_handle_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, normal)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, tangent)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, weights)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, joints)));
    glBindVertexArray(0);

    // GPU Instancing: instance VBO（初始 256 个实例 = 16KB）
    {
        constexpr size_t kInitialInstanceCapacity = 256;
        instance_vbo_capacity_ = kInitialInstanceCapacity * sizeof(glm::mat4);
        if (create_buffer_fn_) {
            instance_vbo_handle_ = create_buffer_fn_(instance_vbo_capacity_, nullptr, true, false);
        } else {
            glGenBuffers(1, &instance_vbo_handle_);
            glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_handle_);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instance_vbo_capacity_), nullptr, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }

    // 白色 1x1 默认纹理
    unsigned char white_texture[] = {255, 255, 255, 255};
    if (create_texture_fn_) {
        white_texture_handle_ = create_texture_fn_(1, 1, white_texture, false);
    } else {
        glGenTextures(1, &white_texture_handle_);
        glBindTexture(GL_TEXTURE_2D, white_texture_handle_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_texture);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void GLDrawExecutor::ShutdownGeometryBuffers() {
    // 白色纹理
    if (white_texture_handle_ != 0) {
        if (delete_texture_fn_) { delete_texture_fn_(white_texture_handle_); }
        else { glDeleteTextures(1, &white_texture_handle_); }
        white_texture_handle_ = 0;
    }
    // Static Mesh VBO 缓存清理
    for (auto& [key, entry] : static_mesh_cache_) {
        if (entry.vao) { glDeleteVertexArrays(1, &entry.vao); }
        if (entry.vbo) { glDeleteBuffers(1, &entry.vbo); }
        if (entry.ibo) { glDeleteBuffers(1, &entry.ibo); }
    }
    static_mesh_cache_.clear();
    // GPU Instancing VBO
    if (instance_vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(instance_vbo_handle_); }
        else { glDeleteBuffers(1, &instance_vbo_handle_); }
        instance_vbo_handle_ = 0;
        instance_vbo_capacity_ = 0;
    }
    // 3D 网格缓冲
    if (mesh_vao_handle_) {
        if (delete_vao_fn_) { delete_vao_fn_(mesh_vao_handle_); }
        else { unsigned int r = mesh_vao_handle_.raw(); glDeleteVertexArrays(1, &r); }
        mesh_vao_handle_ = {};
    }
    if (mesh_vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(mesh_vbo_handle_); }
        else { glDeleteBuffers(1, &mesh_vbo_handle_); }
        mesh_vbo_handle_ = 0;
    }
    if (mesh_ibo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(mesh_ibo_handle_); }
        else { glDeleteBuffers(1, &mesh_ibo_handle_); }
        mesh_ibo_handle_ = 0;
    }
    if (bone_ssbo_ != 0) {
        glDeleteBuffers(1, &bone_ssbo_);
        bone_ssbo_ = 0;
        bone_ssbo_capacity_ = 0;
    }
    if (skinned_inst_ssbo_ != 0) {
        glDeleteBuffers(1, &skinned_inst_ssbo_);
        skinned_inst_ssbo_ = 0;
        skinned_inst_ssbo_capacity_ = 0;
    }
    // 2D 精灵缓冲
    if (vao_handle_) {
        if (delete_vao_fn_) { delete_vao_fn_(vao_handle_); }
        else { unsigned int r = vao_handle_.raw(); glDeleteVertexArrays(1, &r); }
        vao_handle_ = {};
    }
    if (vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(vbo_handle_); }
        else { glDeleteBuffers(1, &vbo_handle_); }
        vbo_handle_ = 0;
    }
    if (ebo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(ebo_handle_); }
        else { glDeleteBuffers(1, &ebo_handle_); }
        ebo_handle_ = 0;
    }
    // 天空盒缓冲
    if (skybox_vao_handle_) {
        if (delete_vao_fn_) { delete_vao_fn_(skybox_vao_handle_); }
        else { unsigned int r = skybox_vao_handle_.raw(); glDeleteVertexArrays(1, &r); }
        skybox_vao_handle_ = {};
    }
    if (skybox_vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(skybox_vbo_handle_); }
        else { glDeleteBuffers(1, &skybox_vbo_handle_); }
        skybox_vbo_handle_ = 0;
    }
    // 后处理全屏四边形
    if (pp_vao_handle_) {
        if (delete_vao_fn_) { delete_vao_fn_(pp_vao_handle_); }
        else { unsigned int r = pp_vao_handle_.raw(); glDeleteVertexArrays(1, &r); }
        pp_vao_handle_ = {};
    }
    if (pp_vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(pp_vbo_handle_); }
        else { glDeleteBuffers(1, &pp_vbo_handle_); }
        pp_vbo_handle_ = 0;
    }
    if (pp_param_ubo_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(pp_param_ubo_); }
        else { glDeleteBuffers(1, &pp_param_ubo_); }
        pp_param_ubo_ = 0;
    }
    // 3D 粒子四边形
    if (particle_quad_vao_handle_) {
        if (delete_vao_fn_) { delete_vao_fn_(particle_quad_vao_handle_); }
        else { unsigned int r = particle_quad_vao_handle_.raw(); glDeleteVertexArrays(1, &r); }
        particle_quad_vao_handle_ = {};
    }
    if (particle_quad_vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(particle_quad_vbo_handle_); }
        else { glDeleteBuffers(1, &particle_quad_vbo_handle_); }
        particle_quad_vbo_handle_ = 0;
    }

    // 毛发渲染资源
    if (hair_vao_handle_) {
        if (delete_vao_fn_) { delete_vao_fn_(hair_vao_handle_); }
        else { unsigned int r = hair_vao_handle_.raw(); glDeleteVertexArrays(1, &r); }
        hair_vao_handle_ = {};
    }
    if (hair_shader_handle_ != 0) {
        glDeleteProgram(hair_shader_handle_);
        hair_shader_handle_ = 0;
        hair_loc_model_ = hair_loc_view_ = hair_loc_proj_ = -1;
        hair_loc_cam_ = hair_loc_ldir_ = hair_loc_lcol_ = -1;
        hair_loc_lint_ = hair_loc_ambient_ = -1;
        hair_loc_root_ = hair_loc_tip_ = hair_loc_opacity_ = -1;
        hair_loc_spec1_ = hair_loc_spec2_ = -1;
        hair_loc_sstr1_ = hair_loc_sstr2_ = hair_loc_scol_ = -1;
    }

    active_render_target_ = 0;
}

// ============================================================
// Static Mesh VBO 缓存
// ============================================================

GLDrawExecutor::StaticMeshEntry GLDrawExecutor::CreateStaticMeshVAO(
    const BatchVertex* vtx_data, size_t vtx_count,
    const uint32_t* idx_data, size_t idx_count) {
    StaticMeshEntry entry;
    entry.vtx_count = vtx_count;
    entry.idx_count = idx_count;

    glGenVertexArrays(1, &entry.vao);
    glGenBuffers(1, &entry.vbo);
    glGenBuffers(1, &entry.ibo);

    glBindVertexArray(entry.vao);

    glBindBuffer(GL_ARRAY_BUFFER, entry.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vtx_count * sizeof(BatchVertex)),
                 vtx_data, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(idx_count * sizeof(uint32_t)),
                 idx_data, GL_STATIC_DRAW);

    // 属性布局与 InitGeometryBuffers 中 mesh_vao_handle_ 完全一致
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex),
                          reinterpret_cast<const void*>(offsetof(BatchVertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex),
                          reinterpret_cast<const void*>(offsetof(BatchVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex),
                          reinterpret_cast<const void*>(offsetof(BatchVertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex),
                          reinterpret_cast<const void*>(offsetof(BatchVertex, normal)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex),
                          reinterpret_cast<const void*>(offsetof(BatchVertex, tangent)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex),
                          reinterpret_cast<const void*>(offsetof(BatchVertex, weights)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex),
                          reinterpret_cast<const void*>(offsetof(BatchVertex, joints)));

    glBindVertexArray(0);
    return entry;
}

// ============================================================
// RenderPass
// ============================================================

void GLDrawExecutor::BeginRenderPass(const RenderPassDesc& render_pass,
                                       GLResourceManager& resource_mgr) {
    bool has_depth = false;
    if (render_pass.render_target == 0) {
        if (active_render_target_ != 0) {
            auto active_rt = resource_mgr.GetRenderTarget(active_render_target_);
            if (active_rt) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }
        active_render_target_ = 0;
        glViewport(0, 0, Screen::width(), Screen::height());
    } else {
        auto rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (rt) {
            if (active_render_target_ != 0 && active_render_target_ != render_pass.render_target) {
                auto active_rt = resource_mgr.GetRenderTarget(active_render_target_);
                if (active_rt) {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo_handle);
            glViewport(0, 0, rt->desc.width, rt->desc.height);
            active_render_target_ = render_pass.render_target;
            has_depth = rt->desc.has_depth;
        }
    }
    global_state_.current_frame_stats.render_passes += 1;
    is_depth_only_pass_ = false;
    if (render_pass.render_target != 0) {
        auto stat_rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (stat_rt && !stat_rt->desc.has_color && stat_rt->desc.has_depth) {
            global_state_.current_frame_stats.shadow_passes += 1;
            is_depth_only_pass_ = true;
        }
    }
    if (render_pass.clear_color_enabled) {
        glClearColor(render_pass.clear_color.r, render_pass.clear_color.g, render_pass.clear_color.b, render_pass.clear_color.a);
        if (has_depth) {
            // 确保 depth mask 开启：glClear(GL_DEPTH_BUFFER_BIT) 受 glDepthMask 控制；
            // 若上一帧 composite/present pass 关闭了 depth write（glDepthMask(GL_FALSE)），
            // 此处 clear 将静默失效，导致 depth buffer 残留旧值而非 1.0。
            glDepthMask(GL_TRUE);
        }
        glClearDepth(1.0);
        glClear(has_depth ? (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) : GL_COLOR_BUFFER_BIT);
    } else if (has_depth) {
        glDepthMask(GL_TRUE);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

#ifdef DSE_VSE_1522_DIAG
    // VSE 15.22 深度诊断：scene pass clear 后验证 depth buffer 初始值
    static int vse1522_beginpass_diag_frames = 0;
    if (vse1522_beginpass_diag_frames < 5 && has_depth && render_pass.render_target != 0) {
        auto diag_rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (diag_rt && diag_rt->desc.has_depth && diag_rt->desc.has_color) {
            // 只对同时有 color+depth 的 render target（即 scene pass）做诊断
            float post_clear_depth = -1.0f;
            glReadPixels(Screen::width() / 2, Screen::height() / 2, 1, 1,
                         GL_DEPTH_COMPONENT, GL_FLOAT, &post_clear_depth);
            GLint fbo = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
            GLint depth_type = 0;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &depth_type);
            GLint depth_bits = 0;
            if (depth_type != GL_NONE) {
                glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                    GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depth_bits);
            }
            GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            DEBUG_LOG_INFO("[3D][VSE15.22][DepthDiag][BeginRenderPass] frame={} rt_handle={} fbo={} has_depth={} depth_attachment_type={} depth_bits={} fb_status={} post_clear_depth={} expected=1.0",
                           vse1522_beginpass_diag_frames,
                           render_pass.render_target,
                           fbo,
                           has_depth ? 1 : 0,
                           depth_type,
                           depth_bits,
                           static_cast<unsigned int>(fb_status),
                           post_clear_depth);
            ++vse1522_beginpass_diag_frames;
        }
    }
#endif // DSE_VSE_1522_DIAG
}

void GLDrawExecutor::EndRenderPass(GLResourceManager& resource_mgr) {
    if (active_render_target_ != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        active_render_target_ = 0;
    }
}

void GLDrawExecutor::ClearColor(const glm::vec4& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

// ============================================================
// 3D PBR 网格绘制
// ============================================================

// ============================================================
// 天空盒绘制
// ============================================================

void GLDrawExecutor::DrawSkybox(unsigned int cubemap_texture_handle,
                                  const glm::mat4& view,
                                  const glm::mat4& projection,
                                  GLShaderManager& shader_mgr) {
    if (cubemap_texture_handle == 0) return;

    // 懒初始化天空盒着色器和几何
    if (shader_mgr.skybox_shader_handle() == 0) {
        shader_mgr.InitSkyboxShader();

        float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
        };

        if (create_vao_fn_ && create_buffer_fn_) {
            skybox_vao_handle_ = create_vao_fn_();
            skybox_vbo_handle_ = create_buffer_fn_(sizeof(skyboxVertices), skyboxVertices, false, false);
            glBindVertexArray(skybox_vao_handle_.raw());
            glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo_handle_);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glBindVertexArray(0);
        }
    }

    const auto& skybox_loc = shader_mgr.skybox_locations();
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glUseProgram(shader_mgr.skybox_shader_handle());
    // 合成 VP 矩阵：移除平移分量后乘以投影
    glm::mat4 skybox_view = glm::mat4(glm::mat3(view));
    glm::mat4 vp = projection * skybox_view;
    glUniformMatrix4fv(skybox_loc.vp, 1, GL_FALSE, glm::value_ptr(vp));

    glBindVertexArray(skybox_vao_handle_.raw());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture_handle);
    glUniform1i(skybox_loc.tex, 0);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    global_state_.current_frame_stats.draw_calls += 1;
}

// ============================================================
// 后处理绘制
// ============================================================

// ============================================================
// 2D 精灵批处理绘制
// ============================================================

void GLDrawExecutor::DrawBatch(const std::vector<SpriteDrawItem>& items,
                                 const glm::mat4& view,
                                 const glm::mat4& projection,
                                 GLPipelineStateManager& state_mgr,
                                 GLShaderManager& shader_mgr,
                                 UBOManager& ubo_mgr) {
    if (items.empty()) return;
    global_state_.current_frame_stats.sprite_count += static_cast<int>(items.size());

    glm::mat4 vp = projection * view;
    glm::mat4 inv_view = glm::inverse(view);

    // === PerFrame UBO ===
    PerFrameUBO per_frame;
    per_frame.vp = vp;
    per_frame.view = view;
    per_frame.camera_pos = glm::vec4(inv_view[3][0], inv_view[3][1], inv_view[3][2], 0.0f);
    ubo_mgr.UploadPerFrame(per_frame);

    // 2D 精灵不需要光照/材质，填充默认值
    PerSceneUBO per_scene{};
    per_scene.light_dir_and_enabled.w = 0.0f;  // lighting_enabled = false
    per_scene.light_params.w = 1.0f;  // skip_tonemapping = true (UI sprites are already sRGB)
    ubo_mgr.UploadPerScene(per_scene);

    PerMaterialUBO per_mat{};
    per_mat.albedo = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    per_mat.roughness_ao = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    ubo_mgr.UploadPerMaterial(per_mat);

    ubo_mgr.BindAll();

    const auto& loc = shader_mgr.pbr_locations();
    const auto& slots = shader_mgr.pbr_texture_slots();
    glUseProgram(shader_mgr.pbr_shader_handle());
    if (state_mgr.active_pipeline_state() != 0) {
        state_mgr.ApplyState(state_mgr.active_pipeline_state());
    } else {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    glUniform1i(loc.texture, slots.albedo);
    if (loc.model >= 0) {
        const glm::mat4 identity_model(1.0f);
        glUniformMatrix4fv(loc.model, 1, GL_FALSE, glm::value_ptr(identity_model));
    }
    if (loc.skinned >= 0) {
        glUniform1i(loc.skinned, 0);
    }
    if (loc.morph_enabled >= 0) {
        glUniform1i(loc.morph_enabled, 0);
    }

    std::vector<BatchVertex> batch_vertices;
    batch_vertices.reserve(MAX_SPRITE_VERTICES);

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
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    };
    apply_blend(current_blend_mode, current_shader_variant);

    auto flush_batch = [&]() {
        if (batch_vertices.empty()) return;
        int batch_sprites = static_cast<int>(batch_vertices.size() / 4);
        global_state_.current_frame_stats.draw_calls += 1;
        global_state_.current_frame_stats.max_batch_sprites = std::max(global_state_.current_frame_stats.max_batch_sprites, batch_sprites);

        if (update_buffer_fn_) {
            update_buffer_fn_(vbo_handle_, 0, batch_vertices.size() * sizeof(BatchVertex), batch_vertices.data(), false);
        }

        apply_blend(current_blend_mode, current_shader_variant);
        glActiveTexture(GL_TEXTURE0 + slots.albedo);
        glBindTexture(GL_TEXTURE_2D, current_texture);
        glBindVertexArray(vao_handle_.raw());
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
            batch_vertices.size() + 4 > MAX_SPRITE_VERTICES) {
            flush_batch();
            current_texture = tex;
            current_material_instance = item.material_instance_id;
            current_shader_variant = item.shader_variant_key;
            current_blend_mode = item.blend_mode;
        }

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

// ============================================================
// 帧生命周期
// ============================================================

void GLDrawExecutor::BeginFrame() {
    global_state_.current_frame_stats = {};
    bone_ssbo_uploaded_this_frame_ = false;
    inst_ssbo_uploaded_this_frame_ = false;
}

void GLDrawExecutor::EndFrame() {
    global_state_.last_frame_stats = global_state_.current_frame_stats;
    glFlush();
}

} // namespace render
} // namespace dse