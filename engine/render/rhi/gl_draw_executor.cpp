/**
 * @file gl_draw_executor.cpp
 * @brief GLDrawExecutor 实现 - 绘制执行器
 */

#include "engine/render/rhi/gl_draw_executor.h"
#include "engine/render/rhi/gl_pipeline_state_manager.h"
#include "engine/render/rhi/gl_shader_manager.h"
#include "engine/render/rhi/gl_resource_manager.h"
#include "engine/render/rhi/ubo_manager.h"
#include "engine/render/rhi/gl_enum_convert.h"
#include "engine/platform/screen.h"
#include "engine/base/debug.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <functional>
#include <cstddef>
#include <cmath>
#include <limits>
#include <sstream>

constexpr size_t MAX_SPRITES = 10000;
constexpr size_t MAX_SPRITE_VERTICES = MAX_SPRITES * 4;
constexpr size_t MAX_SPRITE_INDICES = MAX_SPRITES * 6;
constexpr size_t MAX_MESH_VERTICES = 131072;
constexpr size_t MAX_MESH_INDICES = 262144;

namespace dse {
namespace render {
namespace {

#ifdef DSE_VSE_1522_DIAG

float SignedArea2D(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return 0.5f * ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
}

std::string FormatVec4ForDiag(const glm::vec4& value) {
    std::ostringstream oss;
    oss << "(" << value.x << "," << value.y << "," << value.z << "," << value.w << ")";
    return oss.str();
}

void LogVse1522OceanPlaneTriangleDiagnostics(int frame,
                                             const MeshDrawItem& item,
                                             const glm::mat4& clip_matrix) {
    if (item.debug_label != "OceanPlane") {
        return;
    }
    const std::size_t triangle_count = item.indices.size() / 3;
    for (std::size_t tri = 0; tri < triangle_count; ++tri) {
        const unsigned short i0 = item.indices[tri * 3 + 0];
        const unsigned short i1 = item.indices[tri * 3 + 1];
        const unsigned short i2 = item.indices[tri * 3 + 2];
        if (i0 >= item.vertices.size() || i1 >= item.vertices.size() || i2 >= item.vertices.size()) {
            continue;
        }
        const glm::vec4 c0 = clip_matrix * glm::vec4(item.vertices[i0].pos, 1.0f);
        const glm::vec4 c1 = clip_matrix * glm::vec4(item.vertices[i1].pos, 1.0f);
        const glm::vec4 c2 = clip_matrix * glm::vec4(item.vertices[i2].pos, 1.0f);
        const bool w0_valid = c0.w > 1e-6f;
        const bool w1_valid = c1.w > 1e-6f;
        const bool w2_valid = c2.w > 1e-6f;
        const int valid_w = (w0_valid ? 1 : 0) + (w1_valid ? 1 : 0) + (w2_valid ? 1 : 0);
        const auto safe_ndc = [](const glm::vec4& clip) -> glm::vec3 {
            if (std::abs(clip.w) < 1e-6f) {
                return glm::vec3(0.0f);
            }
            return glm::vec3(clip) / clip.w;
        };
        const glm::vec3 n0 = safe_ndc(c0);
        const glm::vec3 n1 = safe_ndc(c1);
        const glm::vec3 n2 = safe_ndc(c2);
        const float signed_area = SignedArea2D(glm::vec2(n0), glm::vec2(n1), glm::vec2(n2));
        const float ndc_z_min = std::min(n0.z, std::min(n1.z, n2.z));
        const float ndc_z_max = std::max(n0.z, std::max(n1.z, n2.z));
        const bool crosses_near = (c0.z < -c0.w) || (c1.z < -c1.w) || (c2.z < -c2.w);
        const bool crosses_far = (c0.z > c0.w) || (c1.z > c1.w) || (c2.z > c2.w);
        DEBUG_LOG_INFO("[3D][VSE15.22][DepthDiag][OceanPlaneTri] frame={} tri={} indices=({},{},{}) valid_w={} w=({},{},{}) clip0={} clip1={} clip2={} ndc_z_min={} ndc_z_max={} signed_area_ndc={} crosses_near={} crosses_far={}",
                       frame,
                       tri,
                       i0,
                       i1,
                       i2,
                       valid_w,
                       c0.w,
                       c1.w,
                       c2.w,
                       FormatVec4ForDiag(c0),
                       FormatVec4ForDiag(c1),
                       FormatVec4ForDiag(c2),
                       ndc_z_min,
                       ndc_z_max,
                       signed_area,
                       crosses_near,
                       crosses_far);
    }
}

#endif // DSE_VSE_1522_DIAG

} // namespace

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
    glBindVertexArray(vao_handle_);
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
    glBindVertexArray(mesh_vao_handle_);
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
    // 3D 网格缓冲
    if (mesh_vao_handle_ != 0) {
        if (delete_vao_fn_) { delete_vao_fn_(mesh_vao_handle_); }
        else { glDeleteVertexArrays(1, &mesh_vao_handle_); }
        mesh_vao_handle_ = 0;
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
    // 2D 精灵缓冲
    if (vao_handle_ != 0) {
        if (delete_vao_fn_) { delete_vao_fn_(vao_handle_); }
        else { glDeleteVertexArrays(1, &vao_handle_); }
        vao_handle_ = 0;
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
    if (skybox_vao_handle_ != 0) {
        if (delete_vao_fn_) { delete_vao_fn_(skybox_vao_handle_); }
        else { glDeleteVertexArrays(1, &skybox_vao_handle_); }
        skybox_vao_handle_ = 0;
    }
    if (skybox_vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(skybox_vbo_handle_); }
        else { glDeleteBuffers(1, &skybox_vbo_handle_); }
        skybox_vbo_handle_ = 0;
    }
    // 后处理全屏四边形
    if (pp_vao_handle_ != 0) {
        if (delete_vao_fn_) { delete_vao_fn_(pp_vao_handle_); }
        else { glDeleteVertexArrays(1, &pp_vao_handle_); }
        pp_vao_handle_ = 0;
    }
    if (pp_vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(pp_vbo_handle_); }
        else { glDeleteBuffers(1, &pp_vbo_handle_); }
        pp_vbo_handle_ = 0;
    }
    // 3D 粒子四边形
    if (particle_quad_vao_handle_ != 0) {
        if (delete_vao_fn_) { delete_vao_fn_(particle_quad_vao_handle_); }
        else { glDeleteVertexArrays(1, &particle_quad_vao_handle_); }
        particle_quad_vao_handle_ = 0;
    }
    if (particle_quad_vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(particle_quad_vbo_handle_); }
        else { glDeleteBuffers(1, &particle_quad_vbo_handle_); }
        particle_quad_vbo_handle_ = 0;
    }

