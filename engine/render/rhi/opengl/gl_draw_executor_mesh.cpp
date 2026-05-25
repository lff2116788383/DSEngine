/**
 * @file gl_draw_executor_mesh.cpp
 * @brief GLDrawExecutor - 3D PBR mesh drawing (split from gl_draw_executor.cpp)
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
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT 0x0002
#endif
#ifndef GL_MAP_INVALIDATE_BUFFER_BIT
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <limits>
#include <sstream>

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

void GLDrawExecutor::DrawMeshBatch(const std::vector<MeshDrawItem>& items,
                                     const glm::mat4& view,
                                     const glm::mat4& projection,
                                     GLPipelineStateManager& state_mgr,
                                     GLShaderManager& shader_mgr,
                                     GLResourceManager& resource_mgr,
                                     UBOManager& ubo_mgr) {
    if (items.empty()) return;

    // 每次 pass 重置共享模板跟踪（避免跨 pass 误判）
    last_shared_vtx_ptr_ = nullptr;
    last_shared_vtx_count_ = 0;
    // [BlackRectDiag] GL state at DrawMeshBatch entry (scene RT only)
    {
        static int scene_call = 0;
        if (scene_call < 6) {
            GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
            GLboolean blend_on   = glIsEnabled(GL_BLEND);
            GLint vp[4] = {0}; glGetIntegerv(GL_VIEWPORT, vp);
            GLint fbo = 0;     glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
            GLint active_ps = state_mgr.active_pipeline_state();
            // 只记录 scene RT（非 shadow map 尺寸）
            if (vp[2] <= 1600 && vp[3] <= 1200) {
                DEBUG_LOG_INFO("[MeshDiag] call={} depth_test={} blend={} vp=({},{},{},{}) fbo={} active_ps={}",
                    scene_call, (int)depth_test, (int)blend_on,
                    vp[0], vp[1], vp[2], vp[3], fbo, active_ps);
                ++scene_call;
            }
        }
    }
    global_state_.current_frame_stats.mesh_count += static_cast<int>(items.size());
    dse::render::UpdateSortBatchStats(global_state_.current_frame_stats, items);

    glm::mat4 vp = projection * view;
    glm::mat4 inv_view = glm::inverse(view);
#ifdef DSE_VSE_1522_DIAG
    static int vse1522_depth_diag_frames = 0;
    const bool emit_vse1522_depth_diag = vse1522_depth_diag_frames < 5 &&
        std::any_of(items.begin(), items.end(), [](const MeshDrawItem& item) {
            return !item.debug_label.empty();
        });
    if (emit_vse1522_depth_diag) {
        // 检查当前 FBO 绑定和 depth attachment 状态
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

    if (is_depth_only_pass_) {
        shader_mgr.InitShadowShader();
        glUseProgram(shader_mgr.shadow_shader_handle());
    } else if (gbuffer_mode) {
        shader_mgr.InitGBufferShader();
        glUseProgram(shader_mgr.gbuffer_shader_handle());
    } else {
        glUseProgram(shader_mgr.pbr_shader_handle());
    }
    const auto& loc = shader_mgr.pbr_locations();
    const auto& slots = shader_mgr.pbr_texture_slots();

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

    // PerScene UBO 需要在 for 循环中 shading_mode 变更时引用，声明在此
    PerSceneUBO per_scene{};
    if (!gbuffer_mode) {
    // === PerScene UBO：使用第一个 item 的光照数据（同一批次光照通常一致） ===
    const auto& first_item = items[0];
    per_scene.light_dir_and_enabled = glm::vec4(first_item.light_direction, first_item.lighting_enabled ? 1.0f : 0.0f);
    per_scene.light_color_and_ambient = glm::vec4(first_item.light_color, first_item.ambient_intensity);
    per_scene.light_params = glm::vec4(first_item.light_intensity, first_item.shadow_strength, first_item.receive_shadow ? 1.0f : 0.0f, static_cast<float>(first_item.shading_mode));
    per_scene.cascade_splits = glm::vec4(global_state_.cascade_splits[0], global_state_.cascade_splits[1], global_state_.cascade_splits[2], static_cast<float>(first_item.wboit_mode));

    // 编辑器 Unlit 模式: 禁用光照，仅显示 albedo
    if (global_state_.force_unlit) {
        per_scene.light_dir_and_enabled.w = 0.0f;
    }
    for (int i = 0; i < 3; ++i) {
        per_scene.light_space_matrices[i] = global_state_.light_space_matrix[i];
    }
    ubo_mgr.UploadPerScene(per_scene);

    // === LightProbeData UBO：SH 球谐系数 ===
    LightProbeDataUBO lp_data{};
    for (int i = 0; i < 9; ++i) lp_data.sh_coefficients[i] = global_state_.light_probe_sh[i];
    lp_data.probe_params = glm::vec4(global_state_.light_probe_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    ubo_mgr.UploadLightProbeData(lp_data);
    } // !gbuffer_mode

    // 绑定所有 UBO
    ubo_mgr.BindAll();

    // GL 3.3 UBO fallback：BindAll() 会将 UBOManager 的空 PointLights/SpotLights UBO
    // 绑定到 binding 3/4，覆盖 LightBuffer::Bind() 的透明映射。
    // 此处从 MeshDrawItem 的光源列表填充并上传，与 Vulkan 后端 UpdatePointSpotLightUBOs 对齐。
    if (!shader_mgr.supports_ssbo() && !items.empty()) {
        const auto& ref = items[0];

        PointLightsUBO pl_ubo{};
        pl_ubo.u_point_light_count = static_cast<int>(
            (std::min)(ref.point_lights.size(), static_cast<size_t>(kMaxPointLightsUBO)));
        for (int i = 0; i < pl_ubo.u_point_light_count; ++i) {
            const auto& s = ref.point_lights[i];
            auto& d = pl_ubo.u_point_lights[i];
            d.color = s.color;   d.intensity = s.intensity;
            d.position = s.position; d.radius = s.radius;
            d.cast_shadow = s.cast_shadow ? 1 : 0;
            d.shadow_index = s.shadow_index;
        }
        ubo_mgr.UploadPointLights(pl_ubo);

        SpotLightsUBO sl_ubo{};
        sl_ubo.u_spot_light_count = static_cast<int>(
            (std::min)(ref.spot_lights.size(), static_cast<size_t>(kMaxSpotLightsUBO)));
        for (int i = 0; i < sl_ubo.u_spot_light_count; ++i) {
            const auto& s = ref.spot_lights[i];
            auto& d = sl_ubo.u_spot_lights[i];
            d.color = s.color;   d.intensity = s.intensity;
            d.position = s.position; d.radius = s.radius;
            d.direction = s.direction; d.inner_cone = s.inner_cone;
            d.outer_cone = s.outer_cone;
            d.cast_shadow = s.cast_shadow ? 1 : 0;
            d.shadow_index = s.shadow_index;
        }
        ubo_mgr.UploadSpotLights(sl_ubo);
    }

    // 纹理采样器（全局设置，非逐对象变化）绑定
    unsigned int active_shader = gbuffer_mode ? shader_mgr.gbuffer_shader_handle() : shader_mgr.pbr_shader_handle();
    glUniform1i(glGetUniformLocation(active_shader, "u_texture"), slots.albedo);
    const int gbuffer_model_loc = gbuffer_mode ? glGetUniformLocation(active_shader, "u_model") : -1;

    // DDGI uniforms（全局，每 batch 一次）— 使用缓存的 location
    if (!gbuffer_mode && global_state_.ddgi_enabled && global_state_.ddgi_irradiance_atlas != 0) {
        glUniform1f(loc.ddgi_enabled, 1.0f);
        glUniform3fv(loc.ddgi_grid_origin, 1, &global_state_.ddgi_grid_origin.x);
        glUniform3fv(loc.ddgi_grid_spacing, 1, &global_state_.ddgi_grid_spacing.x);
        glUniform3iv(loc.ddgi_grid_resolution, 1, &global_state_.ddgi_grid_resolution.x);
        glUniform1i(loc.ddgi_irradiance_texels, global_state_.ddgi_irradiance_texels);
        glUniform1f(loc.ddgi_gi_intensity, global_state_.ddgi_gi_intensity);
        glUniform1f(loc.ddgi_normal_bias, global_state_.ddgi_normal_bias);
        glActiveTexture(GL_TEXTURE0 + slots.ddgi_atlas);
        glBindTexture(GL_TEXTURE_2D, global_state_.ddgi_irradiance_atlas);
        glUniform1i(loc.ddgi_irradiance_atlas, slots.ddgi_atlas);
    } else if (!gbuffer_mode) {
        glUniform1f(loc.ddgi_enabled, 0.0f);
    }

    // === Bone SSBO: 收集所有蒙皮实例骨骼矩阵，一次上传 ===
    // bone_offsets[i]: 非 instanced item 的单一 bone offset
    // per_inst_bone_offsets[i]: instanced skinned item 的每实例 bone offset
    // 优先使用 bone_palette（去重后数据量极小），fallback 到 per_instance_bones
    std::vector<int> bone_offsets(items.size(), 0);
    std::vector<std::vector<int>> per_inst_bone_offsets(items.size());
    // palette_base_offsets[i]: bone_palette 模式下，palette 条目 j 的 SSBO 起始偏移
    std::vector<std::vector<int>> palette_base_offsets(items.size());
    {
        size_t total_bones = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& it = items[i];
            if (it.skinned && !it.bone_palette.empty()) {
                // Palette 模式：只上传 palette 条目（去重后通常 2-60 条）
                auto& pbo = palette_base_offsets[i];
                pbo.resize(it.bone_palette.size());
                for (size_t p = 0; p < it.bone_palette.size(); ++p) {
                    pbo[p] = static_cast<int>(total_bones);
                    total_bones += std::min(it.bone_palette[p].size(), static_cast<size_t>(kMaxBones));
                }
                // 构建 per-instance offsets 从 palette index 映射
                auto& offsets = per_inst_bone_offsets[i];
                offsets.resize(it.instance_bone_palette_idx.size());
                for (size_t j = 0; j < it.instance_bone_palette_idx.size(); ++j) {
                    int pidx = it.instance_bone_palette_idx[j];
                    offsets[j] = (pidx >= 0 && pidx < static_cast<int>(pbo.size())) ? pbo[pidx] : 0;
                }
            } else if (it.skinned && !it.per_instance_bones.empty()) {
                auto& offsets = per_inst_bone_offsets[i];
                offsets.resize(it.per_instance_bones.size());
                for (size_t j = 0; j < it.per_instance_bones.size(); ++j) {
                    offsets[j] = static_cast<int>(total_bones);
                    total_bones += std::min(it.per_instance_bones[j].size(), static_cast<size_t>(kMaxBones));
                }
            } else if (it.skinned && !it.bone_matrices.empty()) {
                bone_offsets[i] = static_cast<int>(total_bones);
                total_bones += std::min(it.bone_matrices.size(), static_cast<size_t>(kMaxBones));
            }
        }
        if (total_bones > 0) {
            if (!bone_ssbo_uploaded_this_frame_) {
                const size_t ssbo_bytes = total_bones * sizeof(glm::mat4);
                if (bone_ssbo_ == 0) {
                    glGenBuffers(1, &bone_ssbo_);
                }
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, bone_ssbo_);
                if (ssbo_bytes > bone_ssbo_capacity_) {
                    bone_ssbo_capacity_ = ssbo_bytes;
                }
                // orphan: 帧内首次上传，让驱动分配新内存避免 GPU stall
                glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bone_ssbo_capacity_), nullptr, GL_STREAM_DRAW);
                void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(ssbo_bytes),
                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
                if (ptr) {
                    auto* dst = static_cast<glm::mat4*>(ptr);
                    for (size_t i = 0; i < items.size(); ++i) {
                        const auto& it = items[i];
                        if (it.skinned && !it.bone_palette.empty()) {
                            for (size_t p = 0; p < it.bone_palette.size(); ++p) {
                                size_t count = std::min(it.bone_palette[p].size(), static_cast<size_t>(kMaxBones));
                                std::memcpy(dst + palette_base_offsets[i][p],
                                           it.bone_palette[p].data(), count * sizeof(glm::mat4));
                            }
                        } else if (it.skinned && !it.per_instance_bones.empty()) {
                            for (size_t j = 0; j < it.per_instance_bones.size(); ++j) {
                                size_t count = std::min(it.per_instance_bones[j].size(), static_cast<size_t>(kMaxBones));
                                std::memcpy(dst + per_inst_bone_offsets[i][j],
                                           it.per_instance_bones[j].data(), count * sizeof(glm::mat4));
                            }
                        } else if (it.skinned && !it.bone_matrices.empty()) {
                            size_t count = std::min(it.bone_matrices.size(), static_cast<size_t>(kMaxBones));
                            std::memcpy(dst + bone_offsets[i], it.bone_matrices.data(), count * sizeof(glm::mat4));
                        }
                    }
                    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                }
                bone_ssbo_uploaded_this_frame_ = true;
            }
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, bone_ssbo_);
        }
    }

    // === Instance SSBO: 全部 skinned instanced item 一次性打包上传 ===
    struct SkinnedInstGPU {
        glm::mat4 model;
        int bone_offset;
        int _pad[3];
    };
    constexpr GLint kSsboAlignment = 256;
    // 每次 DrawMeshBatch 都要计算 offset（batch cache 保证 items 顺序一致）
    std::vector<GLintptr> inst_ssbo_offsets(items.size(), -1);
    std::vector<GLsizeiptr> inst_ssbo_sizes(items.size(), 0);
    {
        size_t total_inst_bytes = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& it = items[i];
            const bool si = it.skinned
                && (!it.per_instance_bones.empty() || !it.bone_palette.empty())
                && it.instance_transforms.size() > 1;
            if (si) {
                inst_ssbo_offsets[i] = static_cast<GLintptr>(total_inst_bytes);
                size_t item_bytes = it.instance_transforms.size() * sizeof(SkinnedInstGPU);
                inst_ssbo_sizes[i] = static_cast<GLsizeiptr>(item_bytes);
                total_inst_bytes += ((item_bytes + kSsboAlignment - 1) / kSsboAlignment) * kSsboAlignment;
            }
        }
        // 仅首 pass 上传，后续 pass 直接 bind range
        if (total_inst_bytes > 0 && !inst_ssbo_uploaded_this_frame_) {
            if (skinned_inst_ssbo_ == 0) {
                glGenBuffers(1, &skinned_inst_ssbo_);
            }
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, skinned_inst_ssbo_);
            if (total_inst_bytes > skinned_inst_ssbo_capacity_) {
                skinned_inst_ssbo_capacity_ = total_inst_bytes;
            }
            glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(skinned_inst_ssbo_capacity_), nullptr, GL_STREAM_DRAW);
            void* all_ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                static_cast<GLsizeiptr>(total_inst_bytes),
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            if (all_ptr) {
                for (size_t i = 0; i < items.size(); ++i) {
                    if (inst_ssbo_offsets[i] < 0) continue;
                    const auto& it = items[i];
                    const auto& inst_bo = per_inst_bone_offsets[i];
                    auto* dst = reinterpret_cast<SkinnedInstGPU*>(
                        static_cast<char*>(all_ptr) + inst_ssbo_offsets[i]);
                    for (size_t inst = 0; inst < it.instance_transforms.size(); ++inst) {
                        dst[inst].model = it.instance_transforms[inst];
                        dst[inst].bone_offset = (inst < inst_bo.size()) ? inst_bo[inst] : 0;
                        dst[inst]._pad[0] = dst[inst]._pad[1] = dst[inst]._pad[2] = 0;
                    }
                }
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            }
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            inst_ssbo_uploaded_this_frame_ = true;
        }
    }

    unsigned int last_texture_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_normal_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_metallic_roughness_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_emissive_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_occlusion_map_handle = std::numeric_limits<unsigned int>::max();
    unsigned int last_blend_mode = std::numeric_limits<unsigned int>::max();
    int last_shading_mode = items[0].shading_mode;

    for (size_t item_idx = 0; item_idx < items.size(); ++item_idx) {
        const auto& item = items[item_idx];
        if ((item.vertices.empty() && !item.shared_vertex_ptr) || (item.indices.empty() && !item.shared_index_ptr)) continue;

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

            // 检查 GL depth state
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
        glActiveTexture(GL_TEXTURE0 + slots.albedo);
        glBindTexture(GL_TEXTURE_2D, tex);

        if (last_normal_map_handle != item.normal_map_handle) {
            if (last_normal_map_handle != std::numeric_limits<unsigned int>::max()) {
                global_state_.current_frame_stats.material_switches += 1;
            }
            last_normal_map_handle = item.normal_map_handle;
        }
        if (item.normal_map_handle != 0) {
            glActiveTexture(GL_TEXTURE0 + slots.normal);
            glBindTexture(GL_TEXTURE_2D, item.normal_map_handle);
            glUniform1i(loc.normal_map, slots.normal);
        }

        if (last_metallic_roughness_map_handle != item.metallic_roughness_map_handle) {
            if (last_metallic_roughness_map_handle != std::numeric_limits<unsigned int>::max()) {
                global_state_.current_frame_stats.material_switches += 1;
            }
            last_metallic_roughness_map_handle = item.metallic_roughness_map_handle;
        }
        if (item.metallic_roughness_map_handle != 0) {
            glActiveTexture(GL_TEXTURE0 + slots.metallic_roughness);
            glBindTexture(GL_TEXTURE_2D, item.metallic_roughness_map_handle);
            glUniform1i(loc.metallic_roughness_map, slots.metallic_roughness);
        }

        if (last_emissive_map_handle != item.emissive_map_handle) {
            if (last_emissive_map_handle != std::numeric_limits<unsigned int>::max()) {
                global_state_.current_frame_stats.material_switches += 1;
            }
            last_emissive_map_handle = item.emissive_map_handle;
        }
        if (item.emissive_map_handle != 0) {
            glActiveTexture(GL_TEXTURE0 + slots.emissive);
            glBindTexture(GL_TEXTURE_2D, item.emissive_map_handle);
            glUniform1i(loc.emissive_map, slots.emissive);
        }

        if (last_occlusion_map_handle != item.occlusion_map_handle) {
            if (last_occlusion_map_handle != std::numeric_limits<unsigned int>::max()) {
                global_state_.current_frame_stats.material_switches += 1;
            }
            last_occlusion_map_handle = item.occlusion_map_handle;
        }
        if (item.occlusion_map_handle != 0) {
            glActiveTexture(GL_TEXTURE0 + slots.occlusion);
            glBindTexture(GL_TEXTURE_2D, item.occlusion_map_handle);
            glUniform1i(loc.occlusion_map, slots.occlusion);
        }

        // Terrain splatmap
        if (loc.splat_enabled != -1) {
            glUniform1f(loc.splat_enabled, item.splat_enabled ? 1.0f : 0.0f);
        }
        if (item.splat_enabled) {
            if (loc.splat_tiling != -1) {
                glUniform4fv(loc.splat_tiling, 1, &item.splat_tiling.x);
            }
            if (item.splat_weight_map_handle != 0 && loc.splat_weight_map != -1) {
                glActiveTexture(GL_TEXTURE0 + slots.splat_weight);
                glBindTexture(GL_TEXTURE_2D, item.splat_weight_map_handle);
                glUniform1i(loc.splat_weight_map, slots.splat_weight);
            }
            for (int si = 0; si < 4; ++si) {
                if (item.splat_layer_handles[si] != 0 && loc.splat_layer[si] != -1) {
                    glActiveTexture(GL_TEXTURE0 + slots.splat_layer_base + si);
                    glBindTexture(GL_TEXTURE_2D, item.splat_layer_handles[si]);
                    glUniform1i(loc.splat_layer[si], slots.splat_layer_base + si);
                }
            }
        }

        if (!gbuffer_mode) {
        // === PerMaterial UBO：每材质切换上传 ===
        PerMaterialUBO per_mat;
        if (global_state_.overdraw_mode) {
            // Overdraw 可视化：每个 fragment 输出固定低亮度颜色，
            // 通过 additive blend 叠加后高亮区域表示过度绘制
            per_mat.albedo = glm::vec4(0.1f, 0.04f, 0.02f, 0.0f);
            per_mat.roughness_ao = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
            per_mat.emissive = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            per_mat.flags = glm::vec4(0.0f);
        } else {
        per_mat.albedo = glm::vec4(item.material_albedo, item.material_metallic);
        per_mat.roughness_ao = glm::vec4(item.material_roughness, item.material_ao, item.material_normal_strength, item.material_alpha_cutoff);
        per_mat.emissive = glm::vec4(item.material_emissive, item.material_alpha_test ? 1.0f : 0.0f);
        per_mat.flags = glm::vec4(
            item.normal_map_handle != 0 ? 1.0f : 0.0f,
            item.metallic_roughness_map_handle != 0 ? 1.0f : 0.0f,
            item.emissive_map_handle != 0 ? 1.0f : 0.0f,
            item.occlusion_map_handle != 0 ? 1.0f : 0.0f
        );
        }
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

        // 点光源/聚光灯数据：GL 4.3+ 用 LightBuffer SSBO 提供；GL 3.3 已由上方 UBO fallback 上传
        // 仅绑定点光源阴影贴图
        for (int i = 0; i < 4; ++i) {
            if (loc.point_shadow_map[i] != -1) {
                glActiveTexture(GL_TEXTURE0 + slots.point_shadow_base + i);
                glBindTexture(GL_TEXTURE_CUBE_MAP, global_state_.point_shadow_map[i]);
                glUniform1i(loc.point_shadow_map[i], slots.point_shadow_base + i);
            }
        }

        // 聚光灯空间矩阵 UBO 上传
        {
            SpotLightDataUBO sld_ubo{};
            for (int i = 0; i < 4; ++i)
                sld_ubo.u_spot_light_space_matrices[i] = global_state_.spot_light_space_matrix[i];
            ubo_mgr.UploadSpotLightData(sld_ubo);
        }

        // CSM 阴影贴图（sampler2DShadow 需要硬件混合比较）绑定
        for (int i = 0; i < 3; ++i) {
            if (loc.shadow_map[i] != -1) {
                glActiveTexture(GL_TEXTURE0 + slots.shadow_base + i);
                glBindTexture(GL_TEXTURE_2D, global_state_.shadow_map[i]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                glUniform1i(loc.shadow_map[i], slots.shadow_base + i);
            }
        }
        for (int i = 0; i < 4; ++i) {
            if (loc.spot_shadow_map[i] != -1) {
                glActiveTexture(GL_TEXTURE0 + slots.spot_shadow_base + i);
                glBindTexture(GL_TEXTURE_2D, global_state_.spot_shadow_map[i]);
                glUniform1i(loc.spot_shadow_map[i], slots.spot_shadow_base + i);
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

        // 面剔除设置
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

        // 骨骼动画: u_skinned + u_bone_offset（bone SSBO 已在 draw loop 前一次上传）
        // skinned==2 表示 hardware instanced skinned（由 instanced path 设置）
        const bool skinned_instanced = item.skinned
            && (!item.per_instance_bones.empty() || !item.bone_palette.empty())
            && item.instance_transforms.size() > 1;
        {
            const int skinned_loc = is_depth_only_pass_ ? shader_mgr.shadow_locations().skinned : loc.skinned;
            if (skinned_loc != -1) {
                glUniform1i(skinned_loc, skinned_instanced ? 2 : (item.skinned ? 1 : 0));
            }
        }
        {
            const int bo_loc = is_depth_only_pass_ ? shader_mgr.shadow_locations().bone_offset : loc.bone_offset;
            if (bo_loc != -1 && !skinned_instanced) {
                glUniform1i(bo_loc, item.skinned ? bone_offsets[item_idx] : 0);
            }
        }

        // 变形目标（push constant → 独立 uniform + MorphWeights UBO）设置
        {
            const int morph_loc = is_depth_only_pass_ ? shader_mgr.shadow_locations().morph_enabled : loc.morph_enabled;
            if (morph_loc != -1) glUniform1i(morph_loc, item.morph_enabled ? 1 : 0);
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

        // 模型矩阵设置
        if (!is_instanced) {
            int model_loc = is_depth_only_pass_ ? shader_mgr.shadow_locations().model
                          : (gbuffer_mode ? gbuffer_model_loc : loc.model);
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

        // 解析顶点/索引数据源：优先使用 shared_vertex_ptr（零拷贝共享模板）
        const BatchVertex* vtx_data = item.shared_vertex_ptr ? item.shared_vertex_ptr : item.vertices.data();
        const uint32_t* idx_data = item.shared_index_ptr ? item.shared_index_ptr : reinterpret_cast<const uint32_t*>(item.indices.data());
        const size_t vtx_count = item.shared_vertex_ptr ? item.shared_vertex_count : item.vertices.size();
        const size_t idx_count = item.shared_index_ptr ? item.shared_index_count : item.indices.size();

        // === Static Mesh VBO 缓存查找 ===
        // 如果 shared_vertex_ptr 可用，尝试使用持久化 VAO（零上传）
        const StaticMeshEntry* static_entry = nullptr;
        if (vtx_data && idx_data && vtx_count > 0 && idx_count > 0) {
            const void* cache_key = item.shared_vertex_ptr ? static_cast<const void*>(item.shared_vertex_ptr)
                                                           : static_cast<const void*>(item.vertices.data());
            if (cache_key) {
                auto cache_it = static_mesh_cache_.find(cache_key);
                if (cache_it != static_mesh_cache_.end()) {
                    static_entry = &cache_it->second;
                } else {
                    auto entry = CreateStaticMeshVAO(vtx_data, vtx_count, idx_data, idx_count);
                    auto [it, _] = static_mesh_cache_.emplace(cache_key, entry);
                    static_entry = &it->second;
                }
            }
        }

        // 绘制
        if (item.vao_override) {
            glBindVertexArray(item.vao_override.raw());
            if (item.ebo_override) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, item.ebo_override.raw());
            }
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(item.index_count_override), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        } else if (is_instanced && skinned_instanced) {
            // --- Hardware Instanced Skinned Draw ---
            const size_t instance_count = item.instance_transforms.size();

            // 使用预打包 SSBO 的对应 range（帧内已上传）
            if (inst_ssbo_offsets[item_idx] >= 0 && inst_ssbo_sizes[item_idx] > 0) {
                glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 10, skinned_inst_ssbo_,
                    inst_ssbo_offsets[item_idx], inst_ssbo_sizes[item_idx]);
            }

            if (static_entry) {
                glBindVertexArray(static_entry->vao);
            } else {
                glBindVertexArray(mesh_vao_handle_.raw());
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ibo_handle_);
                if (update_buffer_fn_) {
                    update_buffer_fn_(mesh_vbo_handle_, 0, vtx_count * sizeof(BatchVertex), vtx_data, false);
                    update_buffer_fn_(mesh_ibo_handle_, 0, idx_count * sizeof(uint32_t), idx_data, true);
                }
            }
            glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(idx_count),
                                     GL_UNSIGNED_INT, nullptr,
                                     static_cast<GLsizei>(instance_count));
            glBindVertexArray(0);

            global_state_.current_frame_stats.draw_calls += 1;
            global_state_.current_frame_stats.instanced_draw_calls += 1;
            global_state_.current_frame_stats.instanced_mesh_count += static_cast<int>(instance_count);
        } else if (is_instanced) {
            // --- Non-skinned Pseudo-Instancing path ---
            if (static_entry) {
                glBindVertexArray(static_entry->vao);
            } else {
                glBindVertexArray(mesh_vao_handle_.raw());
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ibo_handle_);
                if (update_buffer_fn_) {
                    update_buffer_fn_(mesh_vbo_handle_, 0, vtx_count * sizeof(BatchVertex), vtx_data, false);
                    update_buffer_fn_(mesh_ibo_handle_, 0, idx_count * sizeof(uint32_t), idx_data, true);
                }
            }

            const size_t instance_count = item.instance_transforms.size();
            int model_loc = is_depth_only_pass_ ? shader_mgr.shadow_locations().model
                          : (gbuffer_mode ? gbuffer_model_loc : loc.model);
            for (size_t inst = 0; inst < instance_count; ++inst) {
                if (model_loc != -1) {
                    glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(item.instance_transforms[inst]));
                }
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(idx_count), GL_UNSIGNED_INT, nullptr);
            }
            glBindVertexArray(0);

            global_state_.current_frame_stats.instanced_draw_calls += 1;
            global_state_.current_frame_stats.instanced_mesh_count += static_cast<int>(instance_count);
        } else {
            if (static_entry) {
                glBindVertexArray(static_entry->vao);
            } else {
                glBindVertexArray(mesh_vao_handle_.raw());
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ibo_handle_);
                if (update_buffer_fn_) {
                    update_buffer_fn_(mesh_vbo_handle_, 0, vtx_count * sizeof(BatchVertex), vtx_data, false);
                    update_buffer_fn_(mesh_ibo_handle_, 0, idx_count * sizeof(uint32_t), idx_data, true);
                }
            }
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(idx_count), GL_UNSIGNED_INT, nullptr);
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

        // 分阶段混合采样：Monster/OceanPlane 绘制后立即采样
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

            // 额外诊断：记录当前 GL depth state
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
        {
            const int tri_per_draw = item.vao_override
                ? static_cast<int>(item.index_count_override / 3)
                : static_cast<int>(item.indices.size() / 3);
            const int eff_inst = is_instanced ? static_cast<int>(item.instance_transforms.size()) : 1;
            global_state_.current_frame_stats.triangle_count += tri_per_draw * eff_inst;
        }
    }
    if (emit_vse1522_depth_diag) {
#ifdef DSE_VSE_1522_DIAG
        // 额外诊断：检查当前 FBO 绑定和 depth attachment 状态
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

        // 额外诊断：记录当前 GL depth state
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
} // namespace render
} // namespace dse