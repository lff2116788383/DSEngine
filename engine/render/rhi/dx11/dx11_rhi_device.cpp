/**
 * @file dx11_rhi_device.cpp
 * @brief DX11RhiDevice 实现 — D3D11 后端的 RhiDevice 接口实现
 *
 * 所有 RhiDevice 虚方法委托给子系统：
 * - ResourceManager: 纹理/缓冲区/渲染目标
 * - ShaderManager: 着色器创建/销毁
 * - PipelineStateManager: 管线状态
 * - DrawExecutor: 绘制命令执行 + 全局阴影/光源状态
 */

#include "engine/render/rhi/dx11/dx11_rhi_device.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/base/debug.h"
#include <chrono>

#include <cstdlib>
#include <cstring>
#include <d3dcompiler.h>
#include <d3d11shader.h>

namespace dse {
namespace render {

// ============================================================
// DX11CommandBuffer 实现
// ============================================================

void DX11CommandBuffer::SetDevice(DX11RhiDevice* device) {
    device_ = device;
    base_device_ = device;
}

void DX11CommandBuffer::BeginRenderPass(const RenderPassDesc& render_pass) {
    if (!device_) return;
    device_->draw_executor().BeginRenderPass(render_pass, device_->resource_mgr(), device_->state_mgr());
}

void DX11CommandBuffer::EndRenderPass() {
    if (!device_) return;
    device_->draw_executor().EndRenderPass();
}

void DX11CommandBuffer::ClearColor(const glm::vec4& color) {
    if (!device_) return;
    ID3D11DeviceContext* dc = device_->context().device_context();
    ID3D11RenderTargetView* rtv = device_->context().backbuffer_rtv();
    if (rtv) {
        float c[4] = {color.r, color.g, color.b, color.a};
        dc->ClearRenderTargetView(rtv, c);
    }
}

void DX11CommandBuffer::DispatchComputePass(const ComputeDispatch& dispatch) {
    if (!device_) return;
    device_->draw_executor().DispatchComputePass(dispatch,
        device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::SetViewport(int x, int y, int width, int height) {
    if (!device_) return;
    auto* dc = device_->context().device_context();
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = static_cast<FLOAT>(x);
    vp.TopLeftY = static_cast<FLOAT>(y);
    vp.Width    = static_cast<FLOAT>(width);
    vp.Height   = static_cast<FLOAT>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    dc->RSSetViewports(1, &vp);
    D3D11_RECT scissor{};
    scissor.left   = static_cast<LONG>(x);
    scissor.top    = static_cast<LONG>(y);
    scissor.right  = static_cast<LONG>(x + width);
    scissor.bottom = static_cast<LONG>(y + height);
    dc->RSSetScissorRects(1, &scissor);
}

void DX11CommandBuffer::ClearDepth(float depth) {
    if (!device_) return;
    auto* dc = device_->context().device_context();
    ID3D11DepthStencilView* dsv = nullptr;
    dc->OMGetRenderTargets(0, nullptr, &dsv);
    if (dsv) {
        dc->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, depth, 0);
        dsv->Release();
    }
}

// --- 通用绘制原语 (A1) ---

void DX11CommandBuffer::BindPipeline(unsigned int graphics_pipeline_handle) {
    if (!device_) return;
    const auto* desc = device_->GetGraphicsPipelineDesc(graphics_pipeline_handle);
    if (!desc) return;
    // 恒应用 PSO 子状态（深度/光栅/混合）+ 拓扑；program!=0 时再绑 program（PSO-only 管线 program==0）。
    device_->state_mgr().ApplyPipelineState(desc->pso_state, device_->context().device_context());
    const auto* ps = device_->state_mgr().GetPipelineState(desc->pso_state);
    device_->draw_executor().PrimSetTopology(ps ? ps->desc.topology : PrimitiveTopology::TriangleList);
    if (desc->program != 0) device_->draw_executor().PrimBindShaderProgram(desc->program);
}

void DX11CommandBuffer::BindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                                          const std::vector<VertexAttr>& attrs, VertexInputRate rate) {
    if (!device_) return;
    device_->draw_executor().PrimBindVertexBuffer(slot, buffer_handle, stride, attrs, rate);
}

void DX11CommandBuffer::PushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) {
    if (!device_) return;
    device_->draw_executor().PrimPushConstants(stage, offset, data, size);
}

void DX11CommandBuffer::Draw(uint32_t vertex_count, uint32_t first_vertex) {
    if (!device_) return;
    device_->draw_executor().PrimDraw(vertex_count, first_vertex,
        device_->shader_mgr(), device_->resource_mgr());
}

// --- 通用绘制原语 (B0): 索引 / 2D 纹理 / UBO / 索引绘制 ---

void DX11CommandBuffer::BindIndexBuffer(unsigned int buffer_handle, IndexType type) {
    if (!device_) return;
    device_->draw_executor().PrimBindIndexBuffer(buffer_handle, type);
}

void DX11CommandBuffer::BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
    if (!device_) return;
    device_->draw_executor().PrimBindTexture(slot, texture_handle, dim);
}

void DX11CommandBuffer::BindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                          uint32_t offset, uint32_t size) {
    if (!device_) return;
    device_->draw_executor().PrimBindUniformBuffer(slot, buffer_handle, offset, size);
}

void DX11CommandBuffer::BindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                                          uint32_t offset, uint32_t size) {
    if (!device_) return;
    device_->draw_executor().PrimBindStorageBuffer(slot, buffer_handle, offset, size);
}

