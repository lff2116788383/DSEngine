/**
 * @file dx11_draw_executor.cpp
 * @brief DX11DrawExecutor 实现 — D3D11 绘制执行器
 */

#include "engine/render/rhi/dx11/dx11_draw_executor.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/render/rhi/dx11/dx11_resource_manager.h"
#include "engine/render/rhi/dx11/dx11_pipeline_state_manager.h"
#include "engine/render/rhi/dx11/dx11_shader_manager.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/base/debug.h"

static constexpr int DX11_MAX_BATCH_SPRITES = 4096;
static const unsigned int kSdfVariantKey = static_cast<unsigned int>(std::hash<std::string>{}("TEXT_SDF"));

#include <cstring>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdlib>

namespace dse {
namespace render {
namespace {

static glm::vec3 NormalizeOrFallback(const glm::vec3& value, const glm::vec3& fallback) {
    const float len2 = glm::dot(value, value);
    return (len2 > 1e-12f) ? value * (1.0f / std::sqrt(len2)) : fallback;
}

static bool BuildPreskinnedVertices(const BatchVertex* src, size_t count,
                                    const std::vector<glm::mat4>& palette,
                                    std::vector<BatchVertex>& dst) {
    if (!src || count == 0 || palette.empty()) return false;
    dst.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const BatchVertex& in = src[i];
        BatchVertex out = in;
        glm::mat4 skin(0.0f);
        float total_weight = 0.0f;
        for (int j = 0; j < 4; ++j) {
            const float weight = in.weights[j];
            const int bone_index = static_cast<int>(in.joints[j]);
            if (weight != 0.0f && bone_index >= 0 && bone_index < static_cast<int>(palette.size())) {
                skin += palette[bone_index] * weight;
                total_weight += weight;
            }
        }
        if (total_weight > 0.0f) {
            out.pos = glm::vec3(skin * glm::vec4(in.pos, 1.0f));
            const glm::mat3 skin3(skin);
            out.normal = NormalizeOrFallback(skin3 * in.normal, in.normal);
            out.tangent = NormalizeOrFallback(skin3 * in.tangent, in.tangent);
            out.weights = glm::vec4(0.0f);
            out.joints = glm::vec4(0.0f);
        }
        dst[i] = out;
    }
    return true;
}

} // namespace

void DX11DrawExecutor::Init(DX11Context* context, DX11ResourceManager* resource_mgr) {
    context_ = context;
    resource_mgr_ = resource_mgr;

    per_frame_cb_ = CreateConstantBuffer(sizeof(DX11PerFrameCB));
    per_object_cb_ = CreateConstantBuffer(sizeof(DX11PerObjectCB));
    per_scene_cb_ = CreateConstantBuffer(sizeof(DX11PerSceneCB));
    per_material_cb_ = CreateConstantBuffer(sizeof(DX11PerMaterialCB));
    sprite_push_cb_        = CreateConstantBuffer(128); // [model(64B) | vp(64B)] for sprite.vert
    sdf_ps_cb_             = CreateConstantBuffer(144); // [model(64B) | vp(64B) | sdf_params(16B)] for text_sdf.frag
    vfx_ps_cb_             = CreateConstantBuffer(64);  // [gradient_start | gradient_end | rect_size_and_radius | blur_params]
    per_point_lights_cb_   = CreateConstantBuffer(sizeof(DX11PointLightsCB));
    per_spot_lights_cb_    = CreateConstantBuffer(sizeof(DX11SpotLightsCB));
    per_spot_matrices_cb_  = CreateConstantBuffer(sizeof(DX11SpotMatricesCB));
    terrain_params_cb_     = CreateConstantBuffer(sizeof(DX11TerrainParamsCB));
    bone_matrices_cb_      = CreateConstantBuffer(100 * sizeof(glm::mat4)); // MAX_BONES=100, 6400B
    light_probe_data_cb_   = CreateConstantBuffer(sizeof(DX11LightProbeDataCB));
    draw_id_cb_            = CreateConstantBuffer(16); // GPU-driven draw_id (uint, padded to 16B)

    // 初始化全局光源矩阵
    for (int i = 0; i < 3; ++i)
        global_state_.light_space_matrix[i] = glm::mat4(1.0f);

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

    // 天空盒深度状态：LEQUAL + 禁止深度写入（与 OpenGL glDepthFunc(GL_LEQUAL) 对应）
    {
        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable    = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
        dsd.StencilEnable  = FALSE;
        context_->device()->CreateDepthStencilState(&dsd, skybox_dss_.GetAddressOf());
    }

    // 双面材质光栅化状态（CullMode=NONE, 与 OpenGL/Vulkan 的 material_double_sided 对齐）
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = TRUE;
        rd.DepthClipEnable = TRUE;
        context_->device()->CreateRasterizerState(&rd, no_cull_rasterizer_state_.GetAddressOf());
    }

    // 1×1 白色 fallback 纹理（与 OpenGL white_texture 对齐，texture_handle=0 时使用）
    {
        unsigned char white_pixel[4] = {255, 255, 255, 255};
        D3D11_TEXTURE2D_DESC td{};
        td.Width = 1;
        td.Height = 1;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init_data{};
        init_data.pSysMem = white_pixel;
        init_data.SysMemPitch = 4;
        ComPtr<ID3D11Texture2D> white_tex;
        context_->device()->CreateTexture2D(&td, &init_data, white_tex.GetAddressOf());
        if (white_tex) {
            context_->device()->CreateShaderResourceView(white_tex.Get(), nullptr, white_texture_srv_.GetAddressOf());
        }
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        context_->device()->CreateSamplerState(&sd, white_texture_sampler_.GetAddressOf());
    }

    initialized_ = true;
    DEBUG_LOG_INFO("[D3D11] DrawExecutor initialized");
}

void DX11DrawExecutor::Shutdown() {
    per_frame_cb_.Reset();
    per_object_cb_.Reset();
    per_scene_cb_.Reset();
    per_material_cb_.Reset();
    draw_id_cb_.Reset();
    per_point_lights_cb_.Reset();
    per_spot_lights_cb_.Reset();
    per_spot_matrices_cb_.Reset();
    terrain_params_cb_.Reset();

    sprite_push_cb_.Reset();
    sdf_ps_cb_.Reset();
    vfx_ps_cb_.Reset();
    sprite_quad_vbo_.Reset();
    sprite_quad_ibo_.Reset();
    mesh_dynamic_vbo_.Reset();
    mesh_dynamic_ibo_.Reset();
    mesh_vbo_capacity_ = 0;
    mesh_ibo_capacity_ = 0;
    instance_vbo_.Reset();
    instance_vbo_capacity_ = 0;
    skybox_vbo_.Reset();
    postprocess_vbo_.Reset();
    postprocess_ibo_.Reset();
    shadow_sampler_.Reset();
    skybox_dss_.Reset();
    no_cull_rasterizer_state_.Reset();
    white_texture_srv_.Reset();
    white_texture_sampler_.Reset();
    bone_matrices_cb_.Reset();
    bone_ssbo_buf_.Reset();
    bone_ssbo_srv_.Reset();
    bone_ssbo_capacity_ = 0;
    skinned_inst_buf_.Reset();
    skinned_inst_srv_.Reset();
    skinned_inst_capacity_ = 0;
    static_mesh_cache_.clear();
    light_probe_data_cb_.Reset();

    initialized_ = false;
    DEBUG_LOG_INFO("[D3D11] DrawExecutor shutdown");
}

// ============================================================
// 几何缓冲初始化
// ============================================================

void DX11DrawExecutor::InitGeometryBuffers() {
    ID3D11Device* device = context_->device();

    // ---- 精灵合批 VBO（动态）MAX_BATCH_SPRITES × 4 顶点 × 32B ----
    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = DX11_MAX_BATCH_SPRITES * 4 * 32;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, sprite_quad_vbo_.GetAddressOf());
    }

    // ---- 精灵合批 IBO（静态）预生成 quad 索引 ----
    {
        std::vector<uint16_t> indices(DX11_MAX_BATCH_SPRITES * 6);
        for (int i = 0; i < DX11_MAX_BATCH_SPRITES; ++i) {
            uint16_t base = static_cast<uint16_t>(i * 4);
            indices[i * 6 + 0] = base + 0;
            indices[i * 6 + 1] = base + 1;
            indices[i * 6 + 2] = base + 2;
            indices[i * 6 + 3] = base + 0;
            indices[i * 6 + 4] = base + 2;
            indices[i * 6 + 5] = base + 3;
        }
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint16_t));
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = indices.data();
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

    EnsureInstanceVBOCapacity(80);
    if (instance_vbo_) {
        struct DefaultInstanceData { glm::mat4 model; int bone_offset; int pad[3]; };
        DefaultInstanceData data{glm::mat4(1.0f), 0, {0, 0, 0}};
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context_->device_context()->Map(instance_vbo_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &data, sizeof(data));
            context_->device_context()->Unmap(instance_vbo_.Get(), 0);
        }
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

