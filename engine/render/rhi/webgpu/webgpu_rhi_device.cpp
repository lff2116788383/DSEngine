/**
 * @file webgpu_rhi_device.cpp
 * @brief WebGPU RHI 后端实现（B0 骨架）。详见头文件。
 */

#include "engine/render/rhi/webgpu/webgpu_rhi_device.h"

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/base/debug.h"
#include "engine/render/rhi/draw_executor_common.h"

#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

#include <cstring>

namespace dse {
namespace render {

namespace {

/// B2 命令缓冲：把后端无关的录制接口逐调用转发到 WebGPURhiDevice 的设备级 Cmd*，
/// 由设备直接录制到本帧 frame_encoder_（立即转发，非缓存重放）。所有接口显式实现，
/// 不依赖基类静默默认，避免漏实现时无声吞掉绘制。ClearColor/SetGlobalMat4/三类 ShadowMap/
/// DispatchComputePass/DrawIndexedIndirect 转发到 B2 期保持 no-op 的 Cmd*（留 B3）。
class WebGPUCommandBuffer final : public CommandBuffer {
public:
    explicit WebGPUCommandBuffer(WebGPURhiDevice* device) : device_(device) {}

    void BeginRenderPass(const RenderPassDesc& render_pass) override { device_->CmdBeginRenderPass(render_pass); }
    void EndRenderPass() override { device_->CmdEndRenderPass(); }
    void ClearColor(const glm::vec4& color) override { device_->CmdClearColor(color); }
    void SetGlobalMat4(const std::string& name, const glm::mat4& value) override { device_->CmdSetGlobalMat4(name, value); }
    void SetViewport(int x, int y, int width, int height) override { device_->CmdSetViewport(x, y, width, height); }

    void BindGlobalShadowMap(unsigned int index, unsigned int texture_handle) override { device_->CmdBindGlobalShadowMap(index, texture_handle); }
    void BindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) override { device_->CmdBindGlobalSpotShadowMap(index, texture_handle); }
    void BindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) override { device_->CmdBindGlobalPointShadowMap(index, texture_handle); }

    void BindPipeline(unsigned int graphics_pipeline_handle) override { device_->CmdBindPipeline(graphics_pipeline_handle); }
    void BindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                          const std::vector<VertexAttr>& attrs,
                          VertexInputRate rate) override {
        device_->CmdBindVertexBuffer(slot, buffer_handle, stride, attrs, rate);
    }
    void PushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) override {
        device_->CmdPushConstants(stage, offset, data, size);
    }
    void Draw(uint32_t vertex_count, uint32_t first_vertex) override { device_->CmdDraw(vertex_count, first_vertex); }

    void BindIndexBuffer(unsigned int buffer_handle, IndexType type) override { device_->CmdBindIndexBuffer(buffer_handle, type); }
    void BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) override {
        device_->CmdBindTexture(slot, texture_handle, dim);
    }
    void BindUniformBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) override {
        device_->CmdBindUniformBuffer(slot, buffer_handle, offset, size);
    }
    void BindStorageBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) override {
        device_->CmdBindStorageBuffer(slot, buffer_handle, offset, size);
    }
    void DrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) override {
        device_->CmdDrawIndexed(index_count, first_index, base_vertex);
    }
    void DispatchComputePass(const ComputeDispatch& dispatch) override { device_->CmdDispatchComputePass(dispatch); }
    void DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                              uint32_t first_index, int32_t base_vertex,
                              uint32_t first_instance) override {
        device_->CmdDrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
    }
    void DrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) override {
        device_->CmdDrawIndexedIndirect(indirect_buffer, byte_offset);
    }

private:
    WebGPURhiDevice* device_;
};

// --- B1 资源映射小工具 ---

constexpr uint64_t AlignUp4(uint64_t n) { return (n + 3u) & ~static_cast<uint64_t>(3u); }

/// 解析 WGSL 源中实际声明的 `@group(N) @binding(M)`，填入 out（key=(group<<16)|binding）。
/// 供 explicit pipeline-layout/BindGroup 过滤：仅纳入着色器真正使用的绑定，避免引擎多绑资源
/// 超 per-stage 上限 / 与着色器用量不符。render（vs/fs）与 compute 程序共用此解析。
void ParseWgslBindings(const std::string& src, std::set<uint32_t>& out) {
    for (size_t pos = src.find("@group("); pos != std::string::npos;
         pos = src.find("@group(", pos + 1)) {
        const size_t g0 = pos + 7;
        const size_t g1 = src.find(')', g0);
        if (g1 == std::string::npos) break;
        const size_t bpos = src.find("@binding(", g1);
        if (bpos == std::string::npos) break;
        // @binding 须紧随同一声明（其间只允许空白），否则视为不同声明。
        if (src.find_first_not_of(" \t\r\n", g1 + 1) != bpos) continue;
        const size_t b0 = bpos + 9;
        const size_t b1 = src.find(')', b0);
        if (b1 == std::string::npos) break;
        const uint32_t group = static_cast<uint32_t>(std::strtoul(src.c_str() + g0, nullptr, 10));
        const uint32_t binding = static_cast<uint32_t>(std::strtoul(src.c_str() + b0, nullptr, 10));
        out.insert((group << 16) | binding);
    }
}

// --- B3a compute 自检：异步回读校验上下文 ---
// 自检 compute 着色器把 outbuf[i] = i*2 + base（i<N），并把 indirect DrawCmd 写成
// {count=36, instance=1, first=0, base_vertex=0, base_instance=0}。两路 copy 到 MapRead
// 缓冲后各发起一次 wgpuBufferMapAsync，回调里逐元素校验。pending 计数归零后汇总并释放缓冲。
constexpr uint32_t kCtN = 64;       ///< 输出 SSBO 元素数（= 1 个 workgroup × 64 线程）
constexpr uint32_t kCtBase = 100u;  ///< 输出值偏置（验证 UBO 参数确实进入 compute）
constexpr uint32_t kCtDrawWords = 5;///< DrawCmd 字数（count/instance/first/base_vertex/base_instance）

struct ComputeSelfTestCtx {
    WGPUBuffer rb_out = nullptr;
    WGPUBuffer rb_draw = nullptr;
    int pending = 2;
    bool out_ok = false;
    bool draw_ok = false;
};

void FinalizeComputeSelfTest(ComputeSelfTestCtx* ctx) {
    if (--ctx->pending > 0) return;
    if (ctx->out_ok && ctx->draw_ok) {
        DEBUG_LOG_INFO("WebGPU[B3a] compute 自检 PASS：SSBO 输出（n={}）与 indirect DrawCmd 均符合预期", kCtN);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3a] compute 自检 FAIL：out_ok={} draw_ok={}", ctx->out_ok, ctx->draw_ok);
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    if (ctx->rb_draw) wgpuBufferRelease(ctx->rb_draw);
    delete ctx;
}

void OnComputeSelfTestOutMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<ComputeSelfTestCtx*>(userdata);
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint32_t* p = static_cast<const uint32_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kCtN * sizeof(uint32_t)));
        if (p) {
            bool ok = true;
            for (uint32_t i = 0; i < kCtN; ++i) {
                if (p[i] != i * 2u + kCtBase) { ok = false; break; }
            }
            ctx->out_ok = ok;
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3a] compute 自检：输出 SSBO 回读映射失败 status={}", static_cast<int>(status));
    }
    FinalizeComputeSelfTest(ctx);
}

void OnComputeSelfTestDrawMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<ComputeSelfTestCtx*>(userdata);
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint32_t* p = static_cast<const uint32_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_draw, 0, kCtDrawWords * sizeof(uint32_t)));
        if (p) {
            ctx->draw_ok = (p[0] == 36u && p[1] == 1u && p[2] == 0u && p[3] == 0u && p[4] == 0u);
        }
        wgpuBufferUnmap(ctx->rb_draw);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3a] compute 自检：indirect 回读映射失败 status={}", static_cast<int>(status));
    }
    FinalizeComputeSelfTest(ctx);
}

// --- B2 PSO/顶点格式 → WebGPU 枚举映射 ---

WGPUVertexFormat ToVertexFormat(uint32_t components) {
    switch (components) {
        case 1:  return WGPUVertexFormat_Float32;
        case 2:  return WGPUVertexFormat_Float32x2;
        case 3:  return WGPUVertexFormat_Float32x3;
        default: return WGPUVertexFormat_Float32x4;
    }
}

WGPUTextureViewDimension ToViewDimension(TextureDim dim) {
    switch (dim) {
        case TextureDim::TexCube:    return WGPUTextureViewDimension_Cube;
        case TextureDim::Tex2DArray: return WGPUTextureViewDimension_2DArray;
        case TextureDim::Tex3D:      return WGPUTextureViewDimension_3D;
        case TextureDim::Tex2D:
        default:                     return WGPUTextureViewDimension_2D;
    }
}

WGPUPrimitiveTopology ToTopology(PrimitiveTopology t) {
    switch (t) {
        case PrimitiveTopology::LineStrip: return WGPUPrimitiveTopology_LineStrip;
        case PrimitiveTopology::LineList:  return WGPUPrimitiveTopology_LineList;
        case PrimitiveTopology::PointList: return WGPUPrimitiveTopology_PointList;
        case PrimitiveTopology::TriangleList:
        default:                           return WGPUPrimitiveTopology_TriangleList;
    }
}

WGPUCullMode ToCullMode(CullFace c) {
    switch (c) {
        case CullFace::Front: return WGPUCullMode_Front;
        case CullFace::Back:  return WGPUCullMode_Back;
        case CullFace::None:
        case CullFace::FrontAndBack:  // WebGPU 无 FrontAndBack；退化为 None（双面不剔除）
        default:              return WGPUCullMode_None;
    }
}

WGPUCompareFunction ToCompareFunc(CompareFunc f) {
    switch (f) {
        case CompareFunc::Never:        return WGPUCompareFunction_Never;
        case CompareFunc::Less:         return WGPUCompareFunction_Less;
        case CompareFunc::Equal:        return WGPUCompareFunction_Equal;
        case CompareFunc::LessEqual:    return WGPUCompareFunction_LessEqual;
        case CompareFunc::Greater:      return WGPUCompareFunction_Greater;
        case CompareFunc::NotEqual:     return WGPUCompareFunction_NotEqual;
        case CompareFunc::GreaterEqual: return WGPUCompareFunction_GreaterEqual;
        case CompareFunc::Always:
        default:                        return WGPUCompareFunction_Always;
    }
}

WGPUBlendFactor ToBlendFactor(BlendFactor f) {
    switch (f) {
        case BlendFactor::Zero:             return WGPUBlendFactor_Zero;
        case BlendFactor::One:              return WGPUBlendFactor_One;
        case BlendFactor::SrcAlpha:         return WGPUBlendFactor_SrcAlpha;
        case BlendFactor::OneMinusSrcAlpha: return WGPUBlendFactor_OneMinusSrcAlpha;
        case BlendFactor::DstAlpha:         return WGPUBlendFactor_DstAlpha;
        case BlendFactor::OneMinusDstAlpha: return WGPUBlendFactor_OneMinusDstAlpha;
        case BlendFactor::SrcColor:         return WGPUBlendFactor_Src;
        case BlendFactor::OneMinusSrcColor: return WGPUBlendFactor_OneMinusSrc;
        case BlendFactor::DstColor:         return WGPUBlendFactor_Dst;
        case BlendFactor::OneMinusDstColor: return WGPUBlendFactor_OneMinusDst;
        default:                            return WGPUBlendFactor_One;
    }
}