void DX11CommandBuffer::DrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) {
    if (!device_) return;
    device_->draw_executor().PrimDrawIndexed(index_count, first_index, base_vertex,
        device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                             uint32_t first_index, int32_t base_vertex,
                                             uint32_t first_instance) {
    if (!device_) return;
    device_->draw_executor().PrimDrawIndexedInstanced(index_count, instance_count, first_index,
        base_vertex, first_instance, device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::DrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) {
    if (!device_) return;
    device_->draw_executor().PrimDrawIndexedIndirect(indirect_buffer, byte_offset,
        device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::Reset() {
    ResetBase();
}

// ============================================================
// DX11RhiDevice 实现
// ============================================================

struct DX11RhiDevice::HiZImpl {
    struct HiZTextureInfo {
        ComPtr<ID3D11Texture2D> texture;
        std::vector<ComPtr<ID3D11ShaderResourceView>> mip_srvs;
        std::vector<ComPtr<ID3D11UnorderedAccessView>> mip_uavs;
        ComPtr<ID3D11ShaderResourceView> full_srv; // 全 mip chain SRV
        int width = 0;
        int height = 0;
        int mip_count = 0;
    };
    std::unordered_map<unsigned int, HiZTextureInfo> textures;
    unsigned int next_handle = 850000;
};

DX11RhiDevice::DX11RhiDevice() = default;
DX11RhiDevice::~DX11RhiDevice() = default;

bool DX11RhiDevice::InitDevice(void* window_handle, int width, int height) {
    const char* sdr_env = std::getenv("DSE_FORCE_SDR");
    bool force_sdr = sdr_env && (sdr_env[0] == '1' || sdr_env[0] == 'y' || sdr_env[0] == 'Y');
    const char* vsync_env = std::getenv("DSE_VSYNC");
    vsync_enabled_ = !(vsync_env && vsync_env[0] == '0');
    DEBUG_LOG_INFO("[D3D11] VSync: {}", vsync_enabled_ ? "ON" : "OFF");
    return InitD3D11(window_handle, width, height, false, force_sdr);
}

bool DX11RhiDevice::InitD3D11(void* window_handle, int width, int height, bool enable_debug, bool force_sdr) {
    if (initialized_) return true;

    if (!context_.Init(window_handle, width, height, enable_debug, force_sdr)) {
        DEBUG_LOG_ERROR("[D3D11] Context init failed");
        return false;
    }

    // swap chain 就绪后立即 present 黑屏，消除白屏
    {
        ID3D11DeviceContext* dc = context_.device_context();
        ID3D11RenderTargetView* rtv = context_.backbuffer_rtv();
        if (dc && rtv) {
            float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            dc->ClearRenderTargetView(rtv, black);
            context_.Present(false);
        }
    }
    KeepAlive();

    if (!resource_mgr_.Init(&context_)) {
        DEBUG_LOG_ERROR("[D3D11] ResourceManager init failed");
        return false;
    }

    shader_mgr_.Init(&context_);
    state_mgr_.Init(&context_);
    draw_executor_.Init(&context_, &resource_mgr_);

    KeepAlive();
    // 初始化内置着色器（传入 keep-alive 回调防止编译期间窗口"未响应"）
    shader_mgr_.InitBuiltinShaders(init_keep_alive_);
    shader_mgr_.InitGPUDrivenPBRShader();
    shader_mgr_.InitGPUDrivenShadowShader();
    resource_mgr_.set_ssbo_register_base(
        static_cast<unsigned int>(shader_mgr_.pbr_texture_slots().ssbo_base));

    initialized_ = true;

    // GPU Timestamp Query
    gpu_timer_.Init(&context_);

    DEBUG_LOG_INFO("[D3D11] RhiDevice initialized (all subsystems ready)");
    return true;
}

void DX11RhiDevice::Shutdown() {
    if (!initialized_) return;

    // 释放 wireframe rasterizer state 缓存
    if (wireframe_rasterizer_state_) {
        wireframe_rasterizer_state_->Release();
        wireframe_rasterizer_state_ = nullptr;
    }
    if (solid_rasterizer_state_) {
        solid_rasterizer_state_->Release();
        solid_rasterizer_state_ = nullptr;
    }

    // 清理 Hi-Z 资源（在 device 销毁前释放 COM 引用）
    if (hiz_impl_) {
        hiz_impl_->textures.clear();
    }

    draw_executor_.Shutdown();
    state_mgr_.Shutdown();

    // 清理外部着色器
    for (auto h : external_shader_programs_) {
        shader_mgr_.DeleteProgram(h);
    }
    external_shader_programs_.clear();

    shader_mgr_.Shutdown();
    gpu_timer_.Shutdown();
    resource_mgr_.Shutdown();
    context_.Shutdown();

    initialized_ = false;
    DEBUG_LOG_INFO("[D3D11] RhiDevice shutdown");
}

void DX11RhiDevice::BeginFrame() {
    current_frame_stats_ = RenderStats{};
    resource_mgr_.FlushPendingUploads();
    draw_executor_.BeginFrame();

    // GPU Timestamp Query: 推进帧 index 并开启本帧 disjoint
    gpu_timer_.ResetGpuTimers();

    // 清除后备缓冲区
    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;
    ID3D11RenderTargetView* rtv = context_.backbuffer_rtv();
    ID3D11DepthStencilView* dsv = context_.backbuffer_dsv();
    if (rtv) {
        float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        dc->ClearRenderTargetView(rtv, clear_color);
    }
    if (dsv) {
        dc->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
    // 绑定后备缓冲区为默认渲染目标
    dc->OMSetRenderTargets(1, &rtv, dsv);

    // 设置默认 Viewport
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(context_.width());
    vp.Height = static_cast<float>(context_.height());
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    dc->RSSetViewports(1, &vp);
}

unsigned int DX11RhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    return resource_mgr_.CreateRenderTarget(
        desc.width, desc.height, desc.has_color, desc.has_depth,
        desc.generate_mipmaps, desc.cube_map, desc.msaa_samples, desc.allow_uav,
        desc.color_attachment_count);
}

void DX11RhiDevice::DeleteRenderTarget(unsigned int render_target_handle) {
    resource_mgr_.DeleteRenderTarget(render_target_handle);
}

unsigned int DX11RhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    return resource_mgr_.GetRenderTargetColorTextureHandle(render_target_handle);
}

unsigned int DX11RhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const {
    const auto* rt = resource_mgr_.GetRenderTarget(render_target_handle);
    if (!rt) return 0;
    if (!rt->color_texture_handles_mrt.empty()) {
        if (index >= 0 && index < static_cast<int>(rt->color_texture_handles_mrt.size()))
            return rt->color_texture_handles_mrt[index];
        return 0;
    }
    return (index == 0) ? rt->color_texture_handle : 0;
}

unsigned int DX11RhiDevice::GetRenderTargetDepthTexture(unsigned int render_target_handle) const {
    return resource_mgr_.GetRenderTargetDepthTextureHandle(render_target_handle);
}

std::vector<unsigned char> DX11RhiDevice::ReadRenderTargetColorRgba8(unsigned int render_target_handle) const {
    auto result = resource_mgr_.ReadRenderTargetColor(render_target_handle);
    return std::move(result.pixels);
}

RenderTargetReadback DX11RhiDevice::ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const {
    auto result = resource_mgr_.ReadRenderTargetColor(render_target_handle);
    RenderTargetReadback readback;
    readback.width = result.width;
    readback.height = result.height;
    readback.pixels = std::move(result.pixels);
    return readback;
}

RenderTargetDepthReadback DX11RhiDevice::ReadRenderTargetDepthFloatWithSize(unsigned int render_target_handle) const {
    auto result = resource_mgr_.ReadRenderTargetDepth(render_target_handle);
    RenderTargetDepthReadback readback;
    readback.width = result.width;
    readback.height = result.height;
    readback.depth = std::move(result.depth);
    return readback;
}

// --- 通用即时绘制 / RT blit（编辑器架构 §5.A/§5.B）---

namespace {
DXGI_FORMAT ImmAttrFormat(int components) {
    switch (components) {
        case 1:  return DXGI_FORMAT_R32_FLOAT;
        case 2:  return DXGI_FORMAT_R32G32_FLOAT;
        case 3:  return DXGI_FORMAT_R32G32B32_FLOAT;
        default: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
}
} // namespace

void DX11RhiDevice::ImmediateDraw(const ImmediateDrawDesc& desc) {
    if (!initialized_ || desc.shader_program == 0) return;
    ID3D11Device* dev = context_.device();
    ID3D11DeviceContext* dc = context_.device_context();
    if (!dev || !dc) return;

    const DX11ShaderProgram* prog = shader_mgr_.GetProgram(desc.shader_program);
    if (!prog || !prog->vertex_shader || !prog->pixel_shader || !prog->vs_blob) return;

    // 目标 RTV / DSV / 全尺寸（0 = 交换链后备缓冲）
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    int rt_w = context_.width(), rt_h = context_.height();
    if (desc.render_target == 0) {
        rtv = context_.backbuffer_rtv();
        if (desc.depth_test) dsv = context_.backbuffer_dsv();
    } else {
        const auto* rt = resource_mgr_.GetRenderTarget(desc.render_target);
        if (!rt || !rt->color_rtv) return;
        rtv = rt->color_rtv.Get();
        if (desc.depth_test && rt->depth_dsv) dsv = rt->depth_dsv.Get();
        rt_w = rt->width;
        rt_h = rt->height;
    }
    dc->OMSetRenderTargets(1, &rtv, dsv);

    if (desc.clear && rtv) {
        const float c[4] = {desc.clear_color.r, desc.clear_color.g, desc.clear_color.b, desc.clear_color.a};
        dc->ClearRenderTargetView(rtv, c);
        if (dsv) dc->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
    }

    D3D11_VIEWPORT vp{};
    int vx = desc.viewport.x, vy = desc.viewport.y, vw = desc.viewport.z, vh = desc.viewport.w;
    if (vw <= 0 || vh <= 0) { vx = 0; vy = 0; vw = rt_w; vh = rt_h; }
    vp.TopLeftX = static_cast<float>(vx);
    vp.TopLeftY = static_cast<float>(vy);
    vp.Width = static_cast<float>(vw);
    vp.Height = static_cast<float>(vh);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    dc->RSSetViewports(1, &vp);

    // InputLayout：attribs → 语义 TEXCOORD<location>（与 DX11DrawExecutor 通用原语布局同约定），slot0/逐顶点
    std::vector<D3D11_INPUT_ELEMENT_DESC> elems;
    elems.reserve(desc.attribs.size());
    for (const auto& a : desc.attribs) {
        elems.push_back(D3D11_INPUT_ELEMENT_DESC{
            "TEXCOORD", static_cast<UINT>(a.location), ImmAttrFormat(a.components),
            0u, static_cast<UINT>(a.offset_bytes), D3D11_INPUT_PER_VERTEX_DATA, 0u});
    }
    ID3D11InputLayout* layout = shader_mgr_.GetOrCreatePrimInputLayout(desc.shader_program, elems);
    if (!layout) return;
    dc->IASetInputLayout(layout);

    // 动态顶点缓冲（即时绘制非热路径，每次新建）
    ComPtr<ID3D11Buffer> vb;
    D3D11_BUFFER_DESC vbd{};
    vbd.ByteWidth = static_cast<UINT>(desc.vertex_bytes);
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinit{};
    vinit.pSysMem = desc.vertices;
    if (FAILED(dev->CreateBuffer(&vbd, &vinit, &vb))) return;
    UINT stride = static_cast<UINT>(desc.stride_bytes), voffset = 0;
    ID3D11Buffer* vbs[] = {vb.Get()};
    dc->IASetVertexBuffers(0, 1, vbs, &stride, &voffset);

    D3D11_PRIMITIVE_TOPOLOGY topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    if (desc.topology == ImmediateTopology::Lines) topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    else if (desc.topology == ImmediateTopology::LineStrip) topo = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    dc->IASetPrimitiveTopology(topo);

    dc->VSSetShader(prog->vertex_shader.Get(), nullptr, 0);
    dc->PSSetShader(prog->pixel_shader.Get(), nullptr, 0);

    // uniform → cbuffer：反射 VS/PS 的常量缓冲，按变量名写入对应偏移，整块上传后绑定
    std::vector<ComPtr<ID3D11Buffer>> cb_keepalive;
    auto pack_stage = [&](ID3DBlob* blob, bool is_vs) {
        if (!blob) return;
        ComPtr<ID3D11ShaderReflection> refl;
        if (FAILED(D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&refl)))) return;
        D3D11_SHADER_DESC sd{};
        refl->GetDesc(&sd);
        for (UINT i = 0; i < sd.ConstantBuffers; ++i) {
            ID3D11ShaderReflectionConstantBuffer* rcb = refl->GetConstantBufferByIndex(i);
            D3D11_SHADER_BUFFER_DESC bd{};
            if (FAILED(rcb->GetDesc(&bd)) || bd.Size == 0) continue;
            D3D11_SHADER_INPUT_BIND_DESC ibd{};
            if (FAILED(refl->GetResourceBindingDescByName(bd.Name, &ibd))) continue;

            std::vector<uint8_t> staging(bd.Size, 0u);
            auto write_var = [&](const std::string& name, const float* data, UINT bytes) {
                ID3D11ShaderReflectionVariable* var = rcb->GetVariableByName(name.c_str());
                if (!var) return;
                D3D11_SHADER_VARIABLE_DESC vd{};
                if (FAILED(var->GetDesc(&vd))) return;
                if (vd.StartOffset + bytes > bd.Size) return;
                std::memcpy(staging.data() + vd.StartOffset, data, bytes);
            };
            for (const auto& u : desc.uniforms_f)    write_var(u.first, &u.second, 4u);
            for (const auto& u : desc.uniforms_vec2) write_var(u.first, &u.second.x, 8u);
            for (const auto& u : desc.uniforms_vec4) write_var(u.first, &u.second.x, 16u);

            UINT padded_size = (bd.Size + 15u) & ~15u;
            std::vector<uint8_t> padded(padded_size, 0u);
            std::memcpy(padded.data(), staging.data(), staging.size());
            D3D11_BUFFER_DESC cbd{};
            cbd.ByteWidth = padded_size;
            cbd.Usage = D3D11_USAGE_DEFAULT;
            cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            D3D11_SUBRESOURCE_DATA cinit{};
            cinit.pSysMem = padded.data();
            ComPtr<ID3D11Buffer> cb;
            if (FAILED(dev->CreateBuffer(&cbd, &cinit, &cb))) continue;
            ID3D11Buffer* cbs[] = {cb.Get()};
            if (is_vs) dc->VSSetConstantBuffers(ibd.BindPoint, 1, cbs);
            else       dc->PSSetConstantBuffers(ibd.BindPoint, 1, cbs);
            cb_keepalive.push_back(cb);
        }
    };
    pack_stage(prog->vs_blob.Get(), true);
    pack_stage(prog->ps_blob.Get(), false);

    // 光栅化：关剔除（全屏三角/任意 quad 绕序不确定），solid，开深度裁剪
    ComPtr<ID3D11RasterizerState> rs;
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (SUCCEEDED(dev->CreateRasterizerState(&rd, &rs))) dc->RSSetState(rs.Get());

    ComPtr<ID3D11BlendState> blend_state;
    D3D11_BLEND_DESC bdsc{};
    bdsc.RenderTarget[0].BlendEnable = desc.blend ? TRUE : FALSE;
    if (desc.blend) {
        bdsc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bdsc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bdsc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bdsc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bdsc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        bdsc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    }
    bdsc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (SUCCEEDED(dev->CreateBlendState(&bdsc, &blend_state))) {
        const float bf[4] = {0, 0, 0, 0};
        dc->OMSetBlendState(blend_state.Get(), bf, 0xFFFFFFFFu);
    }

    ComPtr<ID3D11DepthStencilState> depth_state;
    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = desc.depth_test ? TRUE : FALSE;
    dd.DepthWriteMask = desc.depth_test ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    dd.DepthFunc = D3D11_COMPARISON_LESS;
    if (SUCCEEDED(dev->CreateDepthStencilState(&dd, &depth_state))) dc->OMSetDepthStencilState(depth_state.Get(), 0);

    dc->Draw(static_cast<UINT>(desc.vertex_count), 0);
}

void DX11RhiDevice::BlitRenderTarget(unsigned int src_rt, unsigned int dst_rt) {
    if (!initialized_) return;
    ID3D11DeviceContext* dc = context_.device_context();
    const auto* src = resource_mgr_.GetRenderTarget(src_rt);
    const auto* dst = resource_mgr_.GetRenderTarget(dst_rt);
    if (!dc || !src || !dst) return;
    ID3D11Texture2D* src_tex = src->color_texture.Get();
    ID3D11Texture2D* dst_tex = dst->color_texture.Get();
    if (!src_tex || !dst_tex) return;

    D3D11_TEXTURE2D_DESC sd{};
    src_tex->GetDesc(&sd);
    if (sd.SampleDesc.Count > 1) {
        // MSAA 源：先 resolve 到 1x dst
        dc->ResolveSubresource(dst_tex, 0, src_tex, 0, sd.Format);
    } else {
        // 同格式同尺寸直接拷贝（编辑器多视口 RT→RT）
        dc->CopyResource(dst_tex, src_tex);
    }
}

unsigned int DX11RhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    return resource_mgr_.CreateTexture2D(width, height, rgba8_data, linear_filter);
}

unsigned int DX11RhiDevice::CreateCompressedTexture2D(CompressedTextureFormat format,
                                                       const std::vector<CompressedMipLevel>& mips,
                                                       bool linear_filter) {
    return resource_mgr_.CreateCompressedTexture2D(format, mips, linear_filter);
}

unsigned int DX11RhiDevice::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    return resource_mgr_.CreateTextureCube(width, height, rgba8_faces, linear_filter);
}