    active_render_target_ = 0;
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
    current_frame_stats_.render_passes += 1;
    if (render_pass.render_target != 0) {
        auto stat_rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (stat_rt && !stat_rt->desc.has_color && stat_rt->desc.has_depth) {
            current_frame_stats_.shadow_passes += 1;
        }
    }
    if (render_pass.clear_color_enabled) {
        glClearColor(render_pass.clear_color.r, render_pass.clear_color.g, render_pass.clear_color.b, render_pass.clear_color.a);
        if (has_depth) {
            // 确保 depth mask 开启：glClear(GL_DEPTH_BUFFER_BIT) 受 glDepthMask 控制，
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

void GLDrawExecutor::DrawMeshBatch(const std::vector<MeshDrawItem>& items,
                                     const glm::mat4& view,
                                     const glm::mat4& projection,
                                     GLPipelineStateManager& state_mgr,
                                     GLShaderManager& shader_mgr,
                                     GLResourceManager& resource_mgr,
                                     UBOManager& ubo_mgr) {
    if (items.empty()) return;
    current_frame_stats_.mesh_count += static_cast<int>(items.size());

    glm::mat4 vp = projection * view;
    glm::mat4 inv_view = glm::inverse(view);
#ifdef DSE_VSE_1522_DIAG
    static int vse1522_depth_diag_frames = 0;
    const bool emit_vse1522_depth_diag = vse1522_depth_diag_frames < 5 &&
        std::any_of(items.begin(), items.end(), [](const MeshDrawItem& item) {
            return !item.debug_label.empty();
        });
    if (emit_vse1522_depth_diag) {
        // 诊断：检查当前 FBO 绑定和 depth attachment 状态
        GLint bound_fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
        GLint depth_attached_type = 0;
        GLint depth_attached_name = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &depth_attached_type);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depth_attached_name);
        GLint depth_size = 0;
        if (depth_attached_type != GL_NONE) {
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depth_size);
        }
        GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean depth_write_enabled = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write_enabled);
        GLint depth_func = 0;
        glGetIntegerv(GL_DEPTH_FUNC, &depth_func);

        // 尝试读取一个中心像素的深度值，验证 depth readback 是否正常工作
        float center_depth = -1.0f;
        glReadPixels(Screen::width() / 2, Screen::height() / 2, 1, 1,
                     GL_DEPTH_COMPONENT, GL_FLOAT, &center_depth);