bool IsDepthFormat(WGPUTextureFormat f) {
    return f == WGPUTextureFormat_Depth32Float || f == WGPUTextureFormat_Depth24Plus ||
           f == WGPUTextureFormat_Depth24PlusStencil8 || f == WGPUTextureFormat_Depth16Unorm ||
           f == WGPUTextureFormat_Depth32FloatStencil8;
}

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
    // 把 Dawn 未捕获错误（着色器编译 / 管线 / 绑定校验失败等）转出到日志，便于 bring-up 诊断。
    wgpuDeviceSetUncapturedErrorCallback(
        device_,
        [](WGPUErrorType type, char const* message, void*) {
            DEBUG_LOG_ERROR("WebGPU uncaptured error (type={}): {}",
                            static_cast<int>(type), message ? message : "(null)");
        },
        nullptr);
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

    // B2 录制缓存 / 池 / 瞬态：管线缓存（pipeline+layout+4×BGL）、push 缓冲池、本帧 BindGroup、临时面视图。
    for (auto& [key, e] : pipeline_cache_) {
        (void)key;
        if (e.pipeline) wgpuRenderPipelineRelease(e.pipeline);
        if (e.layout)   wgpuPipelineLayoutRelease(e.layout);
        for (WGPUBindGroupLayout bgl : e.bgls) if (bgl) wgpuBindGroupLayoutRelease(bgl);
    }
    pipeline_cache_.clear();
    // B3a compute：着色器 module、compute 管线缓存（pipeline+layout+4×BGL）、未消费的自检回读缓冲。
    for (auto& [h, e] : compute_shaders_) { (void)h; if (e.module) wgpuShaderModuleRelease(e.module); }
    compute_shaders_.clear();
    for (auto& [key, e] : compute_pipeline_cache_) {
        (void)key;
        if (e.pipeline) wgpuComputePipelineRelease(e.pipeline);
        if (e.layout)   wgpuPipelineLayoutRelease(e.layout);
        for (WGPUBindGroupLayout bgl : e.bgls) if (bgl) wgpuBindGroupLayoutRelease(bgl);
    }
    compute_pipeline_cache_.clear();
    if (cur_compute_pass_) { wgpuComputePassEncoderRelease(cur_compute_pass_); cur_compute_pass_ = nullptr; }
    if (ct_rb_out_)  { wgpuBufferRelease(ct_rb_out_);  ct_rb_out_ = nullptr; }
    if (ct_rb_draw_) { wgpuBufferRelease(ct_rb_draw_); ct_rb_draw_ = nullptr; }
    for (WGPUBuffer b : push_pool_) if (b) wgpuBufferRelease(b);
    push_pool_.clear();
    push_pool_used_ = 0;
    for (WGPUBindGroup bg : frame_bindgroups_) if (bg) wgpuBindGroupRelease(bg);
    frame_bindgroups_.clear();
    ReleasePassViews();

    ReleaseSwapChain();
    if (ubo_ring_) { wgpuBufferRelease(ubo_ring_); ubo_ring_ = nullptr; ubo_ring_size_ = 0; ubo_ring_cursor_ = 0; }
    if (geom_ring_) { wgpuBufferRelease(geom_ring_); geom_ring_ = nullptr; geom_ring_size_ = 0; geom_ring_cursor_ = 0; }
    if (surface_) { wgpuSurfaceRelease(surface_); surface_ = nullptr; }
    if (queue_)   { wgpuQueueRelease(queue_);     queue_ = nullptr; }
    if (device_)  { wgpuDeviceRelease(device_);   device_ = nullptr; }
    initialized_ = false;
}

void WebGPURhiDevice::WaitIdle() {
    // WebGPU 无显式 device idle 等待；提交后由浏览器调度。B3 起按需用 onSubmittedWorkDone。
}

bool WebGPURhiDevice::EnsureInitialized() {
    // Web 宿主（Emscripten）以空 native_window_handle 创建设备，FramePipeline 因此不调用
    // InitDevice（仅 D3D11/有窗口句柄时调用，A 阶段 GL 路径无需 swapchain）。这里据画布尺寸
    // 惰性完成 WebGPU 设备 + swapchain 初始化，不触碰 A 阶段回退逻辑。
    if (initialized_) return true;
    int cw = 0, ch = 0;
    emscripten_get_canvas_element_size("#canvas", &cw, &ch);
    if (cw <= 0) cw = 1280;
    if (ch <= 0) ch = 720;
    return InitDevice(nullptr, cw, ch);
}

uint64_t WebGPURhiDevice::AllocUboVersion(const void* data, uint64_t size) {
    if (!device_ || !data || size == 0) return UINT64_MAX;
    constexpr uint64_t kAlign = 256;  // WebGPU minUniformBufferOffsetAlignment 默认 256
    const uint64_t aligned_size = (size + 3) & ~uint64_t(3);
    const uint64_t off = (ubo_ring_cursor_ + kAlign - 1) & ~(kAlign - 1);
    // 环不足以容纳本次分配：本帧已录制的 BindGroup 仍引用现有环缓冲，不能中途重建；
    //   故仅当环尚未创建或在帧首（游标为 0）时按需扩容，运行中溢出则降级（返回失败，回退原存储）。
    if (off + aligned_size > ubo_ring_size_) {
        if (ubo_ring_cursor_ != 0) {
            DEBUG_LOG_WARN("WebGPU: UBO 版本环本帧溢出（需 {} > 容量 {}），该 UBO 回退原存储",
                           static_cast<unsigned long long>(off + aligned_size),
                           static_cast<unsigned long long>(ubo_ring_size_));
            return UINT64_MAX;
        }
        uint64_t new_size = ubo_ring_size_ ? ubo_ring_size_ : (1u << 22);  // 4MB 起步
        while (off + aligned_size > new_size) new_size *= 2;
        if (ubo_ring_) wgpuBufferRelease(ubo_ring_);
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        bd.size = new_size;
        ubo_ring_ = wgpuDeviceCreateBuffer(device_, &bd);
        ubo_ring_size_ = ubo_ring_ ? new_size : 0;
        if (!ubo_ring_) return UINT64_MAX;
    }
    if (aligned_size == size) {
        wgpuQueueWriteBuffer(queue_, ubo_ring_, off, data, size);
    } else {
        std::vector<uint8_t> padded(aligned_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, ubo_ring_, off, padded.data(), aligned_size);
    }
    ubo_ring_cursor_ = off + aligned_size;
    return off;
}

uint64_t WebGPURhiDevice::AllocGeomVersion(const void* data, uint64_t size) {
    if (!device_ || !data || size == 0) return UINT64_MAX;
    // 顶点缓冲偏移须 4 对齐；索引缓冲偏移须为索引元素字节数（2/4）的倍数。取 4 对齐两者均满足。
    constexpr uint64_t kAlign = 4;
    const uint64_t aligned_size = (size + 3) & ~uint64_t(3);
    const uint64_t off = (geom_ring_cursor_ + kAlign - 1) & ~(kAlign - 1);
    // 同 UBO 版本环：本帧已录制的绑定/索引绑定仍引用现有环缓冲，不能中途重建；
    //   故仅在环尚未创建或在帧首（游标为 0）按需扩容，运行中溢出则降级（返回失败，回退原存储）。
    if (off + aligned_size > geom_ring_size_) {
        if (geom_ring_cursor_ != 0) {
            DEBUG_LOG_WARN("WebGPU: 几何版本环本帧溢出（需 {} > 容量 {}），该顶点/索引回退原存储",
                           static_cast<unsigned long long>(off + aligned_size),
                           static_cast<unsigned long long>(geom_ring_size_));
            return UINT64_MAX;
        }
        uint64_t new_size = geom_ring_size_ ? geom_ring_size_ : (1u << 22);  // 4MB 起步
        while (off + aligned_size > new_size) new_size *= 2;
        if (geom_ring_) wgpuBufferRelease(geom_ring_);
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
        bd.size = new_size;
        geom_ring_ = wgpuDeviceCreateBuffer(device_, &bd);
        geom_ring_size_ = geom_ring_ ? new_size : 0;
        if (!geom_ring_) return UINT64_MAX;
    }
    if (aligned_size == size) {
        wgpuQueueWriteBuffer(queue_, geom_ring_, off, data, size);
    } else {
        std::vector<uint8_t> padded(aligned_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, geom_ring_, off, padded.data(), aligned_size);
    }
    geom_ring_cursor_ = off + aligned_size;
    return off;
}

void WebGPURhiDevice::BeginFrame() {
    last_frame_stats_ = RenderStats{};
    if (!EnsureInitialized()) return;
    ubo_ring_cursor_ = 0;
    ubo_versions_.clear();
    geom_ring_cursor_ = 0;
    geom_versions_.clear();
    if (!swapchain_) return;
    backbuffer_view_ = wgpuSwapChainGetCurrentTextureView(swapchain_);
    if (!backbuffer_view_) return;
    frame_encoder_ = wgpuDeviceCreateCommandEncoder(device_, nullptr);
    // 每帧复位录制瞬态：push 池游标归零（缓冲跨帧复用）、自检触发标志、当前绘制绑定。
    backbuffer_drawn_ = false;
    push_pool_used_ = 0;
    ResetDrawState();
}

void WebGPURhiDevice::EndFrame() {
    if (!initialized_ || !backbuffer_view_ || !frame_encoder_) {
        if (backbuffer_view_) { wgpuTextureViewRelease(backbuffer_view_); backbuffer_view_ = nullptr; }
        if (frame_encoder_)   { wgpuCommandEncoderRelease(frame_encoder_); frame_encoder_ = nullptr; }
        return;
    }

    // 本帧若无任何真实绘制落到 backbuffer（B2 期引擎 GLSL 程序无 WGSL module，绘制在录制
    // 期被优雅跳过），跑 bring-up 自检：经 Cmd* 把渐变×棋盘纹理画上屏，验证整条录制链路。
    // 引擎 WGSL 内容（B2b+）就绪并真正上屏后，backbuffer_drawn_ 置真，自检自动不再触发。
    if (!backbuffer_drawn_) {
        RunBringUpSelfTest();
    }
    // 兜底：自检也未成形（资源创建失败）时，至少 clear 一次 backbuffer，避免呈现未定义内容。
    if (!backbuffer_drawn_) {
        WGPURenderPassColorAttachment color_att{};
        color_att.view = backbuffer_view_;
        color_att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        color_att.loadOp = WGPULoadOp_Clear;
        color_att.storeOp = WGPUStoreOp_Store;
        color_att.clearValue = WGPUColor{0.05, 0.05, 0.08, 1.0};
        WGPURenderPassDescriptor pass_desc{};
        pass_desc.colorAttachmentCount = 1;
        pass_desc.colorAttachments = &color_att;
        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(frame_encoder_, &pass_desc);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
        last_frame_stats_.render_passes += 1;
    }

    // B3a：每会话一次 compute 自检——在本帧 frame_encoder_ 上录制 dispatch + storage→回读拷贝，
    // 随帧提交后发起异步 map 校验。仅落地原语并自检，不翻转 SupportsCompute()，不影响渲染输出。
    bool ct_recorded = false;
    if (!compute_selftest_done_) {
        compute_selftest_done_ = true;
        ct_recorded = RecordComputeSelfTest();
    }

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(frame_encoder_, nullptr);
    wgpuQueueSubmit(queue_, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    if (ct_recorded) KickComputeSelfTestReadback();

    wgpuCommandEncoderRelease(frame_encoder_);
    frame_encoder_ = nullptr;
    wgpuTextureViewRelease(backbuffer_view_);
    backbuffer_view_ = nullptr;

    // 提交后 GPU 已不再引用录制期创建的 BindGroup，统一释放（push 缓冲跨帧复用，不在此释放）。
    for (WGPUBindGroup bg : frame_bindgroups_) {
        if (bg) wgpuBindGroupRelease(bg);
    }
    frame_bindgroups_.clear();
}

void WebGPURhiDevice::PresentFrame() {
    // Emscripten 下浏览器在 rAF 回调结束后自动呈现 webgpu 画布上下文；wgpuSwapChainPresent
    // 在 Emscripten 胶水里直接 abort（"unsupported, use requestAnimationFrame"），故不可调用。
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
    if (!EnsureInitialized() || !device_) return 0;
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
    if (!EnsureInitialized() || !device_) return 0;
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
    // B2：以 sentinel 行 `// dse-wgsl`（允许前导空白）区分两类着色器：
    //   - WGSL（内建/自检程序）：vert_src 即整段 WGSL module（含 vs_main/fs_main），编译出 module。
    //   - 引擎 GLSL：无离线 GLSL→WGSL 工具，故不转译、module 留空，其绘制在录制期被优雅跳过。
    ShaderEntry e;
    e.vert_src = vert_src;
    e.frag_src = frag_src;

    const char* kSentinel = "// dse-wgsl";
    const size_t first = vert_src.find_first_not_of(" \t\r\n");
    const bool is_wgsl = first != std::string::npos &&
                         vert_src.compare(first, std::strlen(kSentinel), kSentinel) == 0;
    if (is_wgsl) {
        e.module = CompileWGSL(vert_src, "dse-wgsl-program");
        // 单一 module 同时承载 vs/fs 入口；无 fs_main 视为仅深度 pass（无片元阶段）。
        e.has_fragment = vert_src.find("fn fs_main") != std::string::npos ||
                         vert_src.find("fn " + e.fs_entry) != std::string::npos;
        // 解析 WGSL 实际声明的 `@group(N) @binding(M)`，供 explicit layout/BindGroup 过滤
        //（仅纳入着色器真正使用的绑定，避免引擎多绑资源超 per-stage 上限 / 与着色器用量不符）。
        ParseWgslBindings(vert_src, e.wgsl_bindings);
        if (!e.module) {
            DEBUG_LOG_ERROR("WebGPU: WGSL 着色器编译失败（module 为空），该程序绘制将被跳过");
        }
    }

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

// ============================================================
// B2b/B2c 内建 WGSL 程序（手写；经通用原语上屏）
// ============================================================
//
// 绑定约定（与 CollectGroupBindings 一致）：
//   group0：push 常量（binding0=VS / binding1=FS）  group1：UBO @binding=slot
//   group2：纹理 @binding=slot*2、采样器 @binding=slot*2+1  group3：SSBO @binding=slot
// 着色器只需声明其实际使用的绑定子集；BGL 可含更多条目（WebGPU 允许 layout ⊋ 着色器用量）。

namespace {

// 全屏 quad 直拷（copy/passthrough/fxaa 等）：源纹理在 slot0 → group2 binding0/1。
// 顶点来自 PostProcessRenderer 的 PPVertex：pos(vec2)@0、uv(vec2)@1（clip-space）。
const char* kWgslFullscreenBlit = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);  // GL 风格全屏 quad 在 WebGPU(top-left 纹理原点)需翻转 V
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  return textureSample(src_tex, src_smp, i.uv);
}
)WGSL";

