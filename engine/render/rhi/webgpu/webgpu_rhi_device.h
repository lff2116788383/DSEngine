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
 * 覆盖资源（缓冲/纹理/采样/绑定组/PSO）、WGSL 着色器、绘制与 compute + SSBO 全路径，
 * 复用桌面 GPU-driven / Clustered Forward+ / 延迟链路。
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

#ifdef DSE_WEBGPU_SELFTEST
class WebGpuSelfTestHarness;
#endif

/**
 * @class WebGPURhiDevice
 * @brief RHI 的 WebGPU 实现。
 *
 * 持有 WGPUDevice/Queue/Surface/SwapChain，把设备级命令录制到本帧 frame_encoder_
 * 并在 EndFrame 提交。覆盖缓冲/纹理/采样/绑定组/PSO、WGSL 着色器、绘制与 compute + SSBO。
 */
class WebGPURhiDevice final : public RhiDevice {
public:
    WebGPURhiDevice();
    ~WebGPURhiDevice() override;

    // --- 设备生命周期 ---
    RenderDeviceInfo GetDeviceInfo() const override;
    /// MRT 上限：AcquireDevice 经 wgpuDeviceGetLimits 读取适配器实际
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

    // --- 内建资源（手写 WGSL，经通用原语上屏）---
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
    // 资源表条目（句柄 → WebGPU 原生对象）
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
    /// 名见 vs_entry/fs_entry）。仅内建 WGSL 程序携带 module。
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

    // --- 内部访问器（供 WebGPUCommandBuffer 录制用）---
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
    // 设备级命令录制 API（WebGPUCommandBuffer 逐调用转发至此）
    // ============================================================
    // 录制直接落到本帧 frame_encoder_（BeginFrame 创建、EndFrame 提交），故 CommandBuffer
    // 端是「立即转发」而非「缓存重放」：Begin/End RenderPass 在 frame_encoder_ 上开关
    // WGPURenderPassEncoder，Bind*/PushConstants 累积当前绘制状态，Draw* 惰性组装 PSO 并发起。
    // CmdClearColor 为有意 no-op（清屏由 RenderPassDesc.clear_* 的 loadOp=Clear 表达）；
    // CmdDispatchComputePass 为门控 no-op（唯一调用方 bloom 在 GetBloomComputeShader()==0 回退
    // 全屏 quad）；CmdDrawIndexedIndirect / MultiDrawIndexedIndirect 为真实 indirect 绘制。

    void CmdBeginRenderPass(const RenderPassDesc& desc);
    void CmdEndRenderPass();
    void CmdClearColor(const glm::vec4& color);
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
    // Compute 能力：SupportsCompute()/SupportsSSBOCompute() 均为 true。WebGPU 经
    //   CreateComputeShaderEx 接受手写 WGSL（见下），DispatchCompute 走 explicit-layout 管线
    //   （group1=UBO、group3=SSBO）。消费方按 compute shader 句柄 0 优雅回退：
    //   - morph：已注入手译 WGSL（kMorphTargetCompWGSL）。
    //   - GPU-driven cull/Hi-Z：门控 SupportsIndirectDraw()/hiz_texture，按 gpu_driven profile 激活。
    //   - DDGI：仅 GLSL 源 → 句柄 0 → Init 优雅禁用（一次告警）。
    //   - grass/hair：未传 WGSL → 句柄 0 → CPU 回退（hair 首次失败置 latch 永久跳过）。
    //   skinning 已接好 WGSL compute 通路（GPU→GPU，靠 pass 间 usage 转换屏障保证顺序），但
    //   GPUSkinningSystem 当前休眠：无生产者 Submit、WGSL 渲染端无 u_skinned==3 消费路径、demo 无
    //   蒙皮网格 → 当前帧像素零变化；真实可见激活留待接入生产者 + WGSL 渲染路径。
    // ============================================================
    bool SupportsCompute() const override { return true; }
    bool SupportsSSBOCompute() const override { return true; }


