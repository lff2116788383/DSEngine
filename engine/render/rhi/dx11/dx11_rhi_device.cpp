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

namespace dse {
namespace render {

// ============================================================
// DX11CommandBuffer 实现
// ============================================================

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

void DX11CommandBuffer::SetCamera(const glm::mat4& view, const glm::mat4& projection) {
    view_ = view;
    projection_ = projection;
}

void DX11CommandBuffer::DrawBatch(const std::vector<DrawBatchItem>& items) {
    if (!items.empty()) {
        DrawSpriteBatch(items);
    }
}

void DX11CommandBuffer::DrawMeshBatch(const std::vector<MeshDrawItem>& items) {
    if (!device_ || items.empty()) return;
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

void DX11CommandBuffer::SetGlobalMat4(const std::string& name, const glm::mat4& value) {
    pending_mat4_[name] = value;
}

void DX11CommandBuffer::SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) {
    pending_mat4_array_[name] = values;
}

void DX11CommandBuffer::SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) {
    pending_float_array_[name] = values;
}

void DX11CommandBuffer::DrawSkybox(unsigned int cubemap_texture_handle) {
    if (!device_) return;
    device_->draw_executor().DrawSkybox(cubemap_texture_handle, view_, projection_,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) {
    if (!device_) return;
    device_->draw_executor().DrawPostProcess(source_texture, effect_name, params,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) {
    if (!device_ || items.empty()) return;
    device_->draw_executor().DrawParticles3D(items, view, projection,
        device_->state_mgr(), device_->shader_mgr(), device_->resource_mgr());
}

void DX11CommandBuffer::DeferSetGlobalShadowMap(unsigned int index, unsigned int texture_handle) {
    if (device_) device_->SetGlobalShadowMap(index, texture_handle);
}

void DX11CommandBuffer::DeferSetGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) {
    if (device_) device_->SetGlobalSpotShadowMap(index, texture_handle);
}

void DX11CommandBuffer::DeferSetGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) {
    if (device_) device_->SetGlobalPointShadowMap(index, texture_handle);
}

void DX11CommandBuffer::Reset() {
    view_ = glm::mat4(1.0f);
    projection_ = glm::mat4(1.0f);
    ClearPendingUniforms();
}

// ============================================================
// DX11RhiDevice 实现
// ============================================================

bool DX11RhiDevice::InitD3D11(void* window_handle, int width, int height, bool enable_debug) {
    if (initialized_) return true;

    if (!context_.Init(window_handle, width, height, enable_debug)) {
        DEBUG_LOG_ERROR("[D3D11] Context init failed");
        return false;
    }

    if (!resource_mgr_.Init(&context_)) {
        DEBUG_LOG_ERROR("[D3D11] ResourceManager init failed");
        return false;
    }

    shader_mgr_.Init(&context_);
    state_mgr_.Init(&context_);
    draw_executor_.Init(&context_, &resource_mgr_);

    // 初始化内置着色器
    shader_mgr_.InitBuiltinShaders();

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
    draw_executor_.BeginFrame();
}

unsigned int DX11RhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    return resource_mgr_.CreateRenderTarget(
        desc.width, desc.height, desc.has_color, desc.has_depth,
        desc.generate_mipmaps, desc.cube_map);
}

unsigned int DX11RhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    return resource_mgr_.GetRenderTargetColorTextureHandle(render_target_handle);
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

unsigned int DX11RhiDevice::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    return resource_mgr_.CreateTextureCube(width, height, rgba8_faces, linear_filter);
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
    current_frame_stats_.draw_calls += draw_executor_.current_frame_stats().draw_calls;
    current_frame_stats_.sprite_count += draw_executor_.current_frame_stats().sprite_count;
    current_frame_stats_.mesh_count += draw_executor_.current_frame_stats().mesh_count;
}

void DX11RhiDevice::EndFrame() {
    if (!initialized_) return;

    draw_executor_.EndFrame();
    last_frame_stats_ = current_frame_stats_;
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

} // namespace render
} // namespace dse
