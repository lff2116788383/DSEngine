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

// --- B1 资源映射小工具 ---

constexpr uint64_t AlignUp4(uint64_t n) { return (n + 3u) & ~static_cast<uint64_t>(3u); }

WGPUAddressMode ToAddressMode(TextureWrap w) {
    return w == TextureWrap::ClampToEdge ? WGPUAddressMode_ClampToEdge : WGPUAddressMode_Repeat;
}
WGPUFilterMode ToFilterMode(TextureFilter f) {
    return f == TextureFilter::Linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
}

/// 全 mip 链层数（2D 维度，向下取整 log2(max(w,h))+1）。
uint32_t FullMipCount(uint32_t w, uint32_t h) {
    uint32_t m = (w > h ? w : h);
    uint32_t levels = 1;
    while (m > 1) { m >>= 1; ++levels; }
    return levels;
}

/// 向 mipLevel=0..1 的 2D 纹理写入一层 RGBA8 数据（origin.z 指定 cube 面 / 3D 切片）。
void WriteTextureLayerRGBA8(WGPUQueue queue, WGPUTexture tex, uint32_t mip_level,
                            uint32_t width, uint32_t height, uint32_t z,
                            const unsigned char* rgba8) {
    if (!rgba8) return;
    WGPUImageCopyTexture dst{};
    dst.texture = tex;
    dst.mipLevel = mip_level;
    dst.origin = WGPUOrigin3D{0, 0, z};
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout{};
    layout.offset = 0;
    layout.bytesPerRow = width * 4u;
    layout.rowsPerImage = height;
    WGPUExtent3D extent{width, height, 1u};
    wgpuQueueWriteTexture(queue, &dst, rgba8, static_cast<size_t>(width) * height * 4u, &layout, &extent);
}

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
    // 先释放所有资源对象，再释放交换链/队列/设备。
    for (auto& [h, rt] : render_targets_) {
        (void)h;
        for (unsigned int th : rt.color_textures) {
            auto it = textures_.find(th);
            if (it != textures_.end()) { DestroyTextureEntry(it->second); textures_.erase(it); }
        }
        if (rt.depth_texture) {
            auto it = textures_.find(rt.depth_texture);
            if (it != textures_.end()) { DestroyTextureEntry(it->second); textures_.erase(it); }
        }
    }
    render_targets_.clear();
    for (auto& [h, e] : textures_) { (void)h; DestroyTextureEntry(e); }
    textures_.clear();
    for (auto& [h, e] : buffers_) { (void)h; if (e.buffer) wgpuBufferRelease(e.buffer); }
    buffers_.clear();
    for (auto& [h, e] : shaders_) { (void)h; if (e.module) wgpuShaderModuleRelease(e.module); }
    shaders_.clear();
    pipeline_states_.clear();

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
// B1 资源映射：真实 WebGPU 缓冲 / 纹理 / 采样器 / 渲染目标
// ============================================================
//
// 句柄表（buffers_/textures_/render_targets_/pipeline_states_/shaders_）把后端无关的
// unsigned int 句柄映射到原生 WGPU 对象。命令缓冲录制（B2，依赖 WGSL 着色器与
// WGPURenderPipeline 组装）在此基础上经 FindBuffer/FindTexture 解析句柄发起绘制。

// --- 内部助手 ---

WGPUSampler WebGPURhiDevice::CreateSampler(const TextureSamplerDesc& desc, uint32_t mip_levels) const {
    WGPUSamplerDescriptor sd{};
    const WGPUAddressMode am = ToAddressMode(desc.wrap);
    sd.addressModeU = am; sd.addressModeV = am; sd.addressModeW = am;
    const WGPUFilterMode fm = ToFilterMode(desc.filter);
    sd.magFilter = fm; sd.minFilter = fm;
    sd.mipmapFilter = (desc.filter == TextureFilter::Linear) ? WGPUMipmapFilterMode_Linear
                                                             : WGPUMipmapFilterMode_Nearest;
    sd.lodMinClamp = 0.0f;
    sd.lodMaxClamp = (mip_levels > 1) ? static_cast<float>(mip_levels) : 32.0f;
    sd.maxAnisotropy = 1;
    sd.compare = WGPUCompareFunction_Undefined;  // 比较采样器（阴影 PCF）由 B2 按需另建
    return wgpuDeviceCreateSampler(device_, &sd);
}

