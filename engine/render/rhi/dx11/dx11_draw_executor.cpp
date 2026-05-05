/**
 * @file dx11_draw_executor.cpp
 * @brief DX11DrawExecutor 实现 — D3D11 绘制执行器
 */

#include "engine/render/rhi/dx11/dx11_draw_executor.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/render/rhi/dx11/dx11_resource_manager.h"
#include "engine/render/rhi/dx11/dx11_pipeline_state_manager.h"
#include "engine/render/rhi/dx11/dx11_shader_manager.h"
#include "engine/base/debug.h"

#include <cstring>

namespace dse {
namespace render {

void DX11DrawExecutor::Init(DX11Context* context, DX11ResourceManager* resource_mgr) {
    context_ = context;
    resource_mgr_ = resource_mgr;

    ID3D11Device* device = context->device();

    per_frame_cb_ = CreateConstantBuffer(sizeof(DX11PerFrameCB));
    per_object_cb_ = CreateConstantBuffer(sizeof(DX11PerObjectCB));
    per_scene_cb_ = CreateConstantBuffer(sizeof(DX11PerSceneCB));
    per_material_cb_ = CreateConstantBuffer(sizeof(DX11PerMaterialCB));

    // 初始化全局光源矩阵
    for (int i = 0; i < 3; ++i)
        global_light_space_matrix_[i] = glm::mat4(1.0f);

    initialized_ = true;
    DEBUG_LOG_INFO("[D3D11] DrawExecutor initialized");
}

void DX11DrawExecutor::Shutdown() {
    per_frame_cb_.Reset();
    per_object_cb_.Reset();
    per_scene_cb_.Reset();
    per_material_cb_.Reset();

    initialized_ = false;
    DEBUG_LOG_INFO("[D3D11] DrawExecutor shutdown");
}

ComPtr<ID3D11Buffer> DX11DrawExecutor::CreateConstantBuffer(UINT size) {
    // 对齐到 16 字节
    size = (size + 15) & ~15;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = size;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = context_->device()->CreateBuffer(&bd, nullptr, buffer.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateConstantBuffer failed: 0x{:08X}", static_cast<unsigned>(hr));
        return nullptr;
    }
    return buffer;
}

void DX11DrawExecutor::UpdateConstantBuffer(ID3D11Buffer* buffer, const void* data, UINT size) {
    if (!buffer) return;
    ID3D11DeviceContext* dc = context_->device_context();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = dc->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, data, size);
        dc->Unmap(buffer, 0);
    }
}

void DX11DrawExecutor::BeginFrame() {
    current_frame_stats_ = RenderStats{};
}

void DX11DrawExecutor::EndFrame() {
}

// ============================================================
// RenderPass
// ============================================================

