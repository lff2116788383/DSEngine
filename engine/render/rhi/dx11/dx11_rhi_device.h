/**
 * @file dx11_rhi_device.h
 * @brief D3D11 RHI 设备 — 实现 RhiDevice 抽象接口的 D3D11 后端
 *
 * 架构对标 OpenGLRhiDevice / VulkanRhiDevice，持有子系统并委托调用：
 * - DX11Context：Device/DeviceContext/SwapChain 生命周期
 * - DX11ResourceManager：纹理/Buffer/RenderTarget 生命周期管理
 * - DX11ShaderManager：HLSL 着色器编译与管理
 * - DX11PipelineStateManager：BlendState/DepthStencilState/RasterizerState
 * - DX11DrawExecutor：绘制命令执行
 */

#ifndef DSE_RENDER_DX11_RHI_DEVICE_H
#define DSE_RENDER_DX11_RHI_DEVICE_H

#include "engine/render/rhi/dx11/dx11_command_buffer.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/render/rhi/dx11/dx11_resource_manager.h"
#include "engine/render/rhi/dx11/dx11_shader_manager.h"
#include "engine/render/rhi/dx11/dx11_pipeline_state_manager.h"
#include "engine/render/rhi/dx11/dx11_draw_executor.h"

#include <memory>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "engine/render/rhi/dx11/dx11_gpu_timer.h"

namespace dse {
namespace render {

/**
 * @class DX11RhiDevice
 * @brief RHI 的 D3D11 实现 — 协调器，持有所有子系统并委托调用
 */
class DX11RhiDevice final : public RhiDevice {
public:
    DX11RhiDevice();
    ~DX11RhiDevice();

    // --- RhiDevice 接口 ---
    RenderDeviceInfo GetDeviceInfo() const override {
        return { context_.adapter_name(), context_.is_software() };
    }
    bool InitDevice(void* window_handle, int width, int height) override;
    void Shutdown() override;
    void BeginFrame() override;
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override;
    void DeleteRenderTarget(unsigned int render_target_handle) override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const override;
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const override;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const override;
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
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override;
    void DeleteBuffer(unsigned int handle) override;
    VertexArrayHandle CreateVertexArray() override;
    void DeleteVertexArray(VertexArrayHandle handle) override;
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;
    void EndFrame() override;
    void PresentFrame() override;
    const RenderStats& LastFrameStats() const override;

    // --- RenderGraph 自动屏障（D3D11: 驱动隐式管理，仅处理 UAV 解绑）---
    void TransitionRenderTarget(unsigned int rt_handle,
                                 ResourceState from, ResourceState to) override;

    // --- SSBO（Clustered Forward+ 所需） ---
#pragma warning(push)
#pragma warning(disable: 4996)
    unsigned int CreateSSBO(size_t size, const void* data) override;
    void UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) override;
    void BindSSBO(unsigned int handle, unsigned int binding_point) override;
    void DeleteSSBO(unsigned int handle) override;
    void BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable) override;

    // --- Compute Shader ---
    unsigned int CreateComputeShader(const std::string& source) override;
    void DeleteComputeShader(unsigned int handle) override;
    void DispatchCompute(unsigned int shader_handle, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override;
    void ComputeMemoryBarrier() override;
    void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) override;
    void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                   int mip_level, bool read_only, bool r32f = false) override;
    void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) override;
    bool SupportsCompute() const override {
        return context_.feature_level() >= D3D_FEATURE_LEVEL_11_0;
    }
    bool SupportsSSBOCompute() const override {
        return context_.feature_level() >= D3D_FEATURE_LEVEL_11_0;
    }

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

    // --- 内部辅助 ---
    void FlushComputeParamsCB();
    void ClearComputeParams();

    // --- Indirect Draw Buffer ---
    bool SupportsIndirectDraw() const override { return true; }
    unsigned int CreateIndirectBuffer(size_t size, const void* data) override;
    void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) override;
    void DeleteIndirectBuffer(unsigned int handle) override;
#pragma warning(pop)
    void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride, size_t byte_offset = 0) override;

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

    bool SupportsEfficientReadback() const override { return true; }
    bool BeginGpuReadback(BufferHandle handle, size_t offset, size_t size) override;
    const void* GetLastReadbackResult(size_t* out_size = nullptr) const override;
    bool NeedsTextureYFlip() const override { return true; }
    bool NeedsReadbackYFlip() const override { return false; }

    /// DX11: Z remap only ([-1,1] → [0,1]), Y stays up
    glm::mat4 GetProjectionCorrection() const override {
        return glm::mat4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.0f, 0.0f, 0.5f, 1.0f
        );
    }

    /// Shadow sampling: flip Y for DX11 texture coord convention (V=0 is top).
    /// Shader remaps all axes from [-1,1] to [0,1] via (x*0.5+0.5).
    /// Without Y flip, NDC Y=+1 maps to V=1 (bottom) but was rendered at V=0 (top).
    glm::mat4 GetShadowSampleCorrection() const override {
        return glm::mat4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f,-1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }

    // --- 编辑器场景视图模式 ---
    void SetWireframeMode(bool enable) override;
    void SetForceUnlit(bool enable) override;
    void SetOverdrawMode(bool enable) override;

    /// 初始化 D3D11 上下文
    bool InitD3D11(void* window_handle, int width, int height, bool enable_debug = false, bool force_sdr = false);

    // --- 子系统访问器 ---
    DX11Context& context() { return context_; }
    DX11ResourceManager& resource_mgr() { return resource_mgr_; }
    DX11ShaderManager& shader_mgr() { return shader_mgr_; }
    DX11PipelineStateManager& state_mgr() { return state_mgr_; }
    DX11DrawExecutor& draw_executor() { return draw_executor_; }