void DX11DrawExecutor::EnsureInstanceVBOCapacity(size_t needed_bytes) {
    if (instance_vbo_capacity_ >= needed_bytes) return;
    size_t new_cap = (std::max)(needed_bytes, (std::max)(instance_vbo_capacity_ * 2, (size_t)80));

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = static_cast<UINT>(new_cap);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    instance_vbo_.Reset();
    HRESULT hr = context_->device()->CreateBuffer(&bd, nullptr, instance_vbo_.GetAddressOf());
    if (SUCCEEDED(hr)) instance_vbo_capacity_ = new_cap;
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
    global_state_.current_frame_stats = RenderStats{};
    bone_ssbo_uploaded_this_frame_ = false;
    inst_ssbo_uploaded_this_frame_ = false;
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

    // MRT（color_attachment_count>1）需绑定全部颜色附件 RTV（gbuffer/RSM：gAlbedo/gNormal/gPosition），
    // 否则 gbuffer.frag 的 location1/2 输出被丢弃。单附件/backbuffer 走 rtvs[0]。
    ID3D11RenderTargetView* rtvs[8] = {};
    UINT num_rtvs = 0;
    ID3D11DepthStencilView* dsv = nullptr;

    if (render_pass.render_target != 0) {
        const auto* rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (rt) {
            if (!rt->color_rtvs_mrt.empty()) {
                num_rtvs = static_cast<UINT>(rt->color_rtvs_mrt.size());
                if (num_rtvs > 8) num_rtvs = 8;
                for (UINT i = 0; i < num_rtvs; ++i) rtvs[i] = rt->color_rtvs_mrt[i].Get();
            } else if (rt->color_rtv.Get()) {
                rtvs[0] = rt->color_rtv.Get();
                num_rtvs = 1;
            }
            dsv = rt->depth_dsv.Get();
        }
    } else {
        rtvs[0] = context_->backbuffer_rtv();
        num_rtvs = rtvs[0] ? 1 : 0;
        dsv = context_->backbuffer_dsv();
    }
    // 检测深度 only pass（shadow pass）
    is_depth_only_pass_ = (num_rtvs == 0 && dsv);

    dc->OMSetRenderTargets(num_rtvs, num_rtvs ? rtvs : nullptr, dsv);

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

    // 清除（MRT 时清全部颜色附件为同一 clear_color）
    if (render_pass.clear_color_enabled && num_rtvs) {
        float clear[4] = {render_pass.clear_color.r, render_pass.clear_color.g,
                          render_pass.clear_color.b, render_pass.clear_color.a};
        for (UINT i = 0; i < num_rtvs; ++i) dc->ClearRenderTargetView(rtvs[i], clear);
    }
    if (dsv) {
        dc->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    global_state_.current_frame_stats.render_passes += 1;
    if (is_depth_only_pass_) {
        global_state_.current_frame_stats.shadow_passes += 1;
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

void DX11DrawExecutor::DrawMeshBatch(const std::vector<MeshDrawItem>& items,
                                       const glm::mat4& view, const glm::mat4& projection,
                                       DX11PipelineStateManager& pipeline_mgr,
                                       DX11ShaderManager& shader_mgr,
                                       DX11ResourceManager& resource_mgr) {
    if (items.empty()) return;
    global_state_.current_frame_stats.mesh_count += static_cast<int>(items.size());
    dse::render::UpdateSortBatchStats(global_state_.current_frame_stats, items);
    ID3D11DeviceContext* dc = context_->device_context();

    // 更新 PerFrame CB
    DX11PerFrameCB frame_data;
    frame_data.vp = projection * view;
    frame_data.view = view;
    {
        glm::mat4 inv_view = glm::inverse(view);
        frame_data.camera_pos = glm::vec4(inv_view[3][0], inv_view[3][1], inv_view[3][2], global_state_.global_wetness);
    }
    frame_data.foliage_wind = global_state_.foliage_wind;
    frame_data.foliage_push = global_state_.foliage_push;
    UpdateConstantBuffer(per_frame_cb_.Get(), &frame_data, sizeof(frame_data));

    // 更新 PerScene CB
    DX11PerSceneCB scene_data = dse::render::PreparePerSceneUBO(items[0], global_state_);
    UpdateConstantBuffer(per_scene_cb_.Get(), &scene_data, sizeof(scene_data));

    const bool gbuffer_mode = global_state_.gbuffer_rendering_mode;

    // 选择着色器：深度 only → shadow, GBuffer → gbuffer, 否则 PBR
    unsigned int shader_handle = is_depth_only_pass_
        ? shader_mgr.shadow_shader_handle()
        : (gbuffer_mode ? shader_mgr.gbuffer_shader_handle() : shader_mgr.pbr_shader_handle());
    const auto* program = shader_mgr.GetProgram(shader_handle);
    if (!program) return;
    dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);

    // 绑定 InputLayout + 拓扑
    auto* layout = shader_mgr.GetInputLayout(shader_handle);
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 绑定常量缓冲（gen.h VS: b0=PushConstants/PerObject, b1=PerFrame）
    ID3D11Buffer* vs_cbs[] = {per_object_cb_.Get(), per_frame_cb_.Get()};
    dc->VSSetConstantBuffers(0, 2, vs_cbs);
    const auto& slots = shader_mgr.pbr_texture_slots();
    if (!is_depth_only_pass_ && !gbuffer_mode) {
        // gen.h register 布局: b0=PerFrame, b1=PerScene, b2=LightProbeData, b3=PerMaterial
        ID3D11Buffer* ps_cbs[] = {per_frame_cb_.Get(), per_scene_cb_.Get(), nullptr, per_material_cb_.Get()};
        dc->PSSetConstantBuffers(0, 4, ps_cbs);

        // CSM 阴影贴图
        for (int i = 0; i < 3; ++i) {
            if (global_state_.shadow_map[i] != 0) {
                const auto* sm = resource_mgr.GetTexture(global_state_.shadow_map[i]);
                if (sm) dc->PSSetShaderResources(slots.shadow_base + i, 1, sm->srv.GetAddressOf());
            }
        }
        // 阴影采样器（gen.h: SamplerComparisonState at s5-s7）
        if (shadow_sampler_) {
            ID3D11SamplerState* shadow_samplers[3] = {shadow_sampler_.Get(), shadow_sampler_.Get(), shadow_sampler_.Get()};
            dc->PSSetSamplers(5, 3, shadow_samplers);
        }

        // 点光源/聚光灯数据已由 LightBuffer SSBO 提供

        // 点光源立方体阴影贴图
        for (int i = 0; i < 4; ++i) {
            if (global_state_.point_shadow_map[i] != 0) {
                const auto* sm = resource_mgr.GetTexture(global_state_.point_shadow_map[i]);
                if (sm) dc->PSSetShaderResources(slots.point_shadow_base + i, 1, sm->srv.GetAddressOf());
            }
        }

        // 填充聚光灯光源空间矩阵 CB（gen.h: b5 SpotLightData）并绑定
        {
            DX11SpotMatricesCB sm_cb = dse::render::PrepareSpotLightDataUBO(global_state_);
            UpdateConstantBuffer(per_spot_matrices_cb_.Get(), &sm_cb, sizeof(sm_cb));
            dc->PSSetConstantBuffers(5, 1, per_spot_matrices_cb_.GetAddressOf());
        }

        // 填充 LightProbeData CB（gen.h: b2 LightProbeData）并绑定
        if (light_probe_data_cb_) {
            DX11LightProbeDataCB lp_cb = dse::render::PrepareLightProbeUBO(global_state_);
            UpdateConstantBuffer(light_probe_data_cb_.Get(), &lp_cb, sizeof(lp_cb));
            dc->PSSetConstantBuffers(2, 1, light_probe_data_cb_.GetAddressOf());
        }

        // 聚光灯阴影贴图
        for (int i = 0; i < 4; ++i) {
            if (global_state_.spot_shadow_map[i] != 0) {
                const auto* sm = resource_mgr.GetTexture(global_state_.spot_shadow_map[i]);
                if (sm) dc->PSSetShaderResources(slots.spot_shadow_base + i, 1, sm->srv.GetAddressOf());
            }
        }
    }

    // === Bone SSBO: 收集所有蒙皮实例骨骼矩阵，一次上传 ===
    // 优先使用 bone_palette（去重后数据量极小），fallback 到 per_instance_bones
    std::vector<int> bone_offsets(items.size(), 0);
    std::vector<std::vector<int>> per_inst_bone_offsets(items.size());
    std::vector<std::vector<int>> palette_base_offsets(items.size());
    {
        size_t total_bones = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& it = items[i];
            if (it.skinned && !it.bone_palette.empty()) {
                auto& pbo = palette_base_offsets[i];
                pbo.resize(it.bone_palette.size());
                for (size_t p = 0; p < it.bone_palette.size(); ++p) {
                    pbo[p] = static_cast<int>(total_bones);
                    total_bones += (std::min)(it.bone_palette[p].size(), static_cast<size_t>(255));
                }
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
                    total_bones += (std::min)(it.per_instance_bones[j].size(), static_cast<size_t>(255));
                }
            } else if (it.skinned && !it.bone_matrices.empty()) {
                bone_offsets[i] = static_cast<int>(total_bones);
                total_bones += (std::min)(it.bone_matrices.size(), static_cast<size_t>(255));
            }
        }
        if (total_bones > 0) {
            if (!bone_ssbo_uploaded_this_frame_) {
                const size_t ssbo_bytes = total_bones * sizeof(glm::mat4);
                if (ssbo_bytes > bone_ssbo_capacity_ || !bone_ssbo_buf_) {
                    bone_ssbo_buf_.Reset();
                    bone_ssbo_srv_.Reset();
                    D3D11_BUFFER_DESC bd{};
                    bd.ByteWidth = static_cast<UINT>(ssbo_bytes);
                    bd.Usage = D3D11_USAGE_DYNAMIC;
                    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                    bd.StructureByteStride = 64;
                    HRESULT hr = context_->device()->CreateBuffer(&bd, nullptr, bone_ssbo_buf_.ReleaseAndGetAddressOf());
                    if (SUCCEEDED(hr)) {
                        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
                        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
                        srv_desc.Buffer.FirstElement = 0;
                        srv_desc.Buffer.NumElements = static_cast<UINT>(ssbo_bytes / 64);
                        context_->device()->CreateShaderResourceView(bone_ssbo_buf_.Get(), &srv_desc, bone_ssbo_srv_.ReleaseAndGetAddressOf());
                    }
                    bone_ssbo_capacity_ = ssbo_bytes;
                }
                if (bone_ssbo_buf_) {
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    HRESULT hr = dc->Map(bone_ssbo_buf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
                    if (SUCCEEDED(hr)) {
                        auto* dst = static_cast<glm::mat4*>(mapped.pData);
                        for (size_t i = 0; i < items.size(); ++i) {
                            const auto& it = items[i];
                            if (it.skinned && !it.bone_palette.empty()) {
                                for (size_t p = 0; p < it.bone_palette.size(); ++p) {
                                    size_t count = (std::min)(it.bone_palette[p].size(), static_cast<size_t>(255));
                                    memcpy(dst + palette_base_offsets[i][p],
                                           it.bone_palette[p].data(), count * sizeof(glm::mat4));
                                }
                            } else if (it.skinned && !it.per_instance_bones.empty()) {
                                for (size_t j = 0; j < it.per_instance_bones.size(); ++j) {
                                    size_t count = (std::min)(it.per_instance_bones[j].size(), static_cast<size_t>(255));
                                    memcpy(dst + per_inst_bone_offsets[i][j],
                                           it.per_instance_bones[j].data(), count * sizeof(glm::mat4));
                                }
                            } else if (it.skinned && !it.bone_matrices.empty()) {
                                size_t count = (std::min)(it.bone_matrices.size(), static_cast<size_t>(255));
                                memcpy(dst + bone_offsets[i], it.bone_matrices.data(), count * sizeof(glm::mat4));
                            }
                        }
                        dc->Unmap(bone_ssbo_buf_.Get(), 0);
                    }
                    bone_ssbo_uploaded_this_frame_ = true;
                }
            }
            if (bone_ssbo_srv_) {
                dc->VSSetShaderResources(24, 1, bone_ssbo_srv_.GetAddressOf());
            }
        }
    }

    // === Skinned inst SSBO pre-pack: 一次 Map 打包所有蒙皮实例数据 ===
    struct SkinnedInstGPU { glm::mat4 model; int bone_offset; int _pad[3]; };
    constexpr size_t kInstGPUSize = sizeof(SkinnedInstGPU); // 80 bytes

    // Shadow culling 参数（所有 item 共享，提前计算一次）
    constexpr float kShadowCullMargin     = 150.0f;
    constexpr float kBudgetOrthoThreshold = 2000.0f;
    constexpr float kBudgetBaseInstances  = 800.0f;
    constexpr float kBudgetMinInstances   = 64.0f;
    constexpr float kSkinnedShadowSkipOrtho = 1500.0f;
    constexpr float kSkinnedBudgetOrtho     = 400.0f;
    constexpr float kSkinnedBudgetBase      = 200.0f;

    const bool dx11_is_ortho = std::abs(projection[2][3]) < 0.01f;
    const bool dx11_shadow_cull_active = is_depth_only_pass_ && dx11_is_ortho;
    // 与 GL 保持一致：PreZ (perspective depth-only) 无条件跳过 skinned instanced
    // 蒙皮实例 VS 骨骼计算开销极大，PreZ 收益远不抵成本
    const bool dx11_prez_skip_skinned  = is_depth_only_pass_ && !dx11_is_ortho;
    float dx11_shadow_cull_limit = 0.0f;
    size_t dx11_shadow_inst_budget = SIZE_MAX;
    if (dx11_shadow_cull_active && std::abs(projection[0][0]) > 1e-6f) {
        float ortho_size = 1.0f / projection[0][0];
        dx11_shadow_cull_limit = ortho_size + kShadowCullMargin;
        if (ortho_size > kBudgetOrthoThreshold) {
            dx11_shadow_inst_budget = static_cast<size_t>(
                (std::max)(kBudgetBaseInstances * kBudgetOrthoThreshold / ortho_size, kBudgetMinInstances));
        }
        if (ortho_size > kSkinnedShadowSkipOrtho) {
            dx11_shadow_inst_budget = 0;
        } else if (ortho_size > kSkinnedBudgetOrtho) {
            dx11_shadow_inst_budget = static_cast<size_t>(
                (std::max)(kSkinnedBudgetBase * kSkinnedBudgetOrtho / ortho_size, 0.0f));
        }
    }

    // 计算每个 instanced item 的 byte offset 和总字节数（含 skinned 和 non-skinned）
    std::vector<size_t> inst_byte_offsets(items.size(), SIZE_MAX);
    std::vector<size_t> inst_visible_counts(items.size(), 0);
    size_t total_inst_bytes = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& it = items[i];
        if (it.instance_transforms.size() <= 1) continue;
        const bool si = it.skinned
            && (!it.per_instance_bones.empty() || !it.bone_palette.empty());
        if (si && dx11_prez_skip_skinned) continue;
        inst_byte_offsets[i] = total_inst_bytes;
        total_inst_bytes += it.instance_transforms.size() * kInstGPUSize;
    }

    // 单次 Map 打包所有 instanced item（skinned + non-skinned）
    const bool need_inst_upload = total_inst_bytes > 0
        && (dx11_shadow_cull_active || !inst_ssbo_uploaded_this_frame_);
    if (need_inst_upload) {
        if (total_inst_bytes > skinned_inst_capacity_ || !skinned_inst_buf_) {
            skinned_inst_buf_.Reset();
            skinned_inst_srv_.Reset();
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = static_cast<UINT>(total_inst_bytes);
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            bd.StructureByteStride = 80;
            HRESULT hr = context_->device()->CreateBuffer(&bd, nullptr, skinned_inst_buf_.ReleaseAndGetAddressOf());
            if (SUCCEEDED(hr)) {
                D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.Format = DXGI_FORMAT_UNKNOWN;
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
                srv_desc.Buffer.FirstElement = 0;
                srv_desc.Buffer.NumElements = static_cast<UINT>(total_inst_bytes / kInstGPUSize);
                context_->device()->CreateShaderResourceView(skinned_inst_buf_.Get(), &srv_desc, skinned_inst_srv_.ReleaseAndGetAddressOf());
            }
            skinned_inst_capacity_ = total_inst_bytes;
        }
        if (skinned_inst_buf_) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            HRESULT hr = dc->Map(skinned_inst_buf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            if (SUCCEEDED(hr)) {
                auto pack_instances = [&](void* base, bool update_visible_counts) {
                    for (size_t i = 0; i < items.size(); ++i) {
                        if (inst_byte_offsets[i] == SIZE_MAX) continue;
                        const auto& it = items[i];
                        const auto& inst_bo = per_inst_bone_offsets[i];
                        auto* dst = reinterpret_cast<SkinnedInstGPU*>(
                            static_cast<char*>(base) + inst_byte_offsets[i]);
                        size_t visible = 0;
                        for (size_t j = 0; j < it.instance_transforms.size(); ++j) {
                            if (dx11_shadow_cull_active) {
                                if (visible >= dx11_shadow_inst_budget) break;
                                if (dx11_shadow_cull_limit > 0.0f) {
                                    const glm::vec3 wp(it.instance_transforms[j][3]);
                                    const glm::vec4 ls = view * glm::vec4(wp, 1.0f);
                                    if (std::abs(ls.x) > dx11_shadow_cull_limit || std::abs(ls.y) > dx11_shadow_cull_limit)
                                        continue;
                                }
                            }
                            dst[visible].model = it.instance_transforms[j];
                            dst[visible].bone_offset = (j < inst_bo.size()) ? inst_bo[j] : 0;
                            dst[visible]._pad[0] = dst[visible]._pad[1] = dst[visible]._pad[2] = 0;
                            ++visible;
                        }
                        if (update_visible_counts)
                            inst_visible_counts[i] = visible;
                    }
                };
                pack_instances(mapped.pData, true);
                dc->Unmap(skinned_inst_buf_.Get(), 0);
                if (total_inst_bytes > 0) {
                    EnsureInstanceVBOCapacity(total_inst_bytes);
                    if (instance_vbo_) {
                        D3D11_MAPPED_SUBRESOURCE inst_mapped{};
                        if (SUCCEEDED(dc->Map(instance_vbo_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &inst_mapped))) {
                            pack_instances(inst_mapped.pData, false);
                            dc->Unmap(instance_vbo_.Get(), 0);
                        }
                    }
                }
            }
        }
        if (!dx11_shadow_cull_active) {
            inst_ssbo_uploaded_this_frame_ = true;
        } else {
            inst_ssbo_uploaded_this_frame_ = false;
        }
    } else if (!dx11_shadow_cull_active) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (inst_byte_offsets[i] != SIZE_MAX)
                inst_visible_counts[i] = items[i].instance_transforms.size();
        }
    }

    static const bool dx11_skin_diag_enabled = std::getenv("DSE_DX11_SKIN_DIAG") != nullptr;
    size_t dx11_diag_skinned_items = 0;
    size_t dx11_diag_preskinned_items = 0;
    size_t dx11_diag_srv_items = 0;
    size_t dx11_diag_preskinned_instances = 0;
    size_t dx11_diag_srv_instances = 0;
    size_t dx11_diag_single_palette_items = 0;
    size_t dx11_diag_multi_palette_items = 0;

    unsigned int last_material_tex = (std::numeric_limits<unsigned int>::max)();
    for (size_t item_idx = 0; item_idx < items.size(); ++item_idx) {
        const auto& item = items[item_idx];
        // 解析顶点/索引数据源：优先使用 shared_vertex_ptr
        const BatchVertex* vtx_data = item.shared_vertex_ptr ? item.shared_vertex_ptr : item.vertices.data();
        const uint32_t* idx_data = item.shared_index_ptr ? item.shared_index_ptr : item.indices.data();
        const size_t vtx_count = item.shared_vertex_ptr ? item.shared_vertex_count : item.vertices.size();
        const size_t idx_count = item.shared_index_ptr ? item.shared_index_count : item.indices.size();
        if (vtx_count == 0 || idx_count == 0) continue;
        const bool is_instanced = item.instance_transforms.size() > 1;
        const bool skinned_instanced = item.skinned
            && (!item.per_instance_bones.empty() || !item.bone_palette.empty())
            && item.instance_transforms.size() > 1;
        const bool dx11_preskinned_instanced = skinned_instanced
            && !dx11_prez_skip_skinned
            && item.bone_palette.size() == 1
            && !item.bone_palette[0].empty();
        thread_local std::vector<BatchVertex> dx11_preskinned_vertices;
        bool use_dx11_preskinned_vertices = false;
        if (dx11_preskinned_instanced &&
            BuildPreskinnedVertices(vtx_data, vtx_count, item.bone_palette[0], dx11_preskinned_vertices)) {
            vtx_data = dx11_preskinned_vertices.data();
            use_dx11_preskinned_vertices = true;
        }
        if (dx11_skin_diag_enabled && skinned_instanced) {
            const size_t draw_count = (inst_byte_offsets[item_idx] != SIZE_MAX)
                ? inst_visible_counts[item_idx]
                : item.instance_transforms.size();
            ++dx11_diag_skinned_items;
            if (item.bone_palette.size() == 1) {
                ++dx11_diag_single_palette_items;
            } else if (item.bone_palette.size() > 1) {
                ++dx11_diag_multi_palette_items;
            }
            if (use_dx11_preskinned_vertices) {
                ++dx11_diag_preskinned_items;
                dx11_diag_preskinned_instances += draw_count;
            } else {
                ++dx11_diag_srv_items;
                dx11_diag_srv_instances += draw_count;
            }
        }

        if (item.texture_handle != last_material_tex) {
            if (last_material_tex != (std::numeric_limits<unsigned int>::max)())
                global_state_.current_frame_stats.material_switches++;
            last_material_tex = item.texture_handle;
        }

        // === VBO/IBO: 共享 mesh 使用 IMMUTABLE 缓存，非共享走 dynamic 上传 ===
        ID3D11Buffer* bound_vbo = nullptr;
        ID3D11Buffer* bound_ibo = nullptr;
        if (item.shared_vertex_ptr && !use_dx11_preskinned_vertices) {
            auto cache_it = static_mesh_cache_.find(item.shared_vertex_ptr);
            if (cache_it == static_mesh_cache_.end() || cache_it->second.vtx_count != vtx_count) {
                DX11StaticMeshEntry entry;
                entry.vtx_count = vtx_count;
                entry.idx_count = idx_count;
                {
                    D3D11_BUFFER_DESC bd{};
                    bd.ByteWidth = static_cast<UINT>(vtx_count * sizeof(BatchVertex));
                    bd.Usage = D3D11_USAGE_IMMUTABLE;
                    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                    D3D11_SUBRESOURCE_DATA init{};
                    init.pSysMem = vtx_data;
                    context_->device()->CreateBuffer(&bd, &init, entry.vbo.GetAddressOf());
                }
                {
                    D3D11_BUFFER_DESC bd{};
                    bd.ByteWidth = static_cast<UINT>(idx_count * sizeof(uint32_t));
                    bd.Usage = D3D11_USAGE_IMMUTABLE;
                    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
                    D3D11_SUBRESOURCE_DATA init{};
                    init.pSysMem = idx_data;
                    context_->device()->CreateBuffer(&bd, &init, entry.ibo.GetAddressOf());
                }
                static_mesh_cache_[item.shared_vertex_ptr] = std::move(entry);
                cache_it = static_mesh_cache_.find(item.shared_vertex_ptr);
            }
            bound_vbo = cache_it->second.vbo.Get();
            bound_ibo = cache_it->second.ibo.Get();
        } else {
            size_t vbo_bytes = vtx_count * sizeof(BatchVertex);
            size_t ibo_bytes = idx_count * sizeof(uint32_t);
            EnsureMeshVBOCapacity(vbo_bytes);
            EnsureMeshIBOCapacity(ibo_bytes);
            {
                D3D11_MAPPED_SUBRESOURCE mapped{};
                HRESULT hr = dc->Map(mesh_dynamic_vbo_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
                if (SUCCEEDED(hr)) {
                    memcpy(mapped.pData, vtx_data, vbo_bytes);
                    dc->Unmap(mesh_dynamic_vbo_.Get(), 0);
                }
            }
            {
                D3D11_MAPPED_SUBRESOURCE mapped{};
                HRESULT hr = dc->Map(mesh_dynamic_ibo_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
                if (SUCCEEDED(hr)) {
                    memcpy(mapped.pData, idx_data, ibo_bytes);
                    dc->Unmap(mesh_dynamic_ibo_.Get(), 0);
                }
            }
            bound_vbo = mesh_dynamic_vbo_.Get();
            bound_ibo = mesh_dynamic_ibo_.Get();
        }

        {
            UINT stride = sizeof(BatchVertex);
            UINT vb_offset = 0;
            dc->IASetVertexBuffers(0, 1, &bound_vbo, &stride, &vb_offset);
            ID3D11Buffer* inst_vbo = instance_vbo_.Get();
            UINT inst_stride = static_cast<UINT>(kInstGPUSize);
            UINT inst_offset = (inst_byte_offsets[item_idx] != SIZE_MAX)
                ? static_cast<UINT>(inst_byte_offsets[item_idx])
                : 0;
            dc->IASetVertexBuffers(1, 1, &inst_vbo, &inst_stride, &inst_offset);
        }
        dc->IASetIndexBuffer(bound_ibo, DXGI_FORMAT_R32_UINT, 0);

        // PerObject (PushConstants cbuffer: model, skinned, morph_enabled, bone_offset)
        DX11PerObjectCB obj_data{};
        obj_data.model = item.model;
        const bool nonskinned_instanced = is_instanced && !skinned_instanced;
        obj_data.skinned = use_dx11_preskinned_vertices ? 3 : (skinned_instanced ? 2 : (nonskinned_instanced ? 3 : (item.skinned ? 1 : 0)));
        obj_data.morph_enabled = item.morph_enabled ? 1 : 0;
        obj_data.bone_offset = skinned_instanced
            ? ((inst_byte_offsets[item_idx] != SIZE_MAX) ? static_cast<int>(inst_byte_offsets[item_idx] / kInstGPUSize) : 0)
            : (item.skinned ? bone_offsets[item_idx] : 0);
        obj_data.foliage = item.foliage ? 1 : 0;
        UpdateConstantBuffer(per_object_cb_.Get(), &obj_data, sizeof(obj_data));

        // PerMaterial
        DX11PerMaterialCB mat_data = dse::render::PreparePerMaterialUBO(item, global_state_);
        UpdateConstantBuffer(per_material_cb_.Get(), &mat_data, sizeof(mat_data));

        // Re-upload PerScene CB when shading mode changes per item
        if (static_cast<float>(item.shading_mode) != scene_data.light_params.w) {
            scene_data.light_params.w = static_cast<float>(item.shading_mode);
            UpdateConstantBuffer(per_scene_cb_.Get(), &scene_data, sizeof(scene_data));
        }

        if (!is_depth_only_pass_ && !gbuffer_mode && terrain_params_cb_) {
            DX11TerrainParamsCB terrain_params{};
            terrain_params.flags.x = item.splat_enabled ? 1.0f : 0.0f;
            terrain_params.flags.y = item.snow_coverage;
            terrain_params.flags.z = item.snow_normal_threshold;
            terrain_params.flags.w = item.snow_edge_sharpness;
            terrain_params.tiling = item.splat_tiling;
            terrain_params.snow_params = glm::vec4(item.snow_albedo, item.snow_roughness);
            UpdateConstantBuffer(terrain_params_cb_.Get(), &terrain_params, sizeof(terrain_params));
            dc->PSSetConstantBuffers(4, 1, terrain_params_cb_.GetAddressOf());
        }

        // PBR 纹理绑定（slot 编号由 reflection 驱动）
        const auto* tex = resource_mgr.GetTexture(item.texture_handle);
        if (tex) {
            dc->PSSetShaderResources(slots.albedo, 1, tex->srv.GetAddressOf());
            dc->PSSetSamplers(slots.albedo, 1, tex->sampler.GetAddressOf());
        } else if (white_texture_srv_) {
            dc->PSSetShaderResources(slots.albedo, 1, white_texture_srv_.GetAddressOf());
            dc->PSSetSamplers(slots.albedo, 1, white_texture_sampler_.GetAddressOf());
        }
        if (item.normal_map_handle) {
            const auto* nm = resource_mgr.GetTexture(item.normal_map_handle);
            if (nm) dc->PSSetShaderResources(slots.normal, 1, nm->srv.GetAddressOf());
        }
        if (item.metallic_roughness_map_handle) {
            const auto* mr = resource_mgr.GetTexture(item.metallic_roughness_map_handle);
            if (mr) dc->PSSetShaderResources(slots.metallic_roughness, 1, mr->srv.GetAddressOf());
        }
        if (item.emissive_map_handle) {
            const auto* em = resource_mgr.GetTexture(item.emissive_map_handle);
            if (em) dc->PSSetShaderResources(slots.emissive, 1, em->srv.GetAddressOf());
        }
        if (item.occlusion_map_handle) {
            const auto* oc = resource_mgr.GetTexture(item.occlusion_map_handle);
            if (oc) dc->PSSetShaderResources(slots.occlusion, 1, oc->srv.GetAddressOf());
        }

        // 双面材质面剔除切换（与 OpenGL/Vulkan 的 material_double_sided 对齐）
        ComPtr<ID3D11RasterizerState> prev_rasterizer_state;
        bool rasterizer_switched = false;
        if (item.material_double_sided && no_cull_rasterizer_state_) {
            dc->RSGetState(prev_rasterizer_state.GetAddressOf());
            dc->RSSetState(no_cull_rasterizer_state_.Get());
            rasterizer_switched = true;
        }

        if (is_instanced && skinned_instanced) {
            // 使用预打包的 skinned inst SSBO 数据（pre-pack 阶段已完成 Map/Unmap）
            if (inst_byte_offsets[item_idx] == SIZE_MAX || inst_visible_counts[item_idx] == 0) {
                // PreZ 跳过或全部被阴影剔除
            } else if (skinned_inst_srv_) {
                dc->VSSetShaderResources(26, 1, skinned_inst_srv_.GetAddressOf());

                const UINT draw_count = static_cast<UINT>(inst_visible_counts[item_idx]);
                dc->DrawIndexedInstanced(static_cast<UINT>(idx_count), draw_count, 0, 0, 0);
                global_state_.current_frame_stats.draw_calls += 1;
                global_state_.current_frame_stats.instanced_draw_calls++;
                global_state_.current_frame_stats.instanced_mesh_count += static_cast<int>(draw_count);
            }
        } else if (is_instanced) {
            // Non-skinned hardware instancing (model matrix from instance VBO attributes, skinned=3)
            if (inst_byte_offsets[item_idx] == SIZE_MAX || inst_visible_counts[item_idx] == 0) {
                // 全部被阴影剔除
            } else {
                const UINT draw_count = static_cast<UINT>(inst_visible_counts[item_idx]);
                dc->DrawIndexedInstanced(static_cast<UINT>(idx_count), draw_count, 0, 0, 0);
                global_state_.current_frame_stats.draw_calls += 1;
                global_state_.current_frame_stats.instanced_draw_calls++;
                global_state_.current_frame_stats.instanced_mesh_count += static_cast<int>(draw_count);
            }
        } else {
            dc->DrawIndexed(static_cast<UINT>(idx_count), 0, 0);
            global_state_.current_frame_stats.draw_calls++;
        }
        global_state_.current_frame_stats.triangle_count += static_cast<int>(idx_count / 3) * static_cast<int>(is_instanced ? item.instance_transforms.size() : 1);

        // 恢复光栅化状态
        if (rasterizer_switched) {
            dc->RSSetState(prev_rasterizer_state.Get());
        }
    }
    if (dx11_skin_diag_enabled && dx11_diag_skinned_items > 0) {
        static int dx11_skin_diag_logs = 0;
        if (dx11_skin_diag_logs < 24 || (dx11_skin_diag_logs % 60) == 0) {
            DEBUG_LOG_INFO("[DX11][SkinDiag] pass={} ortho={} items={} single_palette={} multi_palette={} preskinned_items={} preskinned_instances={} srv_items={} srv_instances={} inst_upload={} total_inst_bytes={}",
                           is_depth_only_pass_ ? "depth" : (gbuffer_mode ? "gbuffer" : "forward"),
                           dx11_is_ortho ? 1 : 0,
                           dx11_diag_skinned_items,
                           dx11_diag_single_palette_items,
                           dx11_diag_multi_palette_items,
                           dx11_diag_preskinned_items,
                           dx11_diag_preskinned_instances,
                           dx11_diag_srv_items,
                           dx11_diag_srv_instances,
                           need_inst_upload ? 1 : 0,
                           total_inst_bytes);
        }
        ++dx11_skin_diag_logs;
    }
}

// ============================================================
// 通用绘制原语 (A1)
// ============================================================

void DX11DrawExecutor::PrimBindShaderProgram(unsigned int program_handle) {
    prim_program_handle_ = program_handle;
}

void DX11DrawExecutor::PrimBindVertexBuffer(unsigned int buffer_handle, uint32_t stride,
                                            const std::vector<VertexAttr>& attrs) {
    prim_vbo_handle_ = buffer_handle;
    prim_stride_ = stride;
    prim_attrs_ = attrs;
}

void DX11DrawExecutor::PrimBindTextureCube(unsigned int slot, unsigned int cubemap_handle) {
    prim_cube_slot_ = slot;
    prim_cubemap_ = cubemap_handle;
}

void DX11DrawExecutor::PrimPushConstantsMat4(const glm::mat4& value) {
    prim_push_mat4_ = value;
    prim_has_push_ = true;
}

void DX11DrawExecutor::PrimSetTopology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::LineStrip: prim_topology_ = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
        case PrimitiveTopology::LineList:  prim_topology_ = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;  break;
        case PrimitiveTopology::PointList: prim_topology_ = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
        case PrimitiveTopology::TriangleList:
        default:                           prim_topology_ = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    }
}