// 全屏合成（bloom_composite/tonemapping/ssao_apply）：采样 HDR 场景 → Reinhard tonemap + sRGB。
const char* kWgslComposite = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);  // GL 风格全屏 quad 在 WebGPU(top-left 纹理原点)需翻转 V
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let hdr = textureSample(src_tex, src_smp, i.uv).rgb;
  let mapped = hdr / (hdr + vec3<f32>(1.0));
  let srgb = pow(mapped, vec3<f32>(1.0 / 2.2));
  return vec4<f32>(srgb, 1.0);
}
)WGSL";

// 前向着色（ForwardPbr/ForwardShaded）：顶点已 CPU 预变换到世界空间（见 MeshRenderer），
// 仅需 PerFrame.vp 投影；方向光 Lambert + PerMaterial.albedo + albedo 纹理（slot0）。
// 进阶特征（CSM/SSS/clearcoat/clustered/DDGI/...）留 B2c+；BGL 含全部 8 UBO/20 纹理槽，
// 本着色器仅取其用到的子集（WebGPU 允许）。
const char* kWgslForward = R"WGSL(// dse-wgsl
struct PerFrame {
  vp : mat4x4<f32>,
  view : mat4x4<f32>,
  camera_pos : vec4<f32>,
  foliage_wind : vec4<f32>,
  foliage_push : vec4<f32>,
};
struct PerScene {
  light_dir_and_enabled : vec4<f32>,
  light_color_and_ambient : vec4<f32>,
  light_params : vec4<f32>,
};
struct PerMaterial {
  albedo : vec4<f32>,
  roughness_ao : vec4<f32>,
  emissive : vec4<f32>,
  flags : vec4<f32>,
};
@group(1) @binding(0) var<uniform> per_frame : PerFrame;
@group(1) @binding(1) var<uniform> per_scene : PerScene;
@group(1) @binding(2) var<uniform> per_material : PerMaterial;
@group(2) @binding(0) var albedo_tex : texture_2d<f32>;
@group(2) @binding(1) var albedo_smp : sampler;

struct VsOut {
  @builtin(position) clip : vec4<f32>,
  @location(0) nrm : vec3<f32>,
  @location(1) uv : vec2<f32>,
  @location(2) col : vec4<f32>,
};
@vertex fn vs_main(
  @location(0) pos : vec3<f32>,
  @location(1) color : vec4<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) normal : vec3<f32>,
  @location(4) tangent : vec3<f32>,
) -> VsOut {
  var o : VsOut;
  o.clip = per_frame.vp * vec4<f32>(pos, 1.0);
  o.nrm = normal;
  o.uv = uv;
  o.col = color;
  return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let tex = textureSample(albedo_tex, albedo_smp, i.uv);
  let base = tex.rgb * per_material.albedo.rgb * i.col.rgb;
  let n = normalize(i.nrm);
  let l = normalize(per_scene.light_dir_and_enabled.xyz);
  let ndl = max(dot(n, l), 0.0);
  let enabled = per_scene.light_dir_and_enabled.w;
  let ambient = per_scene.light_color_and_ambient.w;
  let light_col = per_scene.light_color_and_ambient.rgb;
  let intensity = per_scene.light_params.x;
  let diffuse = light_col * (ndl * intensity * enabled);
  let lit = base * (vec3<f32>(ambient) + diffuse);
  let emissive = per_material.emissive.rgb;
  return vec4<f32>(lit + emissive, 1.0);
}
)WGSL";

// 进阶前向着色（ForwardShaded / DrawShaded）：移植 forward_shaded.frag 的特性子集——
//   shading_mode（0 PBR Cook-Torrance / 2 半兰伯特皮肤 / 3 半兰伯特静态 / 4 Toon / Unlit）、
//   SSS、clearcoat、clustered 点光（set3 PointLightUBO ≤64）、CSM 方向光阴影（set1 light_space_matrices
//   + shadow atlas，flat unit11 → group2 binding22/23）。UBO 逐字段对齐 mesh_renderer.cpp 的
//   FwdShadedMaterialUBO(160B)/FwdPerSceneUBO(560B)/PointLightsUBO std140 布局。
// 未纳入子集（白默认纹理/关闭 → 与引擎同结果，留后续）：法线贴图/POM、splatmap/积雪、MR/自发光/AO
//   贴图、DDGI/SH 间接光、聚光灯、anisotropy、watercolor(5)/faceSDF(6)。
// 顶点已 CPU 预变换到世界空间（位置/法线/切线，见 BuildShadedWorldVertexBuffer）。前向输出线性 HDR
//   （不在此 tonemap），由 composite（kWgslComposite）统一 Reinhard + sRGB。
const char* kWgslForwardShaded = R"WGSL(// dse-wgsl
const PI : f32 = 3.14159265359;
struct PerFrame {
  vp : mat4x4<f32>,
  view : mat4x4<f32>,
  camera_pos : vec4<f32>,
  foliage_wind : vec4<f32>,
  foliage_push : vec4<f32>,
};
struct PerScene {
  light_dir_and_enabled : vec4<f32>,
  light_color_and_ambient : vec4<f32>,
  light_params : vec4<f32>,             // x=intensity, y=shadow_strength, z=receive_shadow
  cascade_splits : vec4<f32>,
  light_space_matrices : array<mat4x4<f32>, 3>,
  shadow_atlas_regions : array<vec4<f32>, 3>,
  spot_light_space_matrices : array<mat4x4<f32>, 4>,
};
struct PerMaterial {
  albedo : vec4<f32>,        // xyz=base, w=metallic
  roughness_ao : vec4<f32>,  // x=rough, y=ao, z=normal_strength, w=alpha_cutoff
  emissive : vec4<f32>,      // xyz=emissive, w=alpha_test_on
  flags : vec4<f32>,         // x=normal_map, y=mr_map, z=emissive_map, w=occlusion_map
  mode_params : vec4<f32>,   // x=shading_mode, y=double_sided, z=anisotropy, w=pom
  sss : vec4<f32>,           // xyz=tint, w=strength
  clearcoat : vec4<f32>,     // x=coat, y=coat_roughness
  toon_shadow : vec4<f32>,   // xyz=shadow_color, w=threshold
  toon_params : vec4<f32>,   // x=softness, y=spec_size, z=spec_strength, w=rim
  watercolor : vec4<f32>,
};
struct PointLight {
  color : vec3<f32>, intensity : f32,
  position : vec3<f32>, radius : f32,
  cast_shadow : i32, shadow_index : i32, pad : vec2<f32>,
};
struct PointLights {
  count : i32, p0 : i32, p1 : i32, p2 : i32,
  lights : array<PointLight, 64>,
};
@group(1) @binding(0) var<uniform> per_frame : PerFrame;
@group(1) @binding(1) var<uniform> per_scene : PerScene;
@group(1) @binding(2) var<uniform> per_material : PerMaterial;
@group(1) @binding(3) var<uniform> point_lights : PointLights;
@group(2) @binding(0) var albedo_tex : texture_2d<f32>;
@group(2) @binding(1) var albedo_smp : sampler;
// 注：CSM shadow atlas（flat unit11）暂不在前向通道采样——引擎的 WebGPU 阴影/前向通道尚未做读写
//   屏障分离，将其作为采样绑定会与阴影写入产生同一同步作用域的读写冲突（Dawn 校验报错）。故
//   DirectionalShadow 暂返回 0（demo receive_shadow 关，视觉无差异）；PerScene 的 CSM 字段已按
//   std140 完整声明，待后续阶段补屏障后即可启用采样（见 SampleCascadeShadow 的占位说明）。

fn DistributionGGX(N : vec3<f32>, H : vec3<f32>, roughness : f32) -> f32 {
  let a = roughness * roughness;
  let a2 = a * a;
  let NdotH = max(dot(N, H), 0.0);
  let denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
  return a2 / max(PI * denom * denom, 1e-7);
}
fn GeometrySchlickGGX(NdotV : f32, roughness : f32) -> f32 {
  let r = roughness + 1.0;
  let k = (r * r) / 8.0;
  return NdotV / (NdotV * (1.0 - k) + k);
}
fn GeometrySmith(N : vec3<f32>, V : vec3<f32>, L : vec3<f32>, roughness : f32) -> f32 {
  return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
       * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}
