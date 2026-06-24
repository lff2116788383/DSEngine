/**
 * @file webgpu_rhi_device.cpp
 * @brief WebGPU RHI 后端实现（B0 骨架）。详见头文件。
 */

#include "engine/render/rhi/webgpu/webgpu_rhi_device.h"

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/base/debug.h"
#include "engine/render/rhi/draw_executor_common.h"

#include <emscripten/html5_webgpu.h>

#include <cstring>

namespace dse {
namespace render {

namespace {

/// B0 占位命令缓冲：录制接口为 no-op，由 B1 起接入真实 WebGPU 录制
/// （RenderPassEncoder / RenderPipeline / BindGroup）。所有接口显式实现，
/// 不依赖基类静默默认，避免漏实现时无声吞掉绘制。
class WebGPUCommandBuffer final : public CommandBuffer {
public:
    explicit WebGPUCommandBuffer(WebGPURhiDevice* device) : device_(device) { (void)device_; }

    void BeginRenderPass(const RenderPassDesc& render_pass) override { (void)render_pass; }
    void EndRenderPass() override {}
    void ClearColor(const glm::vec4& color) override { (void)color; }
    void SetGlobalMat4(const std::string& name, const glm::mat4& value) override { (void)name; (void)value; }
    void SetViewport(int x, int y, int width, int height) override { (void)x; (void)y; (void)width; (void)height; }

    void BindGlobalShadowMap(unsigned int index, unsigned int texture_handle) override { (void)index; (void)texture_handle; }
    void BindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) override { (void)index; (void)texture_handle; }
    void BindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) override { (void)index; (void)texture_handle; }

    void BindPipeline(unsigned int graphics_pipeline_handle) override { (void)graphics_pipeline_handle; }
    void BindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                          const std::vector<VertexAttr>& attrs,
                          VertexInputRate rate) override {
        (void)slot; (void)buffer_handle; (void)stride; (void)attrs; (void)rate;
    }
    void PushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) override {
        (void)stage; (void)offset; (void)data; (void)size;
    }
    void Draw(uint32_t vertex_count, uint32_t first_vertex) override { (void)vertex_count; (void)first_vertex; }

    void BindIndexBuffer(unsigned int buffer_handle, IndexType type) override { (void)buffer_handle; (void)type; }
    void BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) override {
        (void)slot; (void)texture_handle; (void)dim;
    }
    void BindUniformBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) override {
        (void)slot; (void)buffer_handle; (void)offset; (void)size;
    }
    void BindStorageBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) override {
        (void)slot; (void)buffer_handle; (void)offset; (void)size;
    }
    void DrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) override {
        (void)index_count; (void)first_index; (void)base_vertex;
    }
    void DispatchComputePass(const ComputeDispatch& dispatch) override { (void)dispatch; }
    void DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                              uint32_t first_index, int32_t base_vertex,
                              uint32_t first_instance) override {
        (void)index_count; (void)instance_count; (void)first_index; (void)base_vertex; (void)first_instance;
    }
    void DrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) override {
        (void)indirect_buffer; (void)byte_offset;
    }

private:
    WebGPURhiDevice* device_;
};

} // namespace

WebGPURhiDevice::WebGPURhiDevice() = default;

WebGPURhiDevice::~WebGPURhiDevice() {
    Shutdown();
}

RenderDeviceInfo WebGPURhiDevice::GetDeviceInfo() const {
    RenderDeviceInfo info;
    info.adapter_name = "WebGPU";
    info.is_software = false;  // 实际软/硬由浏览器适配器决定；B5 经 adapter info 精确填充
    return info;
}

bool WebGPURhiDevice::AcquireDevice() {
    // 设备由 JS 侧（shell.html）经 navigator.gpu.requestAdapter().requestDevice()
    // 预创建并挂到 Module.preinitializedWebGPUDevice；此处同步取得其 C 句柄。
    device_ = emscripten_webgpu_get_device();
    if (!device_) {
        DEBUG_LOG_WARN("WebGPU: emscripten_webgpu_get_device() 返回空 —— JS 侧未预创建设备，"
                       "上层将回退 WebGL2(OpenGL) 后端");
        return false;
    }
    queue_ = wgpuDeviceGetQueue(device_);
    if (!queue_) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceGetQueue 失败");
        return false;
    }
    return true;
}