unsigned int DX11RhiDevice::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) {
    return resource_mgr_.CreateTexture3D(width, height, depth, rgba8_data, linear_filter);
}

void DX11RhiDevice::DeleteTexture(unsigned int texture_handle) {
    resource_mgr_.DeleteTexture(texture_handle);
}

unsigned int DX11RhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
    unsigned int handle = shader_mgr_.CreateProgram(vert_src, frag_src);
    if (handle) external_shader_programs_.insert(handle);
    return handle;
}

void DX11RhiDevice::DeleteShaderProgram(unsigned int program_handle) {
    external_shader_programs_.erase(program_handle);
    shader_mgr_.DeleteProgram(program_handle);
}

unsigned int DX11RhiDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    return state_mgr_.CreatePipelineState(desc);
}

unsigned int DX11RhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    return resource_mgr_.CreateBuffer(size, data, is_dynamic, is_index);
}

// --- 内建资源访问器 ---
// 内建着色器在 InitD3D11→ShaderManager 初始化时已创建（DXBC），此处仅取句柄。

unsigned int DX11RhiDevice::GetBuiltinProgram(BuiltinProgram program) {
    switch (program) {
        case BuiltinProgram::Skybox:      return shader_mgr_.skybox_shader_handle();
        case BuiltinProgram::Sprite2D:    return shader_mgr_.sprite2d_shader_handle();
        case BuiltinProgram::SpriteFxSdf: return shader_mgr_.sprite_fx_sdf_shader_handle();
        case BuiltinProgram::SpriteFxVfx: return shader_mgr_.sprite_fx_vfx_shader_handle();
        case BuiltinProgram::ForwardPbr:  return shader_mgr_.forward_pbr_shader_handle();
        case BuiltinProgram::ForwardPbrSkinned: return shader_mgr_.forward_pbr_skinned_shader_handle();
        case BuiltinProgram::ForwardPbrInstanced: return shader_mgr_.forward_pbr_instanced_shader_handle();
        case BuiltinProgram::ForwardPbrDepth: return shader_mgr_.forward_pbr_depth_shader_handle();
        case BuiltinProgram::ForwardInstancedDepth: return shader_mgr_.forward_instanced_depth_shader_handle();
        case BuiltinProgram::Particle3D: return shader_mgr_.particle3d_shader_handle();
        case BuiltinProgram::HairStrand: return shader_mgr_.hair_strand_shader_handle();
        case BuiltinProgram::ForwardShaded: return shader_mgr_.forward_shaded_shader_handle();
        case BuiltinProgram::ForwardSkinnedShaded: return shader_mgr_.forward_skinned_shaded_shader_handle();
        case BuiltinProgram::ForwardInstancedShaded: return shader_mgr_.forward_instanced_shaded_shader_handle();
        case BuiltinProgram::ForwardSkinnedInstancedShaded: return shader_mgr_.forward_skinned_instanced_shaded_shader_handle();
        case BuiltinProgram::ForwardMorphShaded: return shader_mgr_.forward_morph_shaded_shader_handle();
        case BuiltinProgram::GBufferMesh: return shader_mgr_.gbuffer_mesh_shader_handle();
        case BuiltinProgram::Impostor: return shader_mgr_.impostor_shader_handle();
    }
    return 0;
}

