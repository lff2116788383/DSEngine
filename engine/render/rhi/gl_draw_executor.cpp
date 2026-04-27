/**
 * @file gl_draw_executor.cpp
 * @brief GLDrawExecutor 实现 - 绘制执行器
 */

#include "engine/render/rhi/gl_draw_executor.h"
#include "engine/render/rhi/gl_pipeline_state_manager.h"
#include "engine/render/rhi/gl_shader_manager.h"
#include "engine/render/rhi/gl_resource_manager.h"
#include "engine/render/rhi/ubo_manager.h"
#include "engine/platform/screen.h"
#include "engine/base/debug.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <functional>
#include <cstddef>

constexpr size_t MAX_SPRITES = 10000;
constexpr size_t MAX_VERTICES = MAX_SPRITES * 4;
constexpr size_t MAX_INDICES = MAX_SPRITES * 6;

namespace dse {
namespace render {

// ============================================================
// 几何缓冲区初始化
// ============================================================

void GLDrawExecutor::InitGeometryBuffers(InitCreateVaoFn create_vao_fn,
                                          InitCreateVboFn create_vbo_fn,
                                          InitUpdateVboFn update_vbo_fn) {
    // 构建精灵索引模板
    std::vector<unsigned short> indices(MAX_INDICES);
    for (size_t i = 0, j = 0; i < MAX_SPRITES; ++i) {
        indices[j++] = static_cast<unsigned short>(i * 4 + 0);
        indices[j++] = static_cast<unsigned short>(i * 4 + 1);
        indices[j++] = static_cast<unsigned short>(i * 4 + 2);
        indices[j++] = static_cast<unsigned short>(i * 4 + 2);
        indices[j++] = static_cast<unsigned short>(i * 4 + 3);
        indices[j++] = static_cast<unsigned short>(i * 4 + 0);
    }

    std::vector<BatchVertex> vertices(MAX_VERTICES);

    // 2D 精灵批处理 VAO
    vao_handle_ = create_vao_fn();
    glBindVertexArray(vao_handle_);
    vbo_handle_ = create_vbo_fn(vertices.size() * sizeof(BatchVertex), vertices.data(), true, false);
    unsigned int ebo = create_vbo_fn(indices.size() * sizeof(unsigned short), indices.data(), false, true);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), reinterpret_cast<const void*>(offsetof(BatchVertex, uv)));
    glBindVertexArray(0);
    // ebo 生命周期随 vao，这里不需要单独存储

    // 3D 网格 VAO
    mesh_vbo_handle_ = create_vbo_fn(MAX_VERTICES * sizeof(BatchVertex), nullptr, true, false);
    mesh_ibo_handle_ = create_vbo_fn(MAX_INDICES * sizeof(unsigned short), nullptr, true, true);
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
    glBindVertexArray(0);

    // 白色 1x1 默认纹理
    unsigned char white_texture[] = {255, 255, 255, 255};
    glGenTextures(1, &white_texture_handle_);
    glBindTexture(GL_TEXTURE_2D, white_texture_handle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_texture);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLDrawExecutor::ShutdownGeometryBuffers() {
    // 白色纹理
    if (white_texture_handle_ != 0) {
        glDeleteTextures(1, &white_texture_handle_);
        white_texture_handle_ = 0;
    }
    // 3D 网格缓冲
    if (mesh_vbo_handle_ != 0) { mesh_vbo_handle_ = 0; }
    if (mesh_ibo_handle_ != 0) { mesh_ibo_handle_ = 0; }
    if (mesh_vao_handle_ != 0) { mesh_vao_handle_ = 0; }
    // 2D 精灵缓冲
    if (vbo_handle_ != 0) { vbo_handle_ = 0; }
    if (ebo_handle_ != 0) { ebo_handle_ = 0; }
    if (vao_handle_ != 0) { vao_handle_ = 0; }
    // 天空盒缓冲
    if (skybox_vbo_handle_ != 0) { skybox_vbo_handle_ = 0; }
    if (skybox_vao_handle_ != 0) { skybox_vao_handle_ = 0; }

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
        glClearDepth(1.0);
        glClear(has_depth ? (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) : GL_COLOR_BUFFER_BIT);
    } else if (has_depth) {
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
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

    const auto& loc = shader_mgr.pbr_locations();
    glUseProgram(shader_mgr.pbr_shader_handle());
    if (state_mgr.active_pipeline_state() != 0) {
        state_mgr.ApplyState(state_mgr.active_pipeline_state());
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
                glCullFace(pipeline_state->cull_face);
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

        current_frame_stats_.draw_calls += 1;
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
    // 后处理全屏四边形 VAO/VBO（静态持久化）
    static unsigned int pp_vao = 0;
    static unsigned int pp_vbo = 0;

    if (pp_vao == 0) {
        float quadVertices[] = {
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
    glDisable(GL_BLEND);
    glBindVertexArray(pp_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
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
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glUniform1i(loc.texture, 0);

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
            batch_vertices.size() + 4 > MAX_VERTICES) {
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

    // 粒子四边形 VAO/VBO（静态持久化）
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

        glBindVertexArray(quad_vao);
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
