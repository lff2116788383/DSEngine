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

#include <cstdint>
#include <map>
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

    /// 着色器程序条目：引擎 GLSL 源暂存于此（WebGPU 无离线 GLSL→WGSL，故不转译，module 留空，
    /// 其绘制在录制期被优雅跳过）；内建程序以 WGSL 源创建并编译出 module（vs/fs 同模块，入口
    /// 名见 vs_entry/fs_entry）。B2：仅内建 WGSL 程序携带 module。
    struct ShaderEntry {
        std::string vert_src;
        std::string frag_src;
        WGPUShaderModule module = nullptr;  ///< 仅 WGSL 程序非空
        std::string vs_entry = "vs_main";
        std::string fs_entry = "fs_main";
        bool has_fragment = true;           ///< 仅深度 pass 等可无片元入口
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

    // ============================================================
    // B2 设备级命令录制 API（WebGPUCommandBuffer 逐调用转发至此）
    // ============================================================
    // 录制直接落到本帧 frame_encoder_（BeginFrame 创建、EndFrame 提交），故 CommandBuffer
    // 端是「立即转发」而非「缓存重放」：Begin/End RenderPass 在 frame_encoder_ 上开关
    // WGPURenderPassEncoder，Bind*/PushConstants 累积当前绘制状态，Draw* 惰性组装 PSO 并发起。
    // ClearColor/SetGlobalMat4/三类 ShadowMap/DispatchComputePass/DrawIndexedIndirect 在 B2
    // 暂保持 no-op（留 B3：clear 经 RenderPassDesc.clear_* 表达，全局矩阵/阴影/compute/indirect 后续落地）。

    void CmdBeginRenderPass(const RenderPassDesc& desc);
    void CmdEndRenderPass();
    void CmdClearColor(const glm::vec4& color);
    void CmdSetGlobalMat4(const std::string& name, const glm::mat4& value);
    void CmdSetViewport(int x, int y, int width, int height);

    void CmdBindGlobalShadowMap(unsigned int index, unsigned int texture_handle);
    void CmdBindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle);
    void CmdBindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle);

    void CmdBindPipeline(unsigned int graphics_pipeline_handle);
    void CmdBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                             const std::vector<VertexAttr>& attrs, VertexInputRate rate);
    void CmdBindIndexBuffer(unsigned int buffer_handle, IndexType type);
    void CmdBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim);
    void CmdBindUniformBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size);
    void CmdBindStorageBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size);
    void CmdPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size);

    void CmdDraw(uint32_t vertex_count, uint32_t first_vertex);
    void CmdDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex);
    void CmdDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                 uint32_t first_index, int32_t base_vertex, uint32_t first_instance);
    void CmdDrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset);
    void CmdDispatchComputePass(const ComputeDispatch& dispatch);

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

    // ============================================================
    // B2 命令录制状态与缓存
    // ============================================================

    /// 每 slot 顶点流绑定（含布局 + 步进频率），Draw* 时组装 WGPUVertexBufferLayout。
    struct VbBinding {
        unsigned int handle = 0;
        uint32_t stride = 0;
        std::vector<VertexAttr> attrs;
        VertexInputRate rate = VertexInputRate::PerVertex;
        bool set = false;
    };
    struct UboBinding  { unsigned int handle = 0; uint32_t offset = 0; uint32_t size = 0; };
    struct TexBinding  { unsigned int handle = 0; TextureDim dim = TextureDim::Tex2D; };
    struct SsboBinding { unsigned int handle = 0; uint32_t offset = 0; uint32_t size = 0; };

    /// 单个逻辑绑定（用于在 BGL 条目与 BindGroup 条目间共享同一遍历顺序，杜绝二者发散）。
    struct BindingInfo {
        uint32_t binding = 0;
        enum class Kind { Uniform, Storage, Texture, Sampler } kind = Kind::Uniform;
        WGPUShaderStageFlags visibility = 0;
        WGPUTextureViewDimension view_dim = WGPUTextureViewDimension_2D;
        WGPUTextureSampleType sample_type = WGPUTextureSampleType_Float;
        // BindGroup 端实际资源（BGL 端忽略）：
        WGPUBuffer buffer = nullptr; uint64_t buf_offset = 0; uint64_t buf_size = 0;
        WGPUSampler sampler = nullptr;
        WGPUTextureView view = nullptr;
    };

    /// 管线缓存条目：管线 + 其 explicit 布局所用的 4 组 BGL（BindGroup 须用同一 BGL 以保证兼容）。
    struct PipelineCacheEntry {
        WGPURenderPipeline pipeline = nullptr;
        WGPUPipelineLayout layout = nullptr;
        WGPUBindGroupLayout bgls[4] = {nullptr, nullptr, nullptr, nullptr};
    };

    // --- B2 录制：当前 pass 状态 ---
    WGPURenderPassEncoder cur_pass_ = nullptr;
    bool cur_pass_is_backbuffer_ = false;
    std::vector<WGPUTextureFormat> cur_color_formats_;
    WGPUTextureFormat cur_depth_format_ = WGPUTextureFormat_Undefined;
    uint32_t cur_sample_count_ = 1;
    uint32_t cur_rt_width_ = 0;   ///< 当前 pass 渲染目标宽（视口裁剪用）
    uint32_t cur_rt_height_ = 0;  ///< 当前 pass 渲染目标高
    std::vector<WGPUTextureView> cur_pass_views_;  ///< 本 pass 临时创建的面视图，pass 结束释放

    // --- B2 录制：当前绘制绑定（跨 Draw 持续，BeginRenderPass 时重置）---
    unsigned int cur_pso_handle_ = 0;
    unsigned int cur_program_   = 0;
    std::vector<VbBinding> cur_vbs_;
    unsigned int cur_ib_handle_ = 0;
    WGPUIndexFormat cur_ib_format_ = WGPUIndexFormat_Uint16;
    std::map<uint32_t, UboBinding>  cur_ubos_;
    std::map<uint32_t, TexBinding>  cur_texs_;
    std::map<uint32_t, SsboBinding> cur_ssbos_;
    std::vector<uint8_t> cur_vs_push_;
    std::vector<uint8_t> cur_fs_push_;

    bool backbuffer_drawn_ = false;  ///< 本帧是否有真实绘制落到 backbuffer（决定是否跑自检 pass）

    std::unordered_map<std::string, PipelineCacheEntry> pipeline_cache_;
    std::vector<WGPUBuffer> push_pool_;       ///< push-constant 模拟用 uniform 缓冲池（跨帧复用）
    size_t push_pool_used_ = 0;
    std::vector<WGPUBindGroup> frame_bindgroups_;  ///< 本帧创建的 BindGroup，提交后统一释放

    // --- B2 bring-up 自检资源（验证整条录制链路；引擎 WGSL 内容就绪后自动不再触发）---
    bool selftest_init_ = false;
    unsigned int selftest_program_ = 0;
    unsigned int selftest_pso_ = 0;
    unsigned int selftest_vbo_ = 0;
    unsigned int selftest_tex_ = 0;
    unsigned int selftest_ubo_ = 0;

    // --- B2 录制内部助手 ---
    void ResetDrawState();
    void ReleasePassViews();
    WGPUShaderModule CompileWGSL(const std::string& code, const char* label);
    WGPUTextureView MakeFaceView(const TextureEntry& e, int face);
    std::vector<BindingInfo> CollectGroupBindings(uint32_t group);
    const PipelineCacheEntry* GetOrCreateRenderPipeline();
    void BuildAndSetBindGroups(const PipelineCacheEntry& entry);
    void IssueDraw(bool indexed, uint32_t count, uint32_t instance_count,
                   uint32_t first, int32_t base_vertex, uint32_t first_instance);
    void EnsureSelfTestResources();
    void RunBringUpSelfTest();

    RenderStats last_frame_stats_{};
};

} // namespace render
} // namespace dse

#endif // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif // DSE_WEBGPU_RHI_DEVICE_H