        DEBUG_LOG_INFO("[3D][VSE15.22][DepthDiag][GLDrawExecutor] frame={} batch_items={} camera_pos=({},{},{}) bound_fbo={} depth_attached_type={} depth_attached_name={} depth_size_bits={} depth_test_enabled={} depth_write_enabled={} depth_func={} center_depth_pre_draw={}",
                       vse1522_depth_diag_frames,
                       items.size(),
                       inv_view[3][0],
                       inv_view[3][1],
                       inv_view[3][2],
                       bound_fbo,
                       depth_attached_type,
                       depth_attached_name,
                       depth_size,
                       depth_test_enabled ? 1 : 0,
                       depth_write_enabled ? 1 : 0,
                       static_cast<unsigned int>(depth_func),
                       center_depth);
    }
#else
    const bool emit_vse1522_depth_diag = false;
#endif // DSE_VSE_1522_DIAG

    const auto& loc = shader_mgr.pbr_locations();
    glUseProgram(shader_mgr.pbr_shader_handle());
    if (state_mgr.active_pipeline_state() != 0) {
        state_mgr.ApplyState(state_mgr.active_pipeline_state());
    } else {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    // === PerFrame UBO：每帧上传一次 ===
    PerFrameUBO per_frame;
    per_frame.vp = vp;
    per_frame.view = view;
    per_frame.camera_pos = glm::vec4(inv_view[3][0], inv_view[3][1], inv_view[3][2], 0.0f);
    ubo_mgr.UploadPerFrame(per_frame);

    // === PerScene UBO：使用第一个 item 的光照数据（同一批次光照通常一致） ===
    const auto& first_item = items[0];
    PerSceneUBO per_scene;
    per_scene.light_dir_and_enabled = glm::vec4(first_item.light_direction, first_item.lighting_enabled ? 1.0f : 0.0f);
    per_scene.light_color_and_ambient = glm::vec4(first_item.light_color, first_item.ambient_intensity);
    per_scene.light_params = glm::vec4(first_item.light_intensity, first_item.shadow_strength, first_item.receive_shadow ? 1.0f : 0.0f, 0.0f);
    per_scene.cascade_splits = glm::vec4(global_cascade_splits_[0], global_cascade_splits_[1], global_cascade_splits_[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        per_scene.light_space_matrices[i] = global_light_space_matrix_[i];
    }
    ubo_mgr.UploadPerScene(per_scene);

    // 绑定所有 UBO
    ubo_mgr.BindAll();

    // 纹理采样器（全局设置，非逐对象变化）
    glUniform1i(loc.texture, 0);

    unsigned int last_texture_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_normal_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_metallic_roughness_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_emissive_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_occlusion_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_blend_mode = std::numeric_limits<unsigned int>::max();

    for (const auto& item : items) {
        if (item.vertices.empty() || item.indices.empty()) continue;

        if (emit_vse1522_depth_diag && !item.debug_label.empty()) {
#ifdef DSE_VSE_1522_DIAG
            float ndc_min_z = std::numeric_limits<float>::max();
            float ndc_max_z = std::numeric_limits<float>::lowest();
            float ndc_sum_z = 0.0f;
            float clip_min_w = std::numeric_limits<float>::max();
            float clip_max_w = std::numeric_limits<float>::lowest();
            int valid_clip_vertices = 0;
            int behind_or_zero_w = 0;
            const glm::mat4 clip_matrix = vp * item.model;
            for (const auto& vertex : item.vertices) {
                const glm::vec4 clip = clip_matrix * glm::vec4(vertex.pos, 1.0f);
                clip_min_w = std::min(clip_min_w, clip.w);
                clip_max_w = std::max(clip_max_w, clip.w);
                if (std::abs(clip.w) < 1e-6f || clip.w <= 0.0f) {
                    ++behind_or_zero_w;
                    continue;
                }
                const float ndc_z = clip.z / clip.w;
                ndc_min_z = std::min(ndc_min_z, ndc_z);
                ndc_max_z = std::max(ndc_max_z, ndc_z);
                ndc_sum_z += ndc_z;
                ++valid_clip_vertices;
            }
            const float ndc_avg_z = valid_clip_vertices > 0 ? ndc_sum_z / static_cast<float>(valid_clip_vertices) : 0.0f;

            // 额外诊断：记录当前 GL depth state
            GLboolean gl_depth_test = glIsEnabled(GL_DEPTH_TEST);
            GLboolean gl_depth_write = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &gl_depth_write);
            GLint gl_depth_func = 0;
            glGetIntegerv(GL_DEPTH_FUNC, &gl_depth_func);

            DEBUG_LOG_INFO("[3D][VSE15.22][DepthDiag][GLDrawExecutor] frame={} label={} skinned={} depth_test={} depth_write={} vertices={} indices={} world_min=({},{},{}) world_max=({},{},{}) ndc_z_min={} ndc_z_max={} ndc_z_avg={} clip_w_min={} clip_w_max={} invalid_w={} double_sided={} blend_mode={} gl_depth_test={} gl_depth_write={} gl_depth_func={}",
                           vse1522_depth_diag_frames,
                           item.debug_label,
                           item.skinned,
                           item.depth_test_enabled,
                           item.depth_write_enabled,
                           item.vertices.size(),
                           item.indices.size(),
                           item.debug_world_bounds_min.x,
                           item.debug_world_bounds_min.y,
                           item.debug_world_bounds_min.z,
                           item.debug_world_bounds_max.x,
                           item.debug_world_bounds_max.y,
                           item.debug_world_bounds_max.z,
                           ndc_min_z,
                           ndc_max_z,
                           ndc_avg_z,
                           clip_min_w,
                           clip_max_w,
                           behind_or_zero_w,
                           item.material_double_sided,
                           item.blend_mode,
                           gl_depth_test ? 1 : 0,
                           gl_depth_write ? 1 : 0,
                           static_cast<unsigned int>(gl_depth_func));
            LogVse1522OceanPlaneTriangleDiagnostics(vse1522_depth_diag_frames, item, clip_matrix);
#endif // DSE_VSE_1522_DIAG
        }

        const bool depth_test_changed = !item.depth_test_enabled;
        const bool depth_write_changed = !item.depth_write_enabled;
        if (depth_test_changed) {
            glDisable(GL_DEPTH_TEST);
        }
        if (depth_write_changed) {
            glDepthMask(GL_FALSE);
        }

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
            glUniform1i(loc.normal_map, 1);
        }

        if (last_metallic_roughness_map_handle != item.metallic_roughness_map_handle) {
            if (last_metallic_roughness_map_handle != std::numeric_limits<unsigned int>::max()) {
                current_frame_stats_.material_switches += 1;
            }
            last_metallic_roughness_map_handle = item.metallic_roughness_map_handle;
        }
        if (item.metallic_roughness_map_handle != 0) {
            glActiveTexture(GL_TEXTURE13);
            glBindTexture(GL_TEXTURE_2D, item.metallic_roughness_map_handle);
            glUniform1i(loc.metallic_roughness_map, 13);
        }

        if (last_emissive_map_handle != item.emissive_map_handle) {
            if (last_emissive_map_handle != std::numeric_limits<unsigned int>::max()) {
                current_frame_stats_.material_switches += 1;
            }
            last_emissive_map_handle = item.emissive_map_handle;
        }
        if (item.emissive_map_handle != 0) {
            glActiveTexture(GL_TEXTURE14);
            glBindTexture(GL_TEXTURE_2D, item.emissive_map_handle);
            glUniform1i(loc.emissive_map, 14);
        }

        if (last_occlusion_map_handle != item.occlusion_map_handle) {
            if (last_occlusion_map_handle != std::numeric_limits<unsigned int>::max()) {
                current_frame_stats_.material_switches += 1;
            }
            last_occlusion_map_handle = item.occlusion_map_handle;
        }
        if (item.occlusion_map_handle != 0) {
            glActiveTexture(GL_TEXTURE15);
            glBindTexture(GL_TEXTURE_2D, item.occlusion_map_handle);
            glUniform1i(loc.occlusion_map, 15);
        }

        // === PerMaterial UBO：每材质切换上传 ===
        PerMaterialUBO per_mat;
        per_mat.albedo = glm::vec4(item.material_albedo, item.material_metallic);
        per_mat.roughness_ao = glm::vec4(item.material_roughness, item.material_ao, item.material_normal_strength, item.material_alpha_cutoff);
        per_mat.emissive = glm::vec4(item.material_emissive, item.material_alpha_test ? 1.0f : 0.0f);
        per_mat.flags = glm::vec4(
            item.normal_map_handle != 0 ? 1.0f : 0.0f,
            item.metallic_roughness_map_handle != 0 ? 1.0f : 0.0f,
            item.emissive_map_handle != 0 ? 1.0f : 0.0f,
            item.occlusion_map_handle != 0 ? 1.0f : 0.0f
        );
        ubo_mgr.UploadPerMaterial(per_mat);

        // 点光源（暂保留独立 uniform）
        int point_count = std::min(static_cast<int>(item.point_lights.size()), 4);
        if (loc.point_light_count != -1) glUniform1i(loc.point_light_count, point_count);
        for (int i = 0; i < point_count; ++i) {
            glUniform3f(loc.point_lights[i].color, item.point_lights[i].color.r, item.point_lights[i].color.g, item.point_lights[i].color.b);
            glUniform3f(loc.point_lights[i].position, item.point_lights[i].position.x, item.point_lights[i].position.y, item.point_lights[i].position.z);
            glUniform1f(loc.point_lights[i].intensity, item.point_lights[i].intensity);
            glUniform1f(loc.point_lights[i].radius, item.point_lights[i].radius);
            if (loc.point_lights[i].cast_shadow != -1) glUniform1i(loc.point_lights[i].cast_shadow, item.point_lights[i].cast_shadow ? 1 : 0);
            if (loc.point_lights[i].shadow_index != -1) glUniform1i(loc.point_lights[i].shadow_index, item.point_lights[i].shadow_index);
        }
        for (int i = 0; i < 4; ++i) {
            const int location = glGetUniformLocation(shader_mgr.pbr_shader_handle(), ("u_point_shadow_maps[" + std::to_string(i) + "]").c_str());
            if (location != -1) {
                glActiveTexture(GL_TEXTURE9 + i);
                glBindTexture(GL_TEXTURE_CUBE_MAP, global_point_shadow_map_[i]);
                glUniform1i(location, 9 + i);
            }
        }

        // 聚光灯（暂保留独立 uniform）
        int spot_count = std::min(static_cast<int>(item.spot_lights.size()), 4);
        if (loc.spot_light_count != -1) glUniform1i(loc.spot_light_count, spot_count);
        for (int i = 0; i < spot_count; ++i) {
            glUniform3f(loc.spot_lights[i].color, item.spot_lights[i].color.r, item.spot_lights[i].color.g, item.spot_lights[i].color.b);
            glUniform3f(loc.spot_lights[i].position, item.spot_lights[i].position.x, item.spot_lights[i].position.y, item.spot_lights[i].position.z);
            glUniform3f(loc.spot_lights[i].direction, item.spot_lights[i].direction.x, item.spot_lights[i].direction.y, item.spot_lights[i].direction.z);
            glUniform1f(loc.spot_lights[i].intensity, item.spot_lights[i].intensity);
            glUniform1f(loc.spot_lights[i].radius, item.spot_lights[i].radius);
            glUniform1f(loc.spot_lights[i].inner_cone, item.spot_lights[i].inner_cone);
            glUniform1f(loc.spot_lights[i].outer_cone, item.spot_lights[i].outer_cone);
            if (loc.spot_lights[i].cast_shadow != -1) glUniform1i(loc.spot_lights[i].cast_shadow, item.spot_lights[i].cast_shadow ? 1 : 0);
            if (loc.spot_lights[i].shadow_index != -1) glUniform1i(loc.spot_lights[i].shadow_index, item.spot_lights[i].shadow_index);
        }

        // CSM 阴影贴图（保留独立 uniform - 采样器无法入 UBO）
        for (int i = 0; i < 3; ++i) {
            if (loc.shadow_map[i] != -1) {
                glActiveTexture(GL_TEXTURE2 + i);
                glBindTexture(GL_TEXTURE_2D, global_shadow_map_[i]);
                glUniform1i(loc.shadow_map[i], 2 + i);
            }
        }
        for (int i = 0; i < 4; ++i) {
            if (loc.spot_light_space_matrix[i] != -1) {
                glUniformMatrix4fv(loc.spot_light_space_matrix[i], 1, GL_FALSE, glm::value_ptr(global_spot_light_space_matrix_[i]));
            }
            if (loc.spot_shadow_map[i] != -1) {
                glActiveTexture(GL_TEXTURE5 + i);
                glBindTexture(GL_TEXTURE_2D, global_spot_shadow_map_[i]);
                glUniform1i(loc.spot_shadow_map[i], 5 + i);
            }
        }

        if (last_blend_mode != item.blend_mode) {
            if (last_blend_mode != std::numeric_limits<unsigned int>::max()) {
                current_frame_stats_.material_switches += 1;
            }
            last_blend_mode = item.blend_mode;
        }

        // 双面材质
        if (item.material_double_sided) {
            glDisable(GL_CULL_FACE);
        } else if (state_mgr.active_pipeline_state() != 0) {
            auto pipeline_state = state_mgr.GetPipelineState(state_mgr.active_pipeline_state());
            if (pipeline_state && pipeline_state->culling_enabled) {
                glEnable(GL_CULL_FACE);
                glCullFace(ToGLCullFace(pipeline_state->cull_face));
            } else {
                glDisable(GL_CULL_FACE);
            }
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }

        // 骨骼动画（逐对象 uniform）
        if (loc.skinned != -1) {
            glUniform1i(loc.skinned, item.skinned ? 1 : 0);
        }
        if (item.skinned && loc.bone_matrices != -1 && !item.bone_matrices.empty()) {
            glUniformMatrix4fv(loc.bone_matrices, static_cast<GLsizei>(std::min(item.bone_matrices.size(), static_cast<size_t>(100))), GL_FALSE, glm::value_ptr(item.bone_matrices[0]));
        }

        // 变形目标（逐对象 uniform）
        if (loc.morph_enabled != -1) {
            glUniform1i(loc.morph_enabled, item.morph_enabled ? 1 : 0);
        }
        if (item.morph_enabled && loc.morph_weights != -1 && !item.morph_weights.empty()) {
            glUniform1fv(loc.morph_weights, static_cast<GLsizei>(std::min(item.morph_weights.size(), static_cast<size_t>(4))), item.morph_weights.data());
        }

        // 模型矩阵（逐对象 uniform）
        if (loc.model != -1) {
            glUniformMatrix4fv(loc.model, 1, GL_FALSE, glm::value_ptr(item.model));
        }

        if (item.vertices.size() > MAX_MESH_VERTICES || item.indices.size() > MAX_MESH_INDICES) {
            DEBUG_LOG_WARN("Skipping mesh draw item exceeding dynamic buffer capacity: vertices={} indices={} max_vertices={} max_indices={}",
                           item.vertices.size(), item.indices.size(), MAX_MESH_VERTICES, MAX_MESH_INDICES);
            if (depth_write_changed) {
                glDepthMask(GL_TRUE);
            }
            if (depth_test_changed && state_mgr.active_pipeline_state() != 0) {
                auto pipeline_state = state_mgr.GetPipelineState(state_mgr.active_pipeline_state());
                if (pipeline_state && pipeline_state->depth_test_enabled) {
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(ToGLCompareFunc(pipeline_state->depth_func));
                }
            }
            continue;
        }

        // 实际绘制
        if (item.vao_override > 0) {
            glBindVertexArray(item.vao_override);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(item.index_count_override), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        } else {
            if (update_buffer_fn_) {
                update_buffer_fn_(mesh_vbo_handle_, 0, item.vertices.size() * sizeof(BatchVertex), item.vertices.data(), false);
                update_buffer_fn_(mesh_ibo_handle_, 0, item.indices.size() * sizeof(unsigned short), item.indices.data(), true);
            }

            glBindVertexArray(mesh_vao_handle_);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(item.indices.size()), GL_UNSIGNED_SHORT, nullptr);
            glBindVertexArray(0);
        }

        if (depth_write_changed) {
            glDepthMask(GL_TRUE);
        }
        if (depth_test_changed && state_mgr.active_pipeline_state() != 0) {
            auto pipeline_state = state_mgr.GetPipelineState(state_mgr.active_pipeline_state());
            if (pipeline_state && pipeline_state->depth_test_enabled) {
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(ToGLCompareFunc(pipeline_state->depth_func));
            }
        }

        // 分阶段深度采样：Monster/OceanPlane 绘制后立即采样
        if (emit_vse1522_depth_diag && !item.debug_label.empty()) {
#ifdef DSE_VSE_1522_DIAG
            const int vw = Screen::width();
            const int vh = Screen::height();
            float post_depth = -1.0f;
            glReadPixels(vw / 2, vh / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &post_depth);
            // 在屏幕 1/4 位置（更可能在 Monster 正下方/地面上）采样
            float ground_depth = -1.0f;
            glReadPixels(vw / 2, vh / 4, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &ground_depth);
            unsigned char post_color[4] = {0, 0, 0, 0};
            glReadPixels(vw / 2, vh / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, post_color);
            unsigned char ground_color[4] = {0, 0, 0, 0};
            glReadPixels(vw / 2, vh / 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, ground_color);
            GLboolean post_depth_test = glIsEnabled(GL_DEPTH_TEST);
            GLboolean post_depth_write = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &post_depth_write);
            GLint post_depth_func = 0;
            glGetIntegerv(GL_DEPTH_FUNC, &post_depth_func);
            DEBUG_LOG_INFO("[3D][VSE15.22][DepthDiag][PostDraw] frame={} label={} post_depth_center={} post_depth_quarter={} post_color_center=({},{},{},{}) post_color_quarter=({},{},{},{}) depth_test={} depth_write={} depth_func={}",
                           vse1522_depth_diag_frames,
                           item.debug_label,
                           post_depth,
                           ground_depth,
                           post_color[0], post_color[1], post_color[2], post_color[3],
                           ground_color[0], ground_color[1], ground_color[2], ground_color[3],
                           post_depth_test ? 1 : 0,
                           post_depth_write ? 1 : 0,
                           static_cast<unsigned int>(post_depth_func));
