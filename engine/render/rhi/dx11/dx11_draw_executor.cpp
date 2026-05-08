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
#include <algorithm>

namespace dse {
namespace render {

void DX11DrawExecutor::Init(DX11Context* context, DX11ResourceManager* resource_mgr) {
    context_ = context;
    resource_mgr_ = resource_mgr;

    per_frame_cb_ = CreateConstantBuffer(sizeof(DX11PerFrameCB));
    per_object_cb_ = CreateConstantBuffer(sizeof(DX11PerObjectCB));
    per_scene_cb_ = CreateConstantBuffer(sizeof(DX11PerSceneCB));
    per_material_cb_ = CreateConstantBuffer(sizeof(DX11PerMaterialCB));
    per_point_lights_cb_   = CreateConstantBuffer(sizeof(DX11PointLightsCB));
    per_spot_lights_cb_    = CreateConstantBuffer(sizeof(DX11SpotLightsCB));
    per_spot_matrices_cb_  = CreateConstantBuffer(sizeof(DX11SpotMatricesCB));

    // 初始化全局光源矩阵
    for (int i = 0; i < 3; ++i)
        global_light_space_matrix_[i] = glm::mat4(1.0f);

    InitGeometryBuffers();

    // 创建 PCF 比较采样器（SamplerComparisonState s1，与 kPbrPS 声明匹配）
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter         = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sd.MaxLOD         = D3D11_FLOAT32_MAX;
        context_->device()->CreateSamplerState(&sd, shadow_sampler_.GetAddressOf());
    }

    initialized_ = true;
    DEBUG_LOG_INFO("[D3D11] DrawExecutor initialized");
}

void DX11DrawExecutor::Shutdown() {
    per_frame_cb_.Reset();
    per_object_cb_.Reset();
    per_scene_cb_.Reset();
    per_material_cb_.Reset();
    per_point_lights_cb_.Reset();
    per_spot_lights_cb_.Reset();
    per_spot_matrices_cb_.Reset();

    sprite_quad_vbo_.Reset();
    sprite_quad_ibo_.Reset();
    mesh_dynamic_vbo_.Reset();
    mesh_dynamic_ibo_.Reset();
    mesh_vbo_capacity_ = 0;
    mesh_ibo_capacity_ = 0;
    skybox_vbo_.Reset();
    postprocess_vbo_.Reset();
    postprocess_ibo_.Reset();
    particle_quad_vbo_.Reset();
    particle_quad_ibo_.Reset();
    shadow_sampler_.Reset();

    initialized_ = false;
    DEBUG_LOG_INFO("[D3D11] DrawExecutor shutdown");
}

// ============================================================
// 几何缓冲初始化
// ============================================================

void DX11DrawExecutor::InitGeometryBuffers() {
    ID3D11Device* device = context_->device();

    // ---- 精灵四边形 VBO（动态）4 顶点 × 32字节 ----
    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = 4 * 32; // float2 pos + float2 uv + float4 color = 32B per vertex
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, sprite_quad_vbo_.GetAddressOf());
    }

    // ---- 精灵四边形 IBO（静态）6 索引 ----
    {
        unsigned short indices[] = {0, 1, 2, 0, 2, 3};
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(indices);
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = indices;
        device->CreateBuffer(&bd, &init, sprite_quad_ibo_.GetAddressOf());
    }

    // ---- 天空盒立方体 VBO（静态）36 顶点 ----
    {
        static const float v[] = {
            // Back
            -1, 1,-1, -1,-1,-1,  1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
            // Front
            -1,-1, 1, -1, 1, 1,  1, 1, 1,  1, 1, 1,  1,-1, 1, -1,-1, 1,
            // Left
            -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1,
            // Right
             1, 1,-1,  1, 1, 1,  1,-1, 1,  1,-1, 1,  1,-1,-1,  1, 1,-1,
            // Top
            -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1,
            // Bottom
            -1,-1,-1, -1,-1, 1,  1,-1, 1,  1,-1, 1,  1,-1,-1, -1,-1,-1,
        };
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(v);
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = v;
        device->CreateBuffer(&bd, &init, skybox_vbo_.GetAddressOf());
    }

    // ---- 后处理全屏四边形 VBO（静态）----
    {
        // float2 pos + float2 uv = 16B per vertex
        float verts[] = {
            -1.0f, -1.0f, 0.0f, 1.0f,   // bottom-left
             1.0f, -1.0f, 1.0f, 1.0f,   // bottom-right
             1.0f,  1.0f, 1.0f, 0.0f,   // top-right
            -1.0f,  1.0f, 0.0f, 0.0f,   // top-left
        };
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(verts);
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = verts;
        device->CreateBuffer(&bd, &init, postprocess_vbo_.GetAddressOf());
    }

    // ---- 后处理 IBO（静态）----
    {
        unsigned short indices[] = {0, 1, 2, 0, 2, 3};
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(indices);
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = indices;
        device->CreateBuffer(&bd, &init, postprocess_ibo_.GetAddressOf());
    }

    // ---- 粒子公告板 VBO（静态）----
    {
        // float3 pos + float2 uv = 20B per vertex
        float verts[] = {
            -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f, 0.0f, 0.0f, 0.0f,
        };
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(verts);
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = verts;
        device->CreateBuffer(&bd, &init, particle_quad_vbo_.GetAddressOf());
    }

    // ---- 粒子公告板 IBO（静态）----
    {
        unsigned short indices[] = {0, 1, 2, 0, 2, 3};
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(indices);
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = indices;
        device->CreateBuffer(&bd, &init, particle_quad_ibo_.GetAddressOf());
    }

    DEBUG_LOG_INFO("[D3D11] DrawExecutor geometry buffers initialized");
}