void DX11DrawExecutor::PrimDraw(uint32_t vertex_count, uint32_t first_vertex,
                                DX11ShaderManager& shader_mgr,
                                DX11ResourceManager& resource_mgr) {
    ID3D11DeviceContext* dc = context_->device_context();

    const auto* program = shader_mgr.GetProgram(prim_program_handle_);
    if (!program) return;
    // 顶点缓冲可缺省（vertexless：SV_VertexID 取 SSBO）。prim_vbo_handle_==0 时不绑 VB。
    const auto* buf = resource_mgr.GetBuffer(prim_vbo_handle_);

    // push-constant 风格的 mat4（u_vp）→ PerFrame cbuffer b0。
    // 天空盒 VS 仅读取首个 mat4（PerFrameUBO.vp，offset 0），其余字段无关。
    if (prim_has_push_) {
        DX11PerFrameCB frame_data{};
        frame_data.vp = prim_push_mat4_;
        UpdateConstantBuffer(per_frame_cb_.Get(), &frame_data, sizeof(frame_data));
        ID3D11Buffer* cbs[] = {per_frame_cb_.Get()};
        dc->VSSetConstantBuffers(0, 1, cbs);
    }

    dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);

    // UBO（constant buffer）按 slot → b<slot>，VS+PS 同绑（毛发组合 HairUniforms\@b0 跨 VS/FS 共享）。
    for (const auto& [slot, handle] : prim_ubos_) {
        const auto* cbuf = resource_mgr.GetBuffer(handle);
        if (cbuf && cbuf->buffer) {
            ID3D11Buffer* cb = cbuf->buffer.Get();
            dc->VSSetConstantBuffers(slot, 1, &cb);
            dc->PSSetConstantBuffers(slot, 1, &cb);
        }
    }

    // 2D 纹理按 slot → t<slot>/s<slot>。
    for (const auto& [slot, handle] : prim_textures_) {
        const auto* tex = resource_mgr.GetTexture(handle);
        if (tex) {
            dc->PSSetShaderResources(slot, 1, tex->srv.GetAddressOf());
            if (slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT)
                dc->PSSetSamplers(slot, 1, tex->sampler.GetAddressOf());
        }
    }

    // 图形阶段 SSBO 按 slot → t<slot>，仅绑 VS（毛发 position/tangent 是顶点级资源；
    // @SSBO_LOW_REGISTERS 落低位 t，同绑 PS 会覆盖 PS 纹理槽）。
    for (const auto& [slot, b] : prim_ssbos_) {
        ID3D11ShaderResourceView* srv = resource_mgr.GetSSBORangeSRV(b.handle, b.offset, b.size);
        if (srv) {
            dc->VSSetShaderResources(slot, 1, &srv);
        }
    }

    auto* layout = shader_mgr.GetInputLayout(prim_program_handle_);
    dc->IASetInputLayout((buf && buf->buffer && layout) ? layout : nullptr);
    dc->IASetPrimitiveTopology(prim_topology_);

    if (prim_cubemap_ != 0) {
        const auto* tex = resource_mgr.GetTexture(prim_cubemap_);
        if (tex) {
            dc->PSSetShaderResources(prim_cube_slot_, 1, tex->srv.GetAddressOf());
            dc->PSSetSamplers(prim_cube_slot_, 1, tex->sampler.GetAddressOf());
        }
    }

    if (buf && buf->buffer) {
        UINT stride = prim_stride_;
        UINT offset = 0;
        dc->IASetVertexBuffers(0, 1, buf->buffer.GetAddressOf(), &stride, &offset);
    }

    // 深度/光栅/混合已由 SetPipelineState→ApplyPipelineState 设定，此处不再 save/restore。
    dc->Draw(vertex_count, first_vertex);
    global_state_.current_frame_stats.draw_calls++;
}