bool WebGPURhiDevice::CreateSwapChain(int width, int height) {
    if (!device_) return false;
    if (!instance_) {
        instance_ = wgpuCreateInstance(nullptr);
    }
    if (!surface_) {
        // GLFW(Emscripten) 默认渲染到 Module.canvas（HTML 选择器 "#canvas"）。
        WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc{};
        canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
        canvas_desc.selector = "#canvas";
        WGPUSurfaceDescriptor surf_desc{};
        surf_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&canvas_desc);
        surface_ = wgpuInstanceCreateSurface(instance_, &surf_desc);
        if (!surface_) {
            DEBUG_LOG_ERROR("WebGPU: wgpuInstanceCreateSurface 失败");
            return false;
        }
    }

    ReleaseSwapChain();

    WGPUSwapChainDescriptor sc_desc{};
    sc_desc.usage = WGPUTextureUsage_RenderAttachment;
    sc_desc.format = swapchain_format_;
    sc_desc.width = static_cast<uint32_t>(width > 0 ? width : 1);
    sc_desc.height = static_cast<uint32_t>(height > 0 ? height : 1);
    sc_desc.presentMode = WGPUPresentMode_Fifo;
    swapchain_ = wgpuDeviceCreateSwapChain(device_, surface_, &sc_desc);
    if (!swapchain_) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateSwapChain 失败 ({}x{})", width, height);
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}

void WebGPURhiDevice::ReleaseSwapChain() {
    if (swapchain_) {
        wgpuSwapChainRelease(swapchain_);
        swapchain_ = nullptr;
    }
}

bool WebGPURhiDevice::InitDevice(void* window_handle, int width, int height) {
    (void)window_handle;
    if (initialized_) return true;
    if (!AcquireDevice()) return false;
    if (!CreateSwapChain(width, height)) return false;
    initialized_ = true;
    DEBUG_LOG_INFO("WebGPU 后端初始化成功 ({}x{}, 交换链格式=0x{:x})",
                   width, height, static_cast<unsigned int>(swapchain_format_));
    return true;
}

void WebGPURhiDevice::OnWindowResized(int width, int height) {
    if (!initialized_) return;
    if (width == width_ && height == height_) return;
    CreateSwapChain(width, height);
}

void WebGPURhiDevice::Shutdown() {
    ReleaseSwapChain();
    if (surface_) { wgpuSurfaceRelease(surface_); surface_ = nullptr; }
    if (queue_)   { wgpuQueueRelease(queue_);     queue_ = nullptr; }
    if (device_)  { wgpuDeviceRelease(device_);   device_ = nullptr; }
    initialized_ = false;
}

void WebGPURhiDevice::WaitIdle() {
    // WebGPU 无显式 device idle 等待；提交后由浏览器调度。B3 起按需用 onSubmittedWorkDone。
}

void WebGPURhiDevice::BeginFrame() {
    last_frame_stats_ = RenderStats{};
    if (!initialized_ || !swapchain_) return;
    backbuffer_view_ = wgpuSwapChainGetCurrentTextureView(swapchain_);
    if (!backbuffer_view_) return;
    frame_encoder_ = wgpuDeviceCreateCommandEncoder(device_, nullptr);
}

void WebGPURhiDevice::EndFrame() {
    if (!initialized_ || !backbuffer_view_ || !frame_encoder_) {
        if (backbuffer_view_) { wgpuTextureViewRelease(backbuffer_view_); backbuffer_view_ = nullptr; }
        if (frame_encoder_)   { wgpuCommandEncoderRelease(frame_encoder_); frame_encoder_ = nullptr; }
        return;
    }

    // B0：录制一个 clear 渲染 pass（证明设备/交换链/队列贯通）。B1 起在此 pass
    // 内重放 WebGPUCommandBuffer 录制的真实绘制。
    WGPURenderPassColorAttachment color_att{};
    color_att.view = backbuffer_view_;
    color_att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color_att.resolveTarget = nullptr;
    color_att.loadOp = WGPULoadOp_Clear;
    color_att.storeOp = WGPUStoreOp_Store;
    color_att.clearValue = WGPUColor{0.05, 0.05, 0.08, 1.0};

    WGPURenderPassDescriptor pass_desc{};
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_att;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(frame_encoder_, &pass_desc);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(frame_encoder_, nullptr);
    wgpuQueueSubmit(queue_, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    wgpuCommandEncoderRelease(frame_encoder_);
    frame_encoder_ = nullptr;
    wgpuTextureViewRelease(backbuffer_view_);
    backbuffer_view_ = nullptr;

    last_frame_stats_.render_passes = 1;
}

void WebGPURhiDevice::PresentFrame() {
    // Emscripten 下 swapchain present 由浏览器在 rAF 自动呈现，wgpuSwapChainPresent
    // 为 no-op；保留调用以对齐桌面 Dawn 语义。
    if (swapchain_) wgpuSwapChainPresent(swapchain_);
}

// ============================================================
// B0 占位资源接口（B1 起替换为真实 WebGPU 资源映射）
// ============================================================

unsigned int WebGPURhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    (void)desc;
    return NextHandle();
}