unsigned int DX11RhiDevice::GetGenPPShaderProgram(const std::string& effect_name) {
    // 无参 sampler-only 效果共用内建 passthrough（fullscreen quad 采样源纹理）。
    // PostProcessRenderer 按 effect 名取 gen-PP 程序句柄；未映射效果返回 0（调用方跳过）。
    if (effect_name == "postprocess_passthrough" || effect_name == "copy" ||
        effect_name == "ui_overlay") {
        return shader_mgr_.postprocess_shader_handle();
    }
    if (effect_name == "fxaa") return shader_mgr_.fxaa_shader_handle();
    if (effect_name == "bloom_extract") return shader_mgr_.bloom_extract_shader_handle();
    if (effect_name == "lum_compute") return shader_mgr_.lum_compute_shader_handle();
    if (effect_name == "ssao_blur") return shader_mgr_.ssao_blur_shader_handle();
    if (effect_name == "ssao") return shader_mgr_.ssao_shader_handle();
    if (effect_name == "contact_shadow") return shader_mgr_.contact_shadow_shader_handle();
    if (effect_name == "edge_detect") return shader_mgr_.edge_detect_shader_handle();
    if (effect_name == "lum_adapt") return shader_mgr_.lum_adapt_shader_handle();
    if (effect_name == "dof") return shader_mgr_.dof_shader_handle();
    if (effect_name == "motion_blur") return shader_mgr_.motion_blur_shader_handle();
    if (effect_name == "ssr") return shader_mgr_.ssr_shader_handle();
    if (effect_name == "taa_resolve") return shader_mgr_.taa_resolve_shader_handle();
    if (effect_name == "motion_vector") return shader_mgr_.motion_vector_shader_handle();
    if (effect_name == "volumetric_fog") return shader_mgr_.volumetric_fog_shader_handle();
    if (effect_name == "volumetric_cloud") return shader_mgr_.volumetric_cloud_shader_handle();
    if (effect_name == "water") return shader_mgr_.water_shader_handle();
    if (effect_name == "decal") return shader_mgr_.decal_shader_handle();
    if (effect_name == "wboit_composite") return shader_mgr_.wboit_composite_shader_handle();
    if (effect_name == "weather_particle") return shader_mgr_.weather_particle_shader_handle();
    if (effect_name == "sss_blur") return shader_mgr_.sss_blur_shader_handle();
    if (effect_name == "tonemapping") return shader_mgr_.tonemapping_shader_handle();
    if (effect_name == "ssao_apply") return shader_mgr_.ssao_apply_shader_handle();
    if (effect_name == "light_shaft") return shader_mgr_.light_shaft_shader_handle();
    if (effect_name == "atmosphere_transmittance_lut") return shader_mgr_.atmosphere_transmittance_lut_shader_handle();
    if (effect_name == "atmosphere_sky") return shader_mgr_.atmosphere_sky_shader_handle();
    if (effect_name == "bloom_composite") return shader_mgr_.bloom_composite_ssao_ae_shader_handle();
    return 0;
}

unsigned int DX11RhiDevice::GetBloomComputeShader(bool upsample) const {
    return upsample ? shader_mgr_.bloom_upsample_cs_handle()
                    : shader_mgr_.bloom_downsample_cs_handle();
}

unsigned int DX11RhiDevice::GetSkyboxCubeVertexBuffer() {
    if (skybox_cube_vbo_handle_ == 0) {
        static const float kSkyboxVertices[] = {
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
        skybox_cube_vbo_handle_ = resource_mgr_.CreateBuffer(
            sizeof(kSkyboxVertices), kSkyboxVertices, false, false);
    }
    return skybox_cube_vbo_handle_;
}

BufferHandle DX11RhiDevice::CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) {
    // kUniform（非 storage/indirect/index）→ D3D11 constant buffer
    if (has(desc.usage, GpuBufferUsage::kUniform) &&
        !has(desc.usage, GpuBufferUsage::kStorage) &&
        !has(desc.usage, GpuBufferUsage::kIndirect) &&
        !has(desc.usage, GpuBufferUsage::kIndex)) {
        BufferHandle h{resource_mgr_.CreateConstantBuffer(desc.size, initial_data, desc.is_dynamic)};
        if (h) gpu_buffer_usage_map_[h.raw()] = desc.usage;
        return h;
    }
    return RhiDevice::CreateGpuBuffer(desc, initial_data);
}

void DX11RhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    resource_mgr_.UpdateBuffer(handle, offset, size, data, is_index);
}

void DX11RhiDevice::DeleteBuffer(unsigned int handle) {
    resource_mgr_.DeleteBuffer(handle);
}

VertexArrayHandle DX11RhiDevice::CreateVertexArray() {
    return resource_mgr_.CreateVertexArray();
}

void DX11RhiDevice::DeleteVertexArray(VertexArrayHandle handle) {
    resource_mgr_.DeleteVertexArray(handle);
}

std::shared_ptr<CommandBuffer> DX11RhiDevice::CreateCommandBuffer() {
    auto cmd = std::make_shared<DX11CommandBuffer>();
    cmd->SetDevice(this);
    return cmd;
}

void DX11RhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    if (!initialized_) return;

    auto* dx_cmd = dynamic_cast<DX11CommandBuffer*>(cmd_buffer.get());
    if (!dx_cmd) return;

    // 累加 DrawExecutor 统计
    const auto& ex_stats = draw_executor_.current_frame_stats();
    current_frame_stats_.draw_calls += ex_stats.draw_calls;
    current_frame_stats_.sprite_count += ex_stats.sprite_count;
    current_frame_stats_.mesh_count += ex_stats.mesh_count;
    current_frame_stats_.render_passes += ex_stats.render_passes;
    current_frame_stats_.shadow_passes += ex_stats.shadow_passes;
    current_frame_stats_.material_switches += ex_stats.material_switches;
    current_frame_stats_.instanced_draw_calls += ex_stats.instanced_draw_calls;
    current_frame_stats_.instanced_mesh_count += ex_stats.instanced_mesh_count;
    current_frame_stats_.particle_count += ex_stats.particle_count;
}

void DX11RhiDevice::EndFrame() {
    if (!initialized_) return;

    draw_executor_.EndFrame();
    last_frame_stats_ = current_frame_stats_;

    // GPU Timestamp Query: 结束本帧 disjoint 并收集上一帧结果
    gpu_timer_.ResolveGpuTimers();

    // Present 由 PresentFrame() 单独调用，不在 EndFrame 内执行
    // 这使 render 计时不包含 Present 延迟，与 OpenGL 行为一致
}

void DX11RhiDevice::PresentFrame() {
    if (!initialized_) return;
    context_.Present(vsync_enabled_);
}

const RenderStats& DX11RhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

// --- SSBO ---

unsigned int DX11RhiDevice::CreateSSBO(size_t size, const void* data) {
    return resource_mgr_.CreateSSBO(size, data);
}

void DX11RhiDevice::UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) {
    resource_mgr_.UpdateSSBO(handle, offset, size, data);
}

void DX11RhiDevice::BindSSBO(unsigned int handle, unsigned int binding_point) {
    resource_mgr_.BindSSBO(handle, binding_point);
    bound_ssbos_[binding_point] = {handle, false};
}

void DX11RhiDevice::BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable) {
    // 延迟绑定：仅记录状态，实际 CS 绑定在 DispatchCompute 中执行
    bound_ssbos_[binding_point] = {handle.raw(), writable};
}

void DX11RhiDevice::DeleteSSBO(unsigned int handle) {
    resource_mgr_.DeleteSSBO(handle);
}

// --- Compute Shader ---

unsigned int DX11RhiDevice::CreateComputeShader(const std::string& source) {
    return shader_mgr_.CreateComputeProgram(source);
}