fn FresnelSchlick(cosTheta : f32, F0 : vec3<f32>) -> vec3<f32> {
  return F0 + (vec3<f32>(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// CSM 方向光阴影：占位返回 0（不采样 shadow atlas，原因见上方绑定注释）。
//   级联选择 / light-space 投影 / atlas 区域 / PCF 逻辑与 forward_shaded.frag 的
//   DirectionalShadow+SampleCascadeShadow 一致，待补通道屏障后用 textureLoad(texture_depth_2d) 接回。
fn DirectionalShadow(worldPos : vec3<f32>, N : vec3<f32>, L : vec3<f32>) -> f32 {
  return 0.0;
}
fn PointLightsLo(N : vec3<f32>, V : vec3<f32>, world_pos : vec3<f32>,
                 surface_albedo : vec3<f32>, roughness : f32, metallic : f32, F0 : vec3<f32>) -> vec3<f32> {
  var sum = vec3<f32>(0.0);
  let n = point_lights.count;
  for (var i = 0; i < n; i = i + 1) {
    let pl = point_lights.lights[i];
    let d = pl.position - world_pos;
    let dist = length(d);
    let L = d / max(dist, 1e-4);
    let atten = clamp(1.0 - (dist * dist) / (pl.radius * pl.radius + 1e-4), 0.0, 1.0);
    let radiance = pl.color * pl.intensity * atten;
    let H = normalize(V + L);
    let NDF = DistributionGGX(N, H, roughness);
    let G = GeometrySmith(N, V, L, roughness);
    let F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    let spec = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
    sum = sum + (kD * surface_albedo / PI + spec) * radiance * max(dot(N, L), 0.0);
  }
  return sum;
}
fn SubsurfaceScattering(N : vec3<f32>, L : vec3<f32>, alb : vec3<f32>, sss_s : f32,
                        light_col : vec3<f32>, li : f32, tint : vec3<f32>) -> vec3<f32> {
  let wrap = 0.5 * sss_s;
  let wrapped = max(0.0, (dot(N, L) + wrap) / (1.0 + wrap));
  let diff = wrapped - max(dot(N, L), 0.0);
  var sss_tint = tint;
  if (dot(tint, tint) <= 0.0) { sss_tint = vec3<f32>(1.0, 0.35, 0.2); }
  return alb * sss_tint * diff * light_col * li;
}

struct VsOut {
  @builtin(position) clip : vec4<f32>,
  @location(0) nrm : vec3<f32>,
  @location(1) uv : vec2<f32>,
  @location(2) col : vec4<f32>,
  @location(3) wpos : vec3<f32>,
};
@vertex fn vs_main(
  @location(0) pos : vec3<f32>,
  @location(1) color : vec4<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) normal : vec3<f32>,
  @location(4) tangent : vec3<f32>,
) -> VsOut {
  var o : VsOut;
  o.clip = per_frame.vp * vec4<f32>(pos, 1.0);
  o.nrm = normal;
  o.uv = uv;
  o.col = color;
  o.wpos = pos;
  return o;
}
@fragment fn fs_main(i : VsOut, @builtin(front_facing) ff : bool) -> @location(0) vec4<f32> {
  let shading_mode = i32(per_material.mode_params.x + 0.5);
  let double_sided = per_material.mode_params.y > 0.5;
  var N = normalize(i.nrm);
  if (double_sided && !ff) { N = -N; }
  let V = normalize(per_frame.camera_pos.xyz - i.wpos);

  let texColor = textureSample(albedo_tex, albedo_smp, i.uv) * i.col;
  // alpha-test（emissive.w = 开关，roughness_ao.w = cutoff）。
  if (per_material.emissive.w > 0.5 && texColor.a < clamp(per_material.roughness_ao.w, 0.0, 1.0)) { discard; }

  let light_color = per_scene.light_color_and_ambient.xyz;
  let ambient = per_scene.light_color_and_ambient.w;
  let light_intensity = per_scene.light_params.x;
  let lighting_enabled = per_scene.light_dir_and_enabled.w > 0.5;
  let L = normalize(per_scene.light_dir_and_enabled.xyz);
  let shadow = DirectionalShadow(i.wpos, N, L);

  var color = vec3<f32>(0.0);
  var out_alpha = texColor.a;

  if (!lighting_enabled) {
    color = texColor.rgb * per_material.albedo.xyz;
  } else if (shading_mode == 2) {
    let hl = dot(N, L) * 0.5 + 0.5;
    let base_color = texColor.rgb * per_material.albedo.xyz;
    color = base_color * light_color * (hl * light_intensity * (1.0 - shadow) + ambient * 0.5);
    out_alpha = 1.0;
  } else if (shading_mode == 3) {
    let R = reflect(-L, N);
    let hl = dot(N, L) * 0.5 + 0.5;
    let diffuse = per_material.albedo.xyz * hl * light_color * light_intensity * (1.0 - shadow);
    let spec_power = max(per_material.roughness_ao.x, 1.0);
    let specular = vec3<f32>(per_material.albedo.w) * pow(max(dot(R, V), 0.0), spec_power);
    color = (diffuse + specular + per_material.emissive.xyz) * texColor.rgb;
  } else if (shading_mode == 4) {
    let H = normalize(L + V);
    let NdotL = dot(N, L) * 0.5 + 0.5;
    let soft = per_material.toon_params.x;
    let band1 = smoothstep(per_material.toon_shadow.w - soft, per_material.toon_shadow.w + soft, NdotL);
    let band2 = smoothstep(0.7 - soft, 0.7 + soft, NdotL);
    let cel = band1 * 0.7 + band2 * 0.3;
    let baseColor = texColor.rgb * per_material.albedo.xyz;
    let shadowColor = baseColor * per_material.toon_shadow.xyz;
    var diffuse = mix(shadowColor, baseColor * light_color, cel);
    diffuse = mix(shadowColor, diffuse, 1.0 - shadow);
    let spec = step(per_material.toon_params.y, max(dot(N, H), 0.0)) * per_material.toon_params.z;
    let rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * per_material.toon_params.w;
    color = diffuse + light_color * spec * (1.0 - shadow) + vec3<f32>(rim);
  } else {
    // 默认 PBR（Cook-Torrance）+ SSS / clearcoat / clustered 点光。
    let surface_albedo = pow(texColor.rgb * per_material.albedo.xyz, vec3<f32>(2.2));
    let metallic = clamp(per_material.albedo.w, 0.0, 1.0);
    let roughness = clamp(per_material.roughness_ao.x, 0.04, 1.0);
    let ao = max(per_material.roughness_ao.y, 0.0);
    let F0 = mix(vec3<f32>(0.04), surface_albedo, vec3<f32>(metallic));
    let H = normalize(V + L);
    let NDF = DistributionGGX(N, H, roughness);
    let G = GeometrySmith(N, V, L, roughness);
    let F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    let specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
    let NdotL = max(dot(N, L), 0.0);
    var Lo = (kD * surface_albedo / PI + specular) * light_color * light_intensity * NdotL;
    if (per_material.sss.w > 0.0) {
      Lo = Lo + SubsurfaceScattering(N, L, surface_albedo, per_material.sss.w,
                                     light_color, light_intensity, per_material.sss.xyz);
    }
    if (per_material.clearcoat.x > 0.0) {
      let cc_r = max(per_material.clearcoat.y, 0.04);
      let NDF_cc = DistributionGGX(N, H, cc_r);
      let G_cc = GeometrySmith(N, V, L, cc_r);
      let F_cc = FresnelSchlick(max(dot(H, V), 0.0), vec3<f32>(0.04));
      let spec_cc = (NDF_cc * G_cc * F_cc) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
      Lo = Lo + spec_cc * per_material.clearcoat.x * NdotL * light_color * light_intensity;
    }
    Lo = Lo * (1.0 - shadow);
    Lo = Lo + PointLightsLo(N, V, i.wpos, surface_albedo, roughness, metallic, F0);
    color = vec3<f32>(ambient) * surface_albedo * ao + Lo + per_material.emissive.xyz;
  }
  // 与 forward_shaded.frag 末尾一致：Reinhard tonemap + gamma（保证三后端 LDR 输出一致）。
  // 后续 composite（kWgslComposite）再做一次 Reinhard+sRGB，与 WebGL2 参考帧（同样双重处理）对齐。
  color = color / (color + vec3<f32>(1.0));
  color = pow(max(color, vec3<f32>(0.0)), vec3<f32>(1.0 / 2.2));
  return vec4<f32>(color, out_alpha);
}
)WGSL";

// 天空盒：vp 推送常量（VS push, group0 binding0）；cubemap 在 slot0 → group2 binding0/1。
// 顶点为 36 顶点立方体（vec3 pos）。z=w 使深度落远平面（配 LEQUAL 深度测试）。
const char* kWgslSkybox = R"WGSL(// dse-wgsl
struct VsPush { vp : mat4x4<f32> };
@group(0) @binding(0) var<uniform> vs_push : VsPush;
@group(2) @binding(0) var sky_tex : texture_cube<f32>;
@group(2) @binding(1) var sky_smp : sampler;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) dir : vec3<f32> };
@vertex fn vs_main(@location(0) in_pos : vec3<f32>) -> VsOut {
  var o : VsOut;
  let p = vs_push.vp * vec4<f32>(in_pos, 1.0);
  o.pos = p.xyww;
  o.dir = in_pos;
  return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  return textureSample(sky_tex, sky_smp, normalize(i.dir));
}
)WGSL";

// 内建天空盒立方体（36 顶点，每顶点 vec3，逆时针外向；与桌面后端一致）。
const float kSkyboxCubeVerts[] = {
  -1,  1, -1,  -1, -1, -1,   1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
  -1, -1,  1,  -1, -1, -1,  -1,  1, -1,  -1,  1, -1,  -1,  1,  1,  -1, -1,  1,
   1, -1, -1,   1, -1,  1,   1,  1,  1,   1,  1,  1,   1,  1, -1,   1, -1, -1,
  -1, -1,  1,  -1,  1,  1,   1,  1,  1,   1,  1,  1,   1, -1,  1,  -1, -1,  1,
  -1,  1, -1,   1,  1, -1,   1,  1,  1,   1,  1,  1,  -1,  1,  1,  -1,  1, -1,
  -1, -1, -1,  -1, -1,  1,   1, -1, -1,   1, -1, -1,  -1, -1,  1,   1, -1,  1,
};

}  // namespace

unsigned int WebGPURhiDevice::GetOrCreateWgslProgram(const std::string& key, const std::string& wgsl) {
    auto it = wgsl_program_cache_.find(key);
    if (it != wgsl_program_cache_.end()) return it->second;
    const unsigned int h = CreateShaderProgram(wgsl, wgsl);
    wgsl_program_cache_[key] = h;
    DEBUG_LOG_INFO("WebGPU: 内建 WGSL 程序 '{}' -> handle {}", key, h);
    return h;
}

unsigned int WebGPURhiDevice::GetBuiltinProgram(BuiltinProgram program) {
    switch (program) {
        case BuiltinProgram::Skybox:
            return GetOrCreateWgslProgram("builtin.skybox", kWgslSkybox);
        // B2c：静态前向 PBR（最小前向 WGSL，64B PerMaterial）。
        case BuiltinProgram::ForwardPbr:
            return GetOrCreateWgslProgram("builtin.forward", kWgslForward);
        // B2c 进阶：高级 shading（shading_mode/SSS/clearcoat/点光/CSM，160B PerMaterial）。
        case BuiltinProgram::ForwardShaded:
            return GetOrCreateWgslProgram("builtin.forward.shaded", kWgslForwardShaded);
        default:
            // 蒙皮/实例化/morph/粒子/毛发/GBuffer 等需 SSBO 或专用布局，留后续阶段。
            return 0;
    }
}

unsigned int WebGPURhiDevice::GetGenPPShaderProgram(const std::string& effect_name) {
    // 合成族（HDR→LDR tonemap）与直拷族分流；未迁移效果返回 0（其 pass 优雅跳过）。
    if (effect_name == "bloom_composite" || effect_name == "tonemapping" ||
        effect_name == "ssao_apply") {
        return GetOrCreateWgslProgram("genpp.composite", kWgslComposite);
    }
    if (effect_name == "copy" || effect_name == "passthrough" || effect_name == "fxaa") {
        return GetOrCreateWgslProgram("genpp.blit", kWgslFullscreenBlit);
    }
    return 0;
}

unsigned int WebGPURhiDevice::GetSkyboxCubeVertexBuffer() {
    if (skybox_cube_vbo_) return skybox_cube_vbo_;
    skybox_cube_vbo_ = CreateBuffer(sizeof(kSkyboxCubeVerts), kSkyboxCubeVerts,
                                    /*is_dynamic=*/false, /*is_index=*/false);
    return skybox_cube_vbo_;
}

unsigned int WebGPURhiDevice::CreateBufferRaw(size_t logical_size, const void* data,
                                              WGPUBufferUsageFlags usage, bool is_index) {
    if (logical_size == 0 || !EnsureInitialized() || !device_) return 0;
    const uint64_t alloc = AlignUp4(logical_size);
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
        if (mapped) std::memcpy(mapped, data, logical_size);
        wgpuBufferUnmap(buf);
    }

    BufferEntry e;
    e.buffer = buf;
    e.size = alloc;
    e.logical_size = logical_size;
    e.usage = usage;
    e.is_index = is_index;
    const unsigned int h = NextHandle();
    buffers_[h] = e;
    return h;
}

unsigned int WebGPURhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    (void)is_dynamic;  // WebGPU 缓冲无静/动态区分；动态更新经 wgpuQueueWriteBuffer。
    WGPUBufferUsageFlags usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    if (is_index) {
        usage |= WGPUBufferUsage_Index;
    } else {
        // 非索引缓冲可能用作顶点流或 uniform，同时授予两种 usage（WebGPU 允许组合）。
        usage |= WGPUBufferUsage_Vertex | WGPUBufferUsage_Uniform;
    }
    return CreateBufferRaw(size, data, usage, is_index);
}

