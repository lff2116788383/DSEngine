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
static const unsigned int kAdditiveVariantKey = static_cast<unsigned int>(std::hash<std::string>{}("SPRITE_ADDITIVE"));
static const unsigned int kSdfVariantKey = static_cast<unsigned int>(std::hash<std::string>{}("TEXT_SDF"));

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
    // 通用原语 VAO (A1)
    if (prim_vao_handle_) {
        if (delete_vao_fn_) { delete_vao_fn_(prim_vao_handle_); }
        else { unsigned int r = prim_vao_handle_.raw(); glDeleteVertexArrays(1, &r); }
        prim_vao_handle_ = {};
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
// 通用绘制原语 (A1)
// ============================================================

void GLDrawExecutor::PrimBindShaderProgram(unsigned int program_handle) {
    prim_program_ = program_handle;
    glUseProgram(program_handle);
}

void GLDrawExecutor::PrimBindVertexBuffer(unsigned int buffer_handle, uint32_t stride,
                                          const std::vector<VertexAttr>& attrs) {
    if (!prim_vao_handle_ && create_vao_fn_) {
        prim_vao_handle_ = create_vao_fn_();
    }
    glBindVertexArray(prim_vao_handle_.raw());
    glBindBuffer(GL_ARRAY_BUFFER, buffer_handle);
    for (const auto& a : attrs) {
        glEnableVertexAttribArray(a.location);
        glVertexAttribPointer(a.location, static_cast<GLint>(a.components), GL_FLOAT, GL_FALSE,
                              static_cast<GLsizei>(stride),
                              reinterpret_cast<const void*>(static_cast<uintptr_t>(a.offset)));
    }
}

void GLDrawExecutor::PrimBindTextureCube(unsigned int slot, unsigned int cubemap_handle) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_handle);
    // GLSL sampler uniform 默认绑定到纹理单元 0；spike 仅用 slot 0，无需显式设置 sampler uniform。
}

void GLDrawExecutor::PrimPushConstantsMat4(const glm::mat4& value) {
    if (prim_program_ == 0) return;
    // push_constant 块在 GL 后端被 lower 为名为 "u_vp" 的 uniform。
    GLint loc = glGetUniformLocation(prim_program_, "u_vp");
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}

void GLDrawExecutor::PrimDraw(uint32_t vertex_count, uint32_t first_vertex) {
    glBindVertexArray(prim_vao_handle_.raw());
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
    glBindVertexArray(0);
    global_state_.current_frame_stats.draw_calls += 1;
}

// --- 通用绘制原语 (B0): 索引 / 2D 纹理 / UBO / 索引绘制 ---

void GLDrawExecutor::PrimBindIndexBuffer(unsigned int buffer_handle, IndexType type) {
    // EBO 绑定记录在当前 VAO 状态里，故须先确保 prim VAO 已绑定
    // （SpriteRenderer 先调 BindVertexBuffer 创建/绑定该 VAO）。
    if (!prim_vao_handle_ && create_vao_fn_) {
        prim_vao_handle_ = create_vao_fn_();
    }
    glBindVertexArray(prim_vao_handle_.raw());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_handle);
    prim_index_type_ = (type == IndexType::UInt16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}

void GLDrawExecutor::PrimBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
    GLenum target = GL_TEXTURE_2D;
    switch (dim) {
        case TextureDim::TexCube:    target = GL_TEXTURE_CUBE_MAP;       break;
        case TextureDim::Tex2DArray: target = GL_TEXTURE_2D_ARRAY;       break;
        case TextureDim::Tex2D:      default: target = GL_TEXTURE_2D;     break;
    }
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(target, texture_handle);
    // GLSL sampler 须指向该纹理单元。各内建着色器在初始化时把其采样器绑定到约定单元
    // （见 GLShaderManager::InitSprite2DShader / InitSkyboxShader）。
}