void DX11RhiDevice::DeleteComputeShader(unsigned int handle) {
    // 委托给 shader_mgr_ 实际销毁 ID3D11ComputeShader 资源
    shader_mgr_.DeleteComputeProgram(handle);
}

void DX11RhiDevice::DispatchCompute(unsigned int shader_handle,
                                     unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
    if (!initialized_ || shader_handle == 0) return;

    const auto* prog = shader_mgr_.GetComputeProgram(shader_handle);
    if (!prog || !prog->cs) return;

    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;

    dc->CSSetShader(prog->cs.Get(), nullptr, 0);

    // 将 uniform staging 内容上传到 scratch cbuffer，绑定到 b0
    FlushComputeParamsCB();

    // SSBO 绑定: readonly → CS SRV (t16+), writable → CS UAV (u0+)
    for (auto& [binding_point, info] : bound_ssbos_) {
        const auto* ssbo = resource_mgr_.GetSSBO(info.handle);
        if (!ssbo) continue;
        if (info.writable) {
            ID3D11UnorderedAccessView* uav = ssbo->uav.Get();
            if (uav) {
                UINT init_count = static_cast<UINT>(-1);
                dc->CSSetUnorderedAccessViews(binding_point, 1, &uav, &init_count);
            }
        } else {
            ID3D11ShaderResourceView* srv = ssbo->srv.Get();
            if (srv) dc->CSSetShaderResources(16 + binding_point, 1, &srv);
        }
    }

    dc->Dispatch(groups_x, groups_y, groups_z);

    // 解绑 CS 资源
    for (auto& [binding_point, info] : bound_ssbos_) {
        if (info.writable) {
            ID3D11UnorderedAccessView* null_uav = nullptr;
            UINT init_count = static_cast<UINT>(-1);
            dc->CSSetUnorderedAccessViews(binding_point, 1, &null_uav, &init_count);
        } else {
            ID3D11ShaderResourceView* null_srv = nullptr;
            dc->CSSetShaderResources(16 + binding_point, 1, &null_srv);
        }
    }
    { ID3D11Buffer* null_cb = nullptr; dc->CSSetConstantBuffers(0, 1, &null_cb); }
    dc->CSSetShader(nullptr, nullptr, 0);
    ClearComputeParams();
}

// ============================================================
// RenderGraph 自动屏障（D3D11: 驱动隐式管理，从 UAV 离开时解绑）
// ============================================================

void DX11RhiDevice::TransitionRenderTarget(unsigned int rt_handle,
                                            ResourceState from, ResourceState to) {
    (void)rt_handle;
    if (from == to) return;

    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;

    // D3D11 运行时自动追踪 resource hazard，但以下场景需主动解绑：
    // 1. UAV → 非UAV: 解绑 CS UAV，避免 "still bound as UAV" 警告
    // 2. 非UAV → UAV: 解绑 SRV，避免 "still bound as SRV" 警告

    if (to == ResourceState::UnorderedAccess && from != ResourceState::UnorderedAccess) {
        // 进入 UAV 前解绑 CS SRV（避免同 resource 同时绑定为 SRV+UAV）
        static ID3D11ShaderResourceView* null_srvs[8] = {};
        dc->CSSetShaderResources(0, 8, null_srvs);
    }

    if (from == ResourceState::UnorderedAccess && to != ResourceState::UnorderedAccess) {
        // 离开 UAV 后解绑 CS UAV，避免 "still bound as UAV" 调试警告
        static ID3D11UnorderedAccessView* null_uavs[8] = {};
        dc->CSSetUnorderedAccessViews(0, 8, null_uavs, nullptr);
    }
}

void DX11RhiDevice::ComputeMemoryBarrier() {
    // D3D11 驱动自动追踪 resource hazard，但 CS UAV 写入后若
    // 同一 resource 随后被 PS 作为 SRV 读取，调试层会报警告。
    // 主动解绑 CS 阶段的 UAV 和 SRV 以避免跨管线阶段 hazard。
    if (!initialized_) return;
    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;
    static ID3D11UnorderedAccessView* null_uavs[8] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    dc->CSSetUnorderedAccessViews(0, 8, null_uavs, nullptr);
    static ID3D11ShaderResourceView* null_srvs[8] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    dc->CSSetShaderResources(0, 8, null_srvs);
    bound_ssbos_.clear();
}

void DX11RhiDevice::SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) {
    if (!initialized_) return;

    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;

    if (read_only) {
        // 绑定为 SRV 到 CS
        const auto* tex = resource_mgr_.GetTexture(texture_handle);
        if (tex && tex->srv) {
            ID3D11ShaderResourceView* srv = tex->srv.Get();
            dc->CSSetShaderResources(binding, 1, &srv);
        }
    } else {
        // 先尝试按纹理字典的 UAV（CreateComputeWriteTexture2D 创建）
        const auto* tex = resource_mgr_.GetTexture(texture_handle);
        if (tex && tex->uav) {
            ID3D11UnorderedAccessView* uav = tex->uav.Get();
            UINT ic = static_cast<UINT>(-1);
            dc->CSSetUnorderedAccessViews(binding, 1, &uav, &ic);
        } else {
            // 备用：尝试按 render target 的 UAV
            const auto* rt = resource_mgr_.GetRenderTarget(texture_handle);
            if (rt && rt->color_uav) {
                ID3D11UnorderedAccessView* uav = rt->color_uav.Get();
                UINT ic = static_cast<UINT>(-1);
                dc->CSSetUnorderedAccessViews(binding, 1, &uav, &ic);
            }
        }
    }
}

void DX11RhiDevice::SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                               int mip_level, bool read_only, bool r32f) {
    if (!initialized_) return;
    ID3D11DeviceContext* dc = context_.device_context();
    ID3D11Device* dev = context_.device();
    if (!dc || !dev) return;

    // 优先检查 Hi-Z 纹理
    if (hiz_impl_) {
        auto it = hiz_impl_->textures.find(texture_handle);
        if (it != hiz_impl_->textures.end()) {
            auto& info = it->second;
            if (mip_level < 0 || mip_level >= info.mip_count) return;
            if (read_only) {
                ID3D11ShaderResourceView* srv = info.mip_srvs[mip_level].Get();
                dc->CSSetShaderResources(binding, 1, &srv);
            } else {
                ID3D11UnorderedAccessView* uav = info.mip_uavs[mip_level].Get();
                UINT initial_count = static_cast<UINT>(-1);
                dc->CSSetUnorderedAccessViews(binding, 1, &uav, &initial_count);
            }
            return;
        }
    }

    // 普通纹理回退：创建临时 per-mip view（只对 Hi-Z 适用的简化路径）
    const auto* tex = resource_mgr_.GetTexture(texture_handle);
    if (!tex) return;

    if (read_only) {
        if (tex->srv) {
            dc->CSSetShaderResources(binding, 1, tex->srv.GetAddressOf());
        }
    } else {
        if (tex->uav) {
            ID3D11UnorderedAccessView* uav = tex->uav.Get();
            UINT ic = static_cast<UINT>(-1);
            dc->CSSetUnorderedAccessViews(binding, 1, &uav, &ic);
        }
    }
    (void)r32f;
}

void DX11RhiDevice::SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) {
    if (!initialized_) return;
    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;

    // 检查 Hi-Z 纹理
    if (hiz_impl_) {
        auto it = hiz_impl_->textures.find(texture_handle);
        if (it != hiz_impl_->textures.end() && it->second.full_srv) {
            ID3D11ShaderResourceView* srv = it->second.full_srv.Get();
            dc->CSSetShaderResources(unit, 1, &srv);
            return;
        }
    }

    const auto* tex = resource_mgr_.GetTexture(texture_handle);
    if (tex && tex->srv) {
        ID3D11ShaderResourceView* srv = tex->srv.Get();
        dc->CSSetShaderResources(unit, 1, &srv);
    }
    if (tex && tex->sampler) {
        ID3D11SamplerState* ss = tex->sampler.Get();
        dc->CSSetSamplers(unit, 1, &ss);
    }
}