void DX11DrawExecutor::BeginRenderPass(const RenderPassDesc& render_pass,
                                         DX11ResourceManager& resource_mgr,
                                         DX11PipelineStateManager& pipeline_mgr) {
    ID3D11DeviceContext* dc = context_->device_context();

    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;

    if (render_pass.render_target != 0) {
        const auto* rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (rt) {
            rtv = rt->color_rtv.Get();
            dsv = rt->depth_dsv.Get();
        }
    } else {
        rtv = context_->backbuffer_rtv();
        dsv = context_->backbuffer_dsv();
    }

    dc->OMSetRenderTargets(rtv ? 1 : 0, rtv ? &rtv : nullptr, dsv);

    // Viewport
    int vp_width = 0, vp_height = 0;
    if (render_pass.render_target != 0) {
        const auto* rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (rt) { vp_width = rt->width; vp_height = rt->height; }
    }
    if (vp_width == 0 || vp_height == 0) {
        vp_width = context_->width();
        vp_height = context_->height();
    }

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(vp_width);
    vp.Height = static_cast<float>(vp_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    dc->RSSetViewports(1, &vp);

    // 清除
    if (render_pass.clear_color_enabled && rtv) {
        float clear[4] = {render_pass.clear_color.r, render_pass.clear_color.g,
                          render_pass.clear_color.b, render_pass.clear_color.a};
        dc->ClearRenderTargetView(rtv, clear);
    }
    if (dsv) {
        dc->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
}

void DX11DrawExecutor::EndRenderPass() {
    // D3D11 不需要显式结束 render pass
}

// ============================================================
// 绘制命令
// ============================================================

void DX11DrawExecutor::DrawSpriteBatch(const std::vector<SpriteDrawItem>& items,
                                         const glm::mat4& view, const glm::mat4& projection,
                                         DX11PipelineStateManager& pipeline_mgr,
                                         DX11ShaderManager& shader_mgr,
                                         DX11ResourceManager& resource_mgr) {
    if (items.empty()) return;
    ID3D11DeviceContext* dc = context_->device_context();

    // 更新 PerFrame CB
    DX11PerFrameCB frame_data;
    frame_data.vp = projection * view;
    frame_data.view = view;
    frame_data.camera_pos = glm::vec4(0.0f);
    UpdateConstantBuffer(per_frame_cb_.Get(), &frame_data, sizeof(frame_data));

    // 绑定 PerFrame CB 到 b0
    ID3D11Buffer* cbs[] = {per_frame_cb_.Get(), per_object_cb_.Get()};
    dc->VSSetConstantBuffers(0, 2, cbs);

    // 绑定 sprite 着色器
    const auto* program = shader_mgr.GetProgram(shader_mgr.sprite_shader_handle());
    if (program) {
        dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
        dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);
    }

    for (auto& item : items) {
        // 更新 PerObject CB
        DX11PerObjectCB obj_data{};
        obj_data.model = item.model;
        obj_data.skinned = 0;
        obj_data.morph_enabled = 0;
        UpdateConstantBuffer(per_object_cb_.Get(), &obj_data, sizeof(obj_data));

        // 绑定纹理
        const auto* tex = resource_mgr.GetTexture(item.texture_handle);
        if (tex) {
            dc->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
            dc->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());
        }

        current_frame_stats_.draw_calls++;
        current_frame_stats_.sprite_count++;
    }
}

void DX11DrawExecutor::DrawMeshBatch(const std::vector<MeshDrawItem>& items,
                                       const glm::mat4& view, const glm::mat4& projection,
                                       DX11PipelineStateManager& pipeline_mgr,
                                       DX11ShaderManager& shader_mgr,
                                       DX11ResourceManager& resource_mgr) {
    if (items.empty()) return;
    ID3D11DeviceContext* dc = context_->device_context();

    // 更新 PerFrame CB
    DX11PerFrameCB frame_data;
    frame_data.vp = projection * view;
    frame_data.view = view;
    frame_data.camera_pos = glm::vec4(0.0f);
    UpdateConstantBuffer(per_frame_cb_.Get(), &frame_data, sizeof(frame_data));

    // 更新 PerScene CB (use first item for scene-level data)
    {
        auto& first = items[0];
        DX11PerSceneCB scene_data{};
        scene_data.light_dir_and_enabled = glm::vec4(
            first.light_direction, first.lighting_enabled ? 1.0f : 0.0f);
        scene_data.light_color_and_ambient = glm::vec4(
            first.light_color, first.ambient_intensity);
        scene_data.light_params = glm::vec4(
            first.light_intensity, first.shadow_strength,
            first.receive_shadow ? 1.0f : 0.0f, 0.0f);
        scene_data.cascade_splits = glm::vec4(
            global_cascade_splits_[0], global_cascade_splits_[1], global_cascade_splits_[2], 0.0f);
        for (int i = 0; i < 3; ++i)
            scene_data.light_space_matrices[i] = global_light_space_matrix_[i];
        UpdateConstantBuffer(per_scene_cb_.Get(), &scene_data, sizeof(scene_data));
    }

    // 绑定 PBR 着色器
    const auto* program = shader_mgr.GetProgram(shader_mgr.pbr_shader_handle());
    if (program) {
        dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
        dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);
    }

    // 绑定常量缓冲
    ID3D11Buffer* vs_cbs[] = {per_frame_cb_.Get(), per_object_cb_.Get()};
    dc->VSSetConstantBuffers(0, 2, vs_cbs);
    ID3D11Buffer* ps_cbs[] = {per_frame_cb_.Get(), nullptr, per_scene_cb_.Get(), per_material_cb_.Get()};
    dc->PSSetConstantBuffers(0, 4, ps_cbs);

    for (auto& item : items) {
        // PerObject
        DX11PerObjectCB obj_data{};
        obj_data.model = item.model;
        obj_data.skinned = item.skinned ? 1 : 0;
        obj_data.morph_enabled = item.morph_enabled ? 1 : 0;
        UpdateConstantBuffer(per_object_cb_.Get(), &obj_data, sizeof(obj_data));

        // PerMaterial
        DX11PerMaterialCB mat_data{};
        mat_data.albedo = glm::vec4(item.material_albedo, item.material_metallic);
        mat_data.roughness_ao = glm::vec4(item.material_roughness, item.material_ao,
                                           item.material_normal_strength, item.material_alpha_cutoff);
        mat_data.emissive = glm::vec4(item.material_emissive, item.material_alpha_test ? 1.0f : 0.0f);
        mat_data.flags = glm::vec4(
            item.normal_map_handle != 0 ? 1.0f : 0.0f,
            item.metallic_roughness_map_handle != 0 ? 1.0f : 0.0f,
            item.emissive_map_handle != 0 ? 1.0f : 0.0f,
            item.occlusion_map_handle != 0 ? 1.0f : 0.0f);
        UpdateConstantBuffer(per_material_cb_.Get(), &mat_data, sizeof(mat_data));

        // 绑定主纹理
        const auto* tex = resource_mgr.GetTexture(item.texture_handle);
        if (tex) {
            dc->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
            dc->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());
        }

        current_frame_stats_.draw_calls++;
        current_frame_stats_.mesh_count++;
    }
}