void GLDrawExecutor::PrimBindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                           uint32_t offset, uint32_t size) {
    if (size == 0) {
        glBindBufferBase(GL_UNIFORM_BUFFER, slot, buffer_handle);
    } else {
        glBindBufferRange(GL_UNIFORM_BUFFER, slot, buffer_handle,
                          static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    }
}

void GLDrawExecutor::PrimDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) {
    glBindVertexArray(prim_vao_handle_.raw());
    const size_t elem = (prim_index_type_ == GL_UNSIGNED_SHORT) ? 2u : 4u;
    const void* offset_ptr = reinterpret_cast<const void*>(static_cast<uintptr_t>(first_index) * elem);
    if (base_vertex != 0) {
        glDrawElementsBaseVertex(GL_TRIANGLES, static_cast<GLsizei>(index_count),
                                 prim_index_type_, offset_ptr, base_vertex);
    } else {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count),
                       prim_index_type_, offset_ptr);
    }
    glBindVertexArray(0);
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

    // The GL 2D batch normally draws with the PBR program (shared vertex
    // interface). Capability-driven (not platform #ifdef): on contexts that
    // cannot lower PBR to their dialect (e.g. WebGL2), gl_rhi_device initializes
    // the dedicated ES3.0 2D batch program instead, so prefer it whenever it
    // exists. On desktop this handle stays 0 and the PBR program is used.
    const PBRShaderLocations* loc_ptr = &shader_mgr.pbr_locations();
    unsigned int default_prog = shader_mgr.pbr_shader_handle();
    if (shader_mgr.sprite2d_shader_handle() != 0) {
        loc_ptr = &shader_mgr.sprite2d_locations();
        default_prog = shader_mgr.sprite2d_shader_handle();
    }
    const auto& loc = *loc_ptr;
    const auto& slots = shader_mgr.pbr_texture_slots();
    glUseProgram(default_prog);
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
    const unsigned int& additive_variant_key = kAdditiveVariantKey;
    const unsigned int& sdf_variant_key = kSdfVariantKey;
    bool using_sdf_shader = false;
    bool using_vfx_shader = false;
    SpriteVisualEffect current_vfx;
    glm::vec4 cur_sdf_params = {items[0].sdf_threshold, items[0].sdf_smoothing, items[0].sdf_outline_width, items[0].sdf_shadow_softness};

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

        // SDF shader 切换 + 参数更新
        bool want_sdf = (current_shader_variant == sdf_variant_key);
        if (want_sdf) {
            if (!using_sdf_shader) {
                shader_mgr.InitTextSdfShader();
                if (shader_mgr.text_sdf_shader_handle() != 0) {
                    glUseProgram(shader_mgr.text_sdf_shader_handle());
                    ubo_mgr.BindAll();
                    using_sdf_shader = true;
                }
            }
            if (using_sdf_shader) {
                const auto& sdf_loc = shader_mgr.text_sdf_locations();
                if (sdf_loc.texture >= 0) glUniform1i(sdf_loc.texture, slots.albedo);
                if (sdf_loc.sdf_threshold >= 0) glUniform1f(sdf_loc.sdf_threshold, cur_sdf_params.x);
                if (sdf_loc.sdf_smoothing >= 0) glUniform1f(sdf_loc.sdf_smoothing, cur_sdf_params.y);
                if (sdf_loc.outline_width >= 0) glUniform1f(sdf_loc.outline_width, cur_sdf_params.z);
                if (sdf_loc.shadow_softness >= 0) glUniform1f(sdf_loc.shadow_softness, cur_sdf_params.w);
            }
        } else if (!want_sdf && using_sdf_shader) {
            glUseProgram(default_prog);
            glUniform1i(loc.texture, slots.albedo);
            if (loc.model >= 0) {
                const glm::mat4 identity(1.0f);
                glUniformMatrix4fv(loc.model, 1, GL_FALSE, glm::value_ptr(identity));
            }
            if (loc.skinned >= 0) glUniform1i(loc.skinned, 0);
            if (loc.morph_enabled >= 0) glUniform1i(loc.morph_enabled, 0);
            ubo_mgr.BindAll();
            using_sdf_shader = false;
        }

        if (current_vfx.enabled && !using_vfx_shader && !using_sdf_shader) {
            shader_mgr.InitUIEffectsShader();
            if (shader_mgr.ui_effects_shader_handle() != 0) {
                glUseProgram(shader_mgr.ui_effects_shader_handle());
                ubo_mgr.BindAll();
                using_vfx_shader = true;
            }
        } else if (!current_vfx.enabled && using_vfx_shader) {
            glUseProgram(default_prog);
            glUniform1i(loc.texture, slots.albedo);
            if (loc.model >= 0) {
                const glm::mat4 identity(1.0f);
                glUniformMatrix4fv(loc.model, 1, GL_FALSE, glm::value_ptr(identity));
            }
            if (loc.skinned >= 0) glUniform1i(loc.skinned, 0);
            if (loc.morph_enabled >= 0) glUniform1i(loc.morph_enabled, 0);
            ubo_mgr.BindAll();
            using_vfx_shader = false;
        }

        if (using_vfx_shader) {
            const auto& vfx_loc = shader_mgr.ui_effects_locations();
            if (vfx_loc.gradient_start >= 0) glUniform4fv(vfx_loc.gradient_start, 1, &current_vfx.gradient_start[0]);
            if (vfx_loc.gradient_end >= 0) glUniform4fv(vfx_loc.gradient_end, 1, &current_vfx.gradient_end[0]);
            if (vfx_loc.rect_size_and_radius >= 0) glUniform4f(vfx_loc.rect_size_and_radius,
                current_vfx.rect_size.x, current_vfx.rect_size.y, current_vfx.corner_radius, current_vfx.gradient_direction);
            if (vfx_loc.blur_params >= 0) glUniform4f(vfx_loc.blur_params, current_vfx.blur_radius, current_vfx.blur_intensity, 0.0f, 0.0f);
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
        bool vfx_changed = (item.visual_effect.enabled != current_vfx.enabled);
        glm::vec4 item_sdf = {item.sdf_threshold, item.sdf_smoothing, item.sdf_outline_width, item.sdf_shadow_softness};
        if (tex != current_texture ||
            item.material_instance_id != current_material_instance ||
            item.shader_variant_key != current_shader_variant ||
            item.blend_mode != current_blend_mode ||
            vfx_changed ||
            std::memcmp(&item_sdf, &cur_sdf_params, sizeof(glm::vec4)) != 0 ||
            batch_vertices.size() + 4 > MAX_SPRITE_VERTICES) {
            flush_batch();
            current_texture = tex;
            current_material_instance = item.material_instance_id;
            current_shader_variant = item.shader_variant_key;
            current_blend_mode = item.blend_mode;
            current_vfx = item.visual_effect;
            cur_sdf_params = item_sdf;
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