// --- 通用绘制原语 (B0): 索引 / 2D 纹理 / UBO / 索引绘制 ---

void DX11DrawExecutor::PrimBindIndexBuffer(unsigned int buffer_handle, IndexType type) {
    prim_index_buffer_handle_ = buffer_handle;
    prim_index_format_ = (type == IndexType::UInt32) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
}

void DX11DrawExecutor::PrimBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim /*dim*/) {
    // DX11 的 SRV 在纹理创建时已按维度定型；slot 直接映射到 t<slot>/s<slot>。
    prim_textures_[slot] = texture_handle;
}

void DX11DrawExecutor::PrimBindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                             uint32_t /*offset*/, uint32_t /*size*/) {
    // D3D11 constant buffer 整块绑定到 b<slot>（offset/size 子区间 v1 暂不支持）。
    prim_ubos_[slot] = buffer_handle;
}

void DX11DrawExecutor::PrimBindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                                             uint32_t offset, uint32_t size) {
    // SSBO(ByteAddressBuffer SRV) 映射到 t<slot>；offset/size!=0 走子区间 SRV。
    prim_ssbos_[slot] = PrimSSBOBinding{buffer_handle, offset, size};
}

void DX11DrawExecutor::PrimDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex,
                                       DX11ShaderManager& shader_mgr,
                                       DX11ResourceManager& resource_mgr) {
    PrimDrawIndexedInstanced(index_count, 1, first_index, base_vertex, 0, shader_mgr, resource_mgr);
}