unsigned int DX11RhiDevice::CreateHiZTexture(int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) return 0;
    if (!hiz_impl_) hiz_impl_ = std::make_unique<HiZImpl>();

    ID3D11Device* dev = context_.device();
    if (!dev) return 0;

    int mip_count = 1;
    {
        int w = width, h = height;
        while (w > 1 || h > 1) {
            w = (std::max)(1, w / 2);
            h = (std::max)(1, h / 2);
            ++mip_count;
        }
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = static_cast<UINT>(mip_count);
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    HiZImpl::HiZTextureInfo info{};
    info.width = width;
    info.height = height;
    info.mip_count = mip_count;

    HRESULT hr = dev->CreateTexture2D(&desc, nullptr, info.texture.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[DX11] Failed to create Hi-Z texture: hr=0x{:08X}", static_cast<unsigned>(hr));
        return 0;
    }

    // 全 mip SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = static_cast<UINT>(mip_count);
    hr = dev->CreateShaderResourceView(info.texture.Get(), &srv_desc, info.full_srv.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[DX11] Hi-Z full SRV creation failed: hr=0x{:08X}", static_cast<unsigned>(hr));
        return 0;
    }

    // Per-mip SRV + UAV
    info.mip_srvs.resize(mip_count);
    info.mip_uavs.resize(mip_count);
    for (int i = 0; i < mip_count; ++i) {
        D3D11_SHADER_RESOURCE_VIEW_DESC mip_srv{};
        mip_srv.Format = DXGI_FORMAT_R32_FLOAT;
        mip_srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        mip_srv.Texture2D.MostDetailedMip = static_cast<UINT>(i);
        mip_srv.Texture2D.MipLevels = 1;
        hr = dev->CreateShaderResourceView(info.texture.Get(), &mip_srv, info.mip_srvs[i].GetAddressOf());
        if (FAILED(hr)) {
            DEBUG_LOG_ERROR("[DX11] Hi-Z mip {} SRV failed: hr=0x{:08X}", i, static_cast<unsigned>(hr));
            return 0;
        }

        D3D11_UNORDERED_ACCESS_VIEW_DESC mip_uav{};
        mip_uav.Format = DXGI_FORMAT_R32_FLOAT;
        mip_uav.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        mip_uav.Texture2D.MipSlice = static_cast<UINT>(i);
        hr = dev->CreateUnorderedAccessView(info.texture.Get(), &mip_uav, info.mip_uavs[i].GetAddressOf());
        if (FAILED(hr)) {
            DEBUG_LOG_ERROR("[DX11] Hi-Z mip {} UAV failed: hr=0x{:08X}", i, static_cast<unsigned>(hr));
            return 0;
        }
    }

    unsigned int handle = hiz_impl_->next_handle++;
    hiz_impl_->textures[handle] = std::move(info);

    DEBUG_LOG_INFO("[DX11] Hi-Z texture created: handle={} {}x{} mips={}", handle, width, height, mip_count);
    return handle;
}

void DX11RhiDevice::DeleteHiZTexture(unsigned int handle) {
    if (!hiz_impl_) return;
    hiz_impl_->textures.erase(handle);
}

int DX11RhiDevice::GetHiZMipCount(unsigned int handle) const {
    if (!hiz_impl_) return 0;
    auto it = hiz_impl_->textures.find(handle);
    return it != hiz_impl_->textures.end() ? it->second.mip_count : 0;
}

unsigned int DX11RhiDevice::GetHiZGpuTexture(unsigned int handle) const {
    if (!hiz_impl_) return 0;
    auto it = hiz_impl_->textures.find(handle);
    return it != hiz_impl_->textures.end() ? handle : 0;
}

size_t DX11RhiDevice::GetOrCreateUniformOffset(unsigned int shader, const char* name, size_t data_size) {
    auto& layout = compute_uniform_layouts_[shader];
    auto it = layout.name_to_offset.find(name);
    if (it != layout.name_to_offset.end()) {
        return it->second;
    }
    // 16-byte 对齐（HLSL cbuffer 对齐要求）
    size_t offset = (compute_uniform_next_offset_ + 15) & ~size_t(15);
    layout.name_to_offset[name] = offset;
    compute_uniform_next_offset_ = offset + data_size;
    return offset;
}

void DX11RhiDevice::FlushComputeParamsCB() {
    if (compute_params_staging_.empty()) return;
    ID3D11Device* dev = context_.device();
    ID3D11DeviceContext* dc = context_.device_context();
    if (!dev || !dc) return;
    // cbuffer 大小必须 16-byte 对齐
    size_t aligned = (compute_params_staging_.size() + 15) & ~size_t(15);
    if (aligned > compute_params_cb_capacity_) {
        compute_params_cb_.Reset();
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth      = static_cast<UINT>(aligned);
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(dev->CreateBuffer(&bd, nullptr, compute_params_cb_.GetAddressOf()))) return;
        compute_params_cb_capacity_ = aligned;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(dc->Map(compute_params_cb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, compute_params_staging_.data(), compute_params_staging_.size());
        dc->Unmap(compute_params_cb_.Get(), 0);
    }
    ID3D11Buffer* cb = compute_params_cb_.Get();
    dc->CSSetConstantBuffers(0, 1, &cb);
}

void DX11RhiDevice::ClearComputeParams() {
    compute_params_staging_.clear();
    compute_uniform_layouts_.clear();
    compute_uniform_next_offset_ = 0;
}

static void EnsureStagingCapacity(std::vector<uint8_t>& buf, size_t offset, size_t write_size) {
    size_t needed = offset + write_size;
    if (buf.size() < needed) buf.resize(needed, 0);
}

void DX11RhiDevice::SetComputeUniformInt(unsigned int shader, const char* name, int v) {
    size_t off = GetOrCreateUniformOffset(shader, name, sizeof(int));
    EnsureStagingCapacity(compute_params_staging_, off, sizeof(int));
    memcpy(compute_params_staging_.data() + off, &v, sizeof(int));
}
void DX11RhiDevice::SetComputeUniformFloat(unsigned int shader, const char* name, float v) {
    size_t off = GetOrCreateUniformOffset(shader, name, sizeof(float));
    EnsureStagingCapacity(compute_params_staging_, off, sizeof(float));
    memcpy(compute_params_staging_.data() + off, &v, sizeof(float));
}
void DX11RhiDevice::SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
    int d[2]{x,y};
    size_t off = GetOrCreateUniformOffset(shader, name, sizeof(d));
    EnsureStagingCapacity(compute_params_staging_, off, sizeof(d));
    memcpy(compute_params_staging_.data() + off, d, sizeof(d));
}
void DX11RhiDevice::SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
    float d[2]{x,y};
    size_t off = GetOrCreateUniformOffset(shader, name, sizeof(d));
    EnsureStagingCapacity(compute_params_staging_, off, sizeof(d));
    memcpy(compute_params_staging_.data() + off, d, sizeof(d));
}
void DX11RhiDevice::SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) {
    float d[3]{x,y,z};
    size_t off = GetOrCreateUniformOffset(shader, name, sizeof(d));
    EnsureStagingCapacity(compute_params_staging_, off, sizeof(d));
    memcpy(compute_params_staging_.data() + off, d, sizeof(d));
}
void DX11RhiDevice::SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) {
    int d[3]{x,y,z};
    size_t off = GetOrCreateUniformOffset(shader, name, sizeof(d));
    EnsureStagingCapacity(compute_params_staging_, off, sizeof(d));
    memcpy(compute_params_staging_.data() + off, d, sizeof(d));
}
void DX11RhiDevice::SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
    float d[4]{x,y,z,w};
    size_t off = GetOrCreateUniformOffset(shader, name, sizeof(d));
    EnsureStagingCapacity(compute_params_staging_, off, sizeof(d));
    memcpy(compute_params_staging_.data() + off, d, sizeof(d));
}
void DX11RhiDevice::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
    size_t off = GetOrCreateUniformOffset(shader, name, 64);
    EnsureStagingCapacity(compute_params_staging_, off, 64);
    memcpy(compute_params_staging_.data() + off, data, 64);
}
void DX11RhiDevice::ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) {
    if (!initialized_ || !dst || size == 0) return;

    const auto* ssbo = resource_mgr_.GetSSBO(handle);
    if (!ssbo || !ssbo->buffer) return;

    ID3D11Device* dev = context_.device();
    ID3D11DeviceContext* dc = context_.device_context();
    if (!dev || !dc) return;

    // 创建 staging buffer 用于 GPU→CPU 读回（同步路径，保留兼容性）
    D3D11_BUFFER_DESC staging_desc{};
    staging_desc.ByteWidth = static_cast<UINT>(size);
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Buffer> staging;
    HRESULT hr = dev->CreateBuffer(&staging_desc, nullptr, staging.GetAddressOf());
    if (FAILED(hr)) return;

    D3D11_BOX box{};
    box.left = static_cast<UINT>(offset);
    box.right = static_cast<UINT>(offset + size);
    box.top = 0; box.bottom = 1;
    box.front = 0; box.back = 1;
    dc->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0,
                               ssbo->buffer.Get(), 0, &box);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = dc->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(dst, mapped.pData, size);
        dc->Unmap(staging.Get(), 0);
    }
}