void DX11DrawExecutor::EnsureMeshVBOCapacity(size_t needed_bytes) {
    if (mesh_vbo_capacity_ >= needed_bytes) return;
    size_t new_cap = (std::max)(needed_bytes, (std::max)(mesh_vbo_capacity_ * 2, (size_t)65536));

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = static_cast<UINT>(new_cap);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    mesh_dynamic_vbo_.Reset();
    HRESULT hr = context_->device()->CreateBuffer(&bd, nullptr, mesh_dynamic_vbo_.GetAddressOf());
    if (SUCCEEDED(hr)) mesh_vbo_capacity_ = new_cap;
}

void DX11DrawExecutor::EnsureMeshIBOCapacity(size_t needed_bytes) {
    if (mesh_ibo_capacity_ >= needed_bytes) return;
    size_t new_cap = (std::max)(needed_bytes, (std::max)(mesh_ibo_capacity_ * 2, (size_t)16384));

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = static_cast<UINT>(new_cap);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    mesh_dynamic_ibo_.Reset();
    HRESULT hr = context_->device()->CreateBuffer(&bd, nullptr, mesh_dynamic_ibo_.GetAddressOf());
    if (SUCCEEDED(hr)) mesh_ibo_capacity_ = new_cap;
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

    // 记录当前 RT 句柄，供 EndRenderPass MSAA resolve 使用
    current_rt_handle_ = render_pass.render_target;

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

    // 检测深度 only pass（shadow pass）
    is_depth_only_pass_ = (!rtv && dsv);

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
    is_depth_only_pass_ = false;
    // MSAA resolve：将多重采样颜色纹理 resolve 到 1x resolve 纹理
    if (current_rt_handle_ != 0 && resource_mgr_) {
        const auto* rt = resource_mgr_->GetRenderTarget(current_rt_handle_);
        if (rt && rt->is_msaa && rt->color_texture && rt->color_resolve_texture) {
            const bool use_hdr = context_ ? context_->hdr_enabled() : false;
            DXGI_FORMAT fmt = use_hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
            context_->device_context()->ResolveSubresource(
                rt->color_resolve_texture.Get(), 0,
                rt->color_texture.Get(), 0, fmt);
        }
    }
    current_rt_handle_ = 0;
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

    // 绑定常量缓冲
    ID3D11Buffer* cbs[] = {per_frame_cb_.Get(), per_object_cb_.Get()};
    dc->VSSetConstantBuffers(0, 2, cbs);

    // 绑定 sprite 着色器
    const auto* program = shader_mgr.GetProgram(shader_mgr.sprite_shader_handle());
    if (!program) return;
    dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);

    // 绑定 InputLayout + 图元拓扑 + IBO
    auto* layout = shader_mgr.GetInputLayout(shader_mgr.sprite_shader_handle());
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->IASetIndexBuffer(sprite_quad_ibo_.Get(), DXGI_FORMAT_R16_UINT, 0);

    for (const auto& item : items) {
        // 构建精灵四边形顶点: float2 pos + float2 uv + float4 color = 32B
        float u0 = item.uv.x, v0 = item.uv.y, u1 = item.uv.z, v1 = item.uv.w;
        float r = item.color.r, g = item.color.g, b = item.color.b, a = item.color.a;
        float verts[4 * 8] = {
            -0.5f, -0.5f, u0, v1, r, g, b, a,
             0.5f, -0.5f, u1, v1, r, g, b, a,
             0.5f,  0.5f, u1, v0, r, g, b, a,
            -0.5f,  0.5f, u0, v0, r, g, b, a,
        };

        // 上传动态 VBO
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = dc->Map(sprite_quad_vbo_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            memcpy(mapped.pData, verts, sizeof(verts));
            dc->Unmap(sprite_quad_vbo_.Get(), 0);
        }

        UINT stride = 32;
        UINT offset = 0;
        dc->IASetVertexBuffers(0, 1, sprite_quad_vbo_.GetAddressOf(), &stride, &offset);

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

        dc->DrawIndexed(6, 0, 0);
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

    // 更新 PerScene CB
    {
        const auto& first = items[0];
        DX11PerSceneCB scene_data{};
        scene_data.light_dir_and_enabled = glm::vec4(
            first.light_direction, first.lighting_enabled ? 1.0f : 0.0f);
        scene_data.light_color_and_ambient = glm::vec4(
            first.light_color, first.ambient_intensity);
        scene_data.light_params = glm::vec4(
            first.light_intensity, first.shadow_strength,
            first.receive_shadow ? 1.0f : 0.0f, static_cast<float>(first.shading_mode));
        scene_data.cascade_splits = glm::vec4(
            global_cascade_splits_[0], global_cascade_splits_[1], global_cascade_splits_[2], 0.0f);
        for (int i = 0; i < 3; ++i)
            scene_data.light_space_matrices[i] = global_light_space_matrix_[i];
        UpdateConstantBuffer(per_scene_cb_.Get(), &scene_data, sizeof(scene_data));
    }

    // 选择着色器：深度 only pass 使用 shadow shader，否则使用 PBR
    unsigned int shader_handle = is_depth_only_pass_
        ? shader_mgr.shadow_shader_handle()
        : shader_mgr.pbr_shader_handle();
    const auto* program = shader_mgr.GetProgram(shader_handle);
    if (!program) return;
    dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);

    // 绑定 InputLayout + 拓扑
    auto* layout = shader_mgr.GetInputLayout(shader_handle);
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 绑定常量缓冲
    ID3D11Buffer* vs_cbs[] = {per_frame_cb_.Get(), per_object_cb_.Get()};
    dc->VSSetConstantBuffers(0, 2, vs_cbs);
    if (!is_depth_only_pass_) {
        ID3D11Buffer* ps_cbs[] = {per_frame_cb_.Get(), nullptr, per_scene_cb_.Get(), per_material_cb_.Get()};
        dc->PSSetConstantBuffers(0, 4, ps_cbs);

        // 绑定 CSM 阴影贴图到 t5/t6/t7
        for (int i = 0; i < 3; ++i) {
            if (global_shadow_map_[i] != 0) {
                const auto* sm = resource_mgr.GetTexture(global_shadow_map_[i]);
                if (sm) dc->PSSetShaderResources(5 + i, 1, sm->srv.GetAddressOf());
            }
        }
        // 绑定阴影采样器到 s1
        if (shadow_sampler_) {
            dc->PSSetSamplers(1, 1, shadow_sampler_.GetAddressOf());
        }

        // 展开点光源/聚光灯 CB 并绑定到 b4/b5
        if (!items.empty()) {
            DX11PointLightsCB pl_cb{};
            pl_cb.count = static_cast<int>(
                (std::min)(items[0].point_lights.size(), (size_t)4));
            for (int i = 0; i < pl_cb.count; ++i) {
                const auto& src = items[0].point_lights[i];
                auto& dst = pl_cb.lights[i];
                dst.color        = src.color;
                dst.intensity    = src.intensity;
                dst.position     = src.position;
                dst.radius       = src.radius;
                dst.cast_shadow  = src.cast_shadow ? 1 : 0;
                dst.shadow_index = src.shadow_index;
            }
            UpdateConstantBuffer(per_point_lights_cb_.Get(), &pl_cb, sizeof(pl_cb));
            dc->PSSetConstantBuffers(4, 1, per_point_lights_cb_.GetAddressOf());

            DX11SpotLightsCB sl_cb{};
            sl_cb.count = static_cast<int>(
                (std::min)(items[0].spot_lights.size(), (size_t)4));
            for (int i = 0; i < sl_cb.count; ++i) {
                const auto& src = items[0].spot_lights[i];
                auto& dst = sl_cb.lights[i];
                dst.color        = src.color;
                dst.intensity    = src.intensity;
                dst.position     = src.position;
                dst.radius       = src.radius;
                dst.direction    = src.direction;
                dst.inner_cone   = src.inner_cone;
                dst.outer_cone   = src.outer_cone;
                dst.cast_shadow  = src.cast_shadow ? 1 : 0;
                dst.shadow_index = src.shadow_index;
            }
            UpdateConstantBuffer(per_spot_lights_cb_.Get(), &sl_cb, sizeof(sl_cb));
            dc->PSSetConstantBuffers(5, 1, per_spot_lights_cb_.GetAddressOf());
        }

        // 绑定点光源立方体阴影贴图到 t8~t11
        for (int i = 0; i < 4; ++i) {
            if (global_point_shadow_map_[i] != 0) {
                const auto* sm = resource_mgr.GetTexture(global_point_shadow_map_[i]);
                if (sm) dc->PSSetShaderResources(8 + i, 1, sm->srv.GetAddressOf());
            }
        }

        // 填充聚光灯光源空间矩阵 CB（b6）并绑定
        {
            DX11SpotMatricesCB sm_cb{};
            for (int i = 0; i < 4; ++i)
                sm_cb.spot_light_space_matrices[i] = global_spot_light_space_matrix_[i];
            UpdateConstantBuffer(per_spot_matrices_cb_.Get(), &sm_cb, sizeof(sm_cb));
            dc->PSSetConstantBuffers(6, 1, per_spot_matrices_cb_.GetAddressOf());
        }

        // 绑定聚光灯阴影贴图到 t12~t15
        for (int i = 0; i < 4; ++i) {
            if (global_spot_shadow_map_[i] != 0) {
                const auto* sm = resource_mgr.GetTexture(global_spot_shadow_map_[i]);
                if (sm) dc->PSSetShaderResources(12 + i, 1, sm->srv.GetAddressOf());
            }
        }
    }

    for (const auto& item : items) {
        if (item.vertices.empty() || item.indices.empty()) continue;

        // 动态 VBO/IBO 容量保证
        size_t vbo_bytes = item.vertices.size() * sizeof(BatchVertex);
        size_t ibo_bytes = item.indices.size() * sizeof(unsigned short);
        EnsureMeshVBOCapacity(vbo_bytes);
        EnsureMeshIBOCapacity(ibo_bytes);

        // 上传顶点数据
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            HRESULT hr = dc->Map(mesh_dynamic_vbo_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            if (SUCCEEDED(hr)) {
                memcpy(mapped.pData, item.vertices.data(), vbo_bytes);
                dc->Unmap(mesh_dynamic_vbo_.Get(), 0);
            }
        }

        // 上传索引数据
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            HRESULT hr = dc->Map(mesh_dynamic_ibo_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            if (SUCCEEDED(hr)) {
                memcpy(mapped.pData, item.indices.data(), ibo_bytes);
                dc->Unmap(mesh_dynamic_ibo_.Get(), 0);
            }
        }

        // 绑定 VBO/IBO
        UINT stride = sizeof(BatchVertex);
        UINT vb_offset = 0;
        dc->IASetVertexBuffers(0, 1, mesh_dynamic_vbo_.GetAddressOf(), &stride, &vb_offset);
        dc->IASetIndexBuffer(mesh_dynamic_ibo_.Get(), DXGI_FORMAT_R16_UINT, 0);

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

        // 绑定纹理（t0 反照/漫反射，t1 法线，t2 MR，t3 发光，t4 AO）
        const auto* tex = resource_mgr.GetTexture(item.texture_handle);
        if (tex) {
            dc->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
            dc->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());
        }
        if (item.normal_map_handle) {
            const auto* nm = resource_mgr.GetTexture(item.normal_map_handle);
            if (nm) dc->PSSetShaderResources(1, 1, nm->srv.GetAddressOf());
        }
        if (item.metallic_roughness_map_handle) {
            const auto* mr = resource_mgr.GetTexture(item.metallic_roughness_map_handle);
            if (mr) dc->PSSetShaderResources(2, 1, mr->srv.GetAddressOf());
        }
        if (item.emissive_map_handle) {
            const auto* em = resource_mgr.GetTexture(item.emissive_map_handle);
            if (em) dc->PSSetShaderResources(3, 1, em->srv.GetAddressOf());
        }
        if (item.occlusion_map_handle) {
            const auto* oc = resource_mgr.GetTexture(item.occlusion_map_handle);
            if (oc) dc->PSSetShaderResources(4, 1, oc->srv.GetAddressOf());
        }

        dc->DrawIndexed(static_cast<UINT>(item.indices.size()), 0, 0);
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
    if (!program) return;
    dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);

    auto* layout = shader_mgr.GetInputLayout(shader_mgr.skybox_shader_handle());
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const auto* tex = resource_mgr.GetTexture(cubemap_texture_handle);
    if (tex) {
        dc->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
        dc->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());
    }

    UINT stride = sizeof(float) * 3;
    UINT offset = 0;
    dc->IASetVertexBuffers(0, 1, skybox_vbo_.GetAddressOf(), &stride, &offset);

    dc->Draw(36, 0);
    current_frame_stats_.draw_calls++;
}