void WebGPURhiDevice::DestroyTextureEntry(TextureEntry& e) {
    if (e.sampler) { wgpuSamplerRelease(e.sampler); e.sampler = nullptr; }
    if (e.view)    { wgpuTextureViewRelease(e.view); e.view = nullptr; }
    if (e.texture && e.owns_texture) { wgpuTextureRelease(e.texture); }
    e.texture = nullptr;
}

unsigned int WebGPURhiDevice::CreateTextureImpl(
        WGPUTextureDimension dim, WGPUTextureViewDimension view_dim,
        uint32_t width, uint32_t height, uint32_t depth_or_layers,
        WGPUTextureFormat format, WGPUTextureUsageFlags usage,
        uint32_t mip_levels, int msaa_samples,
        const std::vector<const unsigned char*>& layer_data,
        const TextureSamplerDesc& sampler) {
    if (!device_) return 0;
    width = width > 0 ? width : 1;
    height = height > 0 ? height : 1;
    depth_or_layers = depth_or_layers > 0 ? depth_or_layers : 1;
    mip_levels = mip_levels > 0 ? mip_levels : 1;

    WGPUTextureDescriptor td{};
    td.usage = usage;
    td.dimension = dim;
    td.size = WGPUExtent3D{width, height, depth_or_layers};
    td.format = format;
    td.mipLevelCount = mip_levels;
    td.sampleCount = static_cast<uint32_t>(msaa_samples > 1 ? msaa_samples : 1);
    WGPUTexture tex = wgpuDeviceCreateTexture(device_, &td);
    if (!tex) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateTexture 失败 ({}x{}x{} fmt=0x{:x})",
                        width, height, depth_or_layers, static_cast<unsigned int>(format));
        return 0;
    }

    // mip0 上传（各层 RGBA8 紧打包；nullptr 层跳过，供 RT 附件 / 后续生成）。
    for (uint32_t z = 0; z < layer_data.size() && z < depth_or_layers; ++z) {
        WriteTextureLayerRGBA8(queue_, tex, 0, width, height, z, layer_data[z]);
    }

    WGPUTextureViewDescriptor vd{};
    vd.format = format;
    vd.dimension = view_dim;
    vd.baseMipLevel = 0;
    vd.mipLevelCount = mip_levels;
    vd.baseArrayLayer = 0;
    vd.arrayLayerCount = (dim == WGPUTextureDimension_3D) ? 1u : depth_or_layers;
    vd.aspect = WGPUTextureAspect_All;
    WGPUTextureView view = wgpuTextureCreateView(tex, &vd);

    TextureEntry e;
    e.texture = tex;
    e.view = view;
    e.sampler = CreateSampler(sampler, mip_levels);
    e.format = format;
    e.view_dim = view_dim;
    e.width = width; e.height = height;
    e.depth = (dim == WGPUTextureDimension_3D) ? depth_or_layers : 1;
    e.array_layers = (dim == WGPUTextureDimension_3D) ? 1 : depth_or_layers;
    e.mip_levels = mip_levels;
    e.msaa_samples = msaa_samples;
    e.owns_texture = true;

    const unsigned int h = NextHandle();
    textures_[h] = e;
    return h;
}

// --- 查表 ---

const WebGPURhiDevice::BufferEntry* WebGPURhiDevice::FindBuffer(unsigned int handle) const {
    auto it = buffers_.find(handle);
    return it != buffers_.end() ? &it->second : nullptr;
}
const WebGPURhiDevice::TextureEntry* WebGPURhiDevice::FindTexture(unsigned int handle) const {
    auto it = textures_.find(handle);
    return it != textures_.end() ? &it->second : nullptr;
}
const WebGPURhiDevice::RenderTargetEntry* WebGPURhiDevice::FindRenderTarget(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? &it->second : nullptr;
}
const PipelineStateDesc* WebGPURhiDevice::FindPipelineState(unsigned int handle) const {
    auto it = pipeline_states_.find(handle);
    return it != pipeline_states_.end() ? &it->second : nullptr;
}