void DX11DrawExecutor::PrimDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                                uint32_t first_index, int32_t base_vertex,
                                                uint32_t first_instance,
                                                DX11ShaderManager& shader_mgr,
                                                DX11ResourceManager& resource_mgr) {
    ID3D11DeviceContext* dc = context_->device_context();

    const auto* program = shader_mgr.GetProgram(prim_program_handle_);
    if (!program) return;
    // VB 可缺省（vertexless：SV_VertexID 取索引值→SSBO，毛发用）；IB 必须存在。
    const auto* vb = resource_mgr.GetBuffer(prim_vbo_handle_);
    const auto* ib = resource_mgr.GetBuffer(prim_index_buffer_handle_);
    if (!ib || !ib->buffer) return;

    dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);

    // UBO（constant buffer）按 slot → b<slot>，VS+PS 同绑（PS 未用则无害）。
    for (const auto& [slot, handle] : prim_ubos_) {
        const auto* cbuf = resource_mgr.GetBuffer(handle);
        if (cbuf && cbuf->buffer) {
            ID3D11Buffer* cb = cbuf->buffer.Get();
            dc->VSSetConstantBuffers(slot, 1, &cb);
            dc->PSSetConstantBuffers(slot, 1, &cb);
        }
    }

    // 2D 纹理按 slot → t<slot>/s<slot>。
    for (const auto& [slot, handle] : prim_textures_) {
        const auto* tex = resource_mgr.GetTexture(handle);
        if (tex) {
            dc->PSSetShaderResources(slot, 1, tex->srv.GetAddressOf());
            // D3D11 仅 16 个采样器槽（s0..s15）。点光 cube 阴影 SRV 落在 t16..t19，
            // 其采样器经 SPIRV-Cross 去重后复用 s10（DDGI 采样器），故 slot>=16 只绑 SRV、
            // 不绑采样器；否则 PSSetSamplers(StartSlot>=16) 越界，触发运行期错误/崩溃。
            if (slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT)
                dc->PSSetSamplers(slot, 1, tex->sampler.GetAddressOf());
        }
    }

    // 图形阶段 SSBO 按 slot → t<slot>，仅绑 VS：骨骼/实例/毛发顶点是顶点级资源，
    // 且离线编译器对 @SSBO_LOW_REGISTERS 着色器把 SSBO 落到低位 t（如 t0），
    // 同绑 PS 会覆盖 PS 纹理槽（如反照率 t0）。VS 无纹理，故仅 VS 安全。
    for (const auto& [slot, b] : prim_ssbos_) {
        ID3D11ShaderResourceView* srv = resource_mgr.GetSSBORangeSRV(b.handle, b.offset, b.size);
        if (srv) {
            dc->VSSetShaderResources(slot, 1, &srv);
        }
    }

    auto* layout = shader_mgr.GetInputLayout(prim_program_handle_);
    dc->IASetInputLayout((vb && vb->buffer && layout) ? layout : nullptr);
    dc->IASetPrimitiveTopology(prim_topology_);

    if (vb && vb->buffer) {
        UINT stride = prim_stride_;
        UINT offset = 0;
        dc->IASetVertexBuffers(0, 1, vb->buffer.GetAddressOf(), &stride, &offset);
    }
    dc->IASetIndexBuffer(ib->buffer.Get(), prim_index_format_, 0);

    // 深度/光栅/混合已由 SetPipelineState→ApplyPipelineState 设定。
    if (instance_count == 1 && first_instance == 0) {
        dc->DrawIndexed(index_count, first_index, base_vertex);
    } else {
        // 注：DX11 的 SV_InstanceID 始终从 0 起，StartInstanceLocation 仅偏移 per-instance 顶点取数；
        // 实例数据偏移需经 SSBO/instance VB 偏移表达（见 RHI_PRIMITIVE_CONTRACT.md §6）。
        dc->DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
        global_state_.current_frame_stats.instanced_draw_calls++;
    }
    global_state_.current_frame_stats.draw_calls++;
}