void DX11DrawExecutor::DrawPostProcess(unsigned int source_texture,
                                         const std::string& effect_name,
                                         const std::vector<float>& params,
                                         DX11PipelineStateManager& pipeline_mgr,
                                         DX11ShaderManager& shader_mgr,
                                         DX11ResourceManager& resource_mgr) {
    ID3D11DeviceContext* dc = context_->device_context();

    // Compute Shader 路径：bloom 降采样/升采样
    const bool is_bloom_cs = (effect_name == "bloom_downsample" || effect_name == "bloom_upsample");
    if (is_bloom_cs && current_rt_handle_ != 0) {
        const unsigned int cs_handle = (effect_name == "bloom_downsample")
            ? shader_mgr.bloom_downsample_cs_handle()
            : shader_mgr.bloom_upsample_cs_handle();

        const auto* rt = resource_mgr.GetRenderTarget(current_rt_handle_);
        if (rt && rt->color_uav) {
            unsigned int uav_rt = current_rt_handle_;
            // 解绑当前 RTV，清空 PS SRV（防止 D3D11 Validation Layer UAV 冲突），切换到 CS 路径
            ID3D11RenderTargetView* null_rtv = nullptr;
            dc->OMSetRenderTargets(0, &null_rtv, nullptr);
            ID3D11ShaderResourceView* null_srvs[8] = {};
            dc->PSSetShaderResources(0, 8, null_srvs);

            UINT dst_w = static_cast<UINT>(rt->width);
            UINT dst_h = static_cast<UINT>(rt->height);
            UINT tx = (dst_w + 7) / 8;
            UINT ty = (dst_h + 7) / 8;
            DispatchCompute(cs_handle, source_texture, uav_rt, tx, ty, shader_mgr, resource_mgr);

            // 重新绑定 RTV（EndRenderPass 会解绑并 resolve）
            ID3D11RenderTargetView* rtv = rt->color_rtv.Get();
            dc->OMSetRenderTargets(rtv ? 1 : 0, rtv ? &rtv : &null_rtv, rt->depth_dsv.Get());
            return;
        }
    }

    // bloom_composite 专用路径：ACES Filmic Tone Mapping
    if (effect_name == "bloom_composite") {
        const auto* comp_prog = shader_mgr.GetProgram(shader_mgr.bloom_composite_shader_handle());
        if (!comp_prog) return;
        dc->VSSetShader(comp_prog->vertex_shader.Get(), nullptr, 0);
        dc->PSSetShader(comp_prog->pixel_shader.Get(), nullptr, 0);
        auto* layout = shader_mgr.GetInputLayout(shader_mgr.bloom_composite_shader_handle());
        if (layout) dc->IASetInputLayout(layout);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const auto* src_tex = resource_mgr.GetTexture(source_texture);
        if (src_tex) {
            dc->PSSetShaderResources(0, 1, src_tex->srv.GetAddressOf());
            dc->PSSetSamplers(0, 1, src_tex->sampler.GetAddressOf());
        }
        if (params.size() >= 1) {
            const auto* bloom_tex = resource_mgr.GetTexture(static_cast<unsigned int>(params[0]));
            if (bloom_tex) dc->PSSetShaderResources(1, 1, bloom_tex->srv.GetAddressOf());
        }

        // 懒惰创建 bloom composite 参数 CB
        if (!bloom_composite_params_cb_ && context_->device()) {
            D3D11_BUFFER_DESC bd{};
            bd.Usage          = D3D11_USAGE_DYNAMIC;
            bd.ByteWidth      = 16;
            bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            struct InitParams { float exposure; float bloomIntensity; float _pad[2]; } init{1.0f, 0.5f, 0, 0};
            D3D11_SUBRESOURCE_DATA sd{};
            sd.pSysMem = &init;
            context_->device()->CreateBuffer(&bd, &sd, bloom_composite_params_cb_.GetAddressOf());
        }
        if (bloom_composite_params_cb_) {
            struct BloomCompositeParams { float exposure; float bloomIntensity; float _pad[2]; } cp{};
            cp.exposure       = (params.size() >= 2) ? params[1] : 1.0f;
            cp.bloomIntensity = (params.size() >= 3) ? params[2] : 0.5f;
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(dc->Map(bloom_composite_params_cb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, &cp, sizeof(cp));
                dc->Unmap(bloom_composite_params_cb_.Get(), 0);
            }
            dc->PSSetConstantBuffers(0, 1, bloom_composite_params_cb_.GetAddressOf());
        }

        UINT stride = sizeof(float) * 4;
        UINT offset = 0;
        dc->IASetVertexBuffers(0, 1, postprocess_vbo_.GetAddressOf(), &stride, &offset);
        dc->IASetIndexBuffer(postprocess_ibo_.Get(), DXGI_FORMAT_R16_UINT, 0);
        dc->DrawIndexed(6, 0, 0);
        current_frame_stats_.draw_calls++;

        ID3D11ShaderResourceView* null_srv = nullptr;
        dc->PSSetShaderResources(1, 1, &null_srv);
        return;
    }

    // ui_overlay: 需要 alpha 混合
    if (effect_name == "ui_overlay") {
        PipelineStateDesc ui_pp_desc;
        ui_pp_desc.blend_enabled = true;
        ui_pp_desc.blend_src = BlendFactor::SrcAlpha;
        ui_pp_desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
        ui_pp_desc.depth_test_enabled = false;
        ui_pp_desc.depth_write_enabled = false;
        ui_pp_desc.culling_enabled = false;
        unsigned int ui_pp_state = pipeline_mgr.CreatePipelineState(ui_pp_desc);
        if (ui_pp_state != 0) {
            pipeline_mgr.ApplyPipelineState(ui_pp_state, dc);
        }
    }

    // 标准全屏四边形路径
    const auto* program = shader_mgr.GetProgram(shader_mgr.postprocess_shader_handle());
    if (!program) return;
    dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);

    auto* layout = shader_mgr.GetInputLayout(shader_mgr.postprocess_shader_handle());
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const auto* tex = resource_mgr.GetTexture(source_texture);
    if (tex) {
        dc->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
        dc->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());
    }

    UINT stride = sizeof(float) * 4; // float2 pos + float2 uv
    UINT offset = 0;
    dc->IASetVertexBuffers(0, 1, postprocess_vbo_.GetAddressOf(), &stride, &offset);
    dc->IASetIndexBuffer(postprocess_ibo_.Get(), DXGI_FORMAT_R16_UINT, 0);

    dc->DrawIndexed(6, 0, 0);
    current_frame_stats_.draw_calls++;
}