// --- 渲染目标 ---

unsigned int WebGPURhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    if (!device_) return 0;
    RenderTargetEntry rt;
    rt.width = desc.width > 0 ? desc.width : 1;
    rt.height = desc.height > 0 ? desc.height : 1;
    rt.is_cube = desc.cube_map;
    // 注：MSAA 解析（多重采样颜色 → 单采样可采样纹理）在 B2 渲染 pass 组装时落地；
    //     B1 先以单采样附件成形资源结构（多重采样纹理不可直接 TextureBinding 采样）。
    rt.msaa_samples = 1;

    const TextureSamplerDesc rt_sampler{TextureFilter::Linear, TextureWrap::ClampToEdge};

    if (desc.has_color) {
        WGPUTextureUsageFlags color_usage = WGPUTextureUsage_RenderAttachment |
                                            WGPUTextureUsage_TextureBinding |
                                            WGPUTextureUsage_CopySrc;
        if (desc.allow_uav) color_usage |= WGPUTextureUsage_StorageBinding;
        // 场景颜色用 HDR RGBA16F，与桌面/GL 后端 color=RGBA16F 一致。
        const WGPUTextureFormat color_fmt = WGPUTextureFormat_RGBA16Float;
        const uint32_t mips = desc.generate_mipmaps ? FullMipCount(rt.width, rt.height) : 1;
        const int n = desc.color_attachment_count > 0 ? desc.color_attachment_count : 1;
        for (int i = 0; i < n; ++i) {
            unsigned int th = desc.cube_map
                ? CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
                                    rt.width, rt.height, 6, color_fmt, color_usage, mips, 1, {}, rt_sampler)
                : CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                                    rt.width, rt.height, 1, color_fmt, color_usage, mips, 1, {}, rt_sampler);
            if (th) rt.color_textures.push_back(th);
        }
    }

    if (desc.has_depth) {
        const WGPUTextureUsageFlags depth_usage = WGPUTextureUsage_RenderAttachment |
                                                  WGPUTextureUsage_TextureBinding;
        const WGPUTextureFormat depth_fmt = WGPUTextureFormat_Depth32Float;
        rt.depth_texture = desc.cube_map
            ? CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
                                rt.width, rt.height, 6, depth_fmt, depth_usage, 1, 1, {}, rt_sampler)
            : CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                                rt.width, rt.height, 1, depth_fmt, depth_usage, 1, 1, {}, rt_sampler);
    }

    const unsigned int h = NextHandle();
    render_targets_[h] = std::move(rt);
    return h;
}

void WebGPURhiDevice::DeleteRenderTarget(unsigned int render_target_handle) {
    auto it = render_targets_.find(render_target_handle);
    if (it == render_targets_.end()) return;
    for (unsigned int th : it->second.color_textures) {
        auto te = textures_.find(th);
        if (te != textures_.end()) { DestroyTextureEntry(te->second); textures_.erase(te); }
    }
    if (it->second.depth_texture) {
        auto te = textures_.find(it->second.depth_texture);
        if (te != textures_.end()) { DestroyTextureEntry(te->second); textures_.erase(te); }
    }
    render_targets_.erase(it);
}

unsigned int WebGPURhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    return GetRenderTargetColorTexture(render_target_handle, 0);
}

unsigned int WebGPURhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const {
    const RenderTargetEntry* rt = FindRenderTarget(render_target_handle);
    if (!rt || index < 0 || static_cast<size_t>(index) >= rt->color_textures.size()) return 0;
    return rt->color_textures[index];
}

unsigned int WebGPURhiDevice::GetRenderTargetDepthTexture(unsigned int render_target_handle) const {
    const RenderTargetEntry* rt = FindRenderTarget(render_target_handle);
    return rt ? rt->depth_texture : 0;
}