BufferHandle WebGPURhiDevice::CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) {
    if (desc.size == 0) return BufferHandle{};
    // 按 GpuBufferUsage 授予 WGPU usage 位（始终带 CopyDst|CopySrc 以支持更新/回读拷贝）。
    WGPUBufferUsageFlags usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    const bool is_index = has(desc.usage, GpuBufferUsage::kIndex);
    if (is_index)                                   usage |= WGPUBufferUsage_Index;
    if (has(desc.usage, GpuBufferUsage::kVertex))   usage |= WGPUBufferUsage_Vertex;
    if (has(desc.usage, GpuBufferUsage::kUniform))  usage |= WGPUBufferUsage_Uniform;
    if (has(desc.usage, GpuBufferUsage::kStorage))  usage |= WGPUBufferUsage_Storage;
    if (has(desc.usage, GpuBufferUsage::kIndirect)) usage |= WGPUBufferUsage_Indirect;
    // 无任何用途位（仅 CopyDst|CopySrc）：退化为顶点+uniform，兼容默认 GpuBufferDesc。
    if ((usage & ~static_cast<WGPUBufferUsageFlags>(WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc)) == 0)
        usage |= WGPUBufferUsage_Vertex | WGPUBufferUsage_Uniform;
    return BufferHandle{CreateBufferRaw(desc.size, initial_data, usage, is_index)};
}

void WebGPURhiDevice::UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) {
    auto it = buffers_.find(handle.raw());
    if (it == buffers_.end() || !data || size == 0) return;
    const BufferEntry& e = it->second;
    // 顶点/索引/uniform 缓冲必须经 UpdateBuffer：引擎前向路径每 draw 全量重写共享 vbo_/ibo_ 与
    //   逐材质 UBO（offset=0），需 geom/UBO 版本环避免 wgpuQueueWriteBuffer 合并丢失（见 UpdateBuffer）。
    // 仅 storage/indirect（B3a 新增、设备级生命周期、不逐 draw 重写）走直写，不入版本环。
    const bool storage_or_indirect =
        (e.usage & (WGPUBufferUsage_Storage | WGPUBufferUsage_Indirect)) != 0;
    if (!storage_or_indirect) {
        UpdateBuffer(handle.raw(), offset, size, data, e.is_index);
        return;
    }
    if (offset % 4 != 0) {
        DEBUG_LOG_WARN("WebGPU UpdateGpuBuffer: offset {} 非 4 对齐，跳过更新", static_cast<unsigned long long>(offset));
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

void WebGPURhiDevice::DeleteGpuBuffer(BufferHandle handle) {
    DeleteBuffer(handle.raw());
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
    // 同时登记 UBO 版本切片：仅全量（offset=0）且 uniform 量级（≤64KB，WebGPU uniform 绑定上限）
    //   的更新参与——大顶点/存储缓冲不在此列。该缓冲若稍后以 UBO 绑定（group1），将改用此切片，
    //   使每 draw 的逐材质数据互不覆盖（见 CollectGroupBindings group1）。
    if (offset == 0 && size <= 65536) {
        const uint64_t voff = AllocUboVersion(data, size);
        if (voff != UINT64_MAX) ubo_versions_[handle] = {ubo_ring_, voff, size};
    }
    // 几何版本切片：引擎前向路径每 draw 把世界空间顶点/索引重写进共享 vbo_/ibo_（offset=0）。
    //   同 UBO 合并问题——故全量更新（offset=0）在 geom 环内分配独立切片，绑定时改用当帧最近版本，
    //   使各 draw 的顶点/索引互不覆盖（见 IssueDraw 的 SetVertexBuffer/SetIndexBuffer）。
    if (offset == 0) {
        const uint64_t goff = AllocGeomVersion(data, size);
        if (goff != UINT64_MAX) geom_versions_[handle] = {geom_ring_, goff, AlignUp4(size)};
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
    // B2：WebGPUCommandBuffer 录制即时落到本帧 frame_encoder_（经设备级 Cmd*），故 Submit
    // 无需重放；整帧命令在 EndFrame 一次 wgpuQueueSubmit 提交。
    (void)cmd_buffer;
}

const RenderStats& WebGPURhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

// ============================================================
// B2 命令录制引擎：设备级 Cmd* + 录制助手
// ============================================================
//
// WebGPUCommandBuffer 逐调用转发至此。录制立即落到本帧 frame_encoder_：BeginRenderPass 在其上
// 开 WGPURenderPassEncoder（cur_pass_），Bind*/PushConstants 累积当前绘制状态，Draw* 经
// GetOrCreateRenderPipeline 惰性组装 explicit-layout PSO 并发起；EndRenderPass 收尾。无 WGSL
// module 的程序（引擎 GLSL）在 IssueDraw 前优雅跳过，故 B2a 期引擎绘制不上屏、由 EndFrame 自检兜底。

// --- 录制助手 ---

void WebGPURhiDevice::ResetDrawState() {
    cur_pso_handle_ = 0;
    cur_program_ = 0;
    cur_vbs_.clear();
    cur_ib_handle_ = 0;
    cur_ib_format_ = WGPUIndexFormat_Uint16;
    cur_ubos_.clear();
    cur_texs_.clear();
    cur_ssbos_.clear();
    cur_vs_push_.clear();
    cur_fs_push_.clear();
}

void WebGPURhiDevice::ReleasePassViews() {
    for (WGPUTextureView v : cur_pass_views_) {
        if (v) wgpuTextureViewRelease(v);
    }
    cur_pass_views_.clear();
}

WGPUShaderModule WebGPURhiDevice::CompileWGSL(const std::string& code, const char* label) {
    if (!device_) return nullptr;
    WGPUShaderModuleWGSLDescriptor wgsl{};
    wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl.code = code.c_str();
    WGPUShaderModuleDescriptor sd{};
    sd.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&wgsl);
    sd.label = label;
    return wgpuDeviceCreateShaderModule(device_, &sd);
}

WGPUTextureView WebGPURhiDevice::MakeFaceView(const TextureEntry& e, int face) {
    WGPUTextureViewDescriptor vd{};
    vd.format = e.format;
    vd.dimension = WGPUTextureViewDimension_2D;
    vd.baseMipLevel = 0;
    vd.mipLevelCount = 1;
    vd.baseArrayLayer = static_cast<uint32_t>(face < 0 ? 0 : face);
    vd.arrayLayerCount = 1;
    vd.aspect = WGPUTextureAspect_All;
    return wgpuTextureCreateView(e.texture, &vd);
}

std::vector<WebGPURhiDevice::BindingInfo> WebGPURhiDevice::CollectGroupBindings(uint32_t group) {
    // 各组遍历顺序在 BGL（GetOrCreateRenderPipeline）与 BindGroup（BuildAndSetBindGroups）间共用，
    // 杜绝二者发散。group0=push（uniform 池）、group1=UBO、group2=texture+sampler、group3=SSBO。
    // std::map 保证按 binding 升序稳定遍历。
    std::vector<BindingInfo> out;
    const WGPUShaderStageFlags kVsFs = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    // 仅纳入当前 WGSL 程序实际声明的绑定：引擎可能绑定多于着色器所需的资源（如 ForwardShaded
    // 绑 8 UBO/20 纹理槽），全量纳入会超 per-stage 采样上限并使 layout 与着色器用量不符。
    const std::set<uint32_t>* used = nullptr;
    {
        auto sit = shaders_.find(cur_program_);
        if (sit != shaders_.end()) used = &sit->second.wgsl_bindings;
    }
    auto declared = [&](uint32_t binding) -> bool {
        return used && used->count((group << 16) | binding) != 0;
    };
    switch (group) {
        case 0: {
            // push 常量经 push 池 uniform 缓冲模拟：binding0=VS、binding1=FS。
            auto alloc_push = [&](const std::vector<uint8_t>& src) -> WGPUBuffer {
                constexpr uint32_t kPushBytes = 256;
                if (push_pool_used_ >= push_pool_.size()) {
                    WGPUBufferDescriptor bd{};
                    bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
                    bd.size = kPushBytes;
                    push_pool_.push_back(wgpuDeviceCreateBuffer(device_, &bd));
                }
                WGPUBuffer b = push_pool_[push_pool_used_++];
                std::vector<uint8_t> tmp(kPushBytes, 0);
                const size_t n = src.size() < kPushBytes ? src.size() : kPushBytes;
                if (n) std::memcpy(tmp.data(), src.data(), n);
                wgpuQueueWriteBuffer(queue_, b, 0, tmp.data(), kPushBytes);
                return b;
            };
            if (!cur_vs_push_.empty() && declared(0)) {
                BindingInfo b;
                b.binding = 0; b.kind = BindingInfo::Kind::Uniform; b.visibility = WGPUShaderStage_Vertex;
                b.buffer = alloc_push(cur_vs_push_); b.buf_offset = 0; b.buf_size = 256;
                out.push_back(b);
            }
            if (!cur_fs_push_.empty() && declared(1)) {
                BindingInfo b;
                b.binding = 1; b.kind = BindingInfo::Kind::Uniform; b.visibility = WGPUShaderStage_Fragment;
                b.buffer = alloc_push(cur_fs_push_); b.buf_offset = 0; b.buf_size = 256;
                out.push_back(b);
            }
            break;
        }
        case 1: {
            for (const auto& [slot, u] : cur_ubos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = FindBuffer(u.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Uniform; b.visibility = kVsFs;
                // 优先用当帧最近一次 UBO 版本切片（无范围偏移绑定时）：使逐 draw 材质数据互不覆盖。
                auto vit = (u.offset == 0) ? ubo_versions_.find(u.handle) : ubo_versions_.end();
                if (vit != ubo_versions_.end() && vit->second.buffer) {
                    b.buffer = vit->second.buffer;
                    b.buf_offset = vit->second.offset;
                    b.buf_size = vit->second.size;
                } else {
                    b.buffer = be->buffer; b.buf_offset = u.offset;
                    b.buf_size = u.size ? u.size : be->size;
                }
                out.push_back(b);
            }
            break;
        }
        case 2: {
            for (const auto& [slot, t] : cur_texs_) {
                // 纹理与采样器同声明（slot*2 / slot*2+1）：着色器声明纹理即纳入二者。
                if (!declared(slot * 2u)) continue;
                const TextureEntry* te = FindTexture(t.handle);
                if (!te || !te->view) continue;
                BindingInfo tb;
                tb.binding = slot * 2u; tb.kind = BindingInfo::Kind::Texture;
                tb.visibility = WGPUShaderStage_Fragment;
                tb.view_dim = ToViewDimension(t.dim);
                tb.sample_type = IsDepthFormat(te->format) ? WGPUTextureSampleType_Depth
                                                           : WGPUTextureSampleType_Float;
                tb.view = te->view;
                out.push_back(tb);
                BindingInfo sb;
                sb.binding = slot * 2u + 1u; sb.kind = BindingInfo::Kind::Sampler;
                sb.visibility = WGPUShaderStage_Fragment;
                sb.sampler = te->sampler;
                out.push_back(sb);
            }
            break;
        }
        case 3: {
            for (const auto& [slot, s] : cur_ssbos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = FindBuffer(s.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Storage; b.visibility = kVsFs;
                b.buffer = be->buffer; b.buf_offset = s.offset;
                b.buf_size = s.size ? s.size : be->size;
                out.push_back(b);
            }
            break;
        }
        default: break;
    }
    return out;
}

const WebGPURhiDevice::PipelineCacheEntry* WebGPURhiDevice::GetOrCreateRenderPipeline() {
    // 无 WGSL module 的程序（引擎 GLSL，无离线转译）：返回 nullptr，调用方跳过该 draw。
    auto sit = shaders_.find(cur_program_);
    if (sit == shaders_.end() || !sit->second.module) return nullptr;
    const ShaderEntry& sh = sit->second;

    // 校验：WGSL 声明的每个绑定都须有当前已绑资源满足；否则 explicit BGL 会缺该绑定，
    // 使 pipeline 创建失败并污染整个命令缓冲。缺失即跳过该 draw（优雅降级）。
    if (!sh.wgsl_bindings.empty()) {
        std::set<uint32_t> present;
        for (uint32_t g = 0; g < 4; ++g) {
            for (const BindingInfo& b : CollectGroupBindings(g)) present.insert((g << 16) | b.binding);
        }
        for (uint32_t key : sh.wgsl_bindings) {
            if (present.count(key)) continue;
            if (logged_incomplete_programs_.insert(cur_program_).second) {
                DEBUG_LOG_WARN("WebGPU: 程序 {} 缺少所需绑定 group={} binding={}（资源未绑定），跳过该 draw",
                               cur_program_, key >> 16, key & 0xffffu);
            }
            return nullptr;
        }
    }

    const PipelineStateDesc fallback_pso;
    const PipelineStateDesc* pso = FindPipelineState(cur_pso_handle_);
    const PipelineStateDesc& ps = pso ? *pso : fallback_pso;

    // 缓存签名 = program + pso + 顶点布局 + 颜色/深度格式 + 采样数 + 绑定签名。
    std::string key;
    key.reserve(256);
    key += "p" + std::to_string(cur_program_) + "s" + std::to_string(cur_pso_handle_);
    for (size_t i = 0; i < cur_vbs_.size(); ++i) {
        const VbBinding& vb = cur_vbs_[i];
        if (!vb.set) continue;
        key += "|v" + std::to_string(i) + ":" + std::to_string(vb.stride) + ":" +
               std::to_string(static_cast<int>(vb.rate));
        for (const VertexAttr& a : vb.attrs) {
            key += "," + std::to_string(a.location) + "-" + std::to_string(a.components) +
                   "-" + std::to_string(a.offset);
        }
    }
    for (WGPUTextureFormat f : cur_color_formats_) key += "c" + std::to_string(static_cast<int>(f));
    key += "d" + std::to_string(static_cast<int>(cur_depth_format_));
    key += "m" + std::to_string(cur_sample_count_);
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectGroupBindings(g);
        key += "g" + std::to_string(g);
        for (const BindingInfo& b : bs) {
            key += ":" + std::to_string(b.binding) + "-" + std::to_string(static_cast<int>(b.kind)) +
                   "-" + std::to_string(static_cast<int>(b.visibility)) +
                   "-" + std::to_string(static_cast<int>(b.view_dim));
        }
    }

    auto cit = pipeline_cache_.find(key);
    if (cit != pipeline_cache_.end()) return &cit->second;

    PipelineCacheEntry entry{};

    // 4 组 explicit BGL（与 BindGroup 共用同序绑定签名）。
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectGroupBindings(g);
        std::vector<WGPUBindGroupLayoutEntry> bgle;
        bgle.reserve(bs.size());
        for (const BindingInfo& b : bs) {
            WGPUBindGroupLayoutEntry e{};
            e.binding = b.binding;
            e.visibility = b.visibility;
            switch (b.kind) {
                case BindingInfo::Kind::Uniform:
                    e.buffer.type = WGPUBufferBindingType_Uniform; break;
                case BindingInfo::Kind::Storage:
                    e.buffer.type = WGPUBufferBindingType_ReadOnlyStorage; break;
                case BindingInfo::Kind::Texture:
                    e.texture.sampleType = b.sample_type;
                    e.texture.viewDimension = b.view_dim;
                    e.texture.multisampled = false;
                    break;
                case BindingInfo::Kind::Sampler:
                    e.sampler.type = WGPUSamplerBindingType_Filtering; break;
            }
            bgle.push_back(e);
        }
        WGPUBindGroupLayoutDescriptor bgld{};
        bgld.entryCount = bgle.size();
        bgld.entries = bgle.empty() ? nullptr : bgle.data();
        entry.bgls[g] = wgpuDeviceCreateBindGroupLayout(device_, &bgld);
    }

    WGPUPipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = 4;
    pld.bindGroupLayouts = entry.bgls;
    entry.layout = wgpuDeviceCreatePipelineLayout(device_, &pld);

    // 顶点缓冲布局（每 set 的 slot 一条），attrs_store/vbls 预留容量以保证内部指针稳定。
    std::vector<std::vector<WGPUVertexAttribute>> attrs_store;
    std::vector<WGPUVertexBufferLayout> vbls;
    attrs_store.reserve(cur_vbs_.size());
    vbls.reserve(cur_vbs_.size());
    for (const VbBinding& vb : cur_vbs_) {
        if (!vb.set) continue;
        std::vector<WGPUVertexAttribute> va;
        va.reserve(vb.attrs.size());
        for (const VertexAttr& a : vb.attrs) {
            WGPUVertexAttribute at{};
            at.format = ToVertexFormat(a.components);
            at.offset = a.offset;
            at.shaderLocation = a.location;
            va.push_back(at);
        }
        attrs_store.push_back(std::move(va));
        WGPUVertexBufferLayout l{};
        l.arrayStride = vb.stride;
        l.stepMode = vb.rate == VertexInputRate::PerInstance ? WGPUVertexStepMode_Instance
                                                             : WGPUVertexStepMode_Vertex;
        l.attributeCount = attrs_store.back().size();
        l.attributes = attrs_store.back().data();
        vbls.push_back(l);
    }

    WGPUVertexState vs{};
    vs.module = sh.module;
    vs.entryPoint = sh.vs_entry.c_str();
    vs.bufferCount = vbls.size();
    vs.buffers = vbls.empty() ? nullptr : vbls.data();

    // 颜色目标（与 RT/backbuffer 附件格式一一对应）。
    std::vector<WGPUColorTargetState> targets;
    std::vector<WGPUBlendState> blends;
    targets.reserve(cur_color_formats_.size());
    blends.reserve(cur_color_formats_.size());
    for (WGPUTextureFormat f : cur_color_formats_) {
        WGPUColorTargetState t{};
        t.format = f;
        t.writeMask = WGPUColorWriteMask_All;
        if (ps.blend_enabled) {
            WGPUBlendState bs{};
            bs.color.operation = WGPUBlendOperation_Add;
            bs.color.srcFactor = ToBlendFactor(ps.blend_src);
            bs.color.dstFactor = ToBlendFactor(ps.blend_dst);
            bs.alpha.operation = WGPUBlendOperation_Add;
            bs.alpha.srcFactor = ToBlendFactor(ps.alpha_blend_src);
            bs.alpha.dstFactor = ToBlendFactor(ps.alpha_blend_dst);
            blends.push_back(bs);
            t.blend = &blends.back();
        }
        targets.push_back(t);
    }

    WGPUFragmentState fs{};
    fs.module = sh.module;
    fs.entryPoint = sh.fs_entry.c_str();
    fs.targetCount = targets.size();
    fs.targets = targets.empty() ? nullptr : targets.data();

    WGPURenderPipelineDescriptor rpd{};
    rpd.layout = entry.layout;
    rpd.vertex = vs;
    rpd.primitive.topology = ToTopology(ps.topology);
    rpd.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    rpd.primitive.frontFace = WGPUFrontFace_CCW;
    rpd.primitive.cullMode = ps.culling_enabled ? ToCullMode(ps.cull_face) : WGPUCullMode_None;

    WGPUDepthStencilState ds{};
    if (cur_depth_format_ != WGPUTextureFormat_Undefined) {
        ds.format = cur_depth_format_;
        ds.depthWriteEnabled = ps.depth_write_enabled;
        ds.depthCompare = ps.depth_test_enabled ? ToCompareFunc(ps.depth_func)
                                                : WGPUCompareFunction_Always;
        ds.stencilFront.compare = WGPUCompareFunction_Always;
        ds.stencilBack.compare = WGPUCompareFunction_Always;
        rpd.depthStencil = &ds;
    }

    rpd.multisample.count = cur_sample_count_ > 0 ? cur_sample_count_ : 1;
    rpd.multisample.mask = 0xFFFFFFFFu;
    rpd.multisample.alphaToCoverageEnabled = false;
    rpd.fragment = sh.has_fragment ? &fs : nullptr;

    entry.pipeline = wgpuDeviceCreateRenderPipeline(device_, &rpd);
    if (!entry.pipeline) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateRenderPipeline 失败 (program={})", cur_program_);
    }

    auto res = pipeline_cache_.emplace(std::move(key), entry);
    return &res.first->second;
}

void WebGPURhiDevice::BuildAndSetBindGroups(const PipelineCacheEntry& entry) {
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectGroupBindings(g);
        std::vector<WGPUBindGroupEntry> bge;
        bge.reserve(bs.size());
        for (const BindingInfo& b : bs) {
            WGPUBindGroupEntry e{};
            e.binding = b.binding;
            switch (b.kind) {
                case BindingInfo::Kind::Texture: e.textureView = b.view; break;
                case BindingInfo::Kind::Sampler: e.sampler = b.sampler; break;
                default:
                    e.buffer = b.buffer; e.offset = b.buf_offset; e.size = b.buf_size; break;
            }
            bge.push_back(e);
        }
        WGPUBindGroupDescriptor bgd{};
        bgd.layout = entry.bgls[g];
        bgd.entryCount = bge.size();
        bgd.entries = bge.empty() ? nullptr : bge.data();
        WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device_, &bgd);
        wgpuRenderPassEncoderSetBindGroup(cur_pass_, g, bg, 0, nullptr);
        frame_bindgroups_.push_back(bg);  // 提交后统一释放
    }
}