    /// 创建 compute shader。仅接受 WGSL（首非空行须为 `// dse-wgsl` 标记）：引擎 GLSL/SPIR-V
    /// compute 源无离线转译，返回 0 跳过。返回设备级 compute shader 句柄（0=失败）。
    unsigned int CreateComputeShader(const std::string& source) override;
    /// 多源 compute 创建——WebGPU 仅取 wgsl_src（手写 WGSL）。wgsl_src 为空表示该 compute
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
    /// 创建可供 compute 写入的 2D 纹理（storage image / UAV）。WebGPU 以
    /// StorageBinding|CopySrc|TextureBinding 用法位 + rgba8unorm 格式建纹理，供 compute
    /// `texture_storage_2d<rgba8unorm, write>` 写入 + 随后采样/回读（Hi-Z 金字塔/bloom 基元）。
    unsigned int CreateComputeWriteTexture2D(int width, int height) override;
    /// 将 storage image 绑定到 compute（group2，binding=unit）。read_only 暂忽略
    /// （WebGPU rgba8unorm storage 仅 write 访问；只读用例走采样路径）。
    void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) override;
    /// 按句柄 + mip 级绑定单 mip 视图到 compute group2 槽（引擎 Hi-Z build 真实 API 面）。
    /// read_only=true→采样读（texture_2d<f32>，textureLoad）；false→storage 写（texture_storage_2d）。
    /// 内部对 (句柄,mip) 缓存单层单 mip 视图后经 SetComputeImageViewExplicit 路由（绕开默认全 mip 视图）。
    void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                   int mip_level, bool read_only, bool r32f = false) override;
    /// 按句柄绑定纹理到 compute 采样单元（group2 binding=unit，texture_2d<f32>，textureLoad）。
    /// 引擎 Hi-Z / GPU-driven 剔除经此面读 Hi-Z/深度纹理（手译 WGSL 用 textureLoad 取代 textureLod）。
    void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) override;
    /// 命名 compute uniform 设置面（引擎 Hi-Z/GPU cull 经名字设 compute 标量/向量/矩阵参数）。
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
    // compute/graphics SSBO 消费方（morph 等）经 BindGpuBuffer 绑定。路由到 cur_ssbos_
    //   （与 CmdBindStorageBuffer 同槽位图，group3，size=0 表整缓冲）。基类默认走 deprecated BindSSBO
    //   （no-op），翻转 SupportsCompute() 后若不覆写则消费方 SSBO 绑定全丢失。writable 在 WebGPU
    //   compute 下无意义（storage 统一 read_write）。
    void BindGpuBuffer(BufferHandle handle, uint32_t binding_point) override;
    void BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable) override;
    void DeleteGpuBuffer(BufferHandle handle) override;

    // WebGPU GPU→CPU 异步「双缓冲延迟回读」覆写。基类默认=同步 ReadGpuBuffer+返回 true
    //   （桌面 GL/VK/DX11 由此保持原语义，不覆写）；但 WebGPU 无同步 buffer 回读（map 异步），
    //   ReadGpuBuffer 在 WebGPU 是 no-op，同步回读读不到数据。本覆写：BeginGpuReadback 在
    //   frame_encoder_（须不在任何 pass 内）上 CopyBufferToBuffer(src→staging[write])、记 pending、
    //   write^=1，并**返回上一帧结果是否就绪**；EndFrame 在 wgpuQueueSubmit 后对 pending staging 发
    //   wgpuBufferMapAsync，回调把数据拷进 async_rb_result_ 并置 ready；GetLastReadbackResult 返回它。
    //   消费方（grass 风场）据 ready 用上帧结果、否则当帧 CPU 回退（1 帧延迟视觉无感）。
    bool BeginGpuReadback(BufferHandle handle, size_t offset, size_t size) override;
    const void* GetLastReadbackResult(size_t* out_size = nullptr) const override;

    // GPU-Driven 执行器（按已绑引擎 draw state 落地）：WebGPU 无 glMultiDrawElementsIndirect，
    //   按当前已绑管线/顶点/索引缓冲循环逐条 wgpuRenderPassEncoderDrawIndexedIndirect，第 i 条从
    //   byte_offset + i*stride 读取 [indexCount,instanceCount,firstIndex,baseVertex,baseInstance]。
    //   instance_count=0（被剔）→ 硬件不绘制。
    void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride,
                                  size_t byte_offset = 0) override;

    // GPU-driven Hi-Z 剔除 + PBR 经手译 WGSL（kHiZCopy/Downsample/Cull/GPUCull，含深度纹理 compute
    //   采样 texture_depth_2d）。门控 gpu_driven profile（如 CustomLiteLua）方激活；默认 Forward3D
    //   （gpu_driven=false）中性不回归。
    bool SupportsIndirectDraw() const override { return true; }

    // Mega VAO——WebGPU 无 VAO 对象，记录 VBO/IBO 句柄 + BatchVertex 92B 布局；
    //   BindMegaVAO 据记录经引擎-facing CmdBindVertexBuffer(92B 7 属性)/CmdBindIndexBuffer 设 draw state。
    VertexArrayHandle CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                    BufferHandle& out_vbo, BufferHandle& out_ibo) override;
    void UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) override;
    void UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) override;
    void DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) override;
    void BindMegaVAO(VertexArrayHandle vao) override;
    void UnbindVAO() override;

    // GPU-driven PBR——手译 PBR WGSL（PerFrame/PerScene UBO + 实例 SSBO(binding5) 取 model
    //   + 材质 SSBO(binding9) + albedo 纹理桶）。SetupGPUDrivenPBRShader 激活程序/PSO + 上传并绑 UBO；
    //   BindGPUDrivenTextures 绑 albedo（handle=0 → 默认白）；HasGPUDrivenPBRShader 报告编译可用性。
    bool HasGPUDrivenPBRShader() const override;
    void SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                 const glm::vec3& camera_pos,
                                 const glm::vec3& light_dir, const glm::vec3& light_color,
                                 float light_intensity, float ambient_intensity,
                                 float shadow_strength = 0.0f) override;
    void BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                               unsigned int metallic_roughness,
                               unsigned int emissive, unsigned int occlusion) override;

    // Hi-Z 遮挡剔除资源——CreateHiZTexture 建 R32Float 完整 mip 链纹理（nearest 过滤，
    //   usage=storage 写 + 采样读 + copy），供 HiZBuildPass 逐级 2×2 max 下采样填充、HiZCullPass 采样判遮挡。
    //   GetHiZGpuTexture 返回引擎纹理句柄（消费方传给 SetComputeTextureImageMip/SetComputeTextureSampler）。
    unsigned int CreateHiZTexture(int width, int height) override;
    void DeleteHiZTexture(unsigned int handle) override;
    int GetHiZMipCount(unsigned int handle) const override;
    unsigned int GetHiZGpuTexture(unsigned int handle) const override;

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

    // --- 资源创建内部助手 ---
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

    // --- GPU→CPU 异步双缓冲延迟回读状态（BeginGpuReadback/GetLastReadbackResult 覆写用）---
    //   两个 MapRead|CopyDst staging 轮换：本帧 CopyBufferToBuffer 写 staging[write]，下帧把
    //   上帧的 staging map 回读 → 避免「同帧写后立即 map」的栅栏停顿；EndFrame 提交后 kick map。
    WGPUBuffer async_rb_staging_[2] = {nullptr, nullptr};  ///< 轮换 staging 缓冲
    size_t     async_rb_capacity_[2] = {0, 0};             ///< 各 staging 当前容量（按需扩容）
    int        async_rb_write_idx_ = 0;                    ///< 本帧写入的 staging 下标（0/1 轮换）
    bool       async_rb_has_pending_ = false;              ///< 是否有一帧拷贝待 map 回读
    size_t     async_rb_pending_size_ = 0;                 ///< 待 map 的字节数
    int        async_rb_pending_idx_ = 0;                  ///< 待 map 的 staging 下标
    bool       async_rb_mapped_[2] = {false, false};       ///< 各 staging 是否正处于异步 map 飞行中
    bool       async_rb_ready_ = false;                    ///< async_rb_result_ 是否含有效（上帧）数据
    std::vector<uint8_t> async_rb_result_;                 ///< 最近一次成功 map 的回读数据
    void KickDeferredReadback();                           ///< EndFrame 提交后对 pending staging 发 mapAsync
    struct DeferredReadbackCtx { WebGPURhiDevice* dev; int idx; size_t size; };
    static void OnDeferredReadbackMapped(WGPUBufferMapAsyncStatus status, void* userdata);

    int width_  = 0;
    int height_ = 0;
    bool initialized_ = false;
    int max_color_attachments_ = 8;  ///< wgpuDeviceGetLimits 探测填充（WebGPU 规范默认 8）

    // 单调递增句柄发号器（0 保留为「无效句柄」，各资源表共享同一序号空间）
    unsigned int next_handle_ = 1;
    unsigned int NextHandle() { return next_handle_++; }

    // 资源表（句柄 → 原生对象）
    std::unordered_map<unsigned int, BufferEntry>       buffers_;
    std::unordered_map<unsigned int, TextureEntry>      textures_;
    std::unordered_map<unsigned int, RenderTargetEntry> render_targets_;
    std::unordered_map<unsigned int, PipelineStateDesc> pipeline_states_;
    std::unordered_map<unsigned int, ShaderEntry>       shaders_;

    // ============================================================
    // 命令录制状态与缓存
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
        bool sampler_nonfiltering = false;  ///< 该采样器须为 NonFiltering（深度纹理配非过滤采样器，如点光 cube 阴影）
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

    // --- 录制：当前 pass 状态 ---
    WGPURenderPassEncoder cur_pass_ = nullptr;
    bool cur_pass_is_backbuffer_ = false;
    std::vector<WGPUTextureFormat> cur_color_formats_;
    WGPUTextureFormat cur_depth_format_ = WGPUTextureFormat_Undefined;
    uint32_t cur_sample_count_ = 1;
    uint32_t cur_rt_width_ = 0;   ///< 当前 pass 渲染目标宽（视口裁剪用）
    uint32_t cur_rt_height_ = 0;  ///< 当前 pass 渲染目标高
    std::vector<WGPUTextureView> cur_pass_views_;  ///< 本 pass 临时创建的面视图，pass 结束释放
    // 当前 pass 的附件纹理句柄（颜色+深度）。用于 CollectGroupBindings 检测读写危险：若某采样 slot
    // 绑定的纹理同时是本 pass 的可写附件（如 shadow atlas 的 Depth32 既作深度附件又被 slot11 采样），
    // 则替换为只读回退纹理，规避 WebGPU「同一同步作用域内同时可写与被采样」校验失败。
    std::vector<unsigned int> cur_pass_attachment_texs_;

    // --- 录制：当前绘制绑定（跨 Draw 持续，BeginRenderPass 时重置）---
    unsigned int cur_pso_handle_ = 0;
    unsigned int cur_program_   = 0;
    std::vector<VbBinding> cur_vbs_;
    unsigned int cur_ib_handle_ = 0;
    WGPUIndexFormat cur_ib_format_ = WGPUIndexFormat_Uint16;
    std::map<uint32_t, UboBinding>  cur_ubos_;
    std::map<uint32_t, TexBinding>  cur_texs_;
    std::map<uint32_t, SsboBinding> cur_ssbos_;
    std::map<uint32_t, unsigned int> cur_compute_images_;  ///< compute storage image（binding→纹理句柄）
    std::map<uint32_t, unsigned int> cur_compute_textures_;  ///< compute 只读采样纹理（textureLoad，binding→纹理句柄）
    // compute 显式纹理视图绑定（per-mip）。Hi-Z 金字塔逐级下采样需把同一纹理的不同 mip
    //   作采样读/storage 写，故绕开「句柄→默认全 mip 视图」走显式单 mip 视图。
    struct ComputeViewBind {
        WGPUTextureView         view     = nullptr;
        WGPUTextureFormat       format   = WGPUTextureFormat_Undefined;
        WGPUTextureViewDimension view_dim = WGPUTextureViewDimension_2D;
    };
    std::map<uint32_t, ComputeViewBind> cur_compute_image_views_;    ///< storage 写显式视图
    std::map<uint32_t, ComputeViewBind> cur_compute_texture_views_;  ///< 采样读显式视图
    // (纹理句柄<<16 | mip) → 该纹理单层单 mip 视图缓存（SetComputeTextureImageMip 复用，
    //   随纹理删除/Shutdown 释放）。避免每次 dispatch 重建视图。
    std::map<uint64_t, WGPUTextureView> compute_mip_views_;
    void InvalidateComputeMipViews(unsigned int texture_handle);  ///< 删纹理时清该句柄全部 mip 视图
    // SetComputeUniform* 命名 uniform 累积（按调用序 16B 对齐定位，整块经 UBO 版本环上传到
    //   group1 保留 binding；每次 DispatchCompute 后清空，与 GL/DX11/Vulkan 同语义）。手译 WGSL 须
    //   声明同名同序、各成员 @align(16) 的 uniform 块到该保留 binding。
    static constexpr uint32_t kComputeNamedUboBinding = 8;
    // GL 分「采样器」「图像」两套 binding 命名空间，同 binding 号（如 Hi-Z copy 的 sampler2D 深度 @0
    //   与 image2D hiz_mip0 @0）不冲突；WebGPU group2 为单一命名空间，须错开。仅当某 storage image 槽
    //   与某采样纹理槽冲突时，把该 storage image 挪到 slot + kComputeStorageBindingBase（手译 WGSL 须
    //   把对应 texture_storage_2d 声明到该错开 binding）；无冲突的 storage 仍按原 slot，既有自检不受影响。
    static constexpr uint32_t kComputeStorageBindingBase = 8;
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


    // --- compute 基础设施 ---
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


    /// 低层缓冲创建（mappedAtCreation 上传 + 登记 buffers_）；CreateBuffer/CreateGpuBuffer 共用。
    unsigned int CreateBufferRaw(size_t logical_size, const void* data,
                                 WGPUBufferUsageFlags usage, bool is_index);

    // --- 内建 WGSL 程序缓存 ---
    /// 按 key 懒创建（经 CreateShaderProgram 编译 WGSL）并缓存内建程序句柄，重复取用零开销。
    unsigned int GetOrCreateWgslProgram(const std::string& key, const std::string& wgsl);
    std::unordered_map<std::string, unsigned int> wgsl_program_cache_;
    unsigned int skybox_cube_vbo_ = 0;  ///< 内建天空盒 36 顶点立方体 VBO（懒初始化）
    std::set<unsigned int> logged_incomplete_programs_;  ///< 已告警过的缺绑定程序（去重日志）

    // --- 5.1b：CSM shadow atlas（slot11）恒亮 Depth32 回退纹理 ---
    //   消费方 mesh_renderer 在无 shadow map 时给 slot11 绑「白 RGBA8」（sampleType=Float），与前向
    //   WGSL 的 texture_depth_2d（sampleType=Depth）声明在同一 PSO 的 BGL 上不可兼容。这里备一张 1×1
    //   Depth32 纹理并清深=1.0（恒无遮挡），在 CollectGroupBindings 处把 slot11 的非深度回退替换为它，
    //   使该 binding 的 sampleType 在所有 draw 上恒为 Depth。真 atlas 已是 Depth32，不受影响。
    unsigned int shadow_fallback_rt_ = 0;          ///< 1×1 depth-only RT（懒建）
    unsigned int shadow_fallback_depth_tex_ = 0;   ///< 其 Depth32 纹理句柄（slot11 回退）
    bool shadow_fallback_cleared_ = false;         ///< 是否已清深=1.0（一次性）
    void EnsureShadowDepthFallback();              ///< 懒建并一次性清深=1.0（须在无活动 pass 时调用）
    // Final-Feat-8：点光源 cube 阴影 1×1 Depth32 回退（恒亮，距离=1→无遮挡）。聚光灯 2D 阴影复用上面的
    //   shadow_fallback_depth_tex_（同为 Depth32 2D）。点光为 cube 维度，须单独建 6 面 Depth32 cube。
    unsigned int point_shadow_fallback_rt_ = 0;        ///< 1×1×6 depth-only cube RT（懒建）
    unsigned int point_shadow_fallback_tex_ = 0;       ///< 其 Depth32 cube 纹理句柄（slot16-19 回退）
    bool point_shadow_fallback_cleared_ = false;       ///< 6 面是否已清深=1.0（一次性）
    WGPUSampler nonfilter_sampler_ = nullptr;          ///< 非过滤采样器（深度 cube 阴影采样所需）
    void EnsurePointShadowFallback();                  ///< 懒建 cube 回退并一次性清 6 面深=1.0（须在无活动 pass 时调用）

    // --- 录制内部助手 ---
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














    // --- Mega VAO 句柄记录（WebGPU 无 VAO 对象，仅记 VBO/IBO 句柄）---
    struct MegaVaoEntry { unsigned int vbo = 0; unsigned int ibo = 0; };
    std::unordered_map<unsigned int, MegaVaoEntry> mega_vaos_;  ///< VAO 句柄 → {vbo,ibo}
    unsigned int next_mega_vao_id_ = 1;                         ///< VAO 句柄发号器（0 表无效）


    // --- GPU-driven PBR 程序/PSO/默认白纹理/PerFrame·PerScene UBO（惰性创建，跨帧复用）---
    bool EnsureGpuDrivenPBRShader();             ///< 惰性编译手译 PBR WGSL 程序 + 建 PSO + 默认白纹理 + UBO
    unsigned int gpu_driven_pbr_program_  = 0;   ///< 手译 PBR WGSL 程序（vs+fs 同源）
    unsigned int gpu_driven_pbr_pso_      = 0;   ///< PBR PSO（depth test/write on、cull none、blend off）
    bool         gpu_driven_pbr_failed_   = false;  ///< 程序编译失败标记（避免每帧重试）
    BufferHandle gpu_driven_perframe_ubo_{};     ///< PerFrame UBO（group1 b0：vp/view/camera_pos）
    BufferHandle gpu_driven_perscene_ubo_{};     ///< PerScene UBO（group1 b1：light_dir/color/params）
    unsigned int white_texture_ = 0;             ///< 1×1 默认白纹理（BindGPUDrivenTextures handle=0 回退）


    // --- Hi-Z 纹理登记表（hiz 句柄 → 引擎纹理句柄，R32Float 完整 mip 链）---
    std::unordered_map<unsigned int, unsigned int> hiz_textures_;  ///< hiz_handle → 引擎纹理句柄







    /// 把显式纹理视图绑到 compute group2 槽（read_only=true→采样读；false→storage 写）。
    void SetComputeImageViewExplicit(uint32_t binding, WGPUTextureView view, WGPUTextureFormat format,
                                     WGPUTextureViewDimension view_dim, bool read_only);


    // --- 自检 harness（诊断代码，外置 + 编译期门控 DSE_WEBGPU_SELFTEST，默认关闭）---
#ifdef DSE_WEBGPU_SELFTEST
    friend class WebGpuSelfTestHarness;
    WebGpuSelfTestHarness* selftest_ = nullptr;
#endif
    RenderStats last_frame_stats_{};
};

} // namespace render
} // namespace dse

#endif // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif // DSE_WEBGPU_RHI_DEVICE_H
