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
    prim_push_cb_          = CreateConstantBuffer(DX11DrawExecutor::kPrimPushMaxBytes); // 通用 push cbuffer（b0）
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

    prim_push_cb_.Reset();
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
    global_state_.current_pass_depth_only = is_depth_only_pass_;

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
    global_state_.current_pass_depth_only = false;
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
// 通用绘制原语 (A1)
// ============================================================

void DX11DrawExecutor::PrimBindShaderProgram(unsigned int program_handle) {
    prim_program_handle_ = program_handle;
}

void DX11DrawExecutor::PrimBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                                            const std::vector<VertexAttr>& attrs, VertexInputRate rate) {
    if (slot == 0) {
        prim_vbo_handle_ = buffer_handle;
        prim_stride_ = stride;
        prim_attrs_ = attrs;
        prim_slot0_rate_ = rate;
    } else {
        prim_extra_vbs_[slot] = PrimVbExtra{buffer_handle, stride, attrs, rate};
    }
}

namespace {
DXGI_FORMAT PrimAttrFormat(uint32_t components) {
    switch (components) {
        case 1:  return DXGI_FORMAT_R32_FLOAT;
        case 2:  return DXGI_FORMAT_R32G32_FLOAT;
        case 3:  return DXGI_FORMAT_R32G32B32_FLOAT;
        default: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
}
}  // namespace

ID3D11InputLayout* DX11DrawExecutor::ResolvePrimInputLayout(DX11ShaderManager& shader_mgr) const {
    // 旧单流 + per-vertex：沿用反射推导布局（与历史行为字节一致）。
    if (prim_extra_vbs_.empty() && prim_slot0_rate_ == VertexInputRate::PerVertex) {
        return shader_mgr.GetInputLayout(prim_program_handle_);
    }
    // 出现 slot>0 或 per-instance：据各 slot 属性 + rate 显式组装多 slot 布局。
    // 反射布局恒落 slot0/per-vertex，无法表达 per-instance，故此处全量自建。
    std::vector<D3D11_INPUT_ELEMENT_DESC> elems;
    auto append = [&elems](uint32_t slot, const std::vector<VertexAttr>& attrs, VertexInputRate rate) {
        const D3D11_INPUT_CLASSIFICATION cls = (rate == VertexInputRate::PerInstance)
            ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
        const UINT step = (rate == VertexInputRate::PerInstance) ? 1u : 0u;
        for (const auto& a : attrs) {
            elems.push_back(D3D11_INPUT_ELEMENT_DESC{
                "TEXCOORD", a.location, PrimAttrFormat(a.components),
                slot, a.offset, cls, step});
        }
    };
    append(0, prim_attrs_, prim_slot0_rate_);
    for (const auto& [slot, b] : prim_extra_vbs_) append(slot, b.attrs, b.rate);
    return shader_mgr.GetOrCreatePrimInputLayout(prim_program_handle_, elems);
}

void DX11DrawExecutor::BindPrimVertexBuffers(ID3D11DeviceContext* dc,
                                             DX11ResourceManager& resource_mgr) const {
    const auto* vb0 = resource_mgr.GetBuffer(prim_vbo_handle_);
    if (vb0 && vb0->buffer) {
        UINT stride = prim_stride_;
        UINT offset = 0;
        dc->IASetVertexBuffers(0, 1, vb0->buffer.GetAddressOf(), &stride, &offset);
    }
    for (const auto& [slot, b] : prim_extra_vbs_) {
        const auto* vb = resource_mgr.GetBuffer(b.handle);
        if (vb && vb->buffer) {
            UINT stride = b.stride;
            UINT offset = 0;
            dc->IASetVertexBuffers(slot, 1, vb->buffer.GetAddressOf(), &stride, &offset);
        }
    }
}

void DX11DrawExecutor::PrimPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) {
    if (!data || size == 0) return;
    if (offset + size > kPrimPushMaxBytes) return; // 越界保护
    std::memcpy(prim_push_data_ + offset, data, size);
    prim_push_stage_mask_ |= static_cast<uint32_t>(stage);
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

    // 通用 push constant 字节块 → push cbuffer b0。须在 UBO 循环之后绑定：prim_ubos_ 跨绘制
    // 累积不清零，b0 可能残留前次 BindUniformBuffer(0)（sprite/hair 等）的句柄；push 在后覆盖
    // 之，确保 skybox/PP 的 push 数据胜出（这些 shader 的 b0 即 push 块，无真 UBO 落 b0）。
    if (prim_has_push_) {
        UpdateConstantBuffer(prim_push_cb_.Get(), prim_push_data_, kPrimPushMaxBytes);
        ID3D11Buffer* push_cbs[] = {prim_push_cb_.Get()};
        if (prim_push_stage_mask_ & static_cast<uint32_t>(ShaderStage::Vertex))
            dc->VSSetConstantBuffers(0, 1, push_cbs);
        if (prim_push_stage_mask_ & static_cast<uint32_t>(ShaderStage::Fragment))
            dc->PSSetConstantBuffers(0, 1, push_cbs);
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

    auto* layout = ResolvePrimInputLayout(shader_mgr);
    dc->IASetInputLayout((buf && buf->buffer && layout) ? layout : nullptr);
    dc->IASetPrimitiveTopology(prim_topology_);

    if (buf && buf->buffer) {
        BindPrimVertexBuffers(dc, resource_mgr);
    }

    // 深度/光栅/混合已由 BindPipeline→ApplyPipelineState 设定，此处不再 save/restore。
    dc->Draw(vertex_count, first_vertex);
    ClearExtraVertexSlots();
    global_state_.current_frame_stats.draw_calls++;

    // push 状态按绘制清零，避免泄漏到后续不写 push 的绘制（其 shader 若有 b0 push 块会读到陈旧字节）。
    prim_has_push_ = false;
    prim_push_stage_mask_ = 0;
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

    // 通用 push constant 字节块 → push cbuffer b0（后处理参数走此路）。须在 UBO 循环之后绑定，
    // 覆盖 prim_ubos_ 残留的 b0 句柄（PP shader 的 b0 即 push 块，无真 UBO 落 b0）。
    if (prim_has_push_) {
        UpdateConstantBuffer(prim_push_cb_.Get(), prim_push_data_, kPrimPushMaxBytes);
        ID3D11Buffer* push_cbs[] = {prim_push_cb_.Get()};
        if (prim_push_stage_mask_ & static_cast<uint32_t>(ShaderStage::Vertex))
            dc->VSSetConstantBuffers(0, 1, push_cbs);
        if (prim_push_stage_mask_ & static_cast<uint32_t>(ShaderStage::Fragment))
            dc->PSSetConstantBuffers(0, 1, push_cbs);
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

    auto* layout = ResolvePrimInputLayout(shader_mgr);
    dc->IASetInputLayout((vb && vb->buffer && layout) ? layout : nullptr);
    dc->IASetPrimitiveTopology(prim_topology_);

    if (vb && vb->buffer) {
        BindPrimVertexBuffers(dc, resource_mgr);
    }
    dc->IASetIndexBuffer(ib->buffer.Get(), prim_index_format_, 0);

    // 深度/光栅/混合已由 BindPipeline→ApplyPipelineState 设定。
    if (instance_count == 1 && first_instance == 0) {
        dc->DrawIndexed(index_count, first_index, base_vertex);
    } else {
        // 注：DX11 的 SV_InstanceID 始终从 0 起，StartInstanceLocation 仅偏移 per-instance 顶点取数；
        // 实例数据偏移需经 SSBO/instance VB 偏移表达（见 RHI_PRIMITIVE_CONTRACT.md §6）。
        dc->DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
        global_state_.current_frame_stats.instanced_draw_calls++;
    }
    ClearExtraVertexSlots();
    global_state_.current_frame_stats.draw_calls++;

    prim_has_push_ = false;
    prim_push_stage_mask_ = 0;
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

    auto* layout = ResolvePrimInputLayout(shader_mgr);
    if (layout) dc->IASetInputLayout(layout);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    BindPrimVertexBuffers(dc, resource_mgr);
    dc->IASetIndexBuffer(ib->buffer.Get(), prim_index_format_, 0);

    // 间接绘制：从 args buffer 的 byte_offset 处读取 5×uint32 参数。
    // 契约：DX11 SV_InstanceID 仍从 0 起，base_instance 偏移须经 SSBO 偏移表达（§6）。
    dc->DrawIndexedInstancedIndirect(args_buf, byte_offset);
    ClearExtraVertexSlots();
    global_state_.current_frame_stats.draw_calls++;
    global_state_.current_frame_stats.indirect_draw_calls++;

    // 间接路径（GPU 驱动网格）不消费 push；仍按绘制清零，保持 push 状态严格的每绘制语义。
    prim_has_push_ = false;
    prim_push_stage_mask_ = 0;
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