void WebGPURhiDevice::IssueDraw(bool indexed, uint32_t count, uint32_t instance_count,
                                uint32_t first, int32_t base_vertex, uint32_t first_instance) {
    if (!cur_pass_ || count == 0 || instance_count == 0) return;
    const PipelineCacheEntry* pe = GetOrCreateRenderPipeline();
    if (!pe || !pe->pipeline) return;  // 无 WGSL module / 组装失败 → 优雅跳过

    wgpuRenderPassEncoderSetPipeline(cur_pass_, pe->pipeline);
    for (size_t i = 0; i < cur_vbs_.size(); ++i) {
        const VbBinding& vb = cur_vbs_[i];
        if (!vb.set) continue;
        // 当帧若有该顶点缓冲的版本切片（每 draw 重写共享 vbo_），改绑版本切片避免合并覆盖。
        auto git = geom_versions_.find(vb.handle);
        if (git != geom_versions_.end() && git->second.buffer) {
            wgpuRenderPassEncoderSetVertexBuffer(cur_pass_, static_cast<uint32_t>(i),
                                                 git->second.buffer, git->second.offset, git->second.size);
            continue;
        }
        const BufferEntry* be = FindBuffer(vb.handle);
        if (!be || !be->buffer) continue;
        wgpuRenderPassEncoderSetVertexBuffer(cur_pass_, static_cast<uint32_t>(i), be->buffer, 0, be->size);
    }
    BuildAndSetBindGroups(*pe);

    if (indexed) {
        // 同顶点：优先当帧索引版本切片（每 draw 重写共享 ibo_），否则原索引缓冲。
        auto git = geom_versions_.find(cur_ib_handle_);
        if (git != geom_versions_.end() && git->second.buffer) {
            wgpuRenderPassEncoderSetIndexBuffer(cur_pass_, git->second.buffer, cur_ib_format_,
                                                git->second.offset, git->second.size);
        } else {
            const BufferEntry* ib = FindBuffer(cur_ib_handle_);
            if (!ib || !ib->buffer) return;
            wgpuRenderPassEncoderSetIndexBuffer(cur_pass_, ib->buffer, cur_ib_format_, 0, ib->size);
        }
        wgpuRenderPassEncoderDrawIndexed(cur_pass_, count, instance_count, first,
                                         base_vertex, first_instance);
    } else {
        wgpuRenderPassEncoderDraw(cur_pass_, count, instance_count, first, first_instance);
    }

    if (cur_pass_is_backbuffer_) backbuffer_drawn_ = true;
    last_frame_stats_.draw_calls += 1;
}

// --- 设备级 Cmd*：render pass / viewport ---