bool DX11RhiDevice::BeginGpuReadback(BufferHandle handle, size_t offset, size_t size) {
    if (!initialized_ || size == 0) return false;

    const auto* ssbo = resource_mgr_.GetSSBO(handle.raw());
    if (!ssbo || !ssbo->buffer) return false;

    ID3D11Device* dev = context_.device();
    ID3D11DeviceContext* dc = context_.device_context();
    if (!dev || !dc) return false;

    auto& rb = async_readback_;
    bool has_result = false;

    // 步骤 1: 读取上一帧的 staging buffer
    // 使用阻塞 Map（flags=0）而非 D3D11_MAP_FLAG_DO_NOT_WAIT。原因：
    // GPU 满载时（帧时间 ≈ GPU 渲染时间），非阻塞 Map 永远返回 STILL_DRAWING，
    // 因为 CopySubresourceRegion 在 GPU 命令流末尾，下一帧 Map 时 GPU 可能还在处理
    // 当前帧的渲染命令。双缓冲 staging 只有 2 个槽位，到第三帧时数据已被覆盖。
    // 阻塞 Map 的实际 stall 接近 0：copy 在上一帧末尾提交，此帧开头 Map 时
    // GPU 已有整帧时间（>100ms）来完成一个微秒级的 20KB buffer copy。
    const int read_idx = 1 - rb.write_idx;
    if (rb.has_pending && rb.staging[read_idx] && rb.pending_size > 0) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = dc->Map(rb.staging[read_idx].Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (SUCCEEDED(hr)) {
            rb.result.resize(rb.pending_size);
            memcpy(rb.result.data(), mapped.pData, rb.pending_size);
            dc->Unmap(rb.staging[read_idx].Get(), 0);
            has_result = true;
        }
    }

    // 步骤 2: 发起本帧的 GPU→staging 拷贝（非阻塞）
    const int cur_write = rb.write_idx;
    if (size > rb.capacity[cur_write]) {
        rb.staging[cur_write].Reset();
        D3D11_BUFFER_DESC staging_desc{};
        staging_desc.ByteWidth = static_cast<UINT>(size);
        staging_desc.Usage = D3D11_USAGE_STAGING;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        dev->CreateBuffer(&staging_desc, nullptr, rb.staging[cur_write].ReleaseAndGetAddressOf());
        rb.capacity[cur_write] = size;
    }
    if (rb.staging[cur_write]) {
        D3D11_BOX box{};
        box.left = static_cast<UINT>(offset);
        box.right = static_cast<UINT>(offset + size);
        box.top = 0; box.bottom = 1;
        box.front = 0; box.back = 1;
        dc->CopySubresourceRegion(rb.staging[cur_write].Get(), 0, 0, 0, 0,
                                   ssbo->buffer.Get(), 0, &box);
    }

    // 翻转索引
    rb.pending_size = size;
    rb.has_pending = true;
    rb.write_idx = 1 - cur_write;

    return has_result;
}

const void* DX11RhiDevice::GetLastReadbackResult(size_t* out_size) const {
    if (out_size) *out_size = async_readback_.result.size();
    return async_readback_.result.empty() ? nullptr : async_readback_.result.data();
}

unsigned int DX11RhiDevice::CreateComputeShaderEx(
    const std::string& /*gl_src*/, const std::string& /*vk_src*/, const std::string& hlsl_src,
    uint32_t /*ssbo_count*/, uint32_t /*storage_image_count*/,
    uint32_t /*sampler_count*/, uint32_t /*push_constant_bytes*/, const std::string& /*wgsl_src*/) {
    return shader_mgr_.CreateComputeProgram(hlsl_src, "main");
}

unsigned int DX11RhiDevice::CreateComputeWriteTexture2D(int width, int height) {
    if (!initialized_) return 0;
    return resource_mgr_.CreateComputeWriteTexture2D(width, height);
}

// ============================================================
// Indirect Draw Buffer
// ============================================================

unsigned int DX11RhiDevice::CreateIndirectBuffer(size_t size, const void* data) {
    return resource_mgr_.CreateIndirectBuffer(size, data);
}

void DX11RhiDevice::UpdateIndirectBuffer(unsigned int handle, size_t offset,
                                          size_t size, const void* data) {
    resource_mgr_.UpdateIndirectBuffer(handle, offset, size, data);
}

void DX11RhiDevice::DeleteIndirectBuffer(unsigned int handle) {
    resource_mgr_.DeleteIndirectBuffer(handle);
}

void DX11RhiDevice::MultiDrawIndexedIndirect(unsigned int indirect_buffer,
                                              int draw_count, size_t stride, size_t byte_offset) {
    if (draw_count <= 0 || indirect_buffer == 0) return;
    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;

    // 查找 ID3D11Buffer*：先查 indirect buffer map，再查 SSBO map（draw cmd SSBO）
    ID3D11Buffer* d3d_buf = nullptr;
    const DX11IndirectBuffer* ibuf = resource_mgr_.GetIndirectBuffer(indirect_buffer);
    if (ibuf && ibuf->buffer) {
        d3d_buf = ibuf->buffer.Get();
    } else {
        const DX11SSBO* sbuf = resource_mgr_.GetSSBO(indirect_buffer);
        if (sbuf && sbuf->buffer) d3d_buf = sbuf->buffer.Get();
    }
    if (!d3d_buf) return;

    // GPU-Driven per-draw：更新 draw_id（VS 从 ByteAddressBuffer t16 读 model）
    // 同时保留 PerObjectCB 回退（gpu_driven shader 不可用时）
    const auto* inst_data = static_cast<const GPUInstanceData*>(cached_gpu_models_);
    const bool use_gpu_driven = (shader_mgr_.gpu_driven_pbr_shader_handle() != 0);

    // 绑定 instance SSBO 到 VS t16（GPU-driven VS 从 ByteAddressBuffer 读 model）
    if (use_gpu_driven) {
        auto it = bound_ssbos_.find(gpu_driven::kSSBOBindingInstances);
        if (it != bound_ssbos_.end()) {
            const DX11SSBO* inst_ssbo = resource_mgr_.GetSSBO(it->second.handle);
            if (inst_ssbo && inst_ssbo->srv) {
                ID3D11ShaderResourceView* vs_srv = inst_ssbo->srv.Get();
                dc->VSSetShaderResources(16, 1, &vs_srv);
            }
        }
    }

    const int base_index = static_cast<int>(byte_offset / stride);
    for (int i = 0; i < draw_count; ++i) {
        const int global_idx = base_index + i;
        if (inst_data && global_idx < cached_gpu_count_) {
            if (use_gpu_driven) {
                draw_executor_.UpdateDrawId(static_cast<uint32_t>(global_idx));
            } else {
                DX11PerObjectCB obj{};
                obj.model = inst_data[global_idx].model;
                obj.skinned = 0;
                obj.morph_enabled = 0;
                obj.bone_offset = 0;
                draw_executor_.UpdatePerObjectCB(obj);
            }
        }
        const UINT draw_byte_offset = static_cast<UINT>(byte_offset + i * stride);
        dc->DrawIndexedInstancedIndirect(d3d_buf, draw_byte_offset);
    }
}

// ============================================================
// Mega Buffer (GPU Driven)
// ============================================================

VertexArrayHandle DX11RhiDevice::CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                                BufferHandle& out_vbo, BufferHandle& out_ibo) {
    unsigned int vbo_h = resource_mgr_.CreateBuffer(vbo_size_bytes, nullptr, true, false);
    unsigned int ibo_h = resource_mgr_.CreateBuffer(ibo_size_bytes, nullptr, true, true);
    if (vbo_h == 0 || ibo_h == 0) {
        if (vbo_h) resource_mgr_.DeleteBuffer(vbo_h);
        if (ibo_h) resource_mgr_.DeleteBuffer(ibo_h);
        out_vbo = {}; out_ibo = {};
        return {};
    }
    unsigned int vao_id = next_vao_id_++;
    vao_bindings_[vao_id] = {vbo_h, ibo_h};
    out_vbo = BufferHandle{vbo_h};
    out_ibo = BufferHandle{ibo_h};
    return VertexArrayHandle{vao_id};
}

void DX11RhiDevice::UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) {
    if (!vbo || size == 0 || !data) return;
    resource_mgr_.UpdateBuffer(vbo.raw(), offset, size, data, false);
}

void DX11RhiDevice::UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) {
    if (!ibo || size == 0 || !data) return;
    resource_mgr_.UpdateBuffer(ibo.raw(), offset, size, data, true);
}

void DX11RhiDevice::DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) {
    if (vbo) resource_mgr_.DeleteBuffer(vbo.raw());
    if (ibo) resource_mgr_.DeleteBuffer(ibo.raw());
    vao_bindings_.erase(vao.raw());
}

