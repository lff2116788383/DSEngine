/**
 * @file vulkan_rhi_device.h
 * @brief Vulkan RHI 设备 — 实现 RhiDevice 抽象接口的 Vulkan 后端
 *
 * 架构对标 OpenGLRhiDevice，持有五个子系统并委托调用：
 * - VulkanContext：Instance/Device/Swapchain 生命周期
 * - VulkanResourceManager：GPU 资源创建/销毁/查询
 * - VulkanPipelineStateManager：VkPipeline/VkRenderPass 缓存与应用
 * - VulkanShaderManager：SPIR-V 模块编译与反射
 * - VulkanDrawExecutor：绘制命令执行
 */

#ifndef DSE_RENDER_VULKAN_RHI_DEVICE_H
#define DSE_RENDER_VULKAN_RHI_DEVICE_H

#include "engine/render/rhi/vulkan/vulkan_command_buffer.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/render/rhi/vulkan/vulkan_resource_manager.h"
#include "engine/render/rhi/vulkan/vulkan_pipeline_state_manager.h"
#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
#include "engine/render/rhi/vulkan/vulkan_draw_executor.h"

#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "engine/render/rhi/vulkan/vulkan_gpu_timer.h"

namespace dse {
namespace render {

/**
 * @class VulkanRhiDevice
 * @brief RHI 的 Vulkan 实现 — 协调器，持有五个子系统并委托调用
 */
class VulkanRhiDevice final : public RhiDevice {
public:
    VulkanRhiDevice();
    ~VulkanRhiDevice();

    // --- RhiDevice 接口 ---
    RenderDeviceInfo GetDeviceInfo() const override;
    bool InitDevice(void* window_handle, int width, int height) override;
    void Shutdown() override;
    void WaitIdle() override;
    void BeginFrame() override;
    // 引擎双缓冲（MAX_FRAMES_IN_FLIGHT）+ 当前在飞槽位（context_.current_frame()）。
    // 供 PerInFlightBuffer 据此 N 缓冲动态资源（RHI_ABSTRACTION_BOUNDARY §8.2 D9）。
    uint32_t FramesInFlight() const override;
    uint32_t CurrentFrameSlot() const override;
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override;
    void DeleteRenderTarget(unsigned int render_target_handle) override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const override;
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const override;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const override;
    RenderTargetDepthReadback ReadRenderTargetDepthFloatWithSize(unsigned int render_target_handle) const override;
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override;
    unsigned int CreateCompressedTexture2D(CompressedTextureFormat format,
                                           const std::vector<CompressedMipLevel>& mips,
                                           bool linear_filter) override;
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) override;
    unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) override;
    void DeleteTexture(unsigned int texture_handle) override;
    unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) override;
    void DeleteShaderProgram(unsigned int program_handle) override;
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override;
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) override;

    // --- 内建资源访问器 ---
    unsigned int GetBuiltinProgram(BuiltinProgram program) override;
    unsigned int GetGenPPShaderProgram(const std::string& effect_name) override;
    unsigned int GetBloomComputeShader(bool upsample) const override;
    unsigned int GetSkyboxCubeVertexBuffer() override;
    // kUniform 用途需走 VK_BUFFER_USAGE_UNIFORM_BUFFER（host-visible 持久映射），覆写基类路由。
    BufferHandle CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) override;
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override;
    void DeleteBuffer(unsigned int handle) override;
    VertexArrayHandle CreateVertexArray() override;
    void DeleteVertexArray(VertexArrayHandle handle) override;
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;
    void EndFrame() override;
    const RenderStats& LastFrameStats() const override;

    // --- 即时绘制 / RT blit 原语（编辑器架构 §5.A / §5.B）---
    void ImmediateDraw(const ImmediateDrawDesc& desc) override;
    void BlitRenderTarget(unsigned int src_rt, unsigned int dst_rt) override;

    // --- RenderGraph 自动屏障（Vulkan 精确 VkImageMemoryBarrier）---
    void TransitionRenderTarget(unsigned int rt_handle,
                                 ResourceState from, ResourceState to) override;

    /// 初始化 Vulkan 上下文（替代 OpenGL 的 glad 初始化）
    /// @param window_handle Win32 HWND
    /// @param width 窗口宽度
    /// @param height 窗口高度
    bool InitVulkan(void* window_handle, int width, int height, bool enable_validation = true);

    // --- SSBO（Clustered Forward+ 所需） ---
