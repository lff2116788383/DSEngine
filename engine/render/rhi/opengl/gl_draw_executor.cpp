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

// glDrawElementsInstancedBaseVertexBaseInstance 属 GL 4.2，而桌面 gl_loader.h 走 <glad/gl.h>
// 仅加载 GL 3.3 core，未声明该符号；与 gl_rhi_device.cpp 一致，首次使用时手动解析其入口。
// GLES（Android/Web）无此入口，恒回退 BaseVertex 路径。
using PFN_DrawElementsInstancedBaseVertexBaseInstance =
    void(GLAD_API_PTR*)(GLenum, GLsizei, GLenum, const void*, GLsizei, GLint, GLuint);
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#if defined(_WIN32)
extern "C" __declspec(dllimport) void* __stdcall wglGetProcAddress(const char*);
#endif
static PFN_DrawElementsInstancedBaseVertexBaseInstance ResolveDrawBaseVertexBaseInstance() {
    static PFN_DrawElementsInstancedBaseVertexBaseInstance pfn =
        []() -> PFN_DrawElementsInstancedBaseVertexBaseInstance {
#if defined(_WIN32)
            return reinterpret_cast<PFN_DrawElementsInstancedBaseVertexBaseInstance>(
                wglGetProcAddress("glDrawElementsInstancedBaseVertexBaseInstance"));
#else
            return nullptr;  // 非 Windows 桌面：缺平台 GL proc 解析，回退 BaseVertex 路径
#endif
        }();
    return pfn;
}
#endif

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
    global_state_.current_pass_depth_only = is_depth_only_pass_;
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
    is_depth_only_pass_ = false;
    global_state_.current_pass_depth_only = false;
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

void GLDrawExecutor::PrimSetTopology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::LineStrip: prim_topology_ = GL_LINE_STRIP; break;
        case PrimitiveTopology::LineList:  prim_topology_ = GL_LINES;      break;
        case PrimitiveTopology::PointList: prim_topology_ = GL_POINTS;     break;
        case PrimitiveTopology::TriangleList:
        default:                           prim_topology_ = GL_TRIANGLES;  break;
    }
}

void GLDrawExecutor::PrimDraw(uint32_t vertex_count, uint32_t first_vertex) {
    // vertexless（毛发：无 VB/IB，gl_VertexID 取 SSBO）须保证有非零 VAO（core profile 不允许默认 0）。
    if (!prim_vao_handle_ && create_vao_fn_) {
        prim_vao_handle_ = create_vao_fn_();
    }
    glBindVertexArray(prim_vao_handle_.raw());
    glDrawArrays(prim_topology_, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
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
        case TextureDim::Tex3D:      target = GL_TEXTURE_3D;             break;
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

void GLDrawExecutor::PrimBindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                                           uint32_t offset, uint32_t size) {
    // 图形阶段 SSBO 绑定到 GL_SHADER_STORAGE_BUFFER 的 binding=slot；size!=0 绑定子区间。
    if (size == 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, buffer_handle);
    } else {
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, slot, buffer_handle,
                          static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    }
}

void GLDrawExecutor::PrimDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) {
    glBindVertexArray(prim_vao_handle_.raw());
    const size_t elem = (prim_index_type_ == GL_UNSIGNED_SHORT) ? 2u : 4u;
    const void* offset_ptr = reinterpret_cast<const void*>(static_cast<uintptr_t>(first_index) * elem);
    if (base_vertex != 0) {
        glDrawElementsBaseVertex(prim_topology_, static_cast<GLsizei>(index_count),
                                 prim_index_type_, offset_ptr, base_vertex);
    } else {
        glDrawElements(prim_topology_, static_cast<GLsizei>(index_count),
                       prim_index_type_, offset_ptr);
    }
    glBindVertexArray(0);
    global_state_.current_frame_stats.draw_calls += 1;
}

void GLDrawExecutor::PrimDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                              uint32_t first_index, int32_t base_vertex,
                                              uint32_t first_instance) {
    glBindVertexArray(prim_vao_handle_.raw());
    const size_t elem = (prim_index_type_ == GL_UNSIGNED_SHORT) ? 2u : 4u;
    const void* offset_ptr = reinterpret_cast<const void*>(static_cast<uintptr_t>(first_index) * elem);
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    static const auto pfn_draw_base_instance = ResolveDrawBaseVertexBaseInstance();
#else
    constexpr PFN_DrawElementsInstancedBaseVertexBaseInstance pfn_draw_base_instance = nullptr;
#endif
    if (first_instance != 0 && pfn_draw_base_instance) {
        pfn_draw_base_instance(
            prim_topology_, static_cast<GLsizei>(index_count), prim_index_type_, offset_ptr,
            static_cast<GLsizei>(instance_count), base_vertex, first_instance);
    } else {
        // first_instance==0（或驱动无 baseInstance）：实例数据偏移由 SSBO/instance VB 偏移表达。
        glDrawElementsInstancedBaseVertex(
            prim_topology_, static_cast<GLsizei>(index_count), prim_index_type_, offset_ptr,
            static_cast<GLsizei>(instance_count), base_vertex);
    }
    glBindVertexArray(0);
    global_state_.current_frame_stats.draw_calls += 1;
    global_state_.current_frame_stats.instanced_draw_calls += 1;
}

// ============================================================
// 后处理绘制
// ============================================================

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