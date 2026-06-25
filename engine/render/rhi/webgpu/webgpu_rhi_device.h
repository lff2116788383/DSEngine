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
#include <set>
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
    /// MRT 上限：B4 能力探测——AcquireDevice 经 wgpuDeviceGetLimits 读取适配器实际
    /// maxColorAttachments 填充（默认 8）。供能力声明式裁剪 requires_mrt 的 pass 精确判定。
    int GetMaxColorAttachments() const override { return max_color_attachments_; }
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

    // --- 内建资源（B2b/B2c：手写 WGSL，经通用原语上屏）---
    // 引擎高层渲染器（MeshRenderer/PostProcessRenderer/SkyboxRenderer）经这些访问器取程序句柄；
    // WebGPU 无离线 GLSL→WGSL，故各内建程序以手写 WGSL 源懒创建并缓存（见 GetOrCreateWgslProgram）。
    unsigned int GetBuiltinProgram(BuiltinProgram program) override;
    unsigned int GetGenPPShaderProgram(const std::string& effect_name) override;
    unsigned int GetSkyboxCubeVertexBuffer() override;

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
        /// WGSL 实际声明的绑定集合（key = (group<<16)|binding），CreateShaderProgram 解析填充。
        /// explicit pipeline-layout/BindGroup 仅纳入此集合内的绑定：引擎可能绑定多于着色器
        /// 所需的资源（如 ForwardShaded 绑 20 纹理槽），全量纳入会超 per-stage 采样上限并使
        /// layout 与着色器用量不符；按此集合过滤即对齐 WebGPU 自动布局语义。
        std::set<uint32_t> wgsl_bindings;
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

    // ============================================================
    // B3a：Compute 基础设施（compute 管线 + SSBO 存储缓冲 + indirect 原语 + WGSL 自检）
    //   落地底层原语并以自检验证（dispatch→SSBO/indirect→回读校验）。
    // B3b 基础：CreateComputeShaderEx 已加 WGSL 源槽（见下），引擎 compute 入口可经其传入
    //   手写 WGSL；自检即经此入口验证整条引擎-facing 通路。仍**不翻转**全局 SupportsCompute()：
    //   高层 GPU-driven/bloom/skinning/hair 等 compute 消费方尚未逐特性手译 WGSL 且需逐消费方
    //   能力门控审计，贸然翻转会使其以 0 句柄走 compute 路径破坏现有渲染。逐特性接入后再翻转
    //   （SSBO 同步读回亦不支持，SupportsSSBOCompute() 保持 false）。
    // ============================================================

    /// 创建 compute shader。仅接受 WGSL（首非空行须为 `// dse-wgsl` 标记）：引擎 GLSL/SPIR-V
    /// compute 源无离线转译，返回 0 跳过。返回设备级 compute shader 句柄（0=失败）。
    unsigned int CreateComputeShader(const std::string& source) override;
    /// B3b：多源 compute 创建——WebGPU 仅取 wgsl_src（手写 WGSL）。wgsl_src 为空表示该 compute
    /// 特性尚未手译 WGSL，返回 0（调用方按句柄 0 优雅回退）。gl/vk/hlsl 源与 VK 布局计数忽略。
    unsigned int CreateComputeShaderEx(
        const std::string& gl_src, const std::string& vk_src, const std::string& hlsl_src,
        uint32_t ssbo_count, uint32_t storage_image_count, uint32_t sampler_count,
        uint32_t push_constant_bytes, const std::string& wgsl_src = "") override;
    void DeleteComputeShader(unsigned int handle) override;
    /// 在当前 compute pass 内 dispatch（须先 BeginComputePass）：组装 explicit-layout compute
    /// 管线（group1=UBO、group3=SSBO，可见性 Compute），建并设 BindGroup，发 workgroups。
    void DispatchCompute(unsigned int shader_handle,
                         unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override;
    void BeginComputePass() override;
    void EndComputePass() override;
    /// B3b-4：创建可供 compute 写入的 2D 纹理（storage image / UAV）。WebGPU 以
    /// StorageBinding|CopySrc|TextureBinding 用法位 + rgba8unorm 格式建纹理，供 compute
    /// `texture_storage_2d<rgba8unorm, write>` 写入 + 随后采样/回读（Hi-Z 金字塔/bloom 基元）。
    unsigned int CreateComputeWriteTexture2D(int width, int height) override;
    /// B3b-4：将 storage image 绑定到 compute（group2，binding=unit）。read_only 暂忽略
    /// （WebGPU rgba8unorm storage 仅 write 访问；只读用例走采样路径）。
    void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) override;
    /// B3b-7：按句柄 + mip 级绑定单 mip 视图到 compute group2 槽（引擎 Hi-Z build 真实 API 面）。
    /// read_only=true→采样读（texture_2d<f32>，textureLoad）；false→storage 写（texture_storage_2d）。
    /// 内部对 (句柄,mip) 缓存单层单 mip 视图后经 SetComputeImageViewExplicit 路由（绕开默认全 mip 视图）。
    void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                   int mip_level, bool read_only, bool r32f = false) override;
    /// B3b-8：按句柄绑定纹理到 compute 采样单元（group2 binding=unit，texture_2d<f32>，textureLoad）。
    /// 引擎 Hi-Z / GPU-driven 剔除经此面读 Hi-Z/深度纹理（手译 WGSL 用 textureLoad 取代 textureLod）。
    void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) override;
    /// B3b-8：命名 compute uniform 设置面（引擎 Hi-Z/GPU cull 经名字设 compute 标量/向量/矩阵参数）。
    /// 按调用序 16B 对齐累积到 CPU 暂存，DispatchCompute 时整块经 UBO 版本环上传到 group1 保留
    /// binding（kComputeNamedUboBinding），dispatch 后清空。手译 WGSL 须把这些参数声明为同名同序、
    /// 各成员 @align(16) 的 uniform 块（与 GL/DX11/Vulkan 的 16B 对齐定位方案一致）。
    void SetComputeUniformInt(unsigned int shader, const char* name, int value) override;
    void SetComputeUniformFloat(unsigned int shader, const char* name, float value) override;
    void SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) override;
    void SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) override;
    void SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) override;
    void SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) override;
    void SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) override;
    void SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) override;

    // 统一 GPU Buffer API：覆写以按 GpuBufferUsage 授予正确 WGPU usage 位
    // （storage/indirect/vertex/index/uniform），不经旧 deprecated CreateSSBO/CreateIndirectBuffer 路由。
    BufferHandle CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) override;
    void UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) override;
    void DeleteGpuBuffer(BufferHandle handle) override;

private:
    bool AcquireDevice();
    bool CreateSwapChain(int width, int height);
    void ReleaseSwapChain();
    /// 惰性初始化设备+swapchain（据画布尺寸）。Web 宿主以空窗口句柄创建设备、不调 InitDevice，
    /// 而引擎在首帧前即创建渲染目标/纹理等设备资源——故须在任一设备资源创建路径上先行确保初始化，
    /// 否则 device_ 为空致资源句柄全为 0（后处理链无源纹理 → 黑屏）。
    bool EnsureInitialized();
    /// 在每帧 UBO 版本环内 bump 分配一块 256 对齐切片并写入 data；返回切片在环内的字节偏移
    /// （失败返回 UINT64_MAX）。环按需懒创建、跨帧复用、每帧 BeginFrame 复位游标。
    uint64_t AllocUboVersion(const void* data, uint64_t size);
    /// 同 AllocUboVersion，但服务顶点/索引缓冲（geom 版本环，usage=Vertex|Index|CopyDst，4 对齐）。
    uint64_t AllocGeomVersion(const void* data, uint64_t size);

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
    int max_color_attachments_ = 8;  ///< B4：wgpuDeviceGetLimits 探测填充（WebGPU 规范默认 8）

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
        enum class Kind { Uniform, Storage, Texture, Sampler, StorageTexture } kind = Kind::Uniform;
        WGPUShaderStageFlags visibility = 0;
        WGPUTextureViewDimension view_dim = WGPUTextureViewDimension_2D;
        WGPUTextureSampleType sample_type = WGPUTextureSampleType_Float;
        WGPUTextureFormat tex_format = WGPUTextureFormat_RGBA8Unorm;  ///< storage texture 格式
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
    std::map<uint32_t, unsigned int> cur_compute_images_;  ///< B3b-4：compute storage image（binding→纹理句柄）
    std::map<uint32_t, unsigned int> cur_compute_textures_;  ///< B3b-5：compute 只读采样纹理（textureLoad，binding→纹理句柄）
    // B3b-6：compute 显式纹理视图绑定（per-mip）。Hi-Z 金字塔逐级下采样需把同一纹理的不同 mip
    //   作采样读/storage 写，故绕开「句柄→默认全 mip 视图」走显式单 mip 视图。
    struct ComputeViewBind {
        WGPUTextureView         view     = nullptr;
        WGPUTextureFormat       format   = WGPUTextureFormat_Undefined;
        WGPUTextureViewDimension view_dim = WGPUTextureViewDimension_2D;
    };
    std::map<uint32_t, ComputeViewBind> cur_compute_image_views_;    ///< storage 写显式视图
    std::map<uint32_t, ComputeViewBind> cur_compute_texture_views_;  ///< 采样读显式视图
    // B3b-7：(纹理句柄<<16 | mip) → 该纹理单层单 mip 视图缓存（SetComputeTextureImageMip 复用，
    //   随纹理删除/Shutdown 释放）。避免每次 dispatch 重建视图。
    std::map<uint64_t, WGPUTextureView> compute_mip_views_;
    void InvalidateComputeMipViews(unsigned int texture_handle);  ///< 删纹理时清该句柄全部 mip 视图
    // B3b-8：SetComputeUniform* 命名 uniform 累积（按调用序 16B 对齐定位，整块经 UBO 版本环上传到
    //   group1 保留 binding；每次 DispatchCompute 后清空，与 GL/DX11/Vulkan 同语义）。手译 WGSL 须
    //   声明同名同序、各成员 @align(16) 的 uniform 块到该保留 binding。
    static constexpr uint32_t kComputeNamedUboBinding = 8;
    std::vector<uint8_t> compute_named_staging_;
    std::unordered_map<std::string, size_t> compute_named_offsets_;
    size_t compute_named_next_ = 0;
    size_t GetOrCreateComputeNamedOffset(const char* name, size_t data_size);  ///< 名字→16B 对齐偏移（按调用序）
    void WriteComputeNamedStaging(size_t offset, const void* data, size_t size);
    // 当前 dispatch 命名 uniform 块在 UBO 版本环内的切片（DispatchCompute 内填，CollectComputeGroupBindings 读）。
    WGPUBuffer cur_compute_named_buffer_ = nullptr;
    uint64_t cur_compute_named_offset_ = 0;
    uint64_t cur_compute_named_size_ = 0;
    std::vector<uint8_t> cur_vs_push_;
    std::vector<uint8_t> cur_fs_push_;

    bool backbuffer_drawn_ = false;  ///< 本帧是否有真实绘制落到 backbuffer（决定是否跑自检 pass）

    std::unordered_map<std::string, PipelineCacheEntry> pipeline_cache_;
    std::vector<WGPUBuffer> push_pool_;       ///< push-constant 模拟用 uniform 缓冲池（跨帧复用）
    size_t push_pool_used_ = 0;
    std::vector<WGPUBindGroup> frame_bindgroups_;  ///< 本帧创建的 BindGroup，提交后统一释放

    // --- 每帧 UBO 版本环（关键：引擎对单个 dynamic UBO 每 draw 调 UpdateGpuBuffer 后再绑定，
    //     而 WebGPU 的 wgpuQueueWriteBuffer 在命令缓冲执行前一律合并，致同一缓冲所有 draw 只见
    //     最后一次写入。故每次 UBO 更新在大环形缓冲内分配独立 256 对齐切片、写入并登记版本；
    //     建 BindGroup 时该 UBO 句柄改绑当帧最近一次版本切片，从而各 draw 见各自材质数据）---
    WGPUBuffer ubo_ring_ = nullptr;
    uint64_t ubo_ring_size_ = 0;
    uint64_t ubo_ring_cursor_ = 0;
    struct UboVersion { WGPUBuffer buffer; uint64_t offset; uint64_t size; };
    std::unordered_map<unsigned int, UboVersion> ubo_versions_;  ///< 句柄 → 当帧最近版本切片（每帧清）

    // --- 每帧几何版本环（同 UBO 版本环，但服务顶点/索引缓冲）---
    //     引擎前向路径每 draw 把世界空间顶点烘焙后重写进共享 vbo_/ibo_（offset=0），WebGPU 的
    //     wgpuQueueWriteBuffer 在命令缓冲执行前一律合并，致同一缓冲所有 draw 只见最后一次写入
    //     （表现为后写入的网格覆盖前者，立方体丢失）。故每次顶点/索引更新在 geom 环内分配独立
    //     切片并登记版本；SetVertexBuffer/SetIndexBuffer 改绑当帧最近版本切片，各 draw 见各自几何。
    WGPUBuffer geom_ring_ = nullptr;
    uint64_t geom_ring_size_ = 0;
    uint64_t geom_ring_cursor_ = 0;
    std::unordered_map<unsigned int, UboVersion> geom_versions_;  ///< 顶点/索引句柄 → 当帧最近版本切片（每帧清）

    // --- B2 bring-up 自检资源（验证整条录制链路；引擎 WGSL 内容就绪后自动不再触发）---
    bool selftest_init_ = false;
    unsigned int selftest_program_ = 0;
    unsigned int selftest_pso_ = 0;
    unsigned int selftest_vbo_ = 0;
    unsigned int selftest_tex_ = 0;
    unsigned int selftest_ubo_ = 0;

    // --- B3a compute 基础设施 ---
    /// compute shader 条目：WGSL module + 入口名 + 实际声明的 @group/@binding 集合
    /// （key=(group<<16)|binding，供 explicit pipeline-layout/BindGroup 过滤）。
    struct ComputeShaderEntry {
        WGPUShaderModule module = nullptr;
        std::string entry = "cs_main";
        std::set<uint32_t> wgsl_bindings;
    };
    /// compute 管线缓存条目：管线 + explicit 布局所用 4 组 BGL。
    struct ComputePipelineCacheEntry {
        WGPUComputePipeline pipeline = nullptr;
        WGPUPipelineLayout layout = nullptr;
        WGPUBindGroupLayout bgls[4] = {nullptr, nullptr, nullptr, nullptr};
    };
    std::unordered_map<unsigned int, ComputeShaderEntry> compute_shaders_;
    std::unordered_map<std::string, ComputePipelineCacheEntry> compute_pipeline_cache_;
    WGPUComputePassEncoder cur_compute_pass_ = nullptr;  ///< 当前 compute pass（BeginComputePass 起）
    /// 收集 compute 当前绑定（group1=UBO、group3=SSBO，可见性 Compute），仅纳入着色器声明项。
    std::vector<BindingInfo> CollectComputeGroupBindings(uint32_t group, const ComputeShaderEntry& sh);
    const ComputePipelineCacheEntry* GetOrCreateComputePipeline(unsigned int shader_handle);

    // --- B3a compute 自检（每会话一次：dispatch 写 SSBO + indirect args → copy 到回读缓冲 → 异步回读校验）---
    bool compute_selftest_done_ = false;
    bool RecordComputeSelfTest();        ///< 在 frame_encoder_ 上录制 compute dispatch + copy（须在无 render/compute pass 时调用）
    void KickComputeSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int ct_shader_ = 0;
    unsigned int ct_params_ = 0;      ///< 参数 UBO（group1 b0）
    unsigned int ct_out_ = 0;         ///< 输出 SSBO（storage，group3 b0）
    unsigned int ct_draw_ = 0;        ///< indirect args SSBO（storage|indirect，group3 b1）
    WGPUBuffer ct_rb_out_ = nullptr;  ///< 输出回读缓冲（MapRead|CopyDst）
    WGPUBuffer ct_rb_draw_ = nullptr; ///< indirect 回读缓冲

    /// 低层缓冲创建（mappedAtCreation 上传 + 登记 buffers_）；CreateBuffer/CreateGpuBuffer 共用。
    unsigned int CreateBufferRaw(size_t logical_size, const void* data,
                                 WGPUBufferUsageFlags usage, bool is_index);

    // --- B2b/B2c 内建 WGSL 程序缓存 ---
    /// 按 key 懒创建（经 CreateShaderProgram 编译 WGSL）并缓存内建程序句柄，重复取用零开销。
    unsigned int GetOrCreateWgslProgram(const std::string& key, const std::string& wgsl);
    std::unordered_map<std::string, unsigned int> wgsl_program_cache_;
    unsigned int skybox_cube_vbo_ = 0;  ///< 内建天空盒 36 顶点立方体 VBO（懒初始化）
    std::set<unsigned int> logged_incomplete_programs_;  ///< 已告警过的缺绑定程序（去重日志）

    // --- B2 录制内部助手 ---
    void ResetDrawState();
    void ReleasePassViews();
    WGPUShaderModule CompileWGSL(const std::string& code, const char* label);
    WGPUTextureView MakeFaceView(const TextureEntry& e, int face);
    std::vector<BindingInfo> CollectGroupBindings(uint32_t group);
    const PipelineCacheEntry* GetOrCreateRenderPipeline();
    void BuildAndSetBindGroups(const PipelineCacheEntry& entry);
    /// 在当前 render pass 上绑定绘制状态（pipeline + 顶点缓冲 + BindGroup + 可选索引缓冲），
    /// 供直接绘制与 indirect 绘制共用。失败（无 WGSL module / 缺索引缓冲）返回 false。
    bool BindPassDrawState(bool indexed, const PipelineCacheEntry*& pe_out);
    void IssueDraw(bool indexed, uint32_t count, uint32_t instance_count,
                   uint32_t first, int32_t base_vertex, uint32_t first_instance);
    void EnsureSelfTestResources();
    void RunBringUpSelfTest();

    // --- B3b-2 GPU-driven 剔除自检（每会话一次：WGSL 视锥剔除 compute 写 per-instance indirect
    //   draw command → 真 wgpuRenderPassEncoderDrawIndexedIndirect 渲到离屏 RT → 回读 SSBO+像素
    //   校验「被剔实例 instance_count=0/无像素，可见实例 instance_count=1/有像素」。离屏隔离，
    //   不碰 demo backbuffer/golden；不翻转全局能力位。验证真 compute→SSBO→indirect-draw→像素链路）---
    bool gpu_cull_selftest_done_ = false;
    bool RecordGpuCullSelfTest();        ///< 在 frame_encoder_ 上录制 cull dispatch + 离屏 indirect 绘制 + copy（须在无 render/compute pass 时调用）
    void KickGpuCullSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int gc_cull_shader_ = 0;    ///< 视锥剔除 compute shader
    unsigned int gc_aabb_ssbo_ = 0;      ///< 实例 AABB（storage，group3 b0）
    unsigned int gc_draw_ssbo_ = 0;      ///< per-instance indirect draw commands（storage|indirect，group3 b1）
    unsigned int gc_params_ubo_ = 0;     ///< 剔除参数 UBO（6 视锥面 + 实例数，group1 b0）
    WGPUBuffer gc_rb_draw_ = nullptr;    ///< draw commands 回读缓冲（MapRead|CopyDst）
    WGPUBuffer gc_rb_pixels_ = nullptr;  ///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）
    // 录制期创建、提交后随回读 ctx 释放的瞬态渲染资源（被命令缓冲引用至 GPU 执行完成）。
    WGPUTexture gc_rt_tex_ = nullptr;
    WGPUTextureView gc_rt_view_ = nullptr;
    WGPURenderPipeline gc_pipeline_ = nullptr;
    WGPUShaderModule gc_render_module_ = nullptr;
    WGPUBuffer gc_vbo_ = nullptr;
    WGPUBuffer gc_ibo_ = nullptr;

    // --- B3b-3 GPU 蒙皮 compute 真链路自检（每会话一次：手译自 skinning.comp 的 WGSL 蒙皮 compute
    //   经骨骼矩阵调色板把绑定空间顶点变形写入 dst SSBO → 该 SSBO 直接作顶点缓冲被真绘制消费渲到
    //   离屏 RT → 回读 dst SSBO（逐顶点校验蒙皮后坐标==CPU 预期）+ 像素（蒙皮位移后的 quad 落在
    //   预期屏幕区域）双重校验。离屏隔离，不碰 demo backbuffer/golden；不翻转全局能力位。
    //   验证真 compute(蒙皮)→SSBO(变形顶点)→draw(顶点拉取)→像素 端到端链路）---
    bool skinning_selftest_done_ = false;
    bool RecordSkinningSelfTest();        ///< 在 frame_encoder_ 上录制蒙皮 dispatch + 离屏绘制 + copy（须在无 render/compute pass 时调用）
    void KickSkinningSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int sk_shader_ = 0;       ///< 蒙皮 compute shader
    unsigned int sk_params_ubo_ = 0;   ///< 参数 UBO（total_vertices/instance_count，group1 b0）
    unsigned int sk_src_ssbo_ = 0;     ///< 源顶点（绑定空间 + 骨骼权重/索引，group3 b0）
    unsigned int sk_dst_ssbo_ = 0;     ///< 蒙皮后顶点（storage|vertex，group3 b1 / 绘制顶点缓冲）
    unsigned int sk_bone_ssbo_ = 0;    ///< 骨骼矩阵调色板（group3 b2）
    unsigned int sk_morph_ssbo_ = 0;   ///< morph delta（本自检 morph_target_count=0，仅占位，group3 b3）
    unsigned int sk_inst_ssbo_ = 0;    ///< 实例信息（group3 b4）
    WGPUBuffer sk_rb_dst_ = nullptr;   ///< 蒙皮后顶点回读缓冲（MapRead|CopyDst）
    WGPUBuffer sk_rb_pixels_ = nullptr;///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）
    // 录制期创建、提交后随回读 ctx 释放的瞬态渲染资源（被命令缓冲引用至 GPU 执行完成）。
    WGPUTexture sk_rt_tex_ = nullptr;
    WGPUTextureView sk_rt_view_ = nullptr;
    WGPURenderPipeline sk_pipeline_ = nullptr;
    WGPUShaderModule sk_render_module_ = nullptr;
    WGPUBuffer sk_ibo_ = nullptr;

    // --- B3b-4 storage-image compute 真链路自检（每会话一次：手写 WGSL compute 经
    //   `texture_storage_2d<rgba8unorm, write>` 把已知渐变（r=x/(N-1), g=y/(N-1)）逐像素 textureStore
    //   进 storage 纹理 → copy 纹理→回读缓冲 → 逐像素校验。验证 compute 写 storage image 端到端
    //   原语（Hi-Z 金字塔 / bloom 下采样的前置能力）。离屏隔离，不翻转能力位、不碰 demo golden）---
    bool storage_image_selftest_done_ = false;
    bool RecordStorageImageSelfTest();        ///< 录制 storage-image 写 compute + copy 纹理→回读缓冲
    void KickStorageImageSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int si_shader_ = 0;       ///< storage-image 写 compute shader
    unsigned int si_params_ubo_ = 0;   ///< 参数 UBO（dim，group1 b0）
    unsigned int si_image_ = 0;        ///< compute 写目标 storage 纹理（group2 b0）
    WGPUBuffer si_rb_pixels_ = nullptr;///< storage 纹理像素回读缓冲（MapRead|CopyDst）

    // --- B3b-5 Hi-Z 下采样核心 compute 真链路自检（每会话一次：两趟 r32float compute —— ①生成趟
    //   经 `texture_storage_2d<r32float, write>` 写已知渐变到 src；②下采样趟用 textureLoad 读 src 采样
    //   纹理 + 取 2×2 max 写 dst storage 纹理 → copy dst→回读缓冲 → 逐像素校验 == CPU 预期 max。
    //   验证 compute 读采样纹理(textureLoad) + 写 r32float storage 的读后写链路（Hi-Z 金字塔逐级
    //   下采样的核心原语）。离屏隔离，不翻转能力位、不碰 demo golden）---
    bool hiz_selftest_done_ = false;
    bool RecordHiZDownsampleSelfTest();        ///< 录制 ①生成趟 + ②下采样趟 compute + copy dst→回读缓冲
    void KickHiZDownsampleSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int hz_gen_shader_ = 0;   ///< 生成趟 compute shader（textureStore 渐变到 src）
    unsigned int hz_down_shader_ = 0;  ///< 下采样趟 compute shader（textureLoad src + 2×2 max → dst）
    unsigned int hz_gen_ubo_ = 0;      ///< 生成趟参数 UBO（src_dim，group1 b0）
    unsigned int hz_down_ubo_ = 0;     ///< 下采样趟参数 UBO（src_dim/dst_dim，group1 b0）
    unsigned int hz_src_tex_ = 0;      ///< src r32float 纹理（生成趟 storage 写 / 下采样趟采样读）
    unsigned int hz_dst_tex_ = 0;      ///< dst r32float 纹理（下采样趟 storage 写 / copy 源）
    WGPUBuffer hz_rb_pixels_ = nullptr;///< dst 纹理像素回读缓冲（MapRead|CopyDst）

    // --- B3b-6 Hi-Z storage-image 金字塔 compute 真链路自检（每会话一次：单张 R32Float mip 链纹理，
    //   ①生成趟 textureStore 已知渐变到 mip0；②逐级下采样趟用 textureLoad 读 mip[k-1] 采样视图 + 取
    //   2×2 max 写 mip[k] storage 视图（per-mip 显式视图绑定）；copy 各级 mip→回读缓冲，逐级逐像素
    //   校验 == CPU 预期递归 max。验证 per-mip 视图绑定 + 多级 storage 金字塔构建（GPU-driven Hi-Z
    //   遮挡剔除金字塔的核心原语，Task 4 前置）。离屏隔离，不翻转能力位、不碰 demo golden）---
    bool hizpyr_selftest_done_ = false;
    bool RecordHiZPyramidSelfTest();        ///< 录制 ①生成趟 + ②逐级下采样趟 compute + copy 各级 mip→回读缓冲
    void KickHiZPyramidSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int hzp_gen_shader_ = 0;       ///< 生成趟 compute shader（textureStore 渐变到 mip0）
    unsigned int hzp_down_shader_ = 0;      ///< 下采样趟 compute shader（textureLoad mip[k-1] + 2×2 max → mip[k]）
    unsigned int hzp_gen_ubo_ = 0;          ///< 生成趟参数 UBO（mip0 dim，group1 b0）
    std::vector<unsigned int> hzp_down_ubos_;  ///< 各下采样级参数 UBO（src_dim/dst_dim，group1 b0）
    unsigned int hzp_tex_ = 0;              ///< 单张 R32Float mip 链纹理（mip0 生成 + 逐级下采样 + copy 源）
    WGPUBuffer hzp_rb_pixels_ = nullptr;    ///< 各级 mip 像素回读缓冲（MapRead|CopyDst）

    // --- B3b-8 命名 uniform + compute 采样器绑定 真链路自检（每会话一次：手写 WGSL compute 经
    //   SetComputeUniform*（命名 i32/f32/vec2i/vec4/mat4，group1 保留 binding 命名块）+
    //   SetComputeTextureSampler（group2 b0，textureLoad 读已知渐变纹理）读入参数与纹理 →
    //   结果写 SSBO → copy SSBO→回读缓冲 → 逐元素校验。验证引擎 Hi-Z/GPU cull 真实 compute API 面
    //   （命名 uniform 块布局 + 句柄采样绑定）。离屏隔离，不翻转能力位、不碰 demo golden）---
    bool compute_bind_selftest_done_ = false;
    bool RecordComputeBindSelfTest();        ///< 录制 命名 uniform + 采样器绑定 compute + copy SSBO→回读缓冲
    void KickComputeBindSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int cb_shader_ = 0;             ///< compute shader（读命名 uniform 块 + textureLoad 采样纹理）
    unsigned int cb_tex_ = 0;                ///< 已知渐变 rgba8unorm 采样纹理（SetComputeTextureSampler 绑定）
    unsigned int cb_out_ = 0;                ///< 结果 SSBO（group3 b0）
    WGPUBuffer cb_rb_out_ = nullptr;         ///< 结果回读缓冲（MapRead|CopyDst）

    // --- B3b-9 Hi-Z 遮挡剔除真链路自检（每会话一次：手译引擎 HiZCullPass compute 为 WGSL —— AABB
    //   SSBO（group3 b0）8 角经命名 uniform u_view_projection 投影 → NDC/UV → off-screen 拒绝 →
    //   屏幕像素跨度选 mip → 5 tab Hi-Z（句柄采样，textureLoad-at-mip）max 深度遮挡判定 → 写可见性
    //   SSBO（group3 b1）→ copy 回读逐元素校验 == CPU 预期 [1,0,0,1]。证明该消费方着色器 WebGPU 可用。
    //   离屏隔离，不翻转能力位、不碰 demo golden）---
    bool hizcull_selftest_done_ = false;
    bool RecordHiZCullSelfTest();          ///< 录制 Hi-Z 剔除 compute + copy 可见性 SSBO→回读缓冲
    void KickHiZCullSelfTestReadback();    ///< 提交后发起异步 map 回读校验
    unsigned int hc_shader_ = 0;           ///< Hi-Z 剔除 compute shader（手译 HiZCullPass）
    unsigned int hc_aabb_ = 0;             ///< AABB SSBO（group3 b0）
    unsigned int hc_vis_ = 0;              ///< 可见性 SSBO（group3 b1）
    unsigned int hc_hiz_tex_ = 0;          ///< Hi-Z r32float 纹理（SetComputeTextureSampler 绑定）
    WGPUBuffer hc_rb_out_ = nullptr;       ///< 可见性回读缓冲（MapRead|CopyDst）

    // --- B3b-10 形变目标（morph）真链路自检（每会话一次：手译引擎 MorphTargetSystem compute 为 WGSL ——
    //   base 顶点 SSBO（group3 b0）+ delta SSBO（b1）+ weights SSBO（b2）经命名 uniform 顶点/目标数 →
    //   Σ weight·delta → normalize 法线 → 写形变顶点 SSBO（b3）→ copy 回读逐顶点校验 == CPU 预期。
    //   离屏隔离，不翻转能力位、不碰 demo golden）---
    bool morph_selftest_done_ = false;
    bool RecordMorphSelfTest();            ///< 录制 morph compute + copy 形变顶点 SSBO→回读缓冲
    void KickMorphSelfTestReadback();      ///< 提交后发起异步 map 回读校验
    unsigned int mf_shader_ = 0;           ///< morph compute shader（手译 MorphTargetSystem）
    unsigned int mf_base_ = 0;             ///< base 顶点 SSBO（group3 b0）
    unsigned int mf_delta_ = 0;            ///< morph delta SSBO（group3 b1）
    unsigned int mf_weight_ = 0;           ///< weights SSBO（group3 b2）
    unsigned int mf_out_ = 0;              ///< 形变顶点输出 SSBO（group3 b3）
    WGPUBuffer mf_rb_out_ = nullptr;       ///< 形变顶点回读缓冲（MapRead|CopyDst）

    /// B3b-6：把显式纹理视图绑到 compute group2 槽（read_only=true→采样读；false→storage 写）。
    void SetComputeImageViewExplicit(uint32_t binding, WGPUTextureView view, WGPUTextureFormat format,
                                     WGPUTextureViewDimension view_dim, bool read_only);

    RenderStats last_frame_stats_{};
};

} // namespace render
} // namespace dse

#endif // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif // DSE_WEBGPU_RHI_DEVICE_H
