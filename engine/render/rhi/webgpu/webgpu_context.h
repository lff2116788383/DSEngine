/**
 * @file webgpu_context.h
 * @brief WebGPU 帧/设备上下文（manager 拆分：叶子类，无 sibling 依赖）。
 *
 * 持有 WebGPU 设备级长生命周期对象（instance/device/queue/surface/swapchain）与每帧瞬态
 * （backbuffer_view_/frame_encoder_）。device_/queue_ 在 device 生命周期内不变（AcquireDevice 设、
 * Shutdown 清）；每帧瞬态由本类持有，其余 manager 经 live 访问器读取（不缓存，杜绝 staleness）。
 *
 * 仅在 Emscripten + DSE_ENABLE_WEBGPU 下编入（与各 webgpu 实现文件一致）。
 */

#ifndef DSE_WEBGPU_CONTEXT_H
#define DSE_WEBGPU_CONTEXT_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include <webgpu/webgpu.h>

#include <cstdint>
#include <functional>

namespace dse {
namespace render {

/**
 * @class WebGPUContext
 * @brief 设备/交换链/每帧编码器的持有者，供 orchestrator 与各 manager 共享。
 */
class WebGPUContext {
public:
    /// 设备句柄获取回调（device 生命周期内仅在 AcquireDevice 成功时触发一次，Shutdown 时以
    /// (nullptr,nullptr) 触发）：orchestrator 据此把同名稳定句柄 device_/queue_ 同步进各 manager。
    using DeviceAcquiredCb = std::function<void(WGPUDevice, WGPUQueue)>;
    void SetDeviceAcquiredCallback(DeviceAcquiredCb cb) { on_device_acquired_ = std::move(cb); }

    // --- 设备生命周期（迁自 device 同名方法）---
    bool AcquireDevice();
    bool CreateSwapChain(int width, int height);
    void ReleaseSwapChain();
    bool InitDevice(void* window_handle, int width, int height);
    void OnWindowResized(int width, int height);
    void WaitIdle();
    bool EnsureInitialized();
    /// orchestrator Shutdown 末步调用：释放交换链/surface/queue/device 并复位 initialized_。
    void Shutdown();

    // --- 每帧编码器助手（供 orchestrator BeginFrame/EndFrame 调用）---
    /// 取当前 backbuffer 视图并创建本帧 frame_encoder_。无 swapchain / 取视图失败时返回 false
    /// （此时 frame_encoder_ 保持空）。
    bool CreateFrameEncoder();
    /// 释放本帧 frame_encoder_ 与 backbuffer_view_（EndFrame 早退路径）。
    void ReleaseFrameEncoder();
    /// finish + QueueSubmit + 释放命令缓冲与 frame_encoder_/backbuffer_view_（EndFrame 正常路径）。
    void SubmitEncoder();

    // --- 访问器（同名稳定句柄 + 每帧瞬态 live 转发）---
    WGPUDevice device() const { return device_; }
    WGPUQueue queue() const { return queue_; }
    WGPUCommandEncoder frame_encoder() const { return frame_encoder_; }
    WGPUTextureView backbuffer_view() const { return backbuffer_view_; }
    WGPUTextureFormat swapchain_format() const { return swapchain_format_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool initialized() const { return initialized_; }
    int max_color_attachments() const { return max_color_attachments_; }
    unsigned int NextHandle() { return next_handle_++; }

private:
    // WebGPU 核心对象（设备由 JS 预创建并 import）
    WGPUInstance instance_   = nullptr;
    WGPUDevice device_       = nullptr;
    WGPUQueue queue_         = nullptr;
    WGPUSurface surface_     = nullptr;
    WGPUSwapChain swapchain_ = nullptr;
    WGPUTextureFormat swapchain_format_ = WGPUTextureFormat_BGRA8Unorm;

    // 每帧瞬态：当前交换链后备缓冲视图（BeginFrame 取得，EndFrame 提交后释放）
    WGPUTextureView backbuffer_view_ = nullptr;
    WGPUCommandEncoder frame_encoder_ = nullptr;

    int width_  = 0;
    int height_ = 0;
    bool initialized_ = false;
    int max_color_attachments_ = 8;  ///< wgpuDeviceGetLimits 探测填充（WebGPU 规范默认 8）

    // 单调递增句柄发号器（0 保留为「无效句柄」，各资源表共享同一序号空间）
    unsigned int next_handle_ = 1;

    DeviceAcquiredCb on_device_acquired_;
};

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif  // DSE_WEBGPU_CONTEXT_H
