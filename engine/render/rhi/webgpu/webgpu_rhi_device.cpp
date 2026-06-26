/**
 * @file webgpu_rhi_device.cpp
 * @brief WebGPU RHI 后端（orchestrator）实现。详见头文件。
 *
 * 本类已退化为 orchestrator：持 ctx/res/pso/shader/exec 五个 manager，自留设备生命周期
 * 编排（ctor/dtor/Init/Shutdown/BeginFrame/EndFrame/PresentFrame）与少量元信息查询；其余
 * 所有 RHI 虚函数 + 设备级 Cmd* 均为一行转发到对应 manager（见文件末「转发」段）。
 */

#include "engine/render/rhi/webgpu/webgpu_rhi_device.h"

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/webgpu/webgpu_command_buffer.h"
#ifdef DSE_WEBGPU_SELFTEST
#include "engine/render/rhi/webgpu/webgpu_selftest_harness.h"
#endif

#include <glm/glm.hpp>

#include <emscripten/html5.h>

#include <memory>

namespace dse {
namespace render {

// ============================================================
// 构造 / 析构：装配五个 manager（构造序 ctx→res→pso→shader→exec），
// 经 DeviceAcquired 回调把同名稳定句柄 device_/queue_ 同步进各 manager。
// ============================================================
WebGPURhiDevice::WebGPURhiDevice() {
    ctx_ = std::make_unique<WebGPUContext>();
    // sibling 指针装配（method 体内 res_->/shader_->/pso_->/ctx_-> 跨界访问据此成立）。
    res_.Init(ctx_.get(), &exec_);
    pso_.Init(ctx_.get());
    shader_.Init(ctx_.get(), &res_, &pso_);
    exec_.Init(ctx_.get(), &res_, &pso_, &shader_, this, &last_frame_stats_);
    // device 生命周期内仅触发两次：AcquireDevice 成功（设句柄）、Shutdown（以空清）。
    ctx_->SetDeviceAcquiredCallback([this](WGPUDevice d, WGPUQueue q) {
        res_.OnDeviceAcquired(d, q);
        shader_.OnDeviceAcquired(d, q);
        exec_.OnDeviceAcquired(d, q);
    });
#ifdef DSE_WEBGPU_SELFTEST
    selftest_ = new WebGpuSelfTestHarness(this);
#endif
}

WebGPURhiDevice::~WebGPURhiDevice() {
    Shutdown();
#ifdef DSE_WEBGPU_SELFTEST
    delete selftest_;
    selftest_ = nullptr;
#endif
}

RenderDeviceInfo WebGPURhiDevice::GetDeviceInfo() const {
    RenderDeviceInfo info;
    info.adapter_name = "WebGPU";
    info.is_software = false;  // 实际软/硬由浏览器适配器决定
    return info;
}

// ============================================================
// Shutdown：录制缓存/资源/设备分层释放（exec→shader→pso→res→ctx）。
// ctx 末步释放 swapchain/surface/queue/device，并经回调把各 manager 句柄清空。
// ============================================================
void WebGPURhiDevice::Shutdown() {
    exec_.Shutdown();
    shader_.Shutdown();
    pso_.Shutdown();
    res_.Shutdown();
    ctx_->Shutdown();
}

// ============================================================
// 每帧编排：BeginFrame 取后备视图 + 复位录制态；EndFrame 兜底自检/clear + 提交 + 回读。
// ============================================================
void WebGPURhiDevice::BeginFrame() {
    last_frame_stats_ = RenderStats{};
    if (!ctx_->EnsureInitialized()) return;
    res_.BeginFrameResetVersions();
    if (!ctx_->CreateFrameEncoder()) return;
    // 每帧复位录制瞬态 + 确保阴影回退（2D 恒亮 Depth32 / 点光 cube）就绪。
    exec_.BeginFrameReset();
}

void WebGPURhiDevice::EndFrame() {
    if (!ctx_->initialized() || !ctx_->backbuffer_view() || !ctx_->frame_encoder()) {
        ctx_->ReleaseFrameEncoder();
        return;
    }

    // 本帧若无任何真实绘制落到 backbuffer：先跑 bring-up 自检（仅 SELFTEST 编入）。
    if (!exec_.backbuffer_drawn()) {
#ifdef DSE_WEBGPU_SELFTEST
        if (selftest_) selftest_->RunBringUp();
#endif
    }
    // 兜底：自检也未成形时至少 clear 一次 backbuffer，避免呈现未定义内容。
    if (!exec_.backbuffer_drawn()) {
        WGPURenderPassColorAttachment color_att{};
        color_att.view = ctx_->backbuffer_view();
        color_att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        color_att.loadOp = WGPULoadOp_Clear;
        color_att.storeOp = WGPUStoreOp_Store;
        color_att.clearValue = WGPUColor{0.05, 0.05, 0.08, 1.0};
        WGPURenderPassDescriptor pass_desc{};
        pass_desc.colorAttachmentCount = 1;
        pass_desc.colorAttachments = &color_att;
        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(ctx_->frame_encoder(), &pass_desc);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
        last_frame_stats_.render_passes += 1;
    }

#ifdef DSE_WEBGPU_SELFTEST
    // 每会话各一次：在本帧 frame_encoder_ 上录制全部 pending 离屏自检（外置 harness）。
    if (selftest_) selftest_->RecordPending();
#endif

    ctx_->SubmitEncoder();  // finish + QueueSubmit + 释放 frame_encoder_/backbuffer_view_

#ifdef DSE_WEBGPU_SELFTEST
    if (selftest_) selftest_->KickPendingReadbacks();
#endif
    res_.KickDeferredReadback();  // 提交后对 pending staging 发 mapAsync
    exec_.EndFrameReleaseBindGroups();
}

void WebGPURhiDevice::PresentFrame() {
    // Emscripten 下浏览器在 rAF 回调结束后自动呈现 webgpu 画布；wgpuSwapChainPresent
    // 在胶水里直接 abort，故不可调用。
}

std::shared_ptr<CommandBuffer> WebGPURhiDevice::CreateCommandBuffer() {
    return std::make_shared<WebGPUCommandBuffer>(this);
}

void WebGPURhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    // WebGPUCommandBuffer 录制即时落到本帧 frame_encoder_，故 Submit 无需重放。
    (void)cmd_buffer;
}

const RenderStats& WebGPURhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

glm::mat4 WebGPURhiDevice::GetProjectionCorrection() const {
    // WebGPU NDC：Y-up、Z∈[0,1]。从引擎默认 GL 约定重映射 Z 到 [0,1]，Y 不翻转。
    glm::mat4 m(1.0f);
    m[2][2] = 0.5f;
    m[3][2] = 0.5f;
    return m;
}

glm::mat4 WebGPURhiDevice::GetShadowSampleCorrection() const {
    // 与投影矫正同源但不含 Z 重映射（着色器内统一把 Z 从 [-1,1] 映到 [0,1]）。
    return glm::mat4(1.0f);
}

// ============================================================
// 转发：以下所有方法为一行转发到对应 manager（gen_forwarders.py 机械生成）。
// 路由 ctx_->/res_./pso_./shader_./exec_.，零功能/零调用方 API 改动。
// ============================================================
bool WebGPURhiDevice::InitDevice(void* window_handle, int width, int height) { return ctx_->InitDevice(window_handle, width, height); }
void WebGPURhiDevice::OnWindowResized(int width, int height) { ctx_->OnWindowResized(width, height); }
void WebGPURhiDevice::WaitIdle() { ctx_->WaitIdle(); }
unsigned int WebGPURhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) { return res_.CreateRenderTarget(desc); }
void WebGPURhiDevice::DeleteRenderTarget(unsigned int render_target_handle) { res_.DeleteRenderTarget(render_target_handle); }
unsigned int WebGPURhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const { return res_.GetRenderTargetColorTexture(render_target_handle); }
unsigned int WebGPURhiDevice::GetRenderTargetDepthTexture(unsigned int render_target_handle) const { return res_.GetRenderTargetDepthTexture(render_target_handle); }
std::vector<unsigned char> WebGPURhiDevice::ReadRenderTargetColorRgba8(unsigned int render_target_handle) const { return res_.ReadRenderTargetColorRgba8(render_target_handle); }
RenderTargetReadback WebGPURhiDevice::ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const { return res_.ReadRenderTargetColorRgba8WithSize(render_target_handle); }
unsigned int WebGPURhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const { return res_.GetRenderTargetColorTexture(render_target_handle, index); }
unsigned int WebGPURhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) { return res_.CreateTexture2D(width, height, rgba8_data, linear_filter); }
unsigned int WebGPURhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, const TextureSamplerDesc& sampler) { return res_.CreateTexture2D(width, height, rgba8_data, sampler); }
unsigned int WebGPURhiDevice::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) { return res_.CreateTextureCube(width, height, rgba8_faces, linear_filter); }
unsigned int WebGPURhiDevice::CreateTextureCubeWithMips(const std::vector<CubeMipLevel>& mips, bool linear_filter) { return res_.CreateTextureCubeWithMips(mips, linear_filter); }
unsigned int WebGPURhiDevice::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) { return res_.CreateTexture3D(width, height, depth, rgba8_data, linear_filter); }
void WebGPURhiDevice::DeleteTexture(unsigned int texture_handle) { res_.DeleteTexture(texture_handle); }
unsigned int WebGPURhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) { return shader_.CreateShaderProgram(vert_src, frag_src); }
void WebGPURhiDevice::DeleteShaderProgram(unsigned int program_handle) { shader_.DeleteShaderProgram(program_handle); }
unsigned int WebGPURhiDevice::CreatePipelineState(const PipelineStateDesc& desc) { return pso_.CreatePipelineState(desc); }
unsigned int WebGPURhiDevice::GetBuiltinProgram(BuiltinProgram program) { return shader_.GetBuiltinProgram(program); }
unsigned int WebGPURhiDevice::GetGenPPShaderProgram(const std::string& effect_name) { return shader_.GetGenPPShaderProgram(effect_name); }
unsigned int WebGPURhiDevice::GetSkyboxCubeVertexBuffer() { return shader_.GetSkyboxCubeVertexBuffer(); }
unsigned int WebGPURhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) { return res_.CreateBuffer(size, data, is_dynamic, is_index); }
void WebGPURhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) { res_.UpdateBuffer(handle, offset, size, data, is_index); }
void WebGPURhiDevice::DeleteBuffer(unsigned int handle) { res_.DeleteBuffer(handle); }
VertexArrayHandle WebGPURhiDevice::CreateVertexArray() { return exec_.CreateVertexArray(); }
void WebGPURhiDevice::DeleteVertexArray(VertexArrayHandle handle) { exec_.DeleteVertexArray(handle); }
void WebGPURhiDevice::CmdBeginRenderPass(const RenderPassDesc& desc) { exec_.CmdBeginRenderPass(desc); }
void WebGPURhiDevice::CmdEndRenderPass() { exec_.CmdEndRenderPass(); }
void WebGPURhiDevice::CmdClearColor(const glm::vec4& color) { exec_.CmdClearColor(color); }
void WebGPURhiDevice::CmdSetViewport(int x, int y, int width, int height) { exec_.CmdSetViewport(x, y, width, height); }
void WebGPURhiDevice::CmdBindGlobalShadowMap(unsigned int index, unsigned int texture_handle) { exec_.CmdBindGlobalShadowMap(index, texture_handle); }
void WebGPURhiDevice::CmdBindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) { exec_.CmdBindGlobalSpotShadowMap(index, texture_handle); }
void WebGPURhiDevice::CmdBindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) { exec_.CmdBindGlobalPointShadowMap(index, texture_handle); }
void WebGPURhiDevice::CmdBindPipeline(unsigned int graphics_pipeline_handle) { exec_.CmdBindPipeline(graphics_pipeline_handle); }
void WebGPURhiDevice::CmdBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride, const std::vector<VertexAttr>& attrs, VertexInputRate rate) { exec_.CmdBindVertexBuffer(slot, buffer_handle, stride, attrs, rate); }
void WebGPURhiDevice::CmdBindIndexBuffer(unsigned int buffer_handle, IndexType type) { exec_.CmdBindIndexBuffer(buffer_handle, type); }
void WebGPURhiDevice::CmdBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) { exec_.CmdBindTexture(slot, texture_handle, dim); }
void WebGPURhiDevice::CmdBindUniformBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) { exec_.CmdBindUniformBuffer(slot, buffer_handle, offset, size); }
void WebGPURhiDevice::CmdBindStorageBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) { exec_.CmdBindStorageBuffer(slot, buffer_handle, offset, size); }
void WebGPURhiDevice::CmdPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) { exec_.CmdPushConstants(stage, offset, data, size); }
void WebGPURhiDevice::CmdDraw(uint32_t vertex_count, uint32_t first_vertex) { exec_.CmdDraw(vertex_count, first_vertex); }
void WebGPURhiDevice::CmdDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) { exec_.CmdDrawIndexed(index_count, first_index, base_vertex); }
void WebGPURhiDevice::CmdDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance) { exec_.CmdDrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance); }
void WebGPURhiDevice::CmdDrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) { exec_.CmdDrawIndexedIndirect(indirect_buffer, byte_offset); }
void WebGPURhiDevice::CmdDispatchComputePass(const ComputeDispatch& dispatch) { exec_.CmdDispatchComputePass(dispatch); }
unsigned int WebGPURhiDevice::CreateComputeShader(const std::string& source) { return shader_.CreateComputeShader(source); }
unsigned int WebGPURhiDevice::CreateComputeShaderEx(const std::string& gl_src, const std::string& vk_src, const std::string& hlsl_src, uint32_t ssbo_count, uint32_t storage_image_count, uint32_t sampler_count, uint32_t push_constant_bytes, const std::string& wgsl_src) { return shader_.CreateComputeShaderEx(gl_src, vk_src, hlsl_src, ssbo_count, storage_image_count, sampler_count, push_constant_bytes, wgsl_src); }
void WebGPURhiDevice::DeleteComputeShader(unsigned int handle) { shader_.DeleteComputeShader(handle); }
void WebGPURhiDevice::DispatchCompute(unsigned int shader_handle, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) { exec_.DispatchCompute(shader_handle, groups_x, groups_y, groups_z); }
void WebGPURhiDevice::BeginComputePass() { exec_.BeginComputePass(); }
void WebGPURhiDevice::EndComputePass() { exec_.EndComputePass(); }
unsigned int WebGPURhiDevice::CreateComputeWriteTexture2D(int width, int height) { return res_.CreateComputeWriteTexture2D(width, height); }
void WebGPURhiDevice::SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) { exec_.SetComputeTextureImage(binding, texture_handle, read_only); }
void WebGPURhiDevice::SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle, int mip_level, bool read_only, bool r32f) { exec_.SetComputeTextureImageMip(binding, texture_handle, mip_level, read_only, r32f); }
void WebGPURhiDevice::SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) { exec_.SetComputeTextureSampler(unit, texture_handle); }
void WebGPURhiDevice::SetComputeUniformInt(unsigned int shader, const char* name, int value) { exec_.SetComputeUniformInt(shader, name, value); }
void WebGPURhiDevice::SetComputeUniformFloat(unsigned int shader, const char* name, float value) { exec_.SetComputeUniformFloat(shader, name, value); }
void WebGPURhiDevice::SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) { exec_.SetComputeUniformVec2i(shader, name, x, y); }
void WebGPURhiDevice::SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) { exec_.SetComputeUniformVec2f(shader, name, x, y); }
void WebGPURhiDevice::SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) { exec_.SetComputeUniformVec3(shader, name, x, y, z); }
void WebGPURhiDevice::SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) { exec_.SetComputeUniformIVec3(shader, name, x, y, z); }
void WebGPURhiDevice::SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) { exec_.SetComputeUniformVec4(shader, name, x, y, z, w); }
void WebGPURhiDevice::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) { exec_.SetComputeUniformMat4(shader, name, data); }
BufferHandle WebGPURhiDevice::CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) { return res_.CreateGpuBuffer(desc, initial_data); }
void WebGPURhiDevice::UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) { res_.UpdateGpuBuffer(handle, offset, size, data); }
void WebGPURhiDevice::BindGpuBuffer(BufferHandle handle, uint32_t binding_point) { exec_.BindGpuBuffer(handle, binding_point); }
void WebGPURhiDevice::BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable) { exec_.BindGpuBuffer(handle, binding_point, writable); }
void WebGPURhiDevice::DeleteGpuBuffer(BufferHandle handle) { res_.DeleteGpuBuffer(handle); }
bool WebGPURhiDevice::BeginGpuReadback(BufferHandle handle, size_t offset, size_t size) { return res_.BeginGpuReadback(handle, offset, size); }
const void* WebGPURhiDevice::GetLastReadbackResult(size_t* out_size) const { return res_.GetLastReadbackResult(out_size); }
void WebGPURhiDevice::MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride, size_t byte_offset) { exec_.MultiDrawIndexedIndirect(indirect_buffer, draw_count, stride, byte_offset); }
VertexArrayHandle WebGPURhiDevice::CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes, BufferHandle& out_vbo, BufferHandle& out_ibo) { return exec_.CreateMegaVAO(vbo_size_bytes, ibo_size_bytes, out_vbo, out_ibo); }
void WebGPURhiDevice::UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) { exec_.UpdateMegaVBO(vbo, offset, size, data); }
void WebGPURhiDevice::UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) { exec_.UpdateMegaIBO(ibo, offset, size, data); }
void WebGPURhiDevice::DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) { exec_.DeleteMegaVAO(vao, vbo, ibo); }
void WebGPURhiDevice::BindMegaVAO(VertexArrayHandle vao) { exec_.BindMegaVAO(vao); }
void WebGPURhiDevice::UnbindVAO() { exec_.UnbindVAO(); }
bool WebGPURhiDevice::HasGPUDrivenPBRShader() const { return shader_.HasGPUDrivenPBRShader(); }
void WebGPURhiDevice::SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camera_pos, const glm::vec3& light_dir, const glm::vec3& light_color, float light_intensity, float ambient_intensity, float shadow_strength) { exec_.SetupGPUDrivenPBRShader(view, proj, camera_pos, light_dir, light_color, light_intensity, ambient_intensity, shadow_strength); }
void WebGPURhiDevice::BindGPUDrivenTextures(unsigned int albedo, unsigned int normal, unsigned int metallic_roughness, unsigned int emissive, unsigned int occlusion) { exec_.BindGPUDrivenTextures(albedo, normal, metallic_roughness, emissive, occlusion); }
unsigned int WebGPURhiDevice::CreateHiZTexture(int width, int height) { return res_.CreateHiZTexture(width, height); }
void WebGPURhiDevice::DeleteHiZTexture(unsigned int handle) { res_.DeleteHiZTexture(handle); }
int WebGPURhiDevice::GetHiZMipCount(unsigned int handle) const { return res_.GetHiZMipCount(handle); }
unsigned int WebGPURhiDevice::GetHiZGpuTexture(unsigned int handle) const { return res_.GetHiZGpuTexture(handle); }

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
