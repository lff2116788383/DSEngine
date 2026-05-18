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
#include "engine/base/debug.h"

#include <cstdlib>

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

void DX11CommandBuffer::SetPipelineState(unsigned int pipeline_state_handle) {
    if (!device_) return;
    device_->state_mgr().ApplyPipelineState(pipeline_state_handle, device_->context().device_context());
}

void DX11CommandBuffer::DrawMeshBatch(const std::vector<MeshDrawItem>& items) {
    if (!device_ || items.empty()) return;
    DispatchPendingLightArrays();
    device_->draw_executor().DrawMeshBatch(items, view_, projection_,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) {
    if (!device_ || items.empty()) return;
    device_->draw_executor().DrawSpriteBatch(items, view_, projection_,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
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

void DX11CommandBuffer::DrawSkybox(unsigned int cubemap_texture_handle) {
    if (!device_) return;
    device_->draw_executor().DrawSkybox(cubemap_texture_handle, view_, projection_,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::DrawPostProcess(dse::render::PostProcessRequest request) {
    if (!device_) return;
    device_->draw_executor().DrawPostProcess(request,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (!device_ || items.empty()) return;
    device_->draw_executor().DrawParticles3D(items, view, projection,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (!device_ || items.empty()) return;
    device_->draw_executor().DrawHairStrands(items, view, projection,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::Reset() {
    ResetBase();
}

// ============================================================
// DX11RhiDevice 实现
// ============================================================

bool DX11RhiDevice::InitDevice(void* window_handle, int width, int height) {
    const char* sdr_env = std::getenv("DSE_FORCE_SDR");
    bool force_sdr = sdr_env && (sdr_env[0] == '1' || sdr_env[0] == 'y' || sdr_env[0] == 'Y');
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

    initialized_ = true;
    DEBUG_LOG_INFO("[D3D11] RhiDevice initialized (all subsystems ready)");
    return true;
}

void DX11RhiDevice::Shutdown() {
    if (!initialized_) return;

    draw_executor_.Shutdown();
    state_mgr_.Shutdown();

    // 清理外部着色器
    for (auto h : external_shader_programs_) {
        shader_mgr_.DeleteProgram(h);
    }
    external_shader_programs_.clear();

    shader_mgr_.Shutdown();
    resource_mgr_.Shutdown();
    context_.Shutdown();

    initialized_ = false;
    DEBUG_LOG_INFO("[D3D11] RhiDevice shutdown");
}

void DX11RhiDevice::BeginFrame() {
    current_frame_stats_ = RenderStats{};
    resource_mgr_.FlushPendingUploads();
    draw_executor_.BeginFrame();

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

void DX11RhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    resource_mgr_.UpdateBuffer(handle, offset, size, data, is_index);
}

void DX11RhiDevice::DeleteBuffer(unsigned int handle) {
    resource_mgr_.DeleteBuffer(handle);
}

unsigned int DX11RhiDevice::CreateVertexArray() {
    return resource_mgr_.CreateVertexArray();
}

void DX11RhiDevice::DeleteVertexArray(unsigned int handle) {
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

    // Present 交换链
    context_.Present(true);
}

const RenderStats& DX11RhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

// --- 阴影/光源全局状态（委托给 DrawExecutor） ---

void DX11RhiDevice::SetGlobalShadowMap(unsigned int index, unsigned int handle) {
    draw_executor_.SetGlobalShadowMap(index, handle);
}

void DX11RhiDevice::SetGlobalSpotShadowMap(unsigned int index, unsigned int handle) {
    draw_executor_.SetGlobalSpotShadowMap(index, handle);
}

void DX11RhiDevice::SetGlobalPointShadowMap(unsigned int index, unsigned int handle) {
    draw_executor_.SetGlobalPointShadowMap(index, handle);
}

void DX11RhiDevice::SetGlobalLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
    draw_executor_.SetGlobalLightSpaceMatrix(index, mat);
}

void DX11RhiDevice::SetGlobalCascadeSplit(unsigned int index, float split) {
    draw_executor_.SetGlobalCascadeSplit(index, split);
}

void DX11RhiDevice::SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
    draw_executor_.SetGlobalSpotLightSpaceMatrix(index, mat);
}

void DX11RhiDevice::SetGlobalLightProbeSH(const glm::vec4 sh[9], bool enabled) {
    draw_executor_.SetGlobalLightProbeSH(sh, enabled);
}

void DX11RhiDevice::SetGlobalGBufferTexture(unsigned int index, unsigned int texture_handle) {
    draw_executor_.SetGlobalGBufferTexture(index, texture_handle);
}

void DX11RhiDevice::SetGBufferRenderingMode(bool enabled) {
    draw_executor_.SetGBufferRenderingMode(enabled);
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
    bound_ssbos_[binding_point] = handle;
}

void DX11RhiDevice::DeleteSSBO(unsigned int handle) {
    resource_mgr_.DeleteSSBO(handle);
}

// --- Compute Shader ---

unsigned int DX11RhiDevice::CreateComputeShader(const std::string& source) {
    return shader_mgr_.CreateComputeProgram(source);
}

void DX11RhiDevice::DeleteComputeShader(unsigned int handle) {
    // shader_mgr_ 的 Shutdown 会清理，此处仅占位
    (void)handle;
}

void DX11RhiDevice::DispatchCompute(unsigned int shader_handle,
                                     unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
    if (!initialized_ || shader_handle == 0) return;

    const auto* prog = shader_mgr_.GetComputeProgram(shader_handle);
    if (!prog || !prog->cs) return;

    ID3D11DeviceContext* dc = context_.device_context();
    if (!dc) return;

    dc->CSSetShader(prog->cs.Get(), nullptr, 0);

    // 将追踪的 SSBO 绑定到 CS 阶段（SRV slot 16+ 与 PS 保持一致）
    for (auto& [binding_point, ssbo_handle] : bound_ssbos_) {
        const auto* ssbo = resource_mgr_.GetSSBO(ssbo_handle);
        if (!ssbo) continue;
        ID3D11ShaderResourceView* srv = ssbo->srv.Get();
        if (srv) {
            dc->CSSetShaderResources(16 + binding_point, 1, &srv);
        }
    }

    dc->Dispatch(groups_x, groups_y, groups_z);

    // 解绑 CS 资源
    for (auto& [binding_point, _] : bound_ssbos_) {
        ID3D11ShaderResourceView* null_srv = nullptr;
        dc->CSSetShaderResources(16 + binding_point, 1, &null_srv);
    }
    dc->CSSetShader(nullptr, nullptr, 0);
}

void DX11RhiDevice::ComputeMemoryBarrier() {
    // D3D11 不需要显式 memory barrier — resource hazard tracking 由运行时自动处理
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
        // 绑定为 UAV（需要从 render target 获取，或创建专用 UAV）
        // 当前仅支持 render target 的 UAV
        const auto* rt = resource_mgr_.GetRenderTarget(texture_handle);
        if (rt && rt->color_uav) {
            ID3D11UnorderedAccessView* uav = rt->color_uav.Get();
            dc->CSSetUnorderedAccessViews(binding, 1, &uav, nullptr);
        }
    }
}

void DX11RhiDevice::SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                               int mip_level, bool read_only, bool r32f) {
    // TODO: DX11 Hi-Z — 创建 per-mip UAV/SRV 并绑定到 CS
    (void)binding; (void)texture_handle; (void)mip_level; (void)read_only; (void)r32f;
}

void DX11RhiDevice::SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) {
    // TODO: DX11 Hi-Z — 绑定纹理 SRV + sampler 到 CS
    (void)unit; (void)texture_handle;
}

unsigned int DX11RhiDevice::CreateHiZTexture(int width, int height) {
    // TODO: DX11 Hi-Z — 创建 DXGI_FORMAT_R32_FLOAT 纹理，完整 mip chain + UAV per mip
    (void)width; (void)height;
    return 0;
}

void DX11RhiDevice::DeleteHiZTexture(unsigned int handle) {
    (void)handle;
}

int DX11RhiDevice::GetHiZMipCount(unsigned int handle) const {
    (void)handle;
    return 0;
}

unsigned int DX11RhiDevice::GetHiZGpuTexture(unsigned int handle) const {
    (void)handle;
    return 0;
}

void DX11RhiDevice::SetComputeUniformInt(unsigned int shader, const char* name, int value) {
    (void)shader; (void)name; (void)value;
}
void DX11RhiDevice::SetComputeUniformFloat(unsigned int shader, const char* name, float value) {
    (void)shader; (void)name; (void)value;
}
void DX11RhiDevice::SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
    (void)shader; (void)name; (void)x; (void)y;
}
void DX11RhiDevice::SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
    (void)shader; (void)name; (void)x; (void)y;
}
void DX11RhiDevice::SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
    (void)shader; (void)name; (void)x; (void)y; (void)z; (void)w;
}
void DX11RhiDevice::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
    (void)shader; (void)name; (void)data;
}
void DX11RhiDevice::ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) {
    (void)handle; (void)offset; (void)size; (void)dst;
}

} // namespace render
} // namespace dse