std::vector<unsigned char> WebGPURhiDevice::ReadRenderTargetColorRgba8(unsigned int render_target_handle) const {
    return ReadRenderTargetColorRgba8WithSize(render_target_handle).pixels;
}

RenderTargetReadback WebGPURhiDevice::ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const {
    // WebGPU 的 GPU→CPU 回读是异步的（texture→staging buffer copy + mapAsync）。在浏览器
    // 主线程同步返回需 ASYNCIFY，B1 不启用。回读供桌面编辑器/CI 像素校验用，Web 运行期渲染
    // 不依赖它；headless WebGPU 回读回归在 B5（Dawn 软件适配器 + onSubmittedWorkDone）落地。
    (void)render_target_handle;
    return {};
}

// --- 纹理 ---

unsigned int WebGPURhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    return CreateTexture2D(width, height, rgba8_data, TextureSamplerDesc::FromLinearFlag(linear_filter));
}

unsigned int WebGPURhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                              const TextureSamplerDesc& sampler) {
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding |
                                        WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;
    return CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                             static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1,
                             WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, {rgba8_data}, sampler);
}

unsigned int WebGPURhiDevice::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    std::vector<const unsigned char*> faces(6, nullptr);
    if (rgba8_faces) for (int i = 0; i < 6; ++i) faces[i] = rgba8_faces[i];
    return CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
                             static_cast<uint32_t>(width), static_cast<uint32_t>(height), 6,
                             WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, faces,
                             TextureSamplerDesc::FromLinearFlag(linear_filter));
}

unsigned int WebGPURhiDevice::CreateTextureCubeWithMips(const std::vector<CubeMipLevel>& mips, bool linear_filter) {
    if (mips.empty()) return 0;
    const uint32_t mip_count = static_cast<uint32_t>(mips.size());
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    // 先建带完整 mip 链的 cube（不在 Impl 内上传），再逐 mip 逐面写入。
    const unsigned int h = CreateTextureImpl(
        WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
        static_cast<uint32_t>(mips[0].width), static_cast<uint32_t>(mips[0].height), 6,
        WGPUTextureFormat_RGBA8Unorm, usage, mip_count, 1, {},
        TextureSamplerDesc::FromLinearFlag(linear_filter));
    const TextureEntry* e = FindTexture(h);
    if (!e || !e->texture) return h;
    for (uint32_t m = 0; m < mip_count; ++m) {
        const uint32_t w = static_cast<uint32_t>(mips[m].width > 0 ? mips[m].width : 1);
        const uint32_t ht = static_cast<uint32_t>(mips[m].height > 0 ? mips[m].height : 1);
        for (uint32_t f = 0; f < 6; ++f) {
            WriteTextureLayerRGBA8(queue_, e->texture, m, w, ht, f, mips[m].faces[f]);
        }
    }
    return h;
}

unsigned int WebGPURhiDevice::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) {
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    const unsigned int h = CreateTextureImpl(
        WGPUTextureDimension_3D, WGPUTextureViewDimension_3D,
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(depth),
        WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, {},
        TextureSamplerDesc::FromLinearFlag(linear_filter));
    const TextureEntry* e = FindTexture(h);
    if (e && e->texture && rgba8_data && width > 0 && height > 0 && depth > 0) {
        // 3D 体一次性整卷上传（rowsPerImage=height，writeSize.depth=depth）。
        WGPUImageCopyTexture dst{};
        dst.texture = e->texture;
        dst.mipLevel = 0;
        dst.origin = WGPUOrigin3D{0, 0, 0};
        dst.aspect = WGPUTextureAspect_All;
        WGPUTextureDataLayout layout{};
        layout.offset = 0;
        layout.bytesPerRow = static_cast<uint32_t>(width) * 4u;
        layout.rowsPerImage = static_cast<uint32_t>(height);
        WGPUExtent3D extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                            static_cast<uint32_t>(depth)};
        wgpuQueueWriteTexture(queue_, &dst, rgba8_data,
                              static_cast<size_t>(width) * height * depth * 4u, &layout, &extent);
    }
    return h;
}