void DX11DrawExecutor::DrawSkybox(unsigned int cubemap_texture_handle,
                                    const glm::mat4& view, const glm::mat4& projection,
                                    DX11PipelineStateManager& pipeline_mgr,
                                    DX11ShaderManager& shader_mgr,
                                    DX11ResourceManager& resource_mgr) {
    ID3D11DeviceContext* dc = context_->device_context();

    DX11PerFrameCB frame_data;
    frame_data.vp = projection * view;
    frame_data.view = view;
    frame_data.camera_pos = glm::vec4(0.0f);
    UpdateConstantBuffer(per_frame_cb_.Get(), &frame_data, sizeof(frame_data));

    ID3D11Buffer* cbs[] = {per_frame_cb_.Get()};
    dc->VSSetConstantBuffers(0, 1, cbs);

    const auto* program = shader_mgr.GetProgram(shader_mgr.skybox_shader_handle());
    if (program) {
        dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
        dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);
    }

    const auto* tex = resource_mgr.GetTexture(cubemap_texture_handle);
    if (tex) {
        dc->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
        dc->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());
    }

    current_frame_stats_.draw_calls++;
}

void DX11DrawExecutor::DrawPostProcess(unsigned int source_texture,
                                         const std::string& effect_name,
                                         const std::vector<float>& params,
                                         DX11PipelineStateManager& pipeline_mgr,
                                         DX11ShaderManager& shader_mgr,
                                         DX11ResourceManager& resource_mgr) {
    ID3D11DeviceContext* dc = context_->device_context();

    const auto* program = shader_mgr.GetProgram(shader_mgr.postprocess_shader_handle());
    if (program) {
        dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
        dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);
    }

    const auto* tex = resource_mgr.GetTexture(source_texture);
    if (tex) {
        dc->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
        dc->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());
    }

    current_frame_stats_.draw_calls++;
}

void DX11DrawExecutor::DrawParticles3D(const std::vector<Particle3DDrawItem>& items,
                                         const glm::mat4& view, const glm::mat4& projection,
                                         DX11PipelineStateManager& pipeline_mgr,
                                         DX11ShaderManager& shader_mgr,
                                         DX11ResourceManager& resource_mgr) {
    if (items.empty()) return;
    ID3D11DeviceContext* dc = context_->device_context();

    DX11PerFrameCB frame_data;
    frame_data.vp = projection * view;
    frame_data.view = view;
    frame_data.camera_pos = glm::vec4(0.0f);
    UpdateConstantBuffer(per_frame_cb_.Get(), &frame_data, sizeof(frame_data));

    ID3D11Buffer* cbs[] = {per_frame_cb_.Get()};
    dc->VSSetConstantBuffers(0, 1, cbs);

    const auto* program = shader_mgr.GetProgram(shader_mgr.particle_shader_handle());
    if (program) {
        dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
        dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);
    }

    current_frame_stats_.draw_calls += static_cast<unsigned int>(items.size());
}

} // namespace render
} // namespace dse