void WebGPURhiDevice::CmdBeginRenderPass(const RenderPassDesc& desc) {
    if (!frame_encoder_) { cur_pass_ = nullptr; return; }
    if (cur_pass_) CmdEndRenderPass();  // 防御：上一 pass 未显式收尾
    ResetDrawState();
    ReleasePassViews();
    cur_color_formats_.clear();
    cur_depth_format_ = WGPUTextureFormat_Undefined;
    cur_sample_count_ = 1;
    cur_pass_is_backbuffer_ = (desc.render_target == 0);
    cur_rt_width_ = 0;
    cur_rt_height_ = 0;

    const WGPULoadOp load = desc.clear_color_enabled ? WGPULoadOp_Clear : WGPULoadOp_Load;
    const WGPUColor clear = WGPUColor{desc.clear_color.r, desc.clear_color.g,
                                      desc.clear_color.b, desc.clear_color.a};

    std::vector<WGPURenderPassColorAttachment> color_atts;
    WGPURenderPassDepthStencilAttachment depth_att{};
    bool has_depth = false;

    if (desc.render_target == 0) {
        if (!backbuffer_view_) { cur_pass_ = nullptr; return; }
        cur_rt_width_ = static_cast<uint32_t>(width_ > 0 ? width_ : 1);
        cur_rt_height_ = static_cast<uint32_t>(height_ > 0 ? height_ : 1);
        WGPURenderPassColorAttachment ca{};
        ca.view = backbuffer_view_;
        ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        ca.loadOp = load;
        ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = clear;
        color_atts.push_back(ca);
        cur_color_formats_.push_back(swapchain_format_);
    } else {
        const RenderTargetEntry* rt = FindRenderTarget(desc.render_target);
        if (!rt) { cur_pass_ = nullptr; return; }
        cur_sample_count_ = static_cast<uint32_t>(rt->msaa_samples > 1 ? rt->msaa_samples : 1);
        cur_rt_width_ = rt->width > 0 ? rt->width : 1;
        cur_rt_height_ = rt->height > 0 ? rt->height : 1;
        for (unsigned int th : rt->color_textures) {
            const TextureEntry* te = FindTexture(th);
            if (!te) continue;
            WGPUTextureView v;
            if (rt->is_cube) {
                v = MakeFaceView(*te, desc.cube_face >= 0 ? desc.cube_face : 0);
                cur_pass_views_.push_back(v);
            } else {
                v = te->view;
            }
            WGPURenderPassColorAttachment ca{};
            ca.view = v;
            ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
            ca.loadOp = load;
            ca.storeOp = WGPUStoreOp_Store;
            ca.clearValue = clear;
            color_atts.push_back(ca);
            cur_color_formats_.push_back(te->format);
        }
        if (rt->depth_texture) {
            const TextureEntry* de = FindTexture(rt->depth_texture);
            if (de) {
                WGPUTextureView v;
                if (rt->is_cube) {
                    v = MakeFaceView(*de, desc.cube_face >= 0 ? desc.cube_face : 0);
                    cur_pass_views_.push_back(v);
                } else {
                    v = de->view;
                }
                depth_att.view = v;
                depth_att.depthLoadOp = WGPULoadOp_Clear;  // B2 简化：深度恒清，B3 据 desc 细化
                depth_att.depthStoreOp = WGPUStoreOp_Store;
                depth_att.depthClearValue = 1.0f;
                depth_att.stencilLoadOp = WGPULoadOp_Undefined;
                depth_att.stencilStoreOp = WGPUStoreOp_Undefined;
                cur_depth_format_ = de->format;
                has_depth = true;
            }
        }
    }

    WGPURenderPassDescriptor pd{};
    pd.colorAttachmentCount = color_atts.size();
    pd.colorAttachments = color_atts.empty() ? nullptr : color_atts.data();  // depth-only：0 颜色附件
    pd.depthStencilAttachment = has_depth ? &depth_att : nullptr;

    cur_pass_ = wgpuCommandEncoderBeginRenderPass(frame_encoder_, &pd);
    last_frame_stats_.render_passes += 1;
}

void WebGPURhiDevice::CmdEndRenderPass() {
    if (cur_pass_) {
        wgpuRenderPassEncoderEnd(cur_pass_);
        wgpuRenderPassEncoderRelease(cur_pass_);
        cur_pass_ = nullptr;
    }
    ReleasePassViews();
}

void WebGPURhiDevice::CmdSetViewport(int x, int y, int width, int height) {
    if (!cur_pass_ || width <= 0 || height <= 0) return;
    // 裁剪到当前 pass 渲染目标范围：WebGPU 要求视口完全落在目标内，否则整个命令缓冲失效。
    // 引擎 GLSL pass 的绘制在 B2 期被跳过，但其 SetViewport 仍会录制，故须在此防御性裁剪。
    int rw = static_cast<int>(cur_rt_width_), rh = static_cast<int>(cur_rt_height_);
    if (rw <= 0 || rh <= 0) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= rw || y >= rh) return;
    if (x + width > rw) width = rw - x;
    if (y + height > rh) height = rh - y;
    if (width <= 0 || height <= 0) return;
    wgpuRenderPassEncoderSetViewport(cur_pass_, static_cast<float>(x), static_cast<float>(y),
                                     static_cast<float>(width), static_cast<float>(height),
                                     0.0f, 1.0f);
}

// --- 设备级 Cmd*：B2 暂保持 no-op（留 B3）---

void WebGPURhiDevice::CmdClearColor(const glm::vec4& color) { (void)color; }
void WebGPURhiDevice::CmdSetGlobalMat4(const std::string& name, const glm::mat4& value) { (void)name; (void)value; }
void WebGPURhiDevice::CmdBindGlobalShadowMap(unsigned int index, unsigned int texture_handle) { (void)index; (void)texture_handle; }
void WebGPURhiDevice::CmdBindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) { (void)index; (void)texture_handle; }
void WebGPURhiDevice::CmdBindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) { (void)index; (void)texture_handle; }
void WebGPURhiDevice::CmdDrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) { (void)indirect_buffer; (void)byte_offset; }
void WebGPURhiDevice::CmdDispatchComputePass(const ComputeDispatch& dispatch) { (void)dispatch; }

// ============================================================
// B3a：Compute 基础设施实现（compute 管线 + SSBO + indirect 原语 + WGSL 自检）
// ============================================================

unsigned int WebGPURhiDevice::CreateComputeShader(const std::string& source) {
    if (!EnsureInitialized() || !device_) return 0;
    // 仅接受 WGSL（首非空行 `// dse-wgsl`）：引擎 GLSL/SPIR-V compute 无离线转译，返回 0 跳过。
    const char* kSentinel = "// dse-wgsl";
    const size_t first = source.find_first_not_of(" \t\r\n");
    const bool is_wgsl = first != std::string::npos &&
                         source.compare(first, std::strlen(kSentinel), kSentinel) == 0;
    if (!is_wgsl) {
        DEBUG_LOG_WARN("WebGPU: CreateComputeShader 收到非 WGSL 源（无 // dse-wgsl 标记），跳过（返回 0）");
        return 0;
    }
    ComputeShaderEntry e;
    e.module = CompileWGSL(source, "dse-wgsl-compute");
    if (!e.module) {
        DEBUG_LOG_ERROR("WebGPU: compute WGSL 编译失败（module 为空）");
        return 0;
    }
    // 入口名：默认 cs_main，允许 `fn main` 兜底。
    if (source.find("fn cs_main") == std::string::npos &&
        source.find("fn main") != std::string::npos) {
        e.entry = "main";
    }
    ParseWgslBindings(source, e.wgsl_bindings);
    const unsigned int h = NextHandle();
    compute_shaders_[h] = std::move(e);
    return h;
}

void WebGPURhiDevice::DeleteComputeShader(unsigned int handle) {
    auto it = compute_shaders_.find(handle);
    if (it == compute_shaders_.end()) return;
    if (it->second.module) wgpuShaderModuleRelease(it->second.module);
    compute_shaders_.erase(it);
}

void WebGPURhiDevice::BeginComputePass() {
    // 不可与 render pass 嵌套；同一时刻仅一个 compute pass。
    if (!frame_encoder_ || cur_compute_pass_ || cur_pass_) return;
    cur_compute_pass_ = wgpuCommandEncoderBeginComputePass(frame_encoder_, nullptr);
}

void WebGPURhiDevice::EndComputePass() {
    if (!cur_compute_pass_) return;
    wgpuComputePassEncoderEnd(cur_compute_pass_);
    wgpuComputePassEncoderRelease(cur_compute_pass_);
    cur_compute_pass_ = nullptr;
}

std::vector<WebGPURhiDevice::BindingInfo>
WebGPURhiDevice::CollectComputeGroupBindings(uint32_t group, const ComputeShaderEntry& sh) {
    // 对齐 render 路径的 group 约定，compute 仅接 group1=UBO、group3=SSBO（可见性 Compute）；
    // group0（push）/group2（texture+sampler）在 B3a 暂不接入（无 compute 纹理用例）。
    std::vector<BindingInfo> out;
    const WGPUShaderStageFlags kCs = WGPUShaderStage_Compute;
    auto declared = [&](uint32_t binding) -> bool {
        return sh.wgsl_bindings.count((group << 16) | binding) != 0;
    };
    switch (group) {
        case 1: {
            for (const auto& [slot, u] : cur_ubos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = FindBuffer(u.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Uniform; b.visibility = kCs;
                b.buffer = be->buffer; b.buf_offset = u.offset;
                b.buf_size = u.size ? u.size : be->size;
                out.push_back(b);
            }
            break;
        }
        case 3: {
            for (const auto& [slot, s] : cur_ssbos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = FindBuffer(s.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Storage; b.visibility = kCs;
                b.buffer = be->buffer; b.buf_offset = s.offset;
                b.buf_size = s.size ? s.size : be->size;
                out.push_back(b);
            }
            break;
        }
        default: break;
    }
    return out;
}

const WebGPURhiDevice::ComputePipelineCacheEntry*
WebGPURhiDevice::GetOrCreateComputePipeline(unsigned int shader_handle) {
    auto sit = compute_shaders_.find(shader_handle);
    if (sit == compute_shaders_.end() || !sit->second.module) return nullptr;
    const ComputeShaderEntry& sh = sit->second;

    // 缓存签名 = shader + 绑定签名（binding 号 + 种类）。
    std::string key = "cs" + std::to_string(shader_handle);
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectComputeGroupBindings(g, sh);
        key += "g" + std::to_string(g);
        for (const BindingInfo& b : bs)
            key += ":" + std::to_string(b.binding) + "-" + std::to_string(static_cast<int>(b.kind));
    }
    auto cit = compute_pipeline_cache_.find(key);
    if (cit != compute_pipeline_cache_.end()) return &cit->second;

    ComputePipelineCacheEntry entry{};
    // 4 组 explicit BGL（与 BindGroup 共用同序绑定签名）；未用组建空 BGL（与 render 路径一致）。
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectComputeGroupBindings(g, sh);
        std::vector<WGPUBindGroupLayoutEntry> bgle;
        bgle.reserve(bs.size());
        for (const BindingInfo& b : bs) {
            WGPUBindGroupLayoutEntry e{};
            e.binding = b.binding;
            e.visibility = b.visibility;
            switch (b.kind) {
                case BindingInfo::Kind::Uniform:
                    e.buffer.type = WGPUBufferBindingType_Uniform; break;
                case BindingInfo::Kind::Storage:
                    // compute SSBO 默认 read_write（自检着色器写 outbuf/draw）；只读 storage 留待按声明细化。
                    e.buffer.type = WGPUBufferBindingType_Storage; break;
                case BindingInfo::Kind::Texture:
                    e.texture.sampleType = b.sample_type;
                    e.texture.viewDimension = b.view_dim;
                    e.texture.multisampled = false; break;
                case BindingInfo::Kind::Sampler:
                    e.sampler.type = WGPUSamplerBindingType_Filtering; break;
            }
            bgle.push_back(e);
        }
        WGPUBindGroupLayoutDescriptor bgld{};
        bgld.entryCount = bgle.size();
        bgld.entries = bgle.empty() ? nullptr : bgle.data();
        entry.bgls[g] = wgpuDeviceCreateBindGroupLayout(device_, &bgld);
    }

    WGPUPipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = 4;
    pld.bindGroupLayouts = entry.bgls;
    entry.layout = wgpuDeviceCreatePipelineLayout(device_, &pld);

    WGPUComputePipelineDescriptor cpd{};
    cpd.layout = entry.layout;
    cpd.compute.module = sh.module;
    cpd.compute.entryPoint = sh.entry.c_str();
    entry.pipeline = wgpuDeviceCreateComputePipeline(device_, &cpd);
    if (!entry.pipeline) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateComputePipeline 失败 (shader={})", shader_handle);
    }

    auto res = compute_pipeline_cache_.emplace(std::move(key), entry);
    return &res.first->second;
}