void WebGPURhiDevice::DeleteRenderTarget(unsigned int render_target_handle) {
    (void)render_target_handle;
}

unsigned int WebGPURhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    (void)render_target_handle;
    return 0;
}

unsigned int WebGPURhiDevice::GetRenderTargetDepthTexture(unsigned int render_target_handle) const {
    (void)render_target_handle;
    return 0;
}

std::vector<unsigned char> WebGPURhiDevice::ReadRenderTargetColorRgba8(unsigned int render_target_handle) const {
    (void)render_target_handle;
    return {};
}

RenderTargetReadback WebGPURhiDevice::ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const {
    (void)render_target_handle;
    return {};
}

unsigned int WebGPURhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    (void)width; (void)height; (void)rgba8_data; (void)linear_filter;
    return NextHandle();
}

unsigned int WebGPURhiDevice::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    (void)width; (void)height; (void)rgba8_faces; (void)linear_filter;
    return NextHandle();
}

unsigned int WebGPURhiDevice::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) {
    (void)width; (void)height; (void)depth; (void)rgba8_data; (void)linear_filter;
    return NextHandle();
}

void WebGPURhiDevice::DeleteTexture(unsigned int texture_handle) {
    (void)texture_handle;
}

unsigned int WebGPURhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
    (void)vert_src; (void)frag_src;
    return NextHandle();
}

void WebGPURhiDevice::DeleteShaderProgram(unsigned int program_handle) {
    (void)program_handle;
}

unsigned int WebGPURhiDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    (void)desc;
    return NextHandle();
}

unsigned int WebGPURhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    (void)size; (void)data; (void)is_dynamic; (void)is_index;
    return NextHandle();
}

void WebGPURhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    (void)handle; (void)offset; (void)size; (void)data; (void)is_index;
}

void WebGPURhiDevice::DeleteBuffer(unsigned int handle) {
    (void)handle;
}

VertexArrayHandle WebGPURhiDevice::CreateVertexArray() {
    return VertexArrayHandle{NextHandle()};
}

void WebGPURhiDevice::DeleteVertexArray(VertexArrayHandle handle) {
    (void)handle;
}

std::shared_ptr<CommandBuffer> WebGPURhiDevice::CreateCommandBuffer() {
    return std::make_shared<WebGPUCommandBuffer>(this);
}

void WebGPURhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    // B0：录制为 no-op，提交无操作；clear pass 在 EndFrame 统一提交。B1 起重放录制。
    (void)cmd_buffer;
}

const RenderStats& WebGPURhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

glm::mat4 WebGPURhiDevice::GetProjectionCorrection() const {
    // WebGPU NDC：Y-up、Z∈[0,1]（同 D3D12/Metal）。从引擎默认 GL 约定（Z∈[-1,1]）
    // 重映射 Z 到 [0,1]；Y 不翻转（WebGPU 帧缓冲 Y-up）。
    glm::mat4 m(1.0f);
    m[2][2] = 0.5f;
    m[3][2] = 0.5f;
    return m;
}

glm::mat4 WebGPURhiDevice::GetShadowSampleCorrection() const {
    // 与投影矫正同源但不含 Z 重映射（着色器内统一把 Z 从 [-1,1] 映到 [0,1]）。
    return glm::mat4(1.0f);
}

} // namespace render
} // namespace dse

#endif // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