#endif // DSE_VSE_1522_DIAG
        }

        current_frame_stats_.draw_calls += 1;
    }
    if (emit_vse1522_depth_diag) {
#ifdef DSE_VSE_1522_DIAG
        // 最终深度采样：验证 FBO 绑定和 depth attachment
        GLint final_fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &final_fbo);
        GLint final_depth_type = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &final_depth_type);
        std::vector<float> depth_samples(9, -2.0f);
        const int viewport_w = Screen::width();
        const int viewport_h = Screen::height();
        const int sample_x[9] = { viewport_w / 4, viewport_w / 2, viewport_w * 3 / 4, viewport_w / 4, viewport_w / 2, viewport_w * 3 / 4, viewport_w / 4, viewport_w / 2, viewport_w * 3 / 4 };
        const int sample_y[9] = { viewport_h / 4, viewport_h / 4, viewport_h / 4, viewport_h / 2, viewport_h / 2, viewport_h / 2, viewport_h * 3 / 4, viewport_h * 3 / 4, viewport_h * 3 / 4 };
        float depth_min = std::numeric_limits<float>::max();
        float depth_max = std::numeric_limits<float>::lowest();
        float depth_sum = 0.0f;
        for (int i = 0; i < 9; ++i) {
            glReadPixels(sample_x[i], sample_y[i], 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth_samples[static_cast<std::size_t>(i)]);
            depth_min = std::min(depth_min, depth_samples[static_cast<std::size_t>(i)]);
            depth_max = std::max(depth_max, depth_samples[static_cast<std::size_t>(i)]);
            depth_sum += depth_samples[static_cast<std::size_t>(i)];
        }

        // 同时采样中心区域的 color 值，确认渲染有输出
        unsigned char center_rgba[4] = {0, 0, 0, 0};
        glReadPixels(viewport_w / 2, viewport_h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center_rgba);

        // 检查 GL depth state
        GLboolean final_depth_test = glIsEnabled(GL_DEPTH_TEST);
        GLboolean final_depth_write = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &final_depth_write);
        GLint final_depth_func = 0;
        glGetIntegerv(GL_DEPTH_FUNC, &final_depth_func);

        DEBUG_LOG_INFO("[3D][VSE15.22][DepthDiag][GLDrawExecutor] frame={} post_draw final_fbo={} final_depth_attachment_type={} final_depth_test={} final_depth_write={} final_depth_func={} depth_samples_3x3 min={} max={} avg={} values=({}, {}, {}, {}, {}, {}, {}, {}, {}) center_color=({},{},{},{})",
                       vse1522_depth_diag_frames,
                       final_fbo,
                       final_depth_type,
                       final_depth_test ? 1 : 0,
                       final_depth_write ? 1 : 0,
                       static_cast<unsigned int>(final_depth_func),
                       depth_min,
                       depth_max,
                       depth_sum / 9.0f,
                       depth_samples[0], depth_samples[1], depth_samples[2],
                       depth_samples[3], depth_samples[4], depth_samples[5],
                       depth_samples[6], depth_samples[7], depth_samples[8],
                       center_rgba[0], center_rgba[1], center_rgba[2], center_rgba[3]);
        ++vse1522_depth_diag_frames;