void DX11DrawExecutor::PrimDrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset,
                                               DX11ShaderManager& shader_mgr,
                                               DX11ResourceManager& resource_mgr) {
    ID3D11DeviceContext* dc = context_->device_context();

    const auto* program = shader_mgr.GetProgram(prim_program_handle_);
    if (!program) return;
    const auto* vb = resource_mgr.GetBuffer(prim_vbo_handle_);
    if (!vb || !vb->buffer) return;
    const auto* ib = resource_mgr.GetBuffer(prim_index_buffer_handle_);
    if (!ib || !ib->buffer) return;

    // 解析 indirect args buffer：先查 indirect map，再退回 SSBO map（draw cmd 存于 SSBO）。
    ID3D11Buffer* args_buf = nullptr;
    const DX11IndirectBuffer* ibuf = resource_mgr.GetIndirectBuffer(indirect_buffer);
    if (ibuf && ibuf->buffer) {
        args_buf = ibuf->buffer.Get();
    } else {
        const DX11SSBO* sbuf = resource_mgr.GetSSBO(indirect_buffer);
        if (sbuf && sbuf->buffer) args_buf = sbuf->buffer.Get();
    }
    if (!args_buf) return;

    dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);

    // UBO（constant buffer）按 slot → b<slot>，VS+PS 同绑（PS 未用则无害）。
    for (const auto& [slot, handle] : prim_ubos_) {
        const auto* cbuf = resource_mgr.GetBuffer(handle);
        if (cbuf && cbuf->buffer) {
            ID3D11Buffer* cb = cbuf->buffer.Get();
            dc->VSSetConstantBuffers(slot, 1, &cb);
            dc->PSSetConstantBuffers(slot, 1, &cb);
        }
    }

    // 2D 纹理按 slot → t<slot>/s<slot>。
    for (const auto& [slot, handle] : prim_textures_) {
        const auto* tex = resource_mgr.GetTexture(handle);
        if (tex) {
            dc->PSSetShaderResources(slot, 1, tex->srv.GetAddressOf());
            // D3D11 仅 16 个采样器槽（s0..s15）。点光 cube 阴影 SRV 落在 t16..t19，
            // 其采样器经 SPIRV-Cross 去重后复用 s10（DDGI 采样器），故 slot>=16 只绑 SRV、
            // 不绑采样器；否则 PSSetSamplers(StartSlot>=16) 越界，触发运行期错误/崩溃。
            if (slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT)
                dc->PSSetSamplers(slot, 1, tex->sampler.GetAddressOf());
        }
    }

    // 图形阶段 SSBO 按 slot → t<slot>，仅绑 VS（与 PrimDrawIndexedInstanced 同语义）。
    for (const auto& [slot, b] : prim_ssbos_) {
        ID3D11ShaderResourceView* srv = resource_mgr.GetSSBORangeSRV(b.handle, b.offset, b.size);
        if (srv) {
            dc->VSSetShaderResources(slot, 1, &srv);
        }
    }

    auto* layout = shader_mgr.GetInputLayout(prim_program_handle_);
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT stride = prim_stride_;
    UINT offset = 0;
    dc->IASetVertexBuffers(0, 1, vb->buffer.GetAddressOf(), &stride, &offset);
    dc->IASetIndexBuffer(ib->buffer.Get(), prim_index_format_, 0);

    // 间接绘制：从 args buffer 的 byte_offset 处读取 5×uint32 参数。
    // 契约：DX11 SV_InstanceID 仍从 0 起，base_instance 偏移须经 SSBO 偏移表达（§6）。
    dc->DrawIndexedInstancedIndirect(args_buf, byte_offset);
    global_state_.current_frame_stats.draw_calls++;
    global_state_.current_frame_stats.indirect_draw_calls++;
}

