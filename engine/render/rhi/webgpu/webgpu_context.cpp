/**
 * @file webgpu_context.cpp
 * @brief WebGPUContext 实现（详见头文件）。方法体迁自 webgpu_rhi_device.cpp（机械抽取）。
 */

#include "engine/render/rhi/webgpu/webgpu_context.h"

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/base/debug.h"

#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

namespace dse {
namespace render {

bool WebGPUContext::AcquireDevice() {
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

    // 能力探测：读取适配器实际 limits 填充 MRT 上限（供能力声明式裁剪 requires_mrt）。
    // 探测失败保持默认 8（WebGPU 规范保证的最低 maxColorAttachments）。
    WGPUSupportedLimits limits{};
    if (wgpuDeviceGetLimits(device_, &limits) && limits.limits.maxColorAttachments > 0) {
        max_color_attachments_ = static_cast<int>(limits.limits.maxColorAttachments);
    }
    // 一次性输出 WebGPU 能力矩阵（浏览器控制台可见），明示当前裁剪路由依据：
    //   supports_compute=true（各 compute 消费方已手译 WGSL）；ssbo-compute 仍 false（无 SSBO 同步读回）。
    //   compute 消费方按各自能力门控接入。
    DEBUG_LOG_INFO("WebGPU 能力矩阵：max_color_attachments={} supports_compute={} "
                   "supports_ssbo={} supports_ssbo_compute={}",
                   max_color_attachments_, true, true, true);
    // device 生命周期内仅此一次：把同名稳定句柄 device_/queue_ 同步进各 manager。
    if (on_device_acquired_) on_device_acquired_(device_, queue_);
    return true;
}

bool WebGPUContext::CreateSwapChain(int width, int height) {
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

void WebGPUContext::ReleaseSwapChain() {
    if (swapchain_) {
        wgpuSwapChainRelease(swapchain_);
        swapchain_ = nullptr;
    }
}

bool WebGPUContext::InitDevice(void* window_handle, int width, int height) {
    (void)window_handle;
    if (initialized_) return true;
    if (!AcquireDevice()) return false;
    if (!CreateSwapChain(width, height)) return false;
    initialized_ = true;
    DEBUG_LOG_INFO("WebGPU 后端初始化成功 ({}x{}, 交换链格式=0x{:x})",
                   width, height, static_cast<unsigned int>(swapchain_format_));
    return true;
}

void WebGPUContext::OnWindowResized(int width, int height) {
    if (!initialized_) return;
    if (width == width_ && height == height_) return;
    CreateSwapChain(width, height);
}

void WebGPUContext::WaitIdle() {
    // WebGPU 无显式 device idle 等待；提交后由浏览器调度（按需用 onSubmittedWorkDone）。
}

bool WebGPUContext::EnsureInitialized() {
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

void WebGPUContext::Shutdown() {
    ReleaseSwapChain();
    if (surface_) { wgpuSurfaceRelease(surface_); surface_ = nullptr; }
    if (queue_)   { wgpuQueueRelease(queue_);     queue_ = nullptr; }
    if (device_)  { wgpuDeviceRelease(device_);   device_ = nullptr; }
    initialized_ = false;
    if (on_device_acquired_) on_device_acquired_(nullptr, nullptr);
}

bool WebGPUContext::CreateFrameEncoder() {
    if (!swapchain_) return false;
    backbuffer_view_ = wgpuSwapChainGetCurrentTextureView(swapchain_);
    if (!backbuffer_view_) return false;
    frame_encoder_ = wgpuDeviceCreateCommandEncoder(device_, nullptr);
    return true;
}

void WebGPUContext::ReleaseFrameEncoder() {
    if (backbuffer_view_) { wgpuTextureViewRelease(backbuffer_view_); backbuffer_view_ = nullptr; }
    if (frame_encoder_)   { wgpuCommandEncoderRelease(frame_encoder_); frame_encoder_ = nullptr; }
}

void WebGPUContext::SubmitEncoder() {
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(frame_encoder_, nullptr);
    wgpuQueueSubmit(queue_, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(frame_encoder_);
    frame_encoder_ = nullptr;
    wgpuTextureViewRelease(backbuffer_view_);
    backbuffer_view_ = nullptr;
}

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
