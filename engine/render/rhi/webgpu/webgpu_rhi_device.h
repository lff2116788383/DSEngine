/**
 * @file webgpu_rhi_device.h
 * @brief WebGPU RHI 后端实现（阶段 B：桌面级 parity）
 *
 * 目标：把 Web 从 WebGL2(GLES3.0) 前向尽力版提升到与桌面 Vulkan/D3D11 对等
 * （Compute + SSBO + GPU-driven 全链路）。本后端与 OpenGL 后端并存：WebGPU 可用
 * 时走 parity 路径，否则由上层回退到阶段 A 的 WebGL2 路径（见 rhi_factory）。
 *
 * 仅在 Emscripten + DSE_ENABLE_WEBGPU 下编入（见顶层 CMakeLists 与 rhi_factory 守卫）。
 * 工具链：Emscripten 内置 WebGPU 绑定（-sUSE_WEBGPU=1，<webgpu/webgpu.h>）。
 * 设备由 JS 侧（shell.html）预创建，C++ 经 emscripten_webgpu_get_device() 取得。
 *
 * 分阶段：
 * - B0（本提交骨架）：工具链集成 + 枚举/工厂 + 设备/交换链创建 + 每帧 clear 提交。
 *   资源/绘制接口先实现为安全占位（句柄计数器 / no-op），确保链接与基本帧循环成立。
 * - B1：缓冲/纹理/采样/绑定组/PSO 全接口映射。
 * - B2：WGSL 着色器路径。
 * - B3：Compute + SSBO → 复用桌面 GPU-driven/Clustered Forward+/延迟全链路。
 */

#ifndef DSE_WEBGPU_RHI_DEVICE_H
#define DSE_WEBGPU_RHI_DEVICE_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/rhi_device.h"

#include <webgpu/webgpu.h>

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace dse {
namespace render {

/**
 * @class WebGPURhiDevice
 * @brief RHI 的 WebGPU 实现（B0 骨架）。
 *
 * B0 范围：持有 WGPUDevice/Queue/Surface/SwapChain，每帧获取交换链纹理视图并提交
 * 一个 clear 渲染 pass（证明设备/交换链/队列贯通）。其余资源/绘制接口为占位实现，
 * 由 B1+ 逐步落地。所有占位实现均显式返回 0/空/no-op，绝不静默吞掉关键渲染状态。
 */
class WebGPURhiDevice final : public RhiDevice {
public:
    WebGPURhiDevice();
    ~WebGPURhiDevice() override;

    // --- 设备生命周期 ---
    RenderDeviceInfo GetDeviceInfo() const override;
    bool InitDevice(void* window_handle, int width, int height) override;
    void OnWindowResized(int width, int height) override;
    void Shutdown() override;
    void WaitIdle() override;
    void BeginFrame() override;
    void EndFrame() override;
    void PresentFrame() override;

    // --- 渲染目标 ---
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override;
    void DeleteRenderTarget(unsigned int render_target_handle) override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override;
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const override;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const override;

    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const override;

    // --- 纹理 ---
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override;
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                 const TextureSamplerDesc& sampler) override;
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) override;
    unsigned int CreateTextureCubeWithMips(const std::vector<CubeMipLevel>& mips, bool linear_filter) override;
    unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) override;
    void DeleteTexture(unsigned int texture_handle) override;

    // --- 着色器 / 管线状态 ---
    unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) override;
    void DeleteShaderProgram(unsigned int program_handle) override;
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override;

    // --- 缓冲 ---
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) override;
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override;
    void DeleteBuffer(unsigned int handle) override;

    // --- VAO / 命令缓冲 ---
    VertexArrayHandle CreateVertexArray() override;
    void DeleteVertexArray(VertexArrayHandle handle) override;
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;

    const RenderStats& LastFrameStats() const override;

    // --- 约定矫正（WebGPU 与 D3D12/Metal 同：Z∈[0,1]，纹理 top-left 原点）---
    bool NeedsTextureYFlip() const override { return false; }
    bool NeedsReadbackYFlip() const override { return false; }
    glm::mat4 GetProjectionCorrection() const override;
    glm::mat4 GetShadowSampleCorrection() const override;

    // ============================================================
    // B1 资源表条目（句柄 → WebGPU 原生对象）
    // ============================================================

    /// 缓冲条目：顶点/索引/uniform 等。usage 记录创建时的 WGPU usage 位，供更新/绑定校验。
    struct BufferEntry {
        WGPUBuffer buffer = nullptr;
        uint64_t   size   = 0;        ///< 已对齐到 4 字节的实际分配大小
        uint64_t   logical_size = 0;  ///< 调用方请求的逻辑大小
        WGPUBufferUsageFlags usage = 0;
        bool is_index = false;
    };

    /// 纹理条目：持有原生纹理 + 默认采样视图 + 采样器。RT 的颜色/深度附件也登记于此，
    /// 以便 BindTexture(slot, handle, dim) 统一查表绑定。
    struct TextureEntry {
        WGPUTexture     texture = nullptr;
        WGPUTextureView view    = nullptr;   ///< 默认（全 mip / 全层）采样视图
        WGPUSampler     sampler = nullptr;
        WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
        WGPUTextureViewDimension view_dim = WGPUTextureViewDimension_2D;
        uint32_t width = 0, height = 0, depth = 1, mip_levels = 1, array_layers = 1;
        int  msaa_samples = 1;
        bool owns_texture = true;            ///< RT 持有的附件由 RT 释放时统一释放
    };

    /// 渲染目标条目：颜色/深度附件均以纹理句柄形式登记于 textures_。
    struct RenderTargetEntry {
        std::vector<unsigned int> color_textures;  ///< textures_ 中的句柄
        unsigned int depth_texture = 0;            ///< textures_ 中的句柄，0=无深度
        uint32_t width = 0, height = 0;
        int  msaa_samples = 1;
        bool is_cube = false;
    };

    /// 着色器程序条目：B1 暂存 GLSL 源，B2 经 GLSL→WGSL 转译后惰性创建 WGPUShaderModule。
    struct ShaderEntry {
        std::string vert_src;
        std::string frag_src;
        WGPUShaderModule module = nullptr;  ///< B2 填充
    };

    // --- 内部访问器（供 WebGPUCommandBuffer 录制用，B1+）---
    WGPUDevice device() const { return device_; }
    WGPUQueue queue() const { return queue_; }
    WGPUTextureView current_backbuffer_view() const { return backbuffer_view_; }
    WGPUTextureFormat swapchain_format() const { return swapchain_format_; }

    /// 查表（找不到返回 nullptr），供命令缓冲录制时解析句柄。
    const BufferEntry*       FindBuffer(unsigned int handle) const;
    const TextureEntry*      FindTexture(unsigned int handle) const;
    const RenderTargetEntry* FindRenderTarget(unsigned int handle) const;
    const PipelineStateDesc* FindPipelineState(unsigned int handle) const;

