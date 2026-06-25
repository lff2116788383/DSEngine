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
    //   手写 WGSL；自检即经此入口验证整条引擎-facing 通路（B3a + B3b-2..13 离屏自检全 PASS）。
    // Task 3：逐消费方门控审计通过后翻转 SupportsCompute()=true。审计结论（仅翻 SupportsCompute()，
    //   保持 SupportsSSBOCompute()=false / SupportsIndirectDraw()=false / CreateHiZTexture()=0）：
    //   - morph：已注入手译 WGSL（kMorphTargetCompWGSL，B3b-10 自检证实算法 + 调度命名 uniform/SSBO
    //     绑定布局），翻转后真消费方路径可用。
    //   - GPU-driven cull/Hi-Z：门控 SupportsIndirectDraw()（false）/ hiz_texture==0（CreateHiZTexture
    //     未覆写返回 0）→ 不激活。
    //   - DDGI：CreateComputeShader(GLSL) 无 WGSL → 返回 0 → Init 优雅失败禁用（仅一次告警）。
    //   - 管线裁剪：无任何 pass 置 requires_compute=true → 翻转对渲染图为中性（harness 双后端不回归）。
    //
    // Task 5（SupportsSSBOCompute 激活）：GPU skinning 已注入手译 WGSL
    //   （gpu_skinning.cpp::kSkinningComputeWGSL，B3b-3 蒙皮离屏自检证实算法 + 命名 uniform group1/b8
    //   + SSBO group3/b0..4），据此如实翻 SupportsSSBOCompute()=true：WebGPU 确实支持 SSBO compute，
    //   且蒙皮渲染数据流是 GPU→GPU（compute 写 dst SSBO → draw 在 GPU 读），靠 WebGPU pass 间自动
    //   usage 转换屏障保证顺序，不需 CPU 回读机制；可选的 GetSkinnedOutput CPU 回读 API（ReadBackPrevFrame）
    //   仍按既有异步 map 走，不阻塞渲染主路径。句柄 0 优雅回退已审计安全——grass/hair/skinning 三消费方
    //   均门控本位：
    //   - grass：CreateComputeShaderEx 未传 WGSL（第 8 槽默认 ""）→ 返回 0 → gpu_compute_enabled_=false
    //     → CPU 风场回退（grass_system.cpp）。
    //   - hair：同未传 WGSL → 返回 0；HairInstance 首次失败置 compute_unavailable_ latch 永久跳过
    //     （不再每帧重试刷屏）。
    //   - skinning：已传 WGSL → Init 可成功，但整套系统当前为休眠态。
    //   CAVEAT（如实标注，非可见激活）：GPUSkinningSystem 休眠——Submit() 全仓库零调用 → pending_requests_
    //   恒空 → Dispatch() 被 GetTotalSkinnedVertices()>0 门控挡住永不执行；WGSL 渲染端无 u_skinned==3 /
    //   ComputeSkinBuf(binding20) 消费路径（仅 GLSL pbr.vert/shadow.vert 有）；demo 无蒙皮网格。故本翻位
    //   属能力声明（如实暴露 WebGPU SSBO compute 能力 + 接好 skinning WGSL 通路），当前帧像素零变化、
    //   harness 双后端不回归。真实可见激活留待接入生产者（mesh_render_system→Submit）+ WGSL u_skinned==3
    //   渲染路径，超出本能力位翻转范围。
    // ============================================================
    bool SupportsCompute() const override { return true; }
    bool SupportsSSBOCompute() const override { return true; }


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
    // Task 3：compute/graphics SSBO 消费方（morph 等）经 BindGpuBuffer 绑定。路由到 cur_ssbos_
    //   （与 CmdBindStorageBuffer 同槽位图，group3，size=0 表整缓冲）。基类默认走 deprecated BindSSBO
    //   （no-op），翻转 SupportsCompute() 后若不覆写则消费方 SSBO 绑定全丢失。writable 在 WebGPU
    //   compute 下无意义（storage 统一 read_write）。
    void BindGpuBuffer(BufferHandle handle, uint32_t binding_point) override;
    void BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable) override;
    void DeleteGpuBuffer(BufferHandle handle) override;

    // --- Task 4 GPU-Driven 执行器（按已绑引擎 draw state 逐子项落地，先离屏自检不翻能力位）---
    // Subtask 1：WebGPU 无 glMultiDrawElementsIndirect，按当前已绑管线/顶点/索引缓冲循环逐条
    //   wgpuRenderPassEncoderDrawIndexedIndirect，第 i 条从 byte_offset + i*stride 读取
    //   [indexCount,instanceCount,firstIndex,baseVertex,baseInstance]。instance_count=0（被剔）→ 硬件不绘制。
    void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride,
                                  size_t byte_offset = 0) override;

    // Subtask 6：四子项离屏自检全 PASS（MultiDrawIndexedIndirect / Mega VAO / GPU-driven PBR WGSL /
    //   Hi-Z 资源建链+遮挡剔除）+ 4 个引擎 compute shader 已注入手译 WGSL（kHiZCopy/Downsample/Cull/
    //   GPUCull WGSL，含深度纹理 compute 采样 texture_depth_2d + storage/sampler 错位 binding）+ 接入
    //   真实 frame_pipeline CreateComputeShaderEx 调用点后翻转。门控 gpu_driven profile（如 CustomLiteLua）
    //   方激活；默认 Forward3D（gpu_driven=false）中性不回归。
    bool SupportsIndirectDraw() const override { return true; }

    // Subtask 2：Mega VAO——WebGPU 无 VAO 对象，记录 VBO/IBO 句柄 + BatchVertex 92B 布局；
    //   BindMegaVAO 据记录经引擎-facing CmdBindVertexBuffer(92B 7 属性)/CmdBindIndexBuffer 设 draw state。
    VertexArrayHandle CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                    BufferHandle& out_vbo, BufferHandle& out_ibo) override;
    void UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) override;
    void UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) override;
    void DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) override;
    void BindMegaVAO(VertexArrayHandle vao) override;
    void UnbindVAO() override;

    // Subtask 3：GPU-driven PBR——手译 PBR WGSL（PerFrame/PerScene UBO + 实例 SSBO(binding5) 取 model
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

    // Subtask 4：Hi-Z 遮挡剔除资源——CreateHiZTexture 建 R32Float 完整 mip 链纹理（nearest 过滤，
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
    // 当前 pass 的附件纹理句柄（颜色+深度）。用于 CollectGroupBindings 检测读写危险：若某采样 slot
    // 绑定的纹理同时是本 pass 的可写附件（如 shadow atlas 的 Depth32 既作深度附件又被 slot11 采样），
    // 则替换为只读回退纹理，规避 WebGPU「同一同步作用域内同时可写与被采样」校验失败。
    std::vector<unsigned int> cur_pass_attachment_texs_;

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

    // --- 5.1b：CSM shadow atlas（slot11）恒亮 Depth32 回退纹理 ---
    //   消费方 mesh_renderer 在无 shadow map 时给 slot11 绑「白 RGBA8」（sampleType=Float），与前向
    //   WGSL 的 texture_depth_2d（sampleType=Depth）声明在同一 PSO 的 BGL 上不可兼容。这里备一张 1×1
    //   Depth32 纹理并清深=1.0（恒无遮挡），在 CollectGroupBindings 处把 slot11 的非深度回退替换为它，
    //   使该 binding 的 sampleType 在所有 draw 上恒为 Depth。真 atlas 已是 Depth32，不受影响。
    unsigned int shadow_fallback_rt_ = 0;          ///< 1×1 depth-only RT（懒建）
    unsigned int shadow_fallback_depth_tex_ = 0;   ///< 其 Depth32 纹理句柄（slot11 回退）
    bool shadow_fallback_cleared_ = false;         ///< 是否已清深=1.0（一次性）
    void EnsureShadowDepthFallback();              ///< 懒建并一次性清深=1.0（须在无活动 pass 时调用）

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

    // --- B3b-11 DDGI 探针更新核心真链路自检（每会话一次：手译引擎 DDGISystem probe-update compute 核心为
    //   WGSL —— probe SSBO（group3 b0）+ 3×RSM 句柄采样（group2 b2/b3/b4，textureLoad）+ 14 命名 uniform →
    //   octahedral 方向 + VPL 累积间接辐照度 → 归一化×0.01 → 写 irradiance/visibility storage image（group2
    //   b0/b1）+ float SSBO 调试输出（group3 b1）→ copy 回读逐 texel 校验 == CPU 预期。离屏隔离、不翻能力位。
    //   注：DDGI 翻转前另需消费方适配 storage/sampler 绑定槽错开 + temporal imageLoad 的 read-write storage）---
    bool ddgi_selftest_done_ = false;
    bool RecordDDGISelfTest();             ///< 录制 DDGI probe-update compute + copy 调试 SSBO→回读缓冲
    void KickDDGISelfTestReadback();       ///< 提交后发起异步 map 回读校验
    unsigned int dg_shader_ = 0;           ///< DDGI probe-update compute shader（手译 DDGISystem 核心）
    unsigned int dg_probe_ = 0;            ///< 探针状态 SSBO（group3 b0）
    unsigned int dg_dbg_ = 0;              ///< 每 texel 调试输出 SSBO（group3 b1，irr.rgb+权重）
    unsigned int dg_irr_tex_ = 0;          ///< irradiance storage image（group2 b0）
    unsigned int dg_vis_tex_ = 0;          ///< visibility storage image（group2 b1）
    unsigned int dg_rsm_pos_ = 0;          ///< RSM 位置采样纹理（group2 b2）
    unsigned int dg_rsm_nrm_ = 0;          ///< RSM 法线采样纹理（group2 b3）
    unsigned int dg_rsm_flux_ = 0;         ///< RSM 通量采样纹理（group2 b4）
    WGPUBuffer dg_rb_out_ = nullptr;       ///< 调试输出回读缓冲（MapRead|CopyDst）

    // --- B3b-12 头发物理 hair Verlet 积分核心真链路自检（每会话一次：手译引擎 HairInstance::Simulate
    //   真 compute（hair_compute_shaders.h::kHairIntegrateSource）Pass 1 为 WGSL —— 4×SSBO（group3 b0..3）
    //   pos_cur/pos_prev/pos_rest/strand_info + 12 命名 uniform（group1 b8）→ 根顶点固定、velocity·(1-damping)
    //   + 重力·dt² → 写回 pos_cur/pos_prev → copy 回读逐分量校验 == CPU 预期。离屏隔离、不翻能力位。---
    bool hair_selftest_done_ = false;
    bool RecordHairSelfTest();             ///< 录制 hair Verlet 积分 compute + copy pos_cur/pos_prev→回读缓冲
    void KickHairSelfTestReadback();       ///< 提交后发起异步 map 回读校验
    unsigned int hr_shader_ = 0;           ///< hair Verlet 积分 compute shader（手译 kHairIntegrateSource）
    unsigned int hr_cur_ = 0;              ///< pos_cur SSBO（group3 b0，读写）
    unsigned int hr_prev_ = 0;             ///< pos_prev SSBO（group3 b1，读写）
    unsigned int hr_rest_ = 0;             ///< pos_rest SSBO（group3 b2，仅 .w 判根）
    unsigned int hr_strand_ = 0;           ///< strand_info SSBO（group3 b3，pass1 不用，绑定齐全）
    WGPUBuffer hr_rb_out_ = nullptr;       ///< pos_cur+pos_prev 回读缓冲（MapRead|CopyDst）

    // --- B3b-13 bloom 双滤波 compute 真链路自检（每会话一次：手译引擎 BloomRenderer 真 compute
    //   （bloom_downsample.comp / bloom_upsample.comp，GLSL 450）核心为 WGSL —— ①gen compute 写已知
    //   公式渐变进 src8/usrc4/ubase4 rgba16f；②下采样 13-tap 加权（src8→down4）；③上采样 3×3 tent +
    //   按 blend 累加（usrc4+ubase4→up4）→ copy down4/up4 回读半精解码逐 texel 逐通道校验 == CPU 预期。
    //   离屏隔离、不翻能力位。注：上采样消费方真翻转前需 ping-pong（in-place imageLoad rgba16f 不支持））---
    bool bloom_selftest_done_ = false;
    bool RecordBloomSelfTest();            ///< 录制 gen + 下采样 + 上采样 compute + copy down4/up4→回读缓冲
    void KickBloomSelfTestReadback();      ///< 提交后发起异步 map 回读校验
    unsigned int bl_gen_shader_  = 0;      ///< 公式渐变生成 compute（kind 选公式，写 rgba16f storage）
    unsigned int bl_down_shader_ = 0;      ///< 下采样 13-tap compute（手译 bloom_downsample.comp）
    unsigned int bl_up_shader_   = 0;      ///< 上采样 3×3 tent + 累加 compute（手译 bloom_upsample.comp）
    unsigned int bl_src8_   = 0;           ///< 下采样源 rgba16f 8×8（gen 写 + 采样读）
    unsigned int bl_down4_  = 0;           ///< 下采样输出 rgba16f 4×4（storage 写 + copy 源）
    unsigned int bl_usrc4_  = 0;           ///< 上采样源 rgba16f 4×4（gen 写 + 采样读）
    unsigned int bl_ubase4_ = 0;           ///< 上采样 base rgba16f 4×4（gen 写 + 采样读，替代 in-place imageLoad）
    unsigned int bl_up4_    = 0;           ///< 上采样输出 rgba16f 4×4（storage 写 + copy 源）
    WGPUBuffer bl_rb_out_ = nullptr;       ///< down4+up4 回读缓冲（MapRead|CopyDst）

    // --- Task 4 Subtask 1 离屏自检（MultiDrawIndexedIndirect 真链路：预置 [1,0,1,0] indirect cmds →
    //   经引擎-facing CmdBeginRenderPass + CmdBind* + MultiDrawIndexedIndirect 渲到 64×64 离屏 RT →
    //   copyTextureToBuffer → 异步回读半精解码校验「可见象限有色、被剔象限为黑」。离屏隔离、不翻能力位）---
    bool t41_mdi_selftest_done_ = false;
    bool RecordMultiDrawIndirectSelfTest();        ///< 在 frame_encoder_ 上录制引擎-facing 间接绘制 + copy（须在无 render/compute pass 时调用）
    void KickMultiDrawIndirectSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int t41_rt_      = 0;          ///< 离屏 RT（引擎 CreateRenderTarget，RGBA16Float + CopySrc）
    unsigned int t41_program_ = 0;          ///< 内建 WGSL 程序（pos.xy@loc0 + color.rgb@loc1）
    unsigned int t41_pso_     = 0;          ///< PSO（无深度/无剔除/三角列表）
    unsigned int t41_vbo_     = 0;          ///< 4 象限 quad 顶点缓冲（pos.xy + color.rgb，stride 20）
    unsigned int t41_ibo_     = 0;          ///< 4 象限索引缓冲（每象限 6 索引，UInt32）
    unsigned int t41_indirect_ = 0;         ///< 预置 4 条 indirect cmd（instance_count=[1,0,1,0]）
    WGPUBuffer   t41_rb_pixels_ = nullptr;  ///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 4 Subtask 2：Mega VAO 句柄记录（WebGPU 无 VAO 对象，仅记 VBO/IBO 句柄）---
    struct MegaVaoEntry { unsigned int vbo = 0; unsigned int ibo = 0; };
    std::unordered_map<unsigned int, MegaVaoEntry> mega_vaos_;  ///< VAO 句柄 → {vbo,ibo}
    unsigned int next_mega_vao_id_ = 1;                         ///< VAO 句柄发号器（0 表无效）

    // --- Task 4 Subtask 2 离屏自检（CreateMegaVAO → UpdateMegaVBO/IBO 上传 4 象限 BatchVertex(92B) 几何 →
    //   BindMegaVAO 设 92B draw state → CmdDrawIndexed 渲到 64×64 离屏 RT → copy 回读半精解码校验
    //   4 象限各自颜色就位（即 92B 布局 pos@0/color@12 解析正确）。离屏隔离、不翻能力位）---
    bool t42_mega_selftest_done_ = false;
    bool RecordMegaVaoSelfTest();          ///< 录制引擎-facing Mega VAO 绑定 + 索引绘制 + copy
    void KickMegaVaoSelfTestReadback();    ///< 提交后发起异步 map 回读校验
    unsigned int t42_rt_      = 0;         ///< 离屏 RT（RGBA16Float + CopySrc）
    unsigned int t42_program_ = 0;         ///< BatchVertex 92B 布局 WGSL 程序（pos@loc0 + color@loc1）
    unsigned int t42_pso_     = 0;         ///< PSO（无深度/无剔除/三角列表）
    VertexArrayHandle t42_vao_{};          ///< 被测 Mega VAO 句柄
    BufferHandle t42_vbo_{};               ///< Mega VBO 句柄
    BufferHandle t42_ibo_{};               ///< Mega IBO 句柄
    WGPUBuffer   t42_rb_pixels_ = nullptr; ///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 4 Subtask 3：GPU-driven PBR 程序/PSO/默认白纹理/PerFrame·PerScene UBO（惰性创建，跨帧复用）---
    bool EnsureGpuDrivenPBRShader();             ///< 惰性编译手译 PBR WGSL 程序 + 建 PSO + 默认白纹理 + UBO
    unsigned int gpu_driven_pbr_program_  = 0;   ///< 手译 PBR WGSL 程序（vs+fs 同源）
    unsigned int gpu_driven_pbr_pso_      = 0;   ///< PBR PSO（depth test/write on、cull none、blend off）
    bool         gpu_driven_pbr_failed_   = false;  ///< 程序编译失败标记（避免每帧重试）
    BufferHandle gpu_driven_perframe_ubo_{};     ///< PerFrame UBO（group1 b0：vp/view/camera_pos）
    BufferHandle gpu_driven_perscene_ubo_{};     ///< PerScene UBO（group1 b1：light_dir/color/params）
    unsigned int white_texture_ = 0;             ///< 1×1 默认白纹理（BindGPUDrivenTextures handle=0 回退）

    // --- Task 4 Subtask 3 离屏自检（SetupGPUDrivenPBRShader 激活 PBR 程序+绑 PerFrame/PerScene UBO →
    //   BindGpuBuffer 实例 SSBO(b5,2 个 model 平移左右)+材质 SSBO(b9,红/绿 albedo) → BindMegaVAO 设 92B
    //   draw state → BindGPUDrivenTextures(白 albedo) → MultiDrawIndexedIndirect(1 cmd,instanceCount=2)
    //   渲到 64×64 离屏 RT → copy 回读半精解码校验左半红、右半绿。离屏隔离、不翻能力位）---
    bool t43_pbr_selftest_done_ = false;
    bool RecordGpuDrivenPBRSelfTest();        ///< 录制引擎-facing GPU-driven PBR indirect 绘制 + copy
    void KickGpuDrivenPBRSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int t43_rt_      = 0;            ///< 离屏 RT（RGBA16Float + 深度，CopySrc）
    VertexArrayHandle t43_vao_{};             ///< 单 quad Mega VAO
    BufferHandle t43_vbo_{};                  ///< Mega VBO
    BufferHandle t43_ibo_{};                  ///< Mega IBO
    BufferHandle t43_inst_ssbo_{};            ///< 实例 SSBO（2×GPUInstanceData，b5）
    BufferHandle t43_mat_ssbo_{};             ///< 材质 SSBO（2×GPUMaterialData，b9）
    BufferHandle t43_indirect_{};             ///< 1 条 indirect cmd（instanceCount=2）
    unsigned int t43_albedo_tex_ = 0;         ///< 白 albedo 纹理（验证纹理采样链路）
    WGPUBuffer   t43_rb_pixels_ = nullptr;    ///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 4 Subtask 4：Hi-Z 纹理登记表（hiz 句柄 → 引擎纹理句柄，R32Float 完整 mip 链）---
    std::unordered_map<unsigned int, unsigned int> hiz_textures_;  ///< hiz_handle → 引擎纹理句柄

    // --- Task 4 Subtask 4 离屏自检（经 CreateHiZTexture 真资源建 mip 链 → 引擎-facing
    //   SetComputeTextureImageMip 写 mip0 占位深度 + 逐级 2×2 max 下采样建金字塔 → HiZCullPass WGSL
    //   经 SetComputeTextureSampler(GetHiZGpuTexture) + GetHiZMipCount 采样判遮挡 → 回读可见性 SSBO
    //   校验近物可见/远物（被金字塔最大深度遮挡）剔除。离屏隔离、不翻 SupportsIndirectDraw()）---
    bool t44_hiz_selftest_done_ = false;
    bool RecordGpuDrivenHiZCullSelfTest();        ///< 录制 Hi-Z 资源建链 + 下采样 + 遮挡剔除 dispatch + copy
    void KickGpuDrivenHiZCullSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int t44_hiz_handle_ = 0;             ///< CreateHiZTexture 返回的 hiz 句柄
    unsigned int t44_gen_shader_ = 0;             ///< 写 mip0 占位深度的 compute
    unsigned int t44_down_shader_ = 0;            ///< 逐级 2×2 max 下采样 compute
    unsigned int t44_cull_shader_ = 0;            ///< HiZCullPass 手译 WGSL（采样金字塔判遮挡）
    BufferHandle t44_gen_ubo_{};                  ///< 生成趟 params（dim）
    std::vector<unsigned int> t44_down_ubos_;     ///< 各级下采样 params（src_dim/dst_dim）
    BufferHandle t44_aabb_{};                     ///< AABB SSBO（group3 b0）
    BufferHandle t44_vis_{};                      ///< 可见性 SSBO（group3 b1）
    WGPUBuffer   t44_rb_out_ = nullptr;           ///< 可见性回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 1 离屏自检（CSM 方向光阴影深度图采样：①阴影深度趟把中心遮挡 quad(z=0.3)
    //   渲入 32×32 Depth32 atlas；②前向趟把 atlas 作 texture_depth_2d 经 textureLoad 3×3 PCF 采样比较
    //   （receiverDepth=0.6）渲到 64×64 RGBA16Float RT → copy 回读校验 中心受遮挡为暗、四角受光为亮。
    //   证明「阴影 pass 写 atlas → 前向 pass 采样」的跨 pass 深度图采样能力，离屏隔离、不翻能力位）---
    bool t51_csm_selftest_done_ = false;
    bool RecordCSMShadowSelfTest();          ///< 录制 阴影深度趟 + 前向采样趟 + copy 颜色→回读缓冲
    void KickCSMShadowSelfTestReadback();    ///< 提交后发起异步 map 回读校验
    unsigned int t51_shadow_rt_   = 0;       ///< shadow atlas RT（含 Depth32 深度附件，TextureBinding 可采样）
    unsigned int t51_color_rt_    = 0;       ///< 离屏 color RT（RGBA16Float + CopySrc）
    unsigned int t51_occ_program_ = 0;       ///< 遮挡 quad 程序（写深度，pos.xyz@loc0）
    unsigned int t51_occ_pso_     = 0;       ///< 遮挡 PSO（depth test/write on、cull none）
    unsigned int t51_recv_program_ = 0;      ///< 前向接收程序（采样 atlas，pos.xy@loc0 + uv@loc1）
    unsigned int t51_recv_pso_    = 0;       ///< 前向 PSO（无深度/无剔除/blend off）
    unsigned int t51_occ_vbo_     = 0;       ///< 遮挡 quad 顶点缓冲（pos.xyz，stride 12）
    unsigned int t51_occ_ibo_     = 0;       ///< 遮挡 quad 索引缓冲（6 索引，UInt32）
    unsigned int t51_recv_vbo_    = 0;       ///< 全屏 quad 顶点缓冲（pos.xy + uv，stride 16）
    unsigned int t51_recv_ibo_    = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t51_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 2 离屏自检（延迟着色：①几何趟把中心 quad 渲入 64×64×3 MRT gbuffer
    //   （albedo/normal/position 三附件）；②全屏光照趟 textureLoad 3 张 gbuffer 做延迟光照渲到 64×64
    //   RGBA16Float RT → copy 回读校验 中心几何受光为红、四角空像素为黑。证明「几何趟写 MRT gbuffer →
    //   光照趟采样 gbuffer」的延迟着色能力，离屏隔离、不翻能力位，逻辑同 deferred_lighting.frag）---
    bool t52_deferred_selftest_done_ = false;
    bool RecordDeferredSelfTest();           ///< 录制 几何趟（MRT gbuffer）+ 光照趟 + copy 颜色→回读缓冲
    void KickDeferredSelfTestReadback();     ///< 提交后发起异步 map 回读校验
    unsigned int t52_gbuffer_rt_  = 0;       ///< gbuffer RT（3 个 RGBA16Float 颜色附件 albedo/normal/position）
    unsigned int t52_color_rt_    = 0;       ///< 离屏 color RT（RGBA16Float + CopySrc）
    unsigned int t52_geom_program_ = 0;      ///< 几何程序（写 MRT 3 附件，pos.xy@loc0）
    unsigned int t52_geom_pso_    = 0;       ///< 几何 PSO（无深度/无剔除/blend off）
    unsigned int t52_light_program_ = 0;     ///< 延迟光照程序（textureLoad 3 张 gbuffer，pos.xy@loc0）
    unsigned int t52_light_pso_   = 0;       ///< 光照 PSO（无深度/无剔除/blend off）
    unsigned int t52_geom_vbo_    = 0;       ///< 几何 quad 顶点缓冲（pos.xy，stride 8）
    unsigned int t52_geom_ibo_    = 0;       ///< 几何 quad 索引缓冲（6 索引，UInt32）
    unsigned int t52_light_vbo_   = 0;       ///< 全屏 quad 顶点缓冲（pos.xy，stride 8）
    unsigned int t52_light_ibo_   = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t52_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 3 离屏自检（HDR auto-exposure 亮度归约 + ACES tonemap：①渲已知 HDR(4,2,1)
    //   到 8×8 场景 RT；②归约趟 textureLoad 整张算平均 log 亮度写 1×1 lum RT；③lum_adapt 趟 0.18/avgLum
    //   曝光写 1×1 exposure RT；④tonemap 趟 ACES(hdr*exposure)+gamma 渲 64×64 RT → 回读 C++ 同公式复算
    //   逐通道校验。逻辑同 lum_compute/lum_adapt/tonemapping.frag，离屏隔离、不翻能力位）---
    bool t53_hdr_selftest_done_ = false;
    bool RecordHDRSelfTest();                ///< 录制 场景趟 + 归约趟 + lum_adapt 趟 + tonemap 趟 + copy
    void KickHDRSelfTestReadback();          ///< 提交后发起异步 map 回读校验
    unsigned int t53_scene_rt_    = 0;       ///< HDR 场景 RT（8×8 RGBA16Float）
    unsigned int t53_lum_rt_      = 0;       ///< 平均 log 亮度 RT（1×1 RGBA16Float）
    unsigned int t53_exposure_rt_ = 0;       ///< 自动曝光 RT（1×1 RGBA16Float）
    unsigned int t53_color_rt_    = 0;       ///< 离屏 color RT（64×64 RGBA16Float + CopySrc）
    unsigned int t53_pso_         = 0;       ///< 共享 PSO（无深度/无剔除/blend off）
    unsigned int t53_scene_program_  = 0;    ///< HDR 场景程序（输出常量 (4,2,1)）
    unsigned int t53_reduce_program_ = 0;    ///< 亮度归约程序（textureLoad 整张算平均 log 亮度）
    unsigned int t53_adapt_program_  = 0;    ///< lum_adapt 程序（0.18/avgLum 曝光）
    unsigned int t53_tonemap_program_ = 0;   ///< tonemap 程序（ACES(hdr*exposure)+gamma）
    unsigned int t53_quad_vbo_    = 0;       ///< 全屏 quad 顶点缓冲（pos.xy，stride 8）
    unsigned int t53_quad_ibo_    = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t53_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 4 离屏自检（IBL：①BRDF LUT 趟 GGX split-sum 积分渲 64×64 LUT RT；②irradiance
    //   趟渲常量辐照度到 1×1 RT；③prefilter 趟渲常量预滤波镜面到 1×1 RT；④PBR 环境项趟绑 LUT/irr/pref
    //   三纹理按 split-sum 合成 ambient 渲 64×64 RT → 回读 C++ 同算法复算逐通道校验。逻辑同 LearnOpenGL
    //   IBL，离屏隔离、不翻能力位）---
    bool t54_ibl_selftest_done_ = false;
    bool RecordIBLSelfTest();                ///< 录制 BRDF LUT 趟 + irradiance 趟 + prefilter 趟 + PBR 趟 + copy
    void KickIBLSelfTestReadback();          ///< 提交后发起异步 map 回读校验
    unsigned int t54_brdf_rt_     = 0;       ///< BRDF LUT RT（64×64 RGBA16Float）
    unsigned int t54_irr_rt_      = 0;       ///< 辐照度 RT（1×1 RGBA16Float）
    unsigned int t54_pref_rt_     = 0;       ///< 预滤波镜面 RT（1×1 RGBA16Float）
    unsigned int t54_color_rt_    = 0;       ///< 离屏 color RT（64×64 RGBA16Float + CopySrc）
    unsigned int t54_pso_         = 0;       ///< 共享 PSO（无深度/无剔除/blend off）
    unsigned int t54_brdf_program_ = 0;      ///< BRDF LUT 程序（GGX split-sum 积分）
    unsigned int t54_irr_program_  = 0;      ///< 辐照度程序（输出常量辐照度）
    unsigned int t54_pref_program_ = 0;      ///< 预滤波镜面程序（输出常量预滤波色）
    unsigned int t54_pbr_program_  = 0;      ///< PBR 环境项程序（split-sum 合成）
    unsigned int t54_quad_vbo_    = 0;       ///< 全屏 quad 顶点缓冲（pos.xy + uv，stride 16）
    unsigned int t54_quad_ibo_    = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t54_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 5 离屏自检（WBOIT：①几何趟把两层半透明片元按 WBOIT 权重 shader 内解析累加写
    //   accum/reveal 2 附件 MRT；②resolve 趟绑 accum/reveal 两纹理做 accum.rgb/max(accum.a,eps)、1-reveal
    //   合成渲 64×64 RT → 回读 C++ 同公式复算逐通道校验。证明 accum/reveal MRT + resolve OIT 混合能力，
    //   离屏隔离、不翻能力位）---
    bool t55_wboit_selftest_done_ = false;
    bool RecordWBOITSelfTest();              ///< 录制 几何趟（accum/reveal MRT）+ resolve 趟 + copy
    void KickWBOITSelfTestReadback();        ///< 提交后发起异步 map 回读校验
    unsigned int t55_mrt_rt_      = 0;       ///< accum/reveal MRT（2 个 RGBA16Float 颜色附件）
    unsigned int t55_color_rt_    = 0;       ///< 离屏 color RT（64×64 RGBA16Float + CopySrc）
    unsigned int t55_geom_pso_    = 0;       ///< 几何 PSO（无深度/无剔除/blend off）
    unsigned int t55_resolve_pso_ = 0;       ///< resolve PSO（无深度/无剔除/blend off）
    unsigned int t55_geom_program_    = 0;   ///< 几何程序（WBOIT 权重解析累加写 MRT）
    unsigned int t55_resolve_program_ = 0;   ///< resolve 程序（accum/reveal 合成）
    unsigned int t55_quad_vbo_    = 0;       ///< 全屏 quad 顶点缓冲（pos.xy，stride 8）
    unsigned int t55_quad_ibo_    = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t55_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）

    /// B3b-6：把显式纹理视图绑到 compute group2 槽（read_only=true→采样读；false→storage 写）。
    void SetComputeImageViewExplicit(uint32_t binding, WGPUTextureView view, WGPUTextureFormat format,
                                     WGPUTextureViewDimension view_dim, bool read_only);

    RenderStats last_frame_stats_{};
};

} // namespace render
} // namespace dse

#endif // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif // DSE_WEBGPU_RHI_DEVICE_H