void DX11DrawExecutor::DispatchComputePass(const ComputeDispatch& dispatch,
                                            DX11ShaderManager& shader_mgr,
                                            DX11ResourceManager& resource_mgr) {
    if (!context_ || dispatch.shader == 0 || current_rt_handle_ == 0) return;
    ID3D11DeviceContext* dc = context_->device_context();

    const auto* rt = resource_mgr.GetRenderTarget(current_rt_handle_);
    if (!rt || !rt->color_uav) return;

    const unsigned int uav_rt = current_rt_handle_;
    // 解绑当前 RTV，清空 PS SRV（防止 D3D11 Validation Layer UAV 冲突），切换到 CS 路径。
    ID3D11RenderTargetView* null_rtv = nullptr;
    dc->OMSetRenderTargets(0, &null_rtv, nullptr);
    ID3D11ShaderResourceView* null_srvs[8] = {};
    dc->PSSetShaderResources(0, 8, null_srvs);

    const UINT dst_w = static_cast<UINT>(rt->width);
    const UINT dst_h = static_cast<UINT>(rt->height);
    const UINT tx = (dst_w + 7) / 8;
    const UINT ty = (dst_h + 7) / 8;
    DispatchCompute(dispatch.shader, dispatch.source_texture, uav_rt, tx, ty,
                    dispatch.blend_weight, shader_mgr, resource_mgr);

    // 重新绑定 RTV（EndRenderPass 会解绑并 resolve）。
    ID3D11RenderTargetView* rtv = rt->color_rtv.Get();
    dc->OMSetRenderTargets(rtv ? 1 : 0, rtv ? &rtv : &null_rtv, rt->depth_dsv.Get());
}