private:
    bool AcquireDevice();
    bool CreateSwapChain(int width, int height);
    void ReleaseSwapChain();

    // --- B1 资源创建内部助手 ---
    /// 创建一张 2D/cube/3D 纹理 + 默认视图 + 采样器，登记入 textures_，返回句柄。
    /// rgba8_layers 为各层（2D=1 层 / cube=6 层 / 3D=depth 层）紧打包的 RGBA8 数据指针，
    /// 任一为 nullptr 则该层不上传（保留未定义内容，供 RT 附件用）。
    unsigned int CreateTextureImpl(WGPUTextureDimension dim, WGPUTextureViewDimension view_dim,
                                   uint32_t width, uint32_t height, uint32_t depth_or_layers,
                                   WGPUTextureFormat format, WGPUTextureUsageFlags usage,
                                   uint32_t mip_levels, int msaa_samples,
                                   const std::vector<const unsigned char*>& layer_data,
                                   const TextureSamplerDesc& sampler);
    WGPUSampler CreateSampler(const TextureSamplerDesc& desc, uint32_t mip_levels) const;
    void DestroyTextureEntry(TextureEntry& e);

    // WebGPU 核心对象（B0：设备由 JS 预创建并 import）
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

    // 单调递增句柄发号器（0 保留为「无效句柄」，各资源表共享同一序号空间）
    unsigned int next_handle_ = 1;
    unsigned int NextHandle() { return next_handle_++; }

    // B1 资源表（句柄 → 原生对象）
    std::unordered_map<unsigned int, BufferEntry>       buffers_;
    std::unordered_map<unsigned int, TextureEntry>      textures_;
    std::unordered_map<unsigned int, RenderTargetEntry> render_targets_;
    std::unordered_map<unsigned int, PipelineStateDesc> pipeline_states_;
    std::unordered_map<unsigned int, ShaderEntry>       shaders_;

    RenderStats last_frame_stats_{};
};

} // namespace render
} // namespace dse

#endif // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif // DSE_WEBGPU_RHI_DEVICE_H