void WebGPURhiDevice::DeleteTexture(unsigned int texture_handle) {
    auto it = textures_.find(texture_handle);
    if (it == textures_.end()) return;
    DestroyTextureEntry(it->second);
    textures_.erase(it);
}

// --- 着色器 / 管线状态 ---

unsigned int WebGPURhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
    // B1：暂存 GLSL 源；WGSL 转译 + WGPUShaderModule 创建在 B2（接 Tint 或 SPIR-V→WGSL）。
    ShaderEntry e;
    e.vert_src = vert_src;
    e.frag_src = frag_src;
    const unsigned int h = NextHandle();
    shaders_[h] = std::move(e);
    return h;
}

void WebGPURhiDevice::DeleteShaderProgram(unsigned int program_handle) {
    auto it = shaders_.find(program_handle);
    if (it == shaders_.end()) return;
    if (it->second.module) wgpuShaderModuleRelease(it->second.module);
    shaders_.erase(it);
}

unsigned int WebGPURhiDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    // B1：登记 PSO 子状态（光栅/混合/深度/拓扑）。WGPURenderPipeline 在 B2 由
    // (pso, program, RT 颜色/深度格式, 顶点布局) 惰性组装并缓存（着色器就绪后）。
    const unsigned int h = NextHandle();
    pipeline_states_[h] = desc;
    return h;
}

unsigned int WebGPURhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    if (!device_ || size == 0) return 0;
    (void)is_dynamic;  // WebGPU 缓冲无静/动态区分；动态更新经 wgpuQueueWriteBuffer。
    const uint64_t alloc = AlignUp4(size);
    WGPUBufferUsageFlags usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    if (is_index) {
        usage |= WGPUBufferUsage_Index;
    } else {
        // 非索引缓冲可能用作顶点流或 uniform，同时授予两种 usage（WebGPU 允许组合）。
        usage |= WGPUBufferUsage_Vertex | WGPUBufferUsage_Uniform;
    }

    WGPUBufferDescriptor bd{};
    bd.usage = usage;
    bd.size = alloc;
    bd.mappedAtCreation = (data != nullptr);
    WGPUBuffer buf = wgpuDeviceCreateBuffer(device_, &bd);
    if (!buf) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateBuffer 失败 (size={})", static_cast<unsigned long long>(alloc));
        return 0;
    }
    if (data) {
        void* mapped = wgpuBufferGetMappedRange(buf, 0, alloc);
        if (mapped) std::memcpy(mapped, data, size);
        wgpuBufferUnmap(buf);
    }

    BufferEntry e;
    e.buffer = buf;
    e.size = alloc;
    e.logical_size = size;
    e.usage = usage;
    e.is_index = is_index;
    const unsigned int h = NextHandle();
    buffers_[h] = e;
    return h;
}

void WebGPURhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    (void)is_index;
    auto it = buffers_.find(handle);
    if (it == buffers_.end() || !data || size == 0) return;
    const BufferEntry& e = it->second;
    // wgpuQueueWriteBuffer 要求 offset 与 size 均为 4 的倍数。常规调用方已 4 对齐；
    // 否则把 size 向上取整到 4（缓冲分配已对齐，越界由下方 clamp 兜底）。
    if (offset % 4 != 0) {
        DEBUG_LOG_WARN("WebGPU UpdateBuffer: offset {} 非 4 对齐，跳过更新", static_cast<unsigned long long>(offset));
        return;
    }
    const uint64_t write_size = AlignUp4(size);
    if (offset + write_size > e.size) return;
    if (write_size == size) {
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, data, size);
    } else {
        std::vector<uint8_t> padded(write_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, padded.data(), write_size);
    }
}

void WebGPURhiDevice::DeleteBuffer(unsigned int handle) {
    auto it = buffers_.find(handle);
    if (it == buffers_.end()) return;
    if (it->second.buffer) wgpuBufferRelease(it->second.buffer);
    buffers_.erase(it);
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