void DX11DrawExecutor::DispatchCompute(unsigned int cs_handle,
                                        unsigned int srv_texture_handle,
                                        unsigned int uav_rt_handle,
                                        UINT threads_x, UINT threads_y,
                                        float blend_weight,
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
        struct BloomParams { float src_w, src_h, dst_w, dst_h, blend_w; } bp;
        bp.src_w = 1.0f / static_cast<float>(src_tex->width > 0 ? src_tex->width : 1);
        bp.src_h = 1.0f / static_cast<float>(src_tex->height > 0 ? src_tex->height : 1);
        bp.dst_w = 1.0f / static_cast<float>(dst_rt->width > 0 ? dst_rt->width : 1);
        bp.dst_h = 1.0f / static_cast<float>(dst_rt->height > 0 ? dst_rt->height : 1);
        bp.blend_w = blend_weight;

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
        UINT ic_bind = static_cast<UINT>(-1);
        dc->CSSetUnorderedAccessViews(0, 1, dst_rt->color_uav.GetAddressOf(), &ic_bind);
    }

    dc->Dispatch(threads_x, threads_y, 1);

    // 解绑 UAV / SRV
    ID3D11UnorderedAccessView* null_uav = nullptr;
    ID3D11ShaderResourceView* null_srv = nullptr;
    UINT ic_unbind = 0;
    dc->CSSetUnorderedAccessViews(0, 1, &null_uav, &ic_unbind);
    dc->CSSetShaderResources(0, 1, &null_srv);
    dc->CSSetShader(nullptr, nullptr, 0);
}


// ============================================================
// GPU-Driven PBR 渲染设置
// ============================================================

void DX11DrawExecutor::SetupGPUDrivenPBR(const glm::mat4& view, const glm::mat4& proj,
                                          const glm::vec3& camera_pos,
                                          const glm::vec3& light_dir, const glm::vec3& light_color,
                                          float light_intensity, float ambient_intensity,
                                          float shadow_strength,
                                          DX11PipelineStateManager& pipeline_mgr,
                                          DX11ShaderManager& shader_mgr) {
    ID3D11DeviceContext* dc = context_->device_context();
    if (!dc) return;

    // 优先使用 GPU-driven shader（VS 从 ByteAddressBuffer t16 读 model via draw_id）
    unsigned int prog = shader_mgr.gpu_driven_pbr_shader_handle();
    const auto* program = shader_mgr.GetProgram(prog);
    if (!program) {
        prog = shader_mgr.pbr_shader_handle();
        program = shader_mgr.GetProgram(prog);
        if (!program) return;
    }
    if (program->vertex_shader) dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    if (program->pixel_shader) dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);
    auto* layout = shader_mgr.GetInputLayout(prog);
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // PerFrame CB
    DX11PerFrameCB frame_data{};
    frame_data.vp = proj * view;
    frame_data.view = view;
    frame_data.camera_pos = glm::vec4(camera_pos, 0.0f);
    UpdateConstantBuffer(per_frame_cb_.Get(), &frame_data, sizeof(frame_data));

    // PerScene CB
    DX11PerSceneCB scene_data{};
    const float dx11_gpu_driven_light = global_state_.force_unlit ? 0.0f : 1.0f;
    scene_data.light_dir_and_enabled   = glm::vec4(light_dir, dx11_gpu_driven_light);
    scene_data.light_color_and_ambient = glm::vec4(light_color, ambient_intensity);
    const float receive_shadow = (shadow_strength > 0.0f) ? 1.0f : 0.0f;
    scene_data.light_params            = glm::vec4(light_intensity, shadow_strength, receive_shadow, 0.0f);
    scene_data.cascade_splits = glm::vec4(
        global_state_.cascade_splits[0], global_state_.cascade_splits[1],
        global_state_.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i)
        scene_data.light_space_matrices[i] = global_state_.light_space_matrix[i];
    for (int i = 0; i < 3; ++i)
        scene_data.shadow_atlas_regions[i] = global_state_.shadow_atlas_region[i];
    UpdateConstantBuffer(per_scene_cb_.Get(), &scene_data, sizeof(scene_data));

    // GPU-driven VS layout: b0=PerFrame, b7=DrawIdCB
    // (gpu_driven_vert 用 b0=PerFrame, 不需要 b0=PushConstants/PerObject)
    ID3D11Buffer* vs_cbs[8] = {};
    vs_cbs[0] = per_frame_cb_.Get();   // b0 = PerFrame (VS)
    vs_cbs[7] = draw_id_cb_.Get();     // b7 = DrawIdCB
    dc->VSSetConstantBuffers(0, 8, vs_cbs);

    DX11SpotMatricesCB sm_cb = dse::render::PrepareSpotLightDataUBO(global_state_);
    UpdateConstantBuffer(per_spot_matrices_cb_.Get(), &sm_cb, sizeof(sm_cb));

    DX11TerrainParamsCB terrain_params{};
    terrain_params.flags.x = 0.0f;
    terrain_params.tiling = glm::vec4(10.0f);
    UpdateConstantBuffer(terrain_params_cb_.Get(), &terrain_params, sizeof(terrain_params));

    ID3D11Buffer* ps_cbs[] = {
        per_frame_cb_.Get(),
        per_scene_cb_.Get(),
        nullptr,
        per_material_cb_.Get(),
        terrain_params_cb_.Get(),
        per_spot_matrices_cb_.Get()
    };
    dc->PSSetConstantBuffers(0, 6, ps_cbs);
}

// ============================================================
// GPU-Driven Shadow 渲染设置
// ============================================================

void DX11DrawExecutor::SetupGPUDrivenShadow(const glm::mat4& light_view, const glm::mat4& light_proj,
                                              DX11PipelineStateManager& pipeline_mgr,
                                              DX11ShaderManager& shader_mgr) {
    ID3D11DeviceContext* dc = context_->device_context();
    if (!dc) return;

    // 优先使用 GPU-driven shadow shader（VS 从 ByteAddressBuffer t16 读 model via draw_id）
    unsigned int prog = shader_mgr.gpu_driven_shadow_shader_handle();
    const auto* program = shader_mgr.GetProgram(prog);
    if (!program) {
        prog = shader_mgr.shadow_shader_handle();
        program = shader_mgr.GetProgram(prog);
        if (!program) return;
    }
    if (program->vertex_shader) dc->VSSetShader(program->vertex_shader.Get(), nullptr, 0);
    if (program->pixel_shader) dc->PSSetShader(program->pixel_shader.Get(), nullptr, 0);
    auto* layout = shader_mgr.GetInputLayout(prog);
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    DX11PerFrameCB frame_data{};
    frame_data.vp = light_proj * light_view;
    frame_data.view = light_view;
    frame_data.camera_pos = glm::vec4(0.0f);
    UpdateConstantBuffer(per_frame_cb_.Get(), &frame_data, sizeof(frame_data));

    // GPU-driven VS: b0=PerFrame, b7=DrawIdCB
    ID3D11Buffer* vs_cbs[8] = {};
    vs_cbs[0] = per_frame_cb_.Get();
    vs_cbs[7] = draw_id_cb_.Get();
    dc->VSSetConstantBuffers(0, 8, vs_cbs);
    ID3D11Buffer* ps_cbs[] = {per_frame_cb_.Get(), per_scene_cb_.Get(), nullptr, per_material_cb_.Get()};
    dc->PSSetConstantBuffers(0, 4, ps_cbs);
}

void DX11DrawExecutor::UpdatePerObjectCB(const DX11PerObjectCB& data) {
    if (per_object_cb_) {
        UpdateConstantBuffer(per_object_cb_.Get(), &data, sizeof(data));
    }
}

void DX11DrawExecutor::UpdateDrawId(uint32_t draw_id) {
    if (draw_id_cb_) {
        // cbuffer DrawIdCB : register(b7) { uint g_draw_id; }; — padded to 16 bytes
        struct alignas(16) { uint32_t id; uint32_t pad[3]; } data = {draw_id, {0,0,0}};
        UpdateConstantBuffer(draw_id_cb_.Get(), &data, sizeof(data));
    }
}

void DX11DrawExecutor::UpdateGPUDrivenMaterial(const void* mat_data) {
    if (!mat_data || !per_material_cb_) return;
    // GPUMaterialData (128B) 与 DX11PerMaterialCB 二进制兼容
    UpdateConstantBuffer(per_material_cb_.Get(), mat_data, sizeof(DX11PerMaterialCB));
}

} // namespace render
} // namespace dse