#pragma warning(push)
#pragma warning(disable: 4996)
    unsigned int CreateSSBO(size_t size, const void* data) override;
    void UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) override;
    void BindSSBO(unsigned int handle, unsigned int binding_point) override {
        // 存储绑定状态，实际绑定在 DrawMeshBatch 的 descriptor set 分配中完成
        bound_ssbos_[binding_point] = handle;
    }
    void DeleteSSBO(unsigned int handle) override;

    // --- Compute Shader ---
    unsigned int CreateComputeShader(const std::string& source) override;
    void DeleteComputeShader(unsigned int handle) override;
    void DispatchCompute(unsigned int shader_handle, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override;
    void ComputeMemoryBarrier() override;
    void BeginComputePass() override;
    void EndComputePass() override;
    void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) override;
    void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                   int mip_level, bool read_only, bool r32f = false) override;
    void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) override;
    bool SupportsCompute() const override { return true; }
    bool SupportsSSBOCompute() const override { return true; }

    // --- Hi-Z Occlusion Culling ---
    unsigned int CreateHiZTexture(int width, int height) override;
    void DeleteHiZTexture(unsigned int handle) override;
    int GetHiZMipCount(unsigned int handle) const override;
    unsigned int GetHiZGpuTexture(unsigned int handle) const override;

    // --- Compute Uniform ---
    void SetComputeUniformInt(unsigned int shader, const char* name, int value) override;
    void SetComputeUniformFloat(unsigned int shader, const char* name, float value) override;
    void SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) override;
    void SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) override;
    void SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) override;
    void SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) override;
    void SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) override;
    void SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) override;
    void ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) override;

    unsigned int CreateComputeShaderEx(
        const std::string& gl_src, const std::string& vk_src, const std::string& hlsl_src,
        uint32_t ssbo_count, uint32_t storage_image_count, uint32_t sampler_count,
        uint32_t push_constant_bytes) override;
    unsigned int CreateComputeWriteTexture2D(int width, int height) override;

    // --- Indirect Draw Buffer ---
    unsigned int CreateIndirectBuffer(size_t size, const void* data) override;
    void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) override;
    void DeleteIndirectBuffer(unsigned int handle) override;
#pragma warning(pop)
    void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride, size_t byte_offset = 0) override;
    bool SupportsIndirectDraw() const override { return true; }

    // --- Mega Buffer (GPU Driven) ---
    VertexArrayHandle CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                               BufferHandle& out_vbo, BufferHandle& out_ibo) override;
    void UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) override;
    void UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) override;
    void DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) override;
    void BindMegaVAO(VertexArrayHandle vao) override;
    void UnbindVAO() override;

    // --- Static Mesh VAO ---
    VertexArrayHandle CreateStaticMeshVAO(
        const void* vertex_data, size_t vertex_bytes,
        const std::vector<const void*>& ebo_datas,
        const std::vector<size_t>& ebo_sizes,
        BufferHandle& out_vbo,
        std::vector<BufferHandle>& out_ebos) override;
    void DeleteStaticMeshVAO(VertexArrayHandle vao, BufferHandle vbo,
                              const std::vector<BufferHandle>& ebos) override;
    void BindVAOWithEBO(VertexArrayHandle vao, BufferHandle ebo) override;

    // --- GPU-Driven PBR ---
    bool HasGPUDrivenPBRShader() const override;
    void SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& camera_pos,
                                  const glm::vec3& light_dir, const glm::vec3& light_color,
                                  float light_intensity, float ambient_intensity,
                                  float shadow_strength = 0.0f) override;
    void SetupGPUDrivenShadowShader(const glm::mat4& light_view, const glm::mat4& light_proj) override;
    void BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                                unsigned int metallic_roughness,
                                unsigned int emissive, unsigned int occlusion) override;
    void CacheGPUDrivenInstanceData(const void* models, const void* cmds, int count) override;
    void UpdateGPUDrivenMaterial(const void* mat_data) override;
    void PatchLastFrameGPUCulledCount(int culled) override {
        last_frame_stats_.gpu_culled_count = culled;
    }

    void SetActiveRenderCommandBuffer(VkCommandBuffer cmd) { active_render_cmd_ = cmd; }
    void ClearActiveRenderCommandBuffer() { active_render_cmd_ = VK_NULL_HANDLE; }
    void FlushPendingGpuTimerReset(VkCommandBuffer cmd);

    bool NeedsTextureYFlip() const override { return true; }
    bool NeedsReadbackYFlip() const override { return false; }

    /// Vulkan: Y-flip (NDC Y-down) + Z remap ([-1,1] → [0,1])
    glm::mat4 GetProjectionCorrection() const override {
        // row-major construction: column 0, column 1, column 2, column 3
        return glm::mat4(
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f
        );
    }

    /// Shadow sampling: Y-flip only, NO Z remap.
    /// Shader will remap Z from [-1,1] to [0,1] uniformly.
    glm::mat4 GetShadowSampleCorrection() const override {
        return glm::mat4(
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 1.0f, 0.0f,
            0.0f,  0.0f, 0.0f, 1.0f
        );
    }

    void SetWireframeMode(bool enable) override;
    void SetForceUnlit(bool enable) override;
    void SetOverdrawMode(bool enable) override;

    void OnWindowResized(int width, int height) override;

    // --- 子系统访问器 ---
    VulkanContext& context() { return context_; }
    VulkanResourceManager& resource_mgr() { return resource_mgr_; }
    VulkanPipelineStateManager& state_mgr() { return state_mgr_; }
    VulkanShaderManager& shader_mgr() { return shader_mgr_; }
    VulkanDrawExecutor& draw_executor() { return draw_executor_; }

    /// 获取当前帧绑定的 SSBO 状态（binding_point → handle）
    const std::unordered_map<unsigned int, unsigned int>& bound_ssbos() const { return bound_ssbos_; }