private:
    /// 子系统实例
    DX11Context context_;
    DX11ResourceManager resource_mgr_;
    DX11ShaderManager shader_mgr_;
    DX11PipelineStateManager state_mgr_;
    DX11DrawExecutor draw_executor_{global_render_state_};

    // --- Wireframe rasterizer state cache ---
    ID3D11RasterizerState* wireframe_rasterizer_state_ = nullptr;
    ID3D11RasterizerState* solid_rasterizer_state_ = nullptr;

    /// 通过 CreateShaderProgram 外部创建的着色器句柄
    std::unordered_set<unsigned int> external_shader_programs_;

    /// Compute Shader 绑定的 SSBO 追踪（binding_point → {handle, writable}）
    struct BoundSSBO { unsigned int handle = 0; bool writable = false; };
    std::unordered_map<unsigned int, BoundSSBO> bound_ssbos_;

    RenderStats last_frame_stats_;
    RenderStats current_frame_stats_;

    // GPU-Driven: CPU 侧实例数据缓存（per-draw model 更新用）
    const void* cached_gpu_models_ = nullptr;   // GPUInstanceData 数组
    const void* cached_gpu_cmds_   = nullptr;   // DrawElementsIndirectCommand 数组
    int         cached_gpu_count_  = 0;

    /// Compute Shader uniform scratch cbuffer（顺序追加后一次性上传到 b0）
    std::vector<uint8_t>           compute_params_staging_;
    ComPtr<ID3D11Buffer>           compute_params_cb_;
    size_t                         compute_params_cb_capacity_ = 0;

    /// Compute Shader uniform name→offset 映射表（按 shader 分组，避免哈希碰撞）
    struct ComputeUniformLayout {
        std::unordered_map<std::string, size_t> name_to_offset;
    };
    std::unordered_map<unsigned int, ComputeUniformLayout> compute_uniform_layouts_;
    size_t compute_uniform_next_offset_ = 0;

    /// 获取或创建指定 shader+name 组合在 cbuffer 中的偏移
    size_t GetOrCreateUniformOffset(unsigned int shader, const char* name, size_t data_size);

    bool initialized_ = false;
    bool vsync_enabled_ = true;   ///< 受 DSE_VSYNC 环境变量控制

    /// Mega/Static VAO 追踪（VAO handle → {vbo_handle, ibo_handle}）
    struct VAOBinding {
        unsigned int vbo_handle = 0;
        unsigned int ibo_handle = 0;
    };
    std::unordered_map<unsigned int, VAOBinding> vao_bindings_;
    unsigned int next_vao_id_ = 900000;

    // Hi-Z pimpl（避免 WINDOWS_EXPORT_ALL_SYMBOLS LNK2001）
    struct HiZImpl;
    std::unique_ptr<HiZImpl> hiz_impl_;

    // 异步 readback 双缓冲（避免同步 GPU pipeline drain）
    struct AsyncReadback {
        ComPtr<ID3D11Buffer> staging[2];   // 双缓冲 staging buffer
        size_t capacity[2] = {0, 0};
        size_t pending_size = 0;           // 上一帧拷贝的字节数
        int write_idx = 0;                 // 本帧写入的 staging index
        bool has_pending = false;          // 是否有待读取的数据
        std::vector<uint8_t> result;       // 上一帧读回的结果
    };
    AsyncReadback async_readback_;

    /// GPU Timestamp Query 子系统
    DX11GpuTimer gpu_timer_;

public:
    // --- IRhiGpuTimer 接口 ---
    bool SupportsGpuTimer() const override { return gpu_timer_.SupportsGpuTimer(); }
    GpuTimerId GetOrCreateGpuTimer(const std::string& name) override { return gpu_timer_.GetOrCreateGpuTimer(name); }
    void BeginGpuTimer(GpuTimerId id) override { gpu_timer_.BeginGpuTimer(id); }
    void EndGpuTimer(GpuTimerId id) override { gpu_timer_.EndGpuTimer(id); }
    float GetGpuTimerResultMs(GpuTimerId id) const override { return gpu_timer_.GetGpuTimerResultMs(id); }
    void ResetGpuTimers() override { gpu_timer_.ResetGpuTimers(); }
    void ResolveGpuTimers() override { gpu_timer_.ResolveGpuTimers(); }
    std::vector<GpuTimerEntry> GetAllGpuTimerResults() const override { return gpu_timer_.GetAllGpuTimerResults(); }
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_RHI_DEVICE_H

