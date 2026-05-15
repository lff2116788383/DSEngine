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
#include "engine/render/rhi/postprocess_common.h"
#include "engine/platform/screen.h"
#include "engine/base/debug.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <functional>
#include <cstddef>
#include <cmath>
#include <cstring>
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
    // GPU Instancing VBO
    if (instance_vbo_handle_ != 0) {
        if (delete_buffer_fn_) { delete_buffer_fn_(instance_vbo_handle_); }
        else { glDeleteBuffers(1, &instance_vbo_handle_); }
        instance_vbo_handle_ = 0;
        instance_vbo_capacity_ = 0;
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
    global_state_.current_frame_stats.render_passes += 1;
    if (render_pass.render_target != 0) {
        auto stat_rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (stat_rt && !stat_rt->desc.has_color && stat_rt->desc.has_depth) {
            global_state_.current_frame_stats.shadow_passes += 1;
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
    global_state_.current_frame_stats.mesh_count += static_cast<int>(items.size());

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

    const bool gbuffer_mode = global_state_.gbuffer_rendering_mode;

    if (gbuffer_mode) {
        shader_mgr.InitGBufferShader();
        glUseProgram(shader_mgr.gbuffer_shader_handle());
    } else {
        glUseProgram(shader_mgr.pbr_shader_handle());
    }
    const auto& loc = shader_mgr.pbr_locations();

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

    // PerScene UBO 需要在 for 循环中被 shading_mode 变更引用，声明在此处
    PerSceneUBO per_scene{};
    if (!gbuffer_mode) {
    // === PerScene UBO：使用第一个 item 的光照数据（同一批次光照通常一致） ===
    const auto& first_item = items[0];
    per_scene.light_dir_and_enabled = glm::vec4(first_item.light_direction, first_item.lighting_enabled ? 1.0f : 0.0f);
    per_scene.light_color_and_ambient = glm::vec4(first_item.light_color, first_item.ambient_intensity);
    per_scene.light_params = glm::vec4(first_item.light_intensity, first_item.shadow_strength, first_item.receive_shadow ? 1.0f : 0.0f, static_cast<float>(first_item.shading_mode));
    per_scene.cascade_splits = glm::vec4(global_state_.cascade_splits[0], global_state_.cascade_splits[1], global_state_.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        per_scene.light_space_matrices[i] = global_state_.light_space_matrix[i];
    }
    ubo_mgr.UploadPerScene(per_scene);

    // === LightProbeData UBO: SH 球谐系数 ===
    LightProbeDataUBO lp_data{};
    for (int i = 0; i < 9; ++i) lp_data.sh_coefficients[i] = global_state_.light_probe_sh[i];
    lp_data.probe_params = glm::vec4(global_state_.light_probe_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    ubo_mgr.UploadLightProbeData(lp_data);
    } // !gbuffer_mode

    // 绑定所有 UBO
    ubo_mgr.BindAll();

    // 纹理采样器（全局设置，非逐对象变化）
    unsigned int active_shader = gbuffer_mode ? shader_mgr.gbuffer_shader_handle() : shader_mgr.pbr_shader_handle();
    glUniform1i(glGetUniformLocation(active_shader, "u_texture"), 0);
    const int gbuffer_model_loc = gbuffer_mode ? glGetUniformLocation(active_shader, "u_model") : -1;

    unsigned int last_texture_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_normal_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_metallic_roughness_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_emissive_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_occlusion_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_blend_mode = std::numeric_limits<unsigned int>::max();
    int last_shading_mode = items[0].shading_mode;

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
                global_state_.current_frame_stats.material_switches += 1;
            }
            last_texture_handle = tex;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        if (last_normal_map_handle != item.normal_map_handle) {
            if (last_normal_map_handle != std::numeric_limits<unsigned int>::max()) {
                global_state_.current_frame_stats.material_switches += 1;
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
                global_state_.current_frame_stats.material_switches += 1;
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
                global_state_.current_frame_stats.material_switches += 1;
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
                global_state_.current_frame_stats.material_switches += 1;
            }
            last_occlusion_map_handle = item.occlusion_map_handle;
        }
        if (item.occlusion_map_handle != 0) {
            glActiveTexture(GL_TEXTURE15);
            glBindTexture(GL_TEXTURE_2D, item.occlusion_map_handle);
            glUniform1i(loc.occlusion_map, 15);
        }

        if (!gbuffer_mode) {
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
        per_mat.extra_params = glm::vec4(
            item.material_sss_strength,
            item.material_clear_coat,
            item.material_clear_coat_roughness,
            item.material_anisotropy);
        per_mat.extra_params2 = glm::vec4(
            item.material_pom_height_scale,
            item.material_sss_tint.x, item.material_sss_tint.y, item.material_sss_tint.z);
        if (item.shading_mode == 5) {
            per_mat.toon_shadow_color = glm::vec4(
                item.watercolor_paper_strength, item.watercolor_edge_darkening,
                item.watercolor_color_bleed, item.watercolor_pigment_density);
            per_mat.toon_params = glm::vec4(0.0f);
        } else {
            per_mat.toon_shadow_color = glm::vec4(item.toon_shadow_color, item.toon_shadow_threshold);
            per_mat.toon_params = glm::vec4(
                item.toon_shadow_softness, item.toon_specular_size,
                item.toon_specular_strength, item.toon_rim_strength);
        }
        ubo_mgr.UploadPerMaterial(per_mat);

        // 点光源/聚光灯数据已由 LightBuffer SSBO 提供（ForwardScenePass 绑定）
        // 仅绑定点光源阴影贴图
        for (int i = 0; i < 4; ++i) {
            if (loc.point_shadow_map[i] != -1) {
                glActiveTexture(GL_TEXTURE9 + i);
                glBindTexture(GL_TEXTURE_CUBE_MAP, global_state_.point_shadow_map[i]);
                glUniform1i(loc.point_shadow_map[i], 9 + i);
            }
        }

        // 聚光灯空间矩阵 UBO
        {
            SpotLightDataUBO sld_ubo{};
            for (int i = 0; i < 4; ++i)
                sld_ubo.u_spot_light_space_matrices[i] = global_state_.spot_light_space_matrix[i];
            ubo_mgr.UploadSpotLightData(sld_ubo);
        }

        // CSM 阴影贴图（sampler2DShadow 需要硬件深度比较）
        for (int i = 0; i < 3; ++i) {
            if (loc.shadow_map[i] != -1) {
                glActiveTexture(GL_TEXTURE2 + i);
                glBindTexture(GL_TEXTURE_2D, global_state_.shadow_map[i]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                glUniform1i(loc.shadow_map[i], 2 + i);
            }
        }
        for (int i = 0; i < 4; ++i) {
            if (loc.spot_shadow_map[i] != -1) {
                glActiveTexture(GL_TEXTURE5 + i);
                glBindTexture(GL_TEXTURE_2D, global_state_.spot_shadow_map[i]);
                glUniform1i(loc.spot_shadow_map[i], 5 + i);
            }
        }

        if (last_blend_mode != item.blend_mode) {
            if (last_blend_mode != std::numeric_limits<unsigned int>::max()) {
                global_state_.current_frame_stats.material_switches += 1;
            }
            last_blend_mode = item.blend_mode;
        }

        // Re-upload PerScene UBO when shading mode changes (PBR vs HalfLambert)
        if (item.shading_mode != last_shading_mode) {
            per_scene.light_params.w = static_cast<float>(item.shading_mode);
            ubo_mgr.UploadPerScene(per_scene);
            last_shading_mode = item.shading_mode;
        }
        } // !gbuffer_mode

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

        // 骨骼动画（push constant → 独立 uniform + BoneMatrices UBO）
        if (loc.skinned != -1) {
            glUniform1i(loc.skinned, item.skinned ? 1 : 0);
        }
        if (item.skinned && !item.bone_matrices.empty()) {
            BoneMatricesUBO bm_ubo{};
            size_t count = std::min(item.bone_matrices.size(), static_cast<size_t>(kMaxBones));
            std::memcpy(bm_ubo.u_bone_matrices, item.bone_matrices.data(), count * sizeof(glm::mat4));
            ubo_mgr.UploadBoneMatrices(bm_ubo);
        }

        // 变形目标（push constant → 独立 uniform + MorphWeights UBO）
        if (loc.morph_enabled != -1) {
            glUniform1i(loc.morph_enabled, item.morph_enabled ? 1 : 0);
        }
        if (item.morph_enabled && !item.morph_weights.empty()) {
            MorphWeightsUBO mw_ubo{};
            size_t count = std::min(item.morph_weights.size(), static_cast<size_t>(kMaxMorphTargets));
            for (size_t i = 0; i < count; ++i)
                mw_ubo.u_morph_weights[i] = glm::vec4(item.morph_weights[i], 0.0f, 0.0f, 0.0f);
            ubo_mgr.UploadMorphWeights(mw_ubo);
        }

        // GPU Instancing 标记
        const bool is_instanced = item.instance_transforms.size() > 1;
        {
            int inst_loc = gbuffer_mode ? -1 : loc.use_instancing;
            if (inst_loc != -1) {
                glUniform1i(inst_loc, is_instanced ? 1 : 0);
            }
        }

        // 模型矩阵（逐对象 uniform）
        if (!is_instanced) {
            int model_loc = gbuffer_mode ? gbuffer_model_loc : loc.model;
            if (model_loc != -1) {
                glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(item.model));
            }
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
        } else if (is_instanced) {
            // --- GPU Instancing path ---
            if (update_buffer_fn_) {
                update_buffer_fn_(mesh_vbo_handle_, 0, item.vertices.size() * sizeof(BatchVertex), item.vertices.data(), false);
                update_buffer_fn_(mesh_ibo_handle_, 0, item.indices.size() * sizeof(unsigned short), item.indices.data(), true);
            }

            const size_t instance_count = item.instance_transforms.size();
            const size_t inst_data_size = instance_count * sizeof(glm::mat4);

            // 动态扩容 instance VBO
            if (inst_data_size > instance_vbo_capacity_) {
                if (instance_vbo_handle_ != 0) {
                    if (delete_buffer_fn_) { delete_buffer_fn_(instance_vbo_handle_); }
                    else { glDeleteBuffers(1, &instance_vbo_handle_); }
                }
                instance_vbo_capacity_ = inst_data_size * 2;
                if (create_buffer_fn_) {
                    instance_vbo_handle_ = create_buffer_fn_(instance_vbo_capacity_, nullptr, true, false);
                } else {
                    glGenBuffers(1, &instance_vbo_handle_);
                    glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_handle_);
                    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instance_vbo_capacity_), nullptr, GL_DYNAMIC_DRAW);
                }
            }

            // 上传 instance model 矩阵
            glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_handle_);
            glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(inst_data_size), item.instance_transforms.data());

            glBindVertexArray(mesh_vao_handle_);

            // 配置 instance attributes (location 7-10, 每列 vec4)
            const GLsizei mat4_stride = static_cast<GLsizei>(sizeof(glm::mat4));
            for (int col = 0; col < 4; ++col) {
                GLuint attr = 7 + static_cast<GLuint>(col);
                glEnableVertexAttribArray(attr);
                glVertexAttribPointer(attr, 4, GL_FLOAT, GL_FALSE, mat4_stride,
                                      reinterpret_cast<const void*>(static_cast<uintptr_t>(col * sizeof(glm::vec4))));
                glVertexAttribDivisor(attr, 1);
            }

            glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(item.indices.size()),
                                    GL_UNSIGNED_SHORT, nullptr, static_cast<GLsizei>(instance_count));

            // 清理 instance attribute 状态
            for (int col = 0; col < 4; ++col) {
                GLuint attr = 7 + static_cast<GLuint>(col);
                glVertexAttribDivisor(attr, 0);
                glDisableVertexAttribArray(attr);
            }
            glBindVertexArray(0);

            global_state_.current_frame_stats.instanced_draw_calls += 1;
            global_state_.current_frame_stats.instanced_mesh_count += static_cast<int>(instance_count);
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

        global_state_.current_frame_stats.draw_calls += 1;
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
    // 合成 VP 矩阵：移除平移分量后乘以投影
    glm::mat4 skybox_view = glm::mat4(glm::mat3(view));
    glm::mat4 vp = projection * skybox_view;
    glUniformMatrix4fv(skybox_loc.vp, 1, GL_FALSE, glm::value_ptr(vp));

    glBindVertexArray(skybox_vao_handle_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture_handle);
    glUniform1i(skybox_loc.tex, 0);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);

    global_state_.current_frame_stats.draw_calls += 1;
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
    } else if (effect_name == "lum_compute") {
        fs_src += R"(
            void main() {
                float logSum = 0.0;
                const int N = 64;
                for (int i = 0; i < 8; i++) {
                    for (int j = 0; j < 8; j++) {
                        vec2 uv = (vec2(float(i), float(j)) + 0.5) / 8.0;
                        vec3 c = texture(screenTexture, uv).rgb;
                        float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
                        logSum += log(max(lum, 0.0001));
                    }
                }
                float avgLogLum = logSum / float(N);
                FragColor = vec4(avgLogLum, 0.0, 0.0, 1.0);
            }
        )";
    } else if (effect_name == "lum_adapt") {
        fs_src += R"(
            uniform sampler2D prevAdaptedTex;
            uniform float u_dt;
            uniform float u_speed_up;
            uniform float u_speed_down;
            uniform float u_min_exposure;
            uniform float u_max_exposure;
            uniform float u_compensation;
            void main() {
                float avgLogLum = texture(screenTexture, vec2(0.5, 0.5)).r;
                float avgLum = exp(avgLogLum);
                float targetExposure = 0.18 / max(avgLum, 0.001);
                targetExposure = clamp(targetExposure * exp2(u_compensation), u_min_exposure, u_max_exposure);
                float prevExposure = texture(prevAdaptedTex, vec2(0.5, 0.5)).r;
                if (prevExposure <= 0.0) prevExposure = targetExposure;
                float speed = (targetExposure > prevExposure) ? u_speed_up : u_speed_down;
                float adapted = prevExposure + (targetExposure - prevExposure) * (1.0 - exp(-u_dt * speed));
                FragColor = vec4(adapted, 0.0, 0.0, 1.0);
            }
        )";
    } else if (effect_name == "tonemapping") {
        fs_src += R"(
            uniform sampler2D autoExposureTex;
            uniform sampler3D u_lut;
            uniform float u_manual_exposure;
            uniform float u_lut_intensity;
            uniform int u_auto_exposure_enabled;
            uniform int u_lut_enabled;
            vec3 AcesFilmic(vec3 x) {
                float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
                return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
            }
            void main() {
                vec3 hdrColor = texture(screenTexture, TexCoords).rgb;
                float finalExposure = u_manual_exposure;
                if (u_auto_exposure_enabled != 0) {
                    finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
                }
                vec3 result = AcesFilmic(hdrColor * finalExposure);
                result = pow(result, vec3(1.0 / 2.2));
                if (u_lut_enabled != 0) {
                    vec3 lutColor = texture(u_lut, clamp(result, 0.0, 1.0)).rgb;
                    result = mix(result, lutColor, u_lut_intensity);
                }
                float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
                result += (ign - 0.5) / 255.0;
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "bloom_composite") {
        fs_src += R"(
            uniform sampler2D bloomBlur;
            uniform sampler2D ssaoTexture;
            uniform sampler2D contactShadowTex;
            uniform sampler2D autoExposureTex;
            uniform sampler3D u_lut;
            uniform float exposure;
            uniform float bloomIntensity;
            uniform float u_lut_intensity;
            uniform float u_vignette_intensity;
            uniform float u_vignette_radius;
            uniform float u_vignette_softness;
            uniform float u_film_grain_intensity;
            uniform float u_film_grain_time;
            uniform int u_bloom_enabled;
            uniform int u_ssao_enabled;
            uniform int u_contact_shadow_enabled;
            uniform float u_contact_shadow_strength;
            uniform int u_auto_exposure_enabled;
            uniform int u_lut_enabled;
            uniform int u_vignette_enabled;
            uniform int u_film_grain_enabled;
            vec3 AcesFilmic(vec3 x) {
                float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
                return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
            }
            float GrainNoise(vec2 uv, float time_seed) {
                return fract(sin(dot(uv + vec2(time_seed, time_seed * 0.37), vec2(12.9898, 78.233))) * 43758.5453);
            }
            void main() {
                vec3 hdrColor = texture(screenTexture, TexCoords).rgb;
                if (u_ssao_enabled != 0) {
                    float ao = texture(ssaoTexture, TexCoords).r;
                    hdrColor *= ao;
                }
                if (u_bloom_enabled != 0) {
                    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
                    hdrColor += bloomColor * bloomIntensity;
                }
                if (u_contact_shadow_enabled != 0) {
                    float cs = texture(contactShadowTex, TexCoords).r;
                    hdrColor *= (1.0 - (1.0 - cs) * u_contact_shadow_strength);
                }
                float finalExposure = exposure;
                if (u_auto_exposure_enabled != 0) {
                    finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
                }
                vec3 result = AcesFilmic(hdrColor * finalExposure);
                result = pow(result, vec3(1.0 / 2.2));
                if (u_lut_enabled != 0) {
                    vec3 lutColor = texture(u_lut, clamp(result, 0.0, 1.0)).rgb;
                    result = mix(result, lutColor, u_lut_intensity);
                }
                if (u_vignette_enabled != 0) {
                    float dist = length(TexCoords - vec2(0.5));
                    float radius = clamp(u_vignette_radius, 0.001, 1.5);
                    float softness = max(u_vignette_softness, 0.0001);
                    float vignette = 1.0 - smoothstep(radius, radius + softness, dist);
                    result *= mix(1.0, vignette, clamp(u_vignette_intensity, 0.0, 1.0));
                }
                if (u_film_grain_enabled != 0) {
                    float grain = GrainNoise(TexCoords * vec2(1280.0, 720.0), u_film_grain_time) - 0.5;
                    result = clamp(result + grain * u_film_grain_intensity, 0.0, 1.0);
                }
                // Anti color-banding dithering (IGN, +-0.5/255)
                float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
                result += (ign - 0.5) / 255.0;
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "ssao") {
        fs_src += R"(
            uniform float u_radius;
            uniform float u_bias;
            uniform float u_near;
            uniform float u_far;
            uniform vec2 u_screen_size;
            float linearizeDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }
            vec3 reconstructNormal(vec2 uv) {
                vec2 texel = 1.0 / u_screen_size;
                float dc = linearizeDepth(texture(screenTexture, uv).r);
                float dl = linearizeDepth(texture(screenTexture, uv - vec2(texel.x, 0.0)).r);
                float dr = linearizeDepth(texture(screenTexture, uv + vec2(texel.x, 0.0)).r);
                float db = linearizeDepth(texture(screenTexture, uv - vec2(0.0, texel.y)).r);
                float dt = linearizeDepth(texture(screenTexture, uv + vec2(0.0, texel.y)).r);
                vec3 n = normalize(vec3(dl - dr, db - dt, 2.0 * texel.x * dc));
                return n;
            }
            const vec3 kernel[16] = vec3[](
                vec3( 0.5381, 0.1856,-0.4319), vec3( 0.1379, 0.2486, 0.4430),
                vec3( 0.3371, 0.5679,-0.0057), vec3(-0.6999,-0.0451,-0.0019),
                vec3( 0.0689,-0.1598,-0.8547), vec3( 0.0560, 0.0069,-0.1843),
                vec3(-0.0146, 0.1402, 0.0762), vec3( 0.0100,-0.1924,-0.0344),
                vec3(-0.3577,-0.5301,-0.4358), vec3(-0.3169, 0.1063, 0.0158),
                vec3( 0.0103,-0.5869, 0.0046), vec3(-0.0897,-0.4940, 0.3287),
                vec3( 0.7119,-0.0154,-0.0918), vec3(-0.0533, 0.0596,-0.5411),
                vec3( 0.0352,-0.0631, 0.5460), vec3(-0.4776, 0.2847,-0.0271)
            );
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                if (depth >= 1.0) { FragColor = vec4(1.0); return; }
                float linDepth = linearizeDepth(depth);
                vec3 normal = reconstructNormal(TexCoords);
                float occlusion = 0.0;
                float rScale = u_radius / linDepth;
                for (int i = 0; i < 16; ++i) {
                    vec3 sampleDir = kernel[i];
                    if (dot(sampleDir, normal) < 0.0) sampleDir = -sampleDir;
                    vec2 sampleUV = TexCoords + sampleDir.xy * rScale * (1.0 / u_screen_size);
                    float sampleDepth = linearizeDepth(texture(screenTexture, sampleUV).r);
                    float rangeCheck = smoothstep(0.0, 1.0, u_radius / abs(linDepth - sampleDepth));
                    if (sampleDepth < linDepth - u_bias) occlusion += rangeCheck;
                }
                occlusion = 1.0 - (occlusion / 16.0);
                FragColor = vec4(vec3(occlusion), 1.0);
            }
        )";
    } else if (effect_name == "ssao_blur") {
        fs_src += R"(
            void main() {
                vec2 texelSize = 1.0 / vec2(textureSize(screenTexture, 0));
                float result = 0.0;
                for (int x = -2; x <= 2; ++x) {
                    for (int y = -2; y <= 2; ++y) {
                        vec2 offset = vec2(float(x), float(y)) * texelSize;
                        result += texture(screenTexture, TexCoords + offset).r;
                    }
                }
                FragColor = vec4(vec3(result / 25.0), 1.0);
            }
        )";
    } else if (effect_name == "ssao_apply") {
        fs_src += R"(
            uniform sampler2D ssaoTexture;
            uniform sampler2D autoExposureTex;
            uniform sampler3D u_lut;
            uniform float exposure;
            uniform float u_lut_intensity;
            uniform int u_auto_exposure_enabled;
            uniform int u_lut_enabled;
            vec3 AcesFilmic(vec3 x) {
                float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
                return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
            }
            void main() {
                vec3 hdrColor = texture(screenTexture, TexCoords).rgb;
                float ao = texture(ssaoTexture, TexCoords).r;
                hdrColor *= ao;
                float finalExposure = exposure;
                if (u_auto_exposure_enabled != 0) {
                    finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
                }
                vec3 result = AcesFilmic(hdrColor * finalExposure);
                result = pow(result, vec3(1.0 / 2.2));
                if (u_lut_enabled != 0) {
                    vec3 lutColor = texture(u_lut, clamp(result, 0.0, 1.0)).rgb;
                    result = mix(result, lutColor, u_lut_intensity);
                }
                float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
                result += (ign - 0.5) / 255.0;
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "contact_shadow") {
        fs_src += R"(
            uniform vec3 u_light_dir;
            uniform float u_near;
            uniform float u_far;
            uniform vec2 u_screen_size;
            uniform float u_strength;
            uniform float u_step_size;
            uniform int u_num_steps;
            float linearizeDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                if (depth >= 1.0) { FragColor = vec4(1.0); return; }
                float linDepth = linearizeDepth(depth);
                vec3 lightDir = normalize(u_light_dir);
                vec2 texelSize = 1.0 / u_screen_size;
                float occlusion = 0.0;
                int validSteps = 0;
                for (int i = 1; i <= u_num_steps; ++i) {
                    float dist = u_step_size * float(i);
                    vec2 sampleUV = TexCoords + lightDir.xy * texelSize * dist * 50.0;
                    if (sampleUV.x < 0.0 || sampleUV.y < 0.0 || sampleUV.x > 1.0 || sampleUV.y > 1.0) break;
                    float sampleDepth = texture(screenTexture, sampleUV).r;
                    if (sampleDepth >= 1.0) continue;
                    float sampleLin = linearizeDepth(sampleDepth);
                    float diff = sampleLin - linDepth;
                    if (diff > 0.0 && diff < u_step_size) {
                        float k = 1.0 - (diff / u_step_size);
                        occlusion += k * k;
                    }
                    ++validSteps;
                }
                float shadow = validSteps > 0 ? 1.0 - clamp(occlusion / float(validSteps) * u_strength, 0.0, 1.0) : 1.0;
                FragColor = vec4(vec3(shadow), 1.0);
            }
        )";
    } else if (effect_name == "color_grading") {
        fs_src += R"(
            uniform sampler3D u_lut;
            uniform float u_lut_intensity;
            void main() {
                vec3 color = texture(screenTexture, TexCoords).rgb;
                vec3 lutColor = texture(u_lut, clamp(color, 0.0, 1.0)).rgb;
                color = mix(color, lutColor, u_lut_intensity);
                float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
                color += (ign - 0.5) / 255.0;
                FragColor = vec4(color, 1.0);
            }
        )";
    } else if (effect_name == "fxaa") {
        fs_src += R"(
            uniform vec2 u_resolution;
            float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
            void main() {
                vec2 texel = 1.0 / u_resolution;
                float lumaM  = luma(texture(screenTexture, TexCoords).rgb);
                float lumaNW = luma(texture(screenTexture, TexCoords + vec2(-1.0,-1.0) * texel).rgb);
                float lumaNE = luma(texture(screenTexture, TexCoords + vec2( 1.0,-1.0) * texel).rgb);
                float lumaSW = luma(texture(screenTexture, TexCoords + vec2(-1.0, 1.0) * texel).rgb);
                float lumaSE = luma(texture(screenTexture, TexCoords + vec2( 1.0, 1.0) * texel).rgb);
                float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
                float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
                float lumaRange = lumaMax - lumaMin;
                if (lumaRange < max(0.0312, lumaMax * 0.125)) {
                    FragColor = texture(screenTexture, TexCoords);
                    return;
                }
                vec2 dir;
                dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
                dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
                float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * 0.25, 1.0/128.0);
                float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
                dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * texel;
                vec3 rgbA = 0.5 * (
                    texture(screenTexture, TexCoords + dir * (1.0/3.0 - 0.5)).rgb +
                    texture(screenTexture, TexCoords + dir * (2.0/3.0 - 0.5)).rgb);
                vec3 rgbB = rgbA * 0.5 + 0.25 * (
                    texture(screenTexture, TexCoords + dir * -0.5).rgb +
                    texture(screenTexture, TexCoords + dir *  0.5).rgb);
                float lumaB = luma(rgbB);
                if (lumaB < lumaMin || lumaB > lumaMax)
                    FragColor = vec4(rgbA, 1.0);
                else
                    FragColor = vec4(rgbB, 1.0);
            }
        )";
    } else if (effect_name == "dof") {
        fs_src += R"(
            uniform sampler2D u_color_texture;
            uniform float u_focus_distance;
            uniform float u_focus_range;
            uniform float u_bokeh_radius;
            uniform float u_near;
            uniform float u_far;
            uniform float u_screen_w;
            uniform float u_screen_h;
            float linearizeDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                float lin_depth = linearizeDepth(depth);
                float coc = clamp(abs(lin_depth - u_focus_distance) / u_focus_range, 0.0, 1.0);
                vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
                float radius = coc * u_bokeh_radius;
                vec3 color = vec3(0.0);
                float total_weight = 0.0;
                const int SAMPLES = 16;
                const float GOLDEN_ANGLE = 2.39996323;
                for (int i = 0; i < SAMPLES; ++i) {
                    float r = sqrt(float(i) / float(SAMPLES)) * radius;
                    float theta = float(i) * GOLDEN_ANGLE;
                    vec2 offset = vec2(cos(theta), sin(theta)) * r * texel;
                    float sd = linearizeDepth(texture(screenTexture, TexCoords + offset).r);
                    float sc = clamp(abs(sd - u_focus_distance) / u_focus_range, 0.0, 1.0);
                    float w = max(sc, coc);
                    color += texture(u_color_texture, TexCoords + offset).rgb * w;
                    total_weight += w;
                }
                if (total_weight > 0.0) color /= total_weight;
                else color = texture(u_color_texture, TexCoords).rgb;
                FragColor = vec4(color, 1.0);
            }
        )";
    } else if (effect_name == "motion_vector") {
        fs_src += R"(
            uniform float u_screen_w;
            uniform float u_screen_h;
            uniform mat4 u_reproj;
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                vec2 ndc = TexCoords * 2.0 - 1.0;
                float z_ndc = depth * 2.0 - 1.0;
                vec4 clip_pos = vec4(ndc, z_ndc, 1.0);
                vec4 prev_clip = u_reproj * clip_pos;
                prev_clip.xy /= prev_clip.w;
                vec2 prev_uv = prev_clip.xy * 0.5 + 0.5;
                vec2 velocity = TexCoords - prev_uv;
                FragColor = vec4(velocity, 0.0, 1.0);
            }
        )";
    } else if (effect_name == "motion_blur") {
        fs_src += R"(
            uniform sampler2D u_color_texture;
            uniform float u_intensity;
            uniform int u_samples;
            uniform float u_screen_w;
            uniform float u_screen_h;
            void main() {
                // screenTexture = motion_vector RT (rg = velocity)
                vec2 velocity = texture(screenTexture, TexCoords).rg * u_intensity;
                vec3 color = texture(u_color_texture, TexCoords).rgb;
                float total = 1.0;
                for (int i = 1; i < u_samples; ++i) {
                    float t = float(i) / float(u_samples);
                    vec2 sample_uv = TexCoords + velocity * t;
                    if (sample_uv.x >= 0.0 && sample_uv.x <= 1.0 && sample_uv.y >= 0.0 && sample_uv.y <= 1.0) {
                        color += texture(u_color_texture, sample_uv).rgb;
                        total += 1.0;
                    }
                }
                FragColor = vec4(color / total, 1.0);
            }
        )";
    } else if (effect_name == "ssr") {
        fs_src += R"(
            uniform sampler2D u_color_texture;
            uniform float u_max_distance;
            uniform float u_thickness;
            uniform float u_step_size;
            uniform int u_max_steps;
            uniform float u_near;
            uniform float u_far;
            uniform float u_screen_w;
            uniform float u_screen_h;
            float linearizeDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }
            vec3 reconstructNormal(vec2 uv) {
                vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
                float dc = linearizeDepth(texture(screenTexture, uv).r);
                float dl = linearizeDepth(texture(screenTexture, uv - vec2(texel.x, 0.0)).r);
                float dr = linearizeDepth(texture(screenTexture, uv + vec2(texel.x, 0.0)).r);
                float db = linearizeDepth(texture(screenTexture, uv - vec2(0.0, texel.y)).r);
                float dt = linearizeDepth(texture(screenTexture, uv + vec2(0.0, texel.y)).r);
                return normalize(vec3(dl - dr, db - dt, 2.0 * texel.x * dc));
            }
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                if (depth >= 1.0) { FragColor = vec4(0.0); return; }
                float lin_depth = linearizeDepth(depth);
                vec3 normal = reconstructNormal(TexCoords);
                vec3 view_dir = normalize(vec3(TexCoords * 2.0 - 1.0, 1.0));
                vec3 reflect_dir = reflect(view_dir, normal);
                vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
                vec2 ray_uv = TexCoords;
                float ray_depth = lin_depth;
                for (int i = 0; i < u_max_steps; ++i) {
                    ray_uv += reflect_dir.xy * texel * u_step_size;
                    if (ray_uv.x < 0.0 || ray_uv.x > 1.0 || ray_uv.y < 0.0 || ray_uv.y > 1.0) break;
                    float sd = linearizeDepth(texture(screenTexture, ray_uv).r);
                    ray_depth += reflect_dir.z * u_step_size;
                    float dd = ray_depth - sd;
                    if (dd > 0.0 && dd < u_thickness) {
                        float fade = 1.0 - float(i) / float(u_max_steps);
                        vec3 hit_color = texture(u_color_texture, ray_uv).rgb;
                        FragColor = vec4(hit_color * fade, fade);
                        return;
                    }
                }
                FragColor = vec4(0.0);
            }
        )";
    } else if (effect_name == "taa_resolve") {
        fs_src += R"(
            uniform sampler2D u_history;
            uniform sampler2D u_motion_vector;
            uniform float u_blend_factor;
            uniform float u_jitter_x;
            uniform float u_jitter_y;
            uniform int u_frame_index;
            uniform float u_screen_w;
            uniform float u_screen_h;
            void main() {
                vec3 current = texture(screenTexture, TexCoords).rgb;
                vec2 mv = texture(u_motion_vector, TexCoords).rg;
                vec2 history_uv = TexCoords - mv - vec2(u_jitter_x, u_jitter_y);
                history_uv = clamp(history_uv, vec2(0.0), vec2(1.0));
                vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
                vec3 m1 = vec3(0.0), m2 = vec3(0.0);
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        vec3 s = texture(screenTexture, TexCoords + vec2(dx, dy) * texel).rgb;
                        m1 += s; m2 += s * s;
                    }
                }
                m1 /= 9.0;
                vec3 sigma = sqrt(max(m2 / 9.0 - m1 * m1, vec3(0.0)));
                vec3 history = texture(u_history, history_uv).rgb;
                history = clamp(history, m1 - 1.25 * sigma, m1 + 1.25 * sigma);
                float velocity_len = length(mv * vec2(u_screen_w, u_screen_h));
                float vel_weight = clamp(velocity_len * 0.5, 0.0, 0.5);
                float alpha = (u_frame_index < 2) ? 1.0 : clamp(u_blend_factor + vel_weight, u_blend_factor, 1.0);
                FragColor = vec4(mix(history, current, alpha), 1.0);
            }
        )";
    } else if (effect_name == "deferred_lighting") {
        fs_src += R"(
            uniform sampler2D u_gbuf_normal;
            uniform sampler2D u_gbuf_position;
            uniform vec3 u_light_dir;
            uniform vec3 u_light_color;
            uniform float u_light_intensity;
            uniform float u_ambient;
            void main() {
                vec3 albedo   = texture(screenTexture, TexCoords).rgb;
                vec3 normal   = texture(u_gbuf_normal, TexCoords).rgb * 2.0 - 1.0;
                vec3 position = texture(u_gbuf_position, TexCoords).rgb;
                if (length(normal) < 0.01) { FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
                normal = normalize(normal);
                float NdotL = max(dot(normal, -normalize(u_light_dir)), 0.0);
                vec3 diffuse = albedo * u_light_color * u_light_intensity * NdotL;
                vec3 ambient = albedo * u_ambient;
                FragColor = vec4(diffuse + ambient, 1.0);
            }
        )";
    } else if (effect_name == "edge_detect") {
        fs_src += R"(
            uniform float u_thickness;
            uniform float u_depth_threshold;
            uniform float u_normal_threshold;
            uniform vec3 u_outline_color;
            uniform float u_near;
            uniform float u_far;
            uniform float u_screen_w;
            uniform float u_screen_h;

            float linearize_depth(float d) {
                float ndc = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - ndc * (u_far - u_near));
            }

            vec3 reconstruct_normal(vec2 uv, vec2 texel_size) {
                float dc = linearize_depth(texture(screenTexture, uv).r);
                float dl = linearize_depth(texture(screenTexture, uv - vec2(texel_size.x, 0.0)).r);
                float dr = linearize_depth(texture(screenTexture, uv + vec2(texel_size.x, 0.0)).r);
                float db = linearize_depth(texture(screenTexture, uv - vec2(0.0, texel_size.y)).r);
                float dt = linearize_depth(texture(screenTexture, uv + vec2(0.0, texel_size.y)).r);
                return normalize(vec3(dl - dr, db - dt, 2.0 * texel_size.x * dc));
            }

            void main() {
                vec2 base_texel = vec2(1.0 / u_screen_w, 1.0 / u_screen_h);
                vec2 texel = base_texel * u_thickness;

                float d_c = linearize_depth(texture(screenTexture, TexCoords).r);
                float d_l = linearize_depth(texture(screenTexture, TexCoords + vec2(-texel.x, 0.0)).r);
                float d_r = linearize_depth(texture(screenTexture, TexCoords + vec2( texel.x, 0.0)).r);
                float d_t = linearize_depth(texture(screenTexture, TexCoords + vec2(0.0,  texel.y)).r);
                float d_b = linearize_depth(texture(screenTexture, TexCoords + vec2(0.0, -texel.y)).r);

                float depth_diff = abs(d_l - d_r) + abs(d_t - d_b);
                float depth_edge = smoothstep(0.0, u_depth_threshold * d_c, depth_diff);

                vec3 n_c = reconstruct_normal(TexCoords, base_texel);
                vec3 n_l = reconstruct_normal(TexCoords + vec2(-texel.x, 0.0), base_texel);
                vec3 n_r = reconstruct_normal(TexCoords + vec2( texel.x, 0.0), base_texel);
                vec3 n_t = reconstruct_normal(TexCoords + vec2(0.0,  texel.y), base_texel);
                vec3 n_b = reconstruct_normal(TexCoords + vec2(0.0, -texel.y), base_texel);
                float normal_diff = length(n_l - n_r) + length(n_t - n_b);
                float normal_edge = smoothstep(0.0, u_normal_threshold, normal_diff);

                float edge = clamp(max(depth_edge, normal_edge), 0.0, 1.0);
                FragColor = vec4(u_outline_color, edge);
            }
        )";
    } else if (effect_name == "ui_overlay") {
        fs_src += R"(
            void main() {
                FragColor = texture(screenTexture, TexCoords);
            }
        )";
    } else if (effect_name == "volumetric_fog") {
        fs_src += R"(
            uniform sampler2D u_depth_tex;
            uniform vec3 u_fog_color;
            uniform float u_fog_density;
            uniform float u_height_falloff;
            uniform float u_height_offset;
            uniform float u_fog_start;
            uniform float u_fog_end;
            uniform float u_fog_steps;
            uniform float u_sun_scatter;
            uniform vec3 u_sun_dir;
            uniform vec3 u_camera_pos;
            uniform float u_near;
            uniform float u_far;
            uniform vec3 u_cam_right;
            uniform vec3 u_cam_up;
            uniform vec3 u_cam_fwd;
            uniform float u_tan_fov_y;
            uniform float u_aspect;

            float VFogLinearDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }

            void main() {
                vec4 scene = texture(screenTexture, TexCoords);
                float depth = texture(u_depth_tex, TexCoords).r;
                if (depth >= 0.9999) { FragColor = scene; return; }

                float viewZ = VFogLinearDepth(depth);
                vec2 ndc = TexCoords * 2.0 - 1.0;
                vec3 viewDir = normalize(u_cam_fwd
                    + ndc.x * u_cam_right * u_tan_fov_y * u_aspect
                    + ndc.y * u_cam_up    * u_tan_fov_y);
                float cosAngle = max(dot(viewDir, u_cam_fwd), 0.0001);
                float rayLen   = viewZ / cosAngle;

                float marchStart = u_fog_start;
                float marchEnd   = min(rayLen, u_fog_end);
                float steps = max(u_fog_steps, 1.0);
                if (marchEnd <= marchStart) { FragColor = scene; return; }

                float stepLen  = (marchEnd - marchStart) / steps;
                float cosTheta = dot(viewDir, -u_sun_dir);
                float g = 0.76; float g2 = g * g;
                float mie = (1.0 - g2) / (4.0 * 3.14159265 *
                    pow(max(1.0 + g2 - 2.0 * g * cosTheta, 0.001), 1.5));

                float transmittance = 1.0;
                vec3 inscatter = vec3(0.0);
                for (float i = 0.0; i < steps; i += 1.0) {
                    float t   = marchStart + (i + 0.5) * stepLen;
                    vec3 pos  = u_camera_pos + viewDir * t;
                    float h   = max(pos.y - u_height_offset, 0.0);
                    float den = u_fog_density * exp(-u_height_falloff * h);
                    float sT  = exp(-den * stepLen);
                    inscatter += transmittance * (1.0 - sT) *
                        (u_fog_color + mie * u_sun_scatter * vec3(1.0));
                    transmittance *= sT;
                    if (transmittance < 0.001) break;
                }
                FragColor = vec4(scene.rgb * transmittance + inscatter, scene.a);
            }
        )";
    } else if (effect_name == "decal") {
        fs_src += R"(
            uniform sampler2D u_depth_tex;
            uniform sampler2D u_decal_tex;
            uniform mat4 u_inv_model_vp;
            uniform vec4 u_color;
            uniform float u_angle_fade;
            uniform vec3 u_decal_up;

            void main() {
                float depth = texture(u_depth_tex, TexCoords).r;
                if (depth >= 0.9999) discard;

                vec4 clip = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
                vec4 local4 = u_inv_model_vp * clip;
                vec3 local = local4.xyz / local4.w;

                if (abs(local.x) > 0.5 || abs(local.y) > 0.5 || abs(local.z) > 0.5) discard;

                vec2 decal_uv = local.xz + 0.5;
                vec4 decal = texture(u_decal_tex, decal_uv) * u_color;

                float angle_factor = 1.0;
                if (u_angle_fade > 0.0) {
                    vec2 texel = 1.0 / textureSize(u_depth_tex, 0);
                    float dl = texture(u_depth_tex, TexCoords + vec2(-texel.x, 0.0)).r;
                    float dr = texture(u_depth_tex, TexCoords + vec2( texel.x, 0.0)).r;
                    float dt = texture(u_depth_tex, TexCoords + vec2(0.0,  texel.y)).r;
                    float db = texture(u_depth_tex, TexCoords + vec2(0.0, -texel.y)).r;
                    vec3 normal = normalize(vec3(dl - dr, dt - db, 2.0 * texel.x));
                    float facing = abs(dot(normal, u_decal_up));
                    angle_factor = smoothstep(0.0, 1.0 - u_angle_fade, facing);
                }
                FragColor = vec4(decal.rgb, decal.a * angle_factor);
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
    } else if (effect_name == "lum_adapt" && params.size() >= 7) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[0]));
        glUniform1i(glGetUniformLocation(shader, "prevAdaptedTex"), 1);
        glUniform1f(glGetUniformLocation(shader, "u_dt"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_speed_up"), params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_speed_down"), params[3]);
        glUniform1f(glGetUniformLocation(shader, "u_min_exposure"), params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_max_exposure"), params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_compensation"), params[6]);
    } else if (effect_name == "tonemapping" && params.size() >= 2) {
        glUniform1f(glGetUniformLocation(shader, "u_manual_exposure"), params[0]);
        unsigned int ae_tex_id = static_cast<unsigned int>(params[1]);
        if (ae_tex_id != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, ae_tex_id);
            glUniform1i(glGetUniformLocation(shader, "autoExposureTex"), 1);
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 0);
        }
        if (params.size() >= 4 && static_cast<unsigned int>(params[2]) != 0) {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_3D, static_cast<unsigned int>(params[2]));
            glUniform1i(glGetUniformLocation(shader, "u_lut"), 4);
            glUniform1f(glGetUniformLocation(shader, "u_lut_intensity"), params[3]);
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 0);
        }
    } else if (effect_name == "color_grading" && params.size() >= 2) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_3D, static_cast<unsigned int>(params[0]));
        glUniform1i(glGetUniformLocation(shader, "u_lut"), 4);
        glUniform1f(glGetUniformLocation(shader, "u_lut_intensity"), params[1]);
    } else if (effect_name == "bloom_composite") {
        const CompositeParamsView composite(params);
        const bool bloom_enabled = composite.Flag(CompositeParamsView::kBloomEnabled) &&
                                   composite.Texture(CompositeParamsView::kBloomTex) != 0;
        if (bloom_enabled) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, composite.Texture(CompositeParamsView::kBloomTex));
            glUniform1i(glGetUniformLocation(shader, "bloomBlur"), 1);
            glUniform1i(glGetUniformLocation(shader, "u_bloom_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_bloom_enabled"), 0);
        }
        glUniform1f(glGetUniformLocation(shader, "exposure"),
                    composite.Float(CompositeParamsView::kExposure, 1.0f));
        glUniform1f(glGetUniformLocation(shader, "bloomIntensity"),
                    composite.Float(CompositeParamsView::kBloomIntensity, 0.5f));
        const unsigned int ssao_tex = composite.Texture(CompositeParamsView::kSsaoTex);
        if (ssao_tex != 0) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, ssao_tex);
            glUniform1i(glGetUniformLocation(shader, "ssaoTexture"), 2);
            glUniform1i(glGetUniformLocation(shader, "u_ssao_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_ssao_enabled"), 0);
        }
        const unsigned int auto_exposure_tex = composite.Texture(CompositeParamsView::kAutoExposureTex);
        if (auto_exposure_tex != 0) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, auto_exposure_tex);
            glUniform1i(glGetUniformLocation(shader, "autoExposureTex"), 3);
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 0);
        }
        const unsigned int lut_tex = composite.Texture(CompositeParamsView::kLutTex);
        if (lut_tex != 0) {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_3D, lut_tex);
            glUniform1i(glGetUniformLocation(shader, "u_lut"), 4);
            glUniform1f(glGetUniformLocation(shader, "u_lut_intensity"),
                        composite.Float(CompositeParamsView::kLutIntensity, 0.0f));
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 0);
        }
        const unsigned int contact_shadow_tex = composite.Texture(CompositeParamsView::kContactShadowTex);
        if (contact_shadow_tex != 0) {
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, contact_shadow_tex);
            glUniform1i(glGetUniformLocation(shader, "contactShadowTex"), 5);
            glUniform1i(glGetUniformLocation(shader, "u_contact_shadow_enabled"), 1);
            glUniform1f(glGetUniformLocation(shader, "u_contact_shadow_strength"),
                        composite.Float(CompositeParamsView::kContactShadowStrength, 0.0f));
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_contact_shadow_enabled"), 0);
        }
        const bool vignette_enabled = composite.Flag(CompositeParamsView::kVignetteEnabled);
        glUniform1i(glGetUniformLocation(shader, "u_vignette_enabled"), vignette_enabled ? 1 : 0);
        if (vignette_enabled) {
            glUniform1f(glGetUniformLocation(shader, "u_vignette_intensity"),
                        composite.Float(CompositeParamsView::kVignetteIntensity, 0.0f));
            glUniform1f(glGetUniformLocation(shader, "u_vignette_radius"),
                        composite.Float(CompositeParamsView::kVignetteRadius, 0.75f));
            glUniform1f(glGetUniformLocation(shader, "u_vignette_softness"),
                        composite.Float(CompositeParamsView::kVignetteSoftness, 0.35f));
        }
        const bool film_grain_enabled = composite.Flag(CompositeParamsView::kFilmGrainEnabled);
        glUniform1i(glGetUniformLocation(shader, "u_film_grain_enabled"), film_grain_enabled ? 1 : 0);
        if (film_grain_enabled) {
            glUniform1f(glGetUniformLocation(shader, "u_film_grain_intensity"),
                        composite.Float(CompositeParamsView::kFilmGrainIntensity, 0.0f));
            glUniform1f(glGetUniformLocation(shader, "u_film_grain_time"),
                        composite.Float(CompositeParamsView::kFilmGrainTime, 0.0f));
        }
    } else if (effect_name == "ssao" && params.size() >= 6) {
        glUniform1f(glGetUniformLocation(shader, "u_radius"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_bias"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[3]);
        glUniform2f(glGetUniformLocation(shader, "u_screen_size"), params[4], params[5]);
    } else if (effect_name == "contact_shadow" && params.size() >= 10) {
        glUniform3f(glGetUniformLocation(shader, "u_light_dir"), params[0], params[1], params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[3]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[4]);
        glUniform2f(glGetUniformLocation(shader, "u_screen_size"), params[5], params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_strength"), params[7]);
        glUniform1f(glGetUniformLocation(shader, "u_step_size"), params[9]);
        glUniform1i(glGetUniformLocation(shader, "u_num_steps"), static_cast<int>(params[8]));
    } else if (effect_name == "ssao_apply" && params.size() >= 2) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[0]));
        glUniform1i(glGetUniformLocation(shader, "ssaoTexture"), 1);
        glUniform1f(glGetUniformLocation(shader, "exposure"), params[1]);
        if (params.size() >= 3 && static_cast<unsigned int>(params[2]) != 0) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[2]));
            glUniform1i(glGetUniformLocation(shader, "autoExposureTex"), 2);
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 0);
        }
        if (params.size() >= 5 && static_cast<unsigned int>(params[3]) != 0) {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_3D, static_cast<unsigned int>(params[3]));
            glUniform1i(glGetUniformLocation(shader, "u_lut"), 4);
            glUniform1f(glGetUniformLocation(shader, "u_lut_intensity"), params[4]);
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 0);
        }
    } else if (effect_name == "dof" && params.size() >= 8) {
        glUniform1f(glGetUniformLocation(shader, "u_focus_distance"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_focus_range"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_bokeh_radius"), params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[3]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[6]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[7]));
        glUniform1i(glGetUniformLocation(shader, "u_color_texture"), 1);
    } else if (effect_name == "motion_vector" && params.size() >= 18) {
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[1]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "u_reproj"), 1, GL_FALSE, &params[2]);
    } else if (effect_name == "motion_blur" && params.size() >= 5) {
        glUniform1f(glGetUniformLocation(shader, "u_intensity"), params[0]);
        glUniform1i(glGetUniformLocation(shader, "u_samples"), static_cast<int>(params[1]));
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[3]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[4]));
        glUniform1i(glGetUniformLocation(shader, "u_color_texture"), 1);
    } else if (effect_name == "ssr" && params.size() >= 9) {
        glUniform1f(glGetUniformLocation(shader, "u_max_distance"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_thickness"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_step_size"), params[2]);
        glUniform1i(glGetUniformLocation(shader, "u_max_steps"), static_cast<int>(params[3]));
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[7]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[8]));
        glUniform1i(glGetUniformLocation(shader, "u_color_texture"), 1);
    } else if (effect_name == "fxaa" && params.size() >= 2) {
        glUniform2f(glGetUniformLocation(shader, "u_resolution"), params[0], params[1]);
    } else if (effect_name == "taa_resolve" && params.size() >= 8) {
        // params: [history_tex, blend_factor, jitter_x, jitter_y, frame_index, mv_tex, screen_w, screen_h]
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[0]));
        glUniform1i(glGetUniformLocation(shader, "u_history"), 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[5]));
        glUniform1i(glGetUniformLocation(shader, "u_motion_vector"), 2);
        glUniform1f(glGetUniformLocation(shader, "u_blend_factor"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_jitter_x"), params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_jitter_y"), params[3]);
        glUniform1i(glGetUniformLocation(shader, "u_frame_index"), static_cast<int>(params[4]));
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[7]);
    } else if (effect_name == "deferred_lighting" && params.size() >= 10) {
        // params: [normal_tex, position_tex, light_dir.xyz, light_color.xyz, intensity, ambient]
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[0]));
        glUniform1i(glGetUniformLocation(shader, "u_gbuf_normal"), 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[1]));
        glUniform1i(glGetUniformLocation(shader, "u_gbuf_position"), 2);
        glUniform3f(glGetUniformLocation(shader, "u_light_dir"), params[2], params[3], params[4]);
        glUniform3f(glGetUniformLocation(shader, "u_light_color"), params[5], params[6], params[7]);
        glUniform1f(glGetUniformLocation(shader, "u_light_intensity"), params[8]);
        glUniform1f(glGetUniformLocation(shader, "u_ambient"), params[9]);
    } else if (effect_name == "edge_detect" && params.size() >= 10) {
        glUniform1f(glGetUniformLocation(shader, "u_thickness"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_depth_threshold"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_normal_threshold"), params[2]);
        glUniform3f(glGetUniformLocation(shader, "u_outline_color"), params[3], params[4], params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[7]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[8]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[9]);
    } else if (effect_name == "volumetric_fog" && params.size() >= 30) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[0]));
        glUniform1i(glGetUniformLocation(shader, "u_depth_tex"), 1);
        glUniform3f(glGetUniformLocation(shader, "u_fog_color"),   params[1], params[2], params[3]);
        glUniform1f(glGetUniformLocation(shader, "u_fog_density"),   params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_height_falloff"),params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_height_offset"), params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_fog_start"),     params[7]);
        glUniform1f(glGetUniformLocation(shader, "u_fog_end"),       params[8]);
        glUniform1f(glGetUniformLocation(shader, "u_fog_steps"),     params[9]);
        glUniform1f(glGetUniformLocation(shader, "u_sun_scatter"),   params[10]);
        glUniform3f(glGetUniformLocation(shader, "u_sun_dir"),    params[11], params[12], params[13]);
        glUniform3f(glGetUniformLocation(shader, "u_camera_pos"), params[14], params[15], params[16]);
        glUniform1f(glGetUniformLocation(shader, "u_near"),  params[17]);
        glUniform1f(glGetUniformLocation(shader, "u_far"),   params[18]);
        glUniform3f(glGetUniformLocation(shader, "u_cam_right"), params[19], params[20], params[21]);
        glUniform3f(glGetUniformLocation(shader, "u_cam_up"),    params[22], params[23], params[24]);
        glUniform3f(glGetUniformLocation(shader, "u_cam_fwd"),   params[25], params[26], params[27]);
        glUniform1f(glGetUniformLocation(shader, "u_tan_fov_y"), params[28]);
        glUniform1f(glGetUniformLocation(shader, "u_aspect"),    params[29]);
    } else if (effect_name == "decal" && params.size() >= 26) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[0]));
        glUniform1i(glGetUniformLocation(shader, "u_depth_tex"), 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(params[1]));
        glUniform1i(glGetUniformLocation(shader, "u_decal_tex"), 2);
        glUniformMatrix4fv(glGetUniformLocation(shader, "u_inv_model_vp"), 1, GL_FALSE, &params[2]);
        glUniform4f(glGetUniformLocation(shader, "u_color"), params[18], params[19], params[20], params[21]);
        glUniform1f(glGetUniformLocation(shader, "u_angle_fade"), params[22]);
        glUniform3f(glGetUniformLocation(shader, "u_decal_up"), params[23], params[24], params[25]);
    }

    glDisable(GL_DEPTH_TEST);
    if (effect_name == "ui_overlay" || effect_name == "decal") {
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
        global_state_.current_frame_stats.draw_calls += 1;
        global_state_.current_frame_stats.max_batch_sprites = std::max(global_state_.current_frame_stats.max_batch_sprites, batch_sprites);

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
// 帧生命周期
// ============================================================

void GLDrawExecutor::BeginFrame() {
    global_state_.current_frame_stats = {};
}

void GLDrawExecutor::EndFrame() {
    global_state_.last_frame_stats = global_state_.current_frame_stats;
    glFlush();
}

} // namespace render
} // namespace dse