void WebGPURhiDevice::DispatchCompute(unsigned int shader_handle,
                                      unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
    if (!cur_compute_pass_ || groups_x == 0 || groups_y == 0 || groups_z == 0) return;
    auto it = compute_shaders_.find(shader_handle);
    if (it == compute_shaders_.end() || !it->second.module) return;
    const ComputePipelineCacheEntry* pe = GetOrCreateComputePipeline(shader_handle);
    if (!pe || !pe->pipeline) return;

    wgpuComputePassEncoderSetPipeline(cur_compute_pass_, pe->pipeline);
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectComputeGroupBindings(g, it->second);
        std::vector<WGPUBindGroupEntry> bge;
        bge.reserve(bs.size());
        for (const BindingInfo& b : bs) {
            WGPUBindGroupEntry e{};
            e.binding = b.binding;
            switch (b.kind) {
                case BindingInfo::Kind::Texture: e.textureView = b.view; break;
                case BindingInfo::Kind::Sampler: e.sampler = b.sampler; break;
                default: e.buffer = b.buffer; e.offset = b.buf_offset; e.size = b.buf_size; break;
            }
            bge.push_back(e);
        }
        WGPUBindGroupDescriptor bgd{};
        bgd.layout = pe->bgls[g];
        bgd.entryCount = bge.size();
        bgd.entries = bge.empty() ? nullptr : bge.data();
        WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device_, &bgd);
        wgpuComputePassEncoderSetBindGroup(cur_compute_pass_, g, bg, 0, nullptr);
        frame_bindgroups_.push_back(bg);  // 提交后统一释放
    }
    wgpuComputePassEncoderDispatchWorkgroups(cur_compute_pass_, groups_x, groups_y, groups_z);
}

bool WebGPURhiDevice::RecordComputeSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

    // 自检 compute：每线程写 outbuf[i]=i*2+base；0 号线程额外把 indirect DrawCmd 写为定值。
    // 同时演练 group1=UBO 参数、group3=两个 read_write storage（普通 SSBO + storage|indirect）。
    static const char* kComputeSelfTestWGSL = R"WGSL(// dse-wgsl
struct Params { n : u32, base : u32, pad0 : u32, pad1 : u32, };
struct DrawCmd { count : u32, instance_count : u32, first_index : u32, base_vertex : u32, base_instance : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(3) @binding(0) var<storage, read_write> outbuf : array<u32>;
@group(3) @binding(1) var<storage, read_write> draw : DrawCmd;
@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let i = gid.x;
  if (i < params.n) { outbuf[i] = i * 2u + params.base; }
  if (i == 0u) {
    draw.count = 36u;
    draw.instance_count = 1u;
    draw.first_index = 0u;
    draw.base_vertex = 0u;
    draw.base_instance = 0u;
  }
}
)WGSL";

    if (!ct_shader_) ct_shader_ = CreateComputeShader(kComputeSelfTestWGSL);
    if (!ct_params_) {
        const uint32_t params[4] = {kCtN, kCtBase, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        ct_params_ = CreateGpuBuffer(d, params).raw();
    }
    if (!ct_out_) {
        GpuBufferDesc d; d.size = kCtN * sizeof(uint32_t); d.usage = GpuBufferUsage::kStorage;
        ct_out_ = CreateGpuBuffer(d, nullptr).raw();
    }
    if (!ct_draw_) {
        GpuBufferDesc d; d.size = kCtDrawWords * sizeof(uint32_t);
        d.usage = GpuBufferUsage::kStorage | GpuBufferUsage::kIndirect;
        ct_draw_ = CreateGpuBuffer(d, nullptr).raw();
    }
    if (!ct_shader_ || !ct_params_ || !ct_out_ || !ct_draw_) {
        DEBUG_LOG_ERROR("WebGPU[B3a] compute 自检资源创建失败，跳过");
        return false;
    }

    // 经与引擎相同的命令录制状态绑定资源（group1 b0 UBO；group3 b0/b1 SSBO）。
    ResetDrawState();
    CmdBindUniformBuffer(0, ct_params_, 0, sizeof(uint32_t) * 4);
    CmdBindStorageBuffer(0, ct_out_, 0, kCtN * sizeof(uint32_t));
    CmdBindStorageBuffer(1, ct_draw_, 0, kCtDrawWords * sizeof(uint32_t));

    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(ct_shader_, (kCtN + 63u) / 64u, 1, 1);
    EndComputePass();
    ResetDrawState();

    // 回读缓冲（MapRead|CopyDst）+ storage→回读 拷贝（在本帧 frame_encoder_ 上录制，随帧提交）。
    auto make_rb = [&](uint64_t bytes) -> WGPUBuffer {
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        bd.size = AlignUp4(bytes);
        return wgpuDeviceCreateBuffer(device_, &bd);
    };
    ct_rb_out_ = make_rb(kCtN * sizeof(uint32_t));
    ct_rb_draw_ = make_rb(kCtDrawWords * sizeof(uint32_t));
    const BufferEntry* be_out = FindBuffer(ct_out_);
    const BufferEntry* be_draw = FindBuffer(ct_draw_);
    if (!ct_rb_out_ || !ct_rb_draw_ || !be_out || !be_draw) {
        if (ct_rb_out_) { wgpuBufferRelease(ct_rb_out_); ct_rb_out_ = nullptr; }
        if (ct_rb_draw_) { wgpuBufferRelease(ct_rb_draw_); ct_rb_draw_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_out->buffer, 0, ct_rb_out_, 0,
                                         kCtN * sizeof(uint32_t));
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_draw->buffer, 0, ct_rb_draw_, 0,
                                         kCtDrawWords * sizeof(uint32_t));
    return true;
}

void WebGPURhiDevice::KickComputeSelfTestReadback() {
    if (!ct_rb_out_ || !ct_rb_draw_) return;
    // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    auto* ctx = new ComputeSelfTestCtx();
    ctx->rb_out = ct_rb_out_;
    ctx->rb_draw = ct_rb_draw_;
    ct_rb_out_ = nullptr;
    ct_rb_draw_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kCtN * sizeof(uint32_t),
                       OnComputeSelfTestOutMapped, ctx);
    wgpuBufferMapAsync(ctx->rb_draw, WGPUMapMode_Read, 0, kCtDrawWords * sizeof(uint32_t),
                       OnComputeSelfTestDrawMapped, ctx);
}

// --- 设备级 Cmd*：绑定状态累积 ---

void WebGPURhiDevice::CmdBindPipeline(unsigned int graphics_pipeline_handle) {
    const GraphicsPipelineDesc* gp = GetGraphicsPipelineDesc(graphics_pipeline_handle);
    if (!gp) return;
    cur_pso_handle_ = gp->pso_state;
    if (gp->program != 0) cur_program_ = gp->program;  // program==0：仅应用 PSO，保留已绑 program
}

void WebGPURhiDevice::CmdBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                                          const std::vector<VertexAttr>& attrs, VertexInputRate rate) {
    if (slot >= cur_vbs_.size()) cur_vbs_.resize(slot + 1);
    VbBinding& vb = cur_vbs_[slot];
    vb.handle = buffer_handle;
    vb.stride = stride;
    vb.attrs = attrs;
    vb.rate = rate;
    vb.set = true;
}

void WebGPURhiDevice::CmdBindIndexBuffer(unsigned int buffer_handle, IndexType type) {
    cur_ib_handle_ = buffer_handle;
    cur_ib_format_ = (type == IndexType::UInt32) ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16;
}

void WebGPURhiDevice::CmdBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
    cur_texs_[slot] = TexBinding{texture_handle, dim};
}

void WebGPURhiDevice::CmdBindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                           uint32_t offset, uint32_t size) {
    cur_ubos_[slot] = UboBinding{buffer_handle, offset, size};
}

void WebGPURhiDevice::CmdBindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                                           uint32_t offset, uint32_t size) {
    cur_ssbos_[slot] = SsboBinding{buffer_handle, offset, size};
}

void WebGPURhiDevice::CmdPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) {
    if (!data || size == 0) return;
    auto write = [&](std::vector<uint8_t>& buf) {
        if (buf.size() < static_cast<size_t>(offset) + size) buf.resize(static_cast<size_t>(offset) + size, 0);
        std::memcpy(buf.data() + offset, data, size);
    };
    if (stage & ShaderStage::Vertex)   write(cur_vs_push_);
    if (stage & ShaderStage::Fragment) write(cur_fs_push_);
}

// --- 设备级 Cmd*：绘制 ---

void WebGPURhiDevice::CmdDraw(uint32_t vertex_count, uint32_t first_vertex) {
    IssueDraw(false, vertex_count, 1, first_vertex, 0, 0);
}

void WebGPURhiDevice::CmdDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) {
    IssueDraw(true, index_count, 1, first_index, base_vertex, 0);
}

void WebGPURhiDevice::CmdDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                              uint32_t first_index, int32_t base_vertex,
                                              uint32_t first_instance) {
    IssueDraw(true, index_count, instance_count, first_index, base_vertex, first_instance);
}

// --- bring-up 自检：经 Cmd* 把渐变×棋盘纹理画到 backbuffer，验证整条录制链路 ---

void WebGPURhiDevice::EnsureSelfTestResources() {
    if (selftest_init_) return;
    selftest_init_ = true;  // 仅尝试一次（失败则后续 EndFrame 走 clear 兜底）

    static const char* kSelfTestWGSL = R"WGSL(// dse-wgsl
struct VsOut {
  @builtin(position) pos : vec4<f32>,
  @location(0) uv : vec2<f32>,
};

@vertex
fn vs_main(@location(0) in_pos : vec2<f32>, @location(1) in_uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(in_pos, 0.0, 1.0);
  o.uv = in_uv;
  return o;
}

struct Tint { color : vec4<f32>, };
@group(1) @binding(0) var<uniform> u_tint : Tint;
@group(2) @binding(0) var u_tex : texture_2d<f32>;
@group(2) @binding(1) var u_samp : sampler;

@fragment
fn fs_main(in : VsOut) -> @location(0) vec4<f32> {
  let tex = textureSample(u_tex, u_samp, in.uv);
  let grad = vec3<f32>(in.uv.x, in.uv.y, 1.0 - in.uv.x);
  return vec4<f32>(grad * tex.rgb * u_tint.color.rgb, 1.0);
}
)WGSL";
    selftest_program_ = CreateShaderProgram(kSelfTestWGSL, "");

    PipelineStateDesc d;
    d.blend_enabled = false;
    d.depth_test_enabled = false;
    d.depth_write_enabled = false;
    d.culling_enabled = false;
    d.cull_face = CullFace::None;
    d.topology = PrimitiveTopology::TriangleList;
    selftest_pso_ = CreatePipelineState(d);

    // 全屏 quad（两三角形，pos.xy + uv.xy，stride 16）。
    const float quad[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };
    selftest_vbo_ = CreateBuffer(sizeof(quad), quad, false, false);

    const float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    selftest_ubo_ = CreateBuffer(sizeof(tint), tint, false, false);

    // 8×8 彩色棋盘（多色，保证 distinctColors 充足）。
    unsigned char checker[8 * 8 * 4];
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const int i = (y * 8 + x) * 4;
            const bool c = ((x + y) & 1) != 0;
            checker[i + 0] = static_cast<unsigned char>(x * 32);
            checker[i + 1] = static_cast<unsigned char>(y * 32);
            checker[i + 2] = c ? 255 : 40;
            checker[i + 3] = 255;
        }
    }
    selftest_tex_ = CreateTexture2D(8, 8, checker, false);
}

void WebGPURhiDevice::RunBringUpSelfTest() {
    EnsureSelfTestResources();
    if (!selftest_program_ || !selftest_pso_ || !selftest_vbo_ || !selftest_ubo_ || !selftest_tex_) return;

    RenderPassDesc rp;
    rp.render_target = 0;
    rp.clear_color_enabled = true;
    rp.clear_color = glm::vec4(0.05f, 0.05f, 0.08f, 1.0f);
    CmdBeginRenderPass(rp);
    if (!cur_pass_) return;
    CmdSetViewport(0, 0, width_, height_);

    const unsigned int pipe = GetGraphicsPipeline(selftest_pso_, selftest_program_);
    CmdBindPipeline(pipe);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0, 2, 0},  // pos.xy
        VertexAttr{1, 2, 8},  // uv.xy
    };
    CmdBindVertexBuffer(0, selftest_vbo_, 16, attrs, VertexInputRate::PerVertex);
    CmdBindUniformBuffer(0, selftest_ubo_, 0, 16);
    CmdBindTexture(0, selftest_tex_, TextureDim::Tex2D);
    CmdDraw(6, 0);
    CmdEndRenderPass();
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