#endif // DSE_VSE_1522_DIAG
    }
}

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
            glBindVertexArray(skybox_vao_handle_);
            glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo_handle_);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glBindVertexArray(0);
        }
    }

    const auto& skybox_loc = shader_mgr.skybox_locations();
    glDepthFunc(GL_LEQUAL);
    glUseProgram(shader_mgr.skybox_shader_handle());
    glm::mat4 skybox_view = glm::mat4(glm::mat3(view));
    glUniformMatrix4fv(skybox_loc.view, 1, GL_FALSE, glm::value_ptr(skybox_view));
    glUniformMatrix4fv(skybox_loc.projection, 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(skybox_vao_handle_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture_handle);
    glUniform1i(skybox_loc.tex, 0);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);

    current_frame_stats_.draw_calls += 1;
}

// ============================================================
// 后处理绘制
// ============================================================

void GLDrawExecutor::DrawPostProcess(unsigned int source_texture,
                                       const std::string& effect_name,
                                       const std::vector<float>& params,
                                       GLShaderManager& shader_mgr) {
    // 后处理全屏四边形 VAO/VBO
    if (pp_vao_handle_ == 0) {
        float quadVertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        if (create_vao_fn_ && create_buffer_fn_) {
            pp_vao_handle_ = create_vao_fn_();
            pp_vbo_handle_ = create_buffer_fn_(sizeof(quadVertices), &quadVertices, false, false);
        } else {
            glGenVertexArrays(1, &pp_vao_handle_);
            glGenBuffers(1, &pp_vbo_handle_);
        }
        glBindVertexArray(pp_vao_handle_);
        glBindBuffer(GL_ARRAY_BUFFER, pp_vbo_handle_);
        if (!create_buffer_fn_) {
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        }
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    // 构建后处理片段着色器
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
                vec3 downsample = e*0.125;
                downsample += (a+c+g+i)*0.03125;
                downsample += (b+d+f+h)*0.0625;
                downsample += (j+k+l+m)*0.125;
                FragColor = vec4(downsample, 1.0);
            }
        )";
    } else if (effect_name == "bloom_upsample") {
        fs_src += R"(
            uniform float filterRadius;
            void main() {
                float x = filterRadius;
                float y = filterRadius;
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
                vec2 tex_offset = 1.0 / textureSize(screenTexture, 0);
                vec3 result = texture(screenTexture, TexCoords).rgb * weight[0];
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
                vec2 tex_offset = 1.0 / textureSize(screenTexture, 0);
                vec3 result = texture(screenTexture, TexCoords).rgb * weight[0];
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
                hdrColor += bloomColor * bloomIntensity;
                vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
                result = pow(result, vec3(1.0 / 2.2));
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "ui_overlay") {
        fs_src += R"(
            void main() {
                FragColor = texture(screenTexture, TexCoords);
            }
        )";
    } else {
        fs_src += R"(
            void main() {
                FragColor = texture(screenTexture, TexCoords);
            }
        )";
    }

    unsigned int shader = shader_mgr.GetOrCreatePostProcessShader(effect_name, vs_src, fs_src);
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
    if (effect_name == "ui_overlay") {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
    glBindVertexArray(pp_vao_handle_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

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
    current_frame_stats_.sprite_count += static_cast<int>(items.size());

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
    glUseProgram(shader_mgr.pbr_shader_handle());
    if (state_mgr.active_pipeline_state() != 0) {
        state_mgr.ApplyState(state_mgr.active_pipeline_state());
    } else {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    glUniform1i(loc.texture, 0);
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
        current_frame_stats_.draw_calls += 1;
        current_frame_stats_.max_batch_sprites = std::max(current_frame_stats_.max_batch_sprites, batch_sprites);

        if (update_buffer_fn_) {
            update_buffer_fn_(vbo_handle_, 0, batch_vertices.size() * sizeof(BatchVertex), batch_vertices.data(), false);
        }

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
    glm::mat4 vp = projection * view;
    glUniformMatrix4fv(p_loc.vp, 1, GL_FALSE, glm::value_ptr(vp));
    glUniform1i(p_loc.texture, 0);

    glm::vec3 camera_right = glm::vec3(view[0][0], view[1][0], view[2][0]);
    glm::vec3 camera_up = glm::vec3(view[0][1], view[1][1], view[2][1]);
    glUniform3fv(glGetUniformLocation(shader_mgr.particle_shader_handle(), "u_camera_right"), 1, glm::value_ptr(camera_right));
    glUniform3fv(glGetUniformLocation(shader_mgr.particle_shader_handle(), "u_camera_up"), 1, glm::value_ptr(camera_up));

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
        current_frame_stats_.draw_calls += 1;

        glVertexAttribDivisor(2, 0);
        glVertexAttribDivisor(3, 0);
        glVertexAttribDivisor(4, 0);
        glBindVertexArray(0);
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ============================================================
// 帧生命周期
// ============================================================

void GLDrawExecutor::BeginFrame() {
    current_frame_stats_ = {};
}

void GLDrawExecutor::EndFrame() {
    last_frame_stats_ = current_frame_stats_;
    glFlush();
}

} // namespace render
} // namespace dse