void DX11DrawExecutor::DispatchCompute(unsigned int cs_handle,
                                        unsigned int srv_texture_handle,
                                        unsigned int uav_rt_handle,
                                        UINT threads_x, UINT threads_y,
                                        DX11ShaderManager& shader_mgr,
                                        DX11ResourceManager& resource_mgr) {
    if (!context_) return;
    ID3D11DeviceContext* dc = context_->device_context();

    const auto* prog = shader_mgr.GetComputeProgram(cs_handle);
    if (!prog || !prog->cs) return;

    // 更新 BloomParams CB（src 和 dst texel size）
    const auto* src_tex = resource_mgr.GetTexture(srv_texture_handle);
    const auto* dst_rt  = resource_mgr.GetRenderTarget(uav_rt_handle);
    if (prog->params_cb && src_tex && dst_rt) {
        struct BloomParams { float src_w, src_h, dst_w, dst_h; } bp;
        bp.src_w = 1.0f / static_cast<float>(src_tex->width > 0 ? src_tex->width : 1);
        bp.src_h = 1.0f / static_cast<float>(src_tex->height > 0 ? src_tex->height : 1);
        bp.dst_w = 1.0f / static_cast<float>(dst_rt->width > 0 ? dst_rt->width : 1);
        bp.dst_h = 1.0f / static_cast<float>(dst_rt->height > 0 ? dst_rt->height : 1);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(dc->Map(prog->params_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &bp, sizeof(bp));
            dc->Unmap(prog->params_cb.Get(), 0);
        }
        ID3D11Buffer* cbs[] = {prog->params_cb.Get()};
        dc->CSSetConstantBuffers(0, 1, cbs);
    }

    // 绑定 CS + SRV + UAV
    dc->CSSetShader(prog->cs.Get(), nullptr, 0);
    if (src_tex && src_tex->srv) {
        dc->CSSetShaderResources(0, 1, src_tex->srv.GetAddressOf());
    }
    if (dst_rt && dst_rt->color_uav) {
        dc->CSSetUnorderedAccessViews(0, 1, dst_rt->color_uav.GetAddressOf(), nullptr);
    }

    dc->Dispatch(threads_x, threads_y, 1);

    // 解绑 UAV / SRV
    ID3D11UnorderedAccessView* null_uav = nullptr;
    ID3D11ShaderResourceView* null_srv = nullptr;
    dc->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
    dc->CSSetShaderResources(0, 1, &null_srv);
    dc->CSSetShader(nullptr, nullptr, 0);
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
    if (!program) return;
    dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);

    auto* layout = shader_mgr.GetInputLayout(shader_mgr.particle_shader_handle());
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->IASetIndexBuffer(particle_quad_ibo_.Get(), DXGI_FORMAT_R16_UINT, 0);

    for (const auto& item : items) {
        if (item.particle_count <= 0) continue;

        // slot 0: 公告板四边形，slot 1: 实例数据
        const auto* inst_buf = resource_mgr.GetBuffer(item.instance_vbo);
        ID3D11Buffer* vbs[2] = {particle_quad_vbo_.Get(),
                                 inst_buf ? inst_buf->buffer.Get() : nullptr};
        UINT strides[2] = {sizeof(float) * 5, sizeof(float) * 8};  // 20B vertex, 32B instance
        UINT offsets[2] = {0, 0};
        dc->IASetVertexBuffers(0, 2, vbs, strides, offsets);

        const auto* tex = resource_mgr.GetTexture(item.texture_handle);
        if (tex) {
            dc->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
            dc->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());
        }

        dc->DrawIndexedInstanced(6, item.particle_count, 0, 0, 0);
        current_frame_stats_.draw_calls++;
        current_frame_stats_.particle_count += item.particle_count;
    }
}

} // namespace render
} // namespace dse