void DX11RhiDevice::BindMegaVAO(VertexArrayHandle vao) {
    auto it = vao_bindings_.find(vao.raw());
    if (it == vao_bindings_.end()) return;

    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;

    const DX11Buffer* vbo_buf = resource_mgr_.GetBuffer(it->second.vbo_handle);
    const DX11Buffer* ibo_buf = resource_mgr_.GetBuffer(it->second.ibo_handle);
    if (vbo_buf && vbo_buf->buffer) {
        UINT stride = sizeof(BatchVertex);
        UINT offset = 0;
        ID3D11Buffer* vb = vbo_buf->buffer.Get();
        dc->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    }
    if (ibo_buf && ibo_buf->buffer) {
        dc->IASetIndexBuffer(ibo_buf->buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    }
}

void DX11RhiDevice::UnbindVAO() {
    // DX11 无需显式解绑
}

// ============================================================
// Static Mesh VAO
// ============================================================

VertexArrayHandle DX11RhiDevice::CreateStaticMeshVAO(
    const void* vertex_data, size_t vertex_bytes,
    const std::vector<const void*>& ebo_datas,
    const std::vector<size_t>& ebo_sizes,
    BufferHandle& out_vbo,
    std::vector<BufferHandle>& out_ebos)
{
    if (!vertex_data || vertex_bytes == 0) { out_vbo = {}; out_ebos.clear(); return {}; }
    if (ebo_datas.size() != ebo_sizes.size()) { out_vbo = {}; out_ebos.clear(); return {}; }

    unsigned int vbo_h = resource_mgr_.CreateBuffer(vertex_bytes, vertex_data, false, false);
    if (vbo_h == 0) { out_vbo = {}; out_ebos.clear(); return {}; }

    out_ebos.resize(ebo_datas.size());
    unsigned int first_ebo = 0;
    for (size_t i = 0; i < ebo_datas.size(); ++i) {
        unsigned int ebo_h = resource_mgr_.CreateBuffer(ebo_sizes[i], ebo_datas[i], false, true);
        out_ebos[i] = BufferHandle{ebo_h};
        if (i == 0) first_ebo = ebo_h;
    }

    unsigned int vao_id = next_vao_id_++;
    vao_bindings_[vao_id] = {vbo_h, first_ebo};
    out_vbo = BufferHandle{vbo_h};
    return VertexArrayHandle{vao_id};
}

void DX11RhiDevice::DeleteStaticMeshVAO(VertexArrayHandle vao, BufferHandle vbo,
                                          const std::vector<BufferHandle>& ebos) {
    for (auto& ebo : ebos) {
        if (ebo) resource_mgr_.DeleteBuffer(ebo.raw());
    }
    if (vbo) resource_mgr_.DeleteBuffer(vbo.raw());
    vao_bindings_.erase(vao.raw());
}

void DX11RhiDevice::BindVAOWithEBO(VertexArrayHandle vao, BufferHandle ebo) {
    auto it = vao_bindings_.find(vao.raw());
    if (it == vao_bindings_.end()) return;

    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;

    const DX11Buffer* vbo_buf = resource_mgr_.GetBuffer(it->second.vbo_handle);
    if (vbo_buf && vbo_buf->buffer) {
        UINT stride = sizeof(BatchVertex);
        UINT offset = 0;
        ID3D11Buffer* vb = vbo_buf->buffer.Get();
        dc->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    }
    const DX11Buffer* ebo_buf = resource_mgr_.GetBuffer(ebo.raw());
    if (ebo_buf && ebo_buf->buffer) {
        dc->IASetIndexBuffer(ebo_buf->buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    }
}

// ============================================================
// GPU-Driven PBR Shader Setup
// ============================================================

bool DX11RhiDevice::HasGPUDrivenPBRShader() const {
    return shader_mgr_.gpu_driven_pbr_shader_handle() != 0;
}

void DX11RhiDevice::SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                              const glm::vec3& camera_pos,
                                              const glm::vec3& light_dir, const glm::vec3& light_color,
                                              float light_intensity, float ambient_intensity,
                                              float shadow_strength) {
    draw_executor_.SetupGPUDrivenPBR(view, proj, camera_pos,
                                      light_dir, light_color,
                                      light_intensity, ambient_intensity,
                                      shadow_strength,
                                      state_mgr_, shader_mgr_);
}

void DX11RhiDevice::SetupGPUDrivenShadowShader(const glm::mat4& light_view, const glm::mat4& light_proj) {
    draw_executor_.SetupGPUDrivenShadow(light_view, light_proj, state_mgr_, shader_mgr_);
}

void DX11RhiDevice::BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                                            unsigned int metallic_roughness,
                                            unsigned int emissive, unsigned int occlusion) {
    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;

    const auto& slots = shader_mgr_.pbr_texture_slots();
    ID3D11ShaderResourceView* white_srv = draw_executor_.white_srv();
    ID3D11SamplerState* white_sam = draw_executor_.white_sampler();

    auto bind_slot = [&](int slot, unsigned int handle) {
        if (handle != 0) {
            const auto* tex = resource_mgr_.GetTexture(handle);
            if (tex && tex->srv) {
                dc->PSSetShaderResources(static_cast<UINT>(slot), 1, tex->srv.GetAddressOf());
                if (tex->sampler)
                    dc->PSSetSamplers(static_cast<UINT>(slot), 1, tex->sampler.GetAddressOf());
                return;
            }
        }
        if (white_srv) dc->PSSetShaderResources(static_cast<UINT>(slot), 1, &white_srv);
        if (white_sam) dc->PSSetSamplers(static_cast<UINT>(slot), 1, &white_sam);
    };

    bind_slot(slots.albedo, albedo);
    bind_slot(slots.normal, normal);
    bind_slot(slots.metallic_roughness, metallic_roughness);
    bind_slot(slots.emissive, emissive);
    bind_slot(slots.occlusion, occlusion);
}

void DX11RhiDevice::CacheGPUDrivenInstanceData(const void* models, const void* cmds, int count) {
    cached_gpu_models_ = models;
    cached_gpu_cmds_   = cmds;
    cached_gpu_count_  = count;
}

void DX11RhiDevice::UpdateGPUDrivenMaterial(const void* mat_data) {
    if (!mat_data) return;
    draw_executor_.UpdateGPUDrivenMaterial(mat_data);
}

// --- 编辑器场景视图模式 ---

void DX11RhiDevice::SetWireframeMode(bool enable) {
    global_render_state_.wireframe_mode = enable;
    auto* ctx = context_.device_context();
    if (!ctx) return;
    
    // 使用缓存的 rasterizer state
    ID3D11RasterizerState* target_state = enable ? wireframe_rasterizer_state_ : solid_rasterizer_state_;
    if (target_state) {
        ctx->RSSetState(target_state);
        return;
    }
    
    // 首次使用时创建并缓存
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = enable ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = TRUE;
    rd.DepthClipEnable = TRUE;
    auto* dev = context_.device();
    if (dev && SUCCEEDED(dev->CreateRasterizerState(&rd, &target_state))) {
        ctx->RSSetState(target_state);
        if (enable) {
            wireframe_rasterizer_state_ = target_state;
        } else {
            solid_rasterizer_state_ = target_state;
        }
    }
}

void DX11RhiDevice::SetForceUnlit(bool enable) {
    global_render_state_.force_unlit = enable;
}

void DX11RhiDevice::SetOverdrawMode(bool enable) {
    auto* ctx = context_.device_context();
    if (!ctx) return;
    if (enable) {
        // Additive blend
        D3D11_BLEND_DESC bd = {};
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        ID3D11BlendState* bs = nullptr;
        auto* dev = context_.device();
        if (dev && SUCCEEDED(dev->CreateBlendState(&bd, &bs))) {
            float factor[4] = {0, 0, 0, 0};
            ctx->OMSetBlendState(bs, factor, 0xFFFFFFFF);
            bs->Release();
        }
        // Disable depth write
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        ID3D11DepthStencilState* dss = nullptr;
        if (dev && SUCCEEDED(dev->CreateDepthStencilState(&dsd, &dss))) {
            ctx->OMSetDepthStencilState(dss, 0);
            dss->Release();
        }
    } else {
        ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        // Restore default depth stencil
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        ID3D11DepthStencilState* dss = nullptr;
        auto* dev = context_.device();
        if (dev && SUCCEEDED(dev->CreateDepthStencilState(&dsd, &dss))) {
            ctx->OMSetDepthStencilState(dss, 0);
            dss->Release();
        }
    }
    global_render_state_.overdraw_mode = enable;
}

} // namespace render
} // namespace dse