private:
    void EnsureInitialized();

    /// 即时绘制（§5.A）动态 VkPipeline 缓存：键 = {VS/FS module + render_pass + topology
    /// + blend/depth + 顶点属性布局} 序列化字符串，避免每次调用重建管线。
    VkPipeline GetOrCreateImmediatePipeline(const ImmediateDrawDesc& desc,
                                            const VulkanShaderProgram* program,
                                            VkRenderPass render_pass,
                                            uint32_t color_attachment_count);
    std::unordered_map<std::string, VkPipeline> immediate_pipelines_;

    /// 子系统实例
    VulkanContext context_;
    VulkanResourceManager resource_mgr_;
    VulkanPipelineStateManager state_mgr_;
    VulkanShaderManager shader_mgr_;
    VulkanDrawExecutor draw_executor_{global_render_state_};

    /// 通过 CreateShaderProgram 外部创建的着色器句柄
    std::unordered_set<unsigned int> external_shader_programs_;

    /// 内建天空盒立方体顶点缓冲句柄（懒初始化，A1 通用原语用）
    unsigned int skybox_cube_vbo_handle_ = 0;

    RenderStats last_frame_stats_;
    RenderStats current_frame_stats_;

    // GPU-Driven: CPU 侧实例数据缓存（per-draw push constants 用）
    const void* cached_gpu_models_ = nullptr;   // GPUInstanceData 数组
    const void* cached_gpu_cmds_   = nullptr;   // DrawElementsIndirectCommand 数组
    int         cached_gpu_count_  = 0;

    /// 本帧待提交的命令缓冲列表
    std::vector<VkCommandBuffer> pending_command_buffers_;

    /// 当前活跃的渲染命令缓冲（由 VulkanCommandBuffer::BeginRenderPass 设置）
    VkCommandBuffer active_render_cmd_ = VK_NULL_HANDLE;

    /// 当前帧绑定的 SSBO 状态 (binding_point → handle)
    std::unordered_map<unsigned int, unsigned int> bound_ssbos_;

    /// Compute pass 批量录制状态
    VkCommandBuffer compute_cmd_buffer_ = VK_NULL_HANDLE;
    bool in_compute_pass_ = false;

    /// Compute push constant 缓冲（用于 SetComputeUniform* 系列）
    std::vector<uint8_t> compute_push_constants_;

    /// Compute Shader uniform name→offset 映射表（按 shader 分组，避免哈希碰撞）
    struct ComputeUniformLayout {
        std::unordered_map<std::string, size_t> name_to_offset;
    };
    std::unordered_map<unsigned int, ComputeUniformLayout> compute_uniform_layouts_;
    size_t compute_uniform_next_offset_ = 0;

    /// 获取或创建指定 shader+name 组合在 push constant 缓冲中的偏移
    size_t GetOrCreateUniformOffset(unsigned int shader, const char* name, size_t data_size);

    /// Pending compute image 绑定 (binding → texture_handle)
    struct ComputeImageBinding {
        unsigned int texture_handle = 0;
        bool read_only = true;
        int mip_level = -1;  ///< -1 表示全 mip
        bool r32f = false;
    };
    std::unordered_map<unsigned int, ComputeImageBinding> pending_compute_images_;
    std::unordered_map<unsigned int, unsigned int> pending_compute_samplers_; ///< unit → tex handle

    bool initialized_ = false;
    bool swapchain_needs_recreate_ = false;
    bool swapchain_recreated_this_frame_ = false;

    /// Mega/Static VAO 追踪（VAO handle → {vbo_handle, ibo_handle}）
    struct VAOBinding {
        unsigned int vbo_handle = 0;
        unsigned int ibo_handle = 0;
    };
    std::unordered_map<unsigned int, VAOBinding> vao_bindings_;
    unsigned int next_vao_id_ = 950000;

    // Hi-Z pimpl（避免 WINDOWS_EXPORT_ALL_SYMBOLS LNK2001）
    struct HiZImpl;
    std::unique_ptr<HiZImpl> hiz_impl_;

    /// GPU Timestamp Query 子系统
    VulkanGpuTimer gpu_timer_;

public:
    // --- IRhiGpuTimer 接口 ---
    bool SupportsGpuTimer() const override { return gpu_timer_.SupportsGpuTimer(); }
    GpuTimerId GetOrCreateGpuTimer(const std::string& name) override { return gpu_timer_.GetOrCreateGpuTimer(name); }
    void BeginGpuTimer(GpuTimerId id) override { gpu_timer_.BeginGpuTimer(id, active_render_cmd_); }
    void EndGpuTimer(GpuTimerId id) override { gpu_timer_.EndGpuTimer(id, active_render_cmd_); }
    float GetGpuTimerResultMs(GpuTimerId id) const override { return gpu_timer_.GetGpuTimerResultMs(id); }
    void ResetGpuTimers() override;
    void ResolveGpuTimers() override { gpu_timer_.ResolveGpuTimers(); }
    std::vector<GpuTimerEntry> GetAllGpuTimerResults() const override { return gpu_timer_.GetAllGpuTimerResults(); }
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_RHI_DEVICE_H