/**
 * @file gl_rhi_device.h
 * @brief OpenGL RHI 后端实现
 *
 * 从 rhi_device.h 拆分：rhi_device.h 只保留纯虚基类 RhiDevice + CommandBuffer，
 * 本文件包含 OpenGLRhiDevice 及其 GL 子系统依赖。
 */

#ifndef DSE_GL_RHI_DEVICE_H
#define DSE_GL_RHI_DEVICE_H

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/opengl/gl_resource_manager.h"
#include "engine/render/rhi/opengl/gl_pipeline_state_manager.h"
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/opengl/gl_draw_executor.h"
#include "engine/render/rhi/opengl/ubo_manager.h"
#include <unordered_set>
#include <unordered_map>
#include <memory>

namespace dse {
namespace render {

/**
 * @class OpenGLRhiDevice
 * @brief RHI 的 OpenGL 实现 - 协调器，持有五个子系统并委托调用
 *
 * 子系统架构：
 * - GLResourceManager：GPU 资源创建/销毁/查询（RenderTarget/Texture/Buffer/VAO/PipelineState）
 * - GLPipelineStateManager：渲染状态缓存与应用（blend/depth/culling）
 * - GLShaderManager：着色器编译/链接/Uniform location 缓存
 * - GLDrawExecutor：绘制命令执行（2D 精灵/3D 网格/天空盒/后处理/粒子）
 * - UBOManager：Uniform Buffer Object 生命周期与数据更新（PerFrame/PerScene/PerMaterial）
 */
class OpenGLRhiDevice final : public RhiDevice {
public:
    OpenGLRhiDevice();
    ~OpenGLRhiDevice() override;

    void Shutdown() override;
    void BeginFrame() override;
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override;
    void DeleteRenderTarget(unsigned int render_target_handle) override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const override;
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const override;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const override;
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) override;
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override;
    void DeleteBuffer(unsigned int handle) override;
    VertexArrayHandle CreateVertexArray() override;
    void DeleteVertexArray(VertexArrayHandle handle) override;
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
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;
    void EndFrame() override;
    const RenderStats& LastFrameStats() const override;

    // --- RenderGraph 自动屏障（GL: glTextureBarrier / glMemoryBarrier）---
    void TransitionRenderTarget(unsigned int rt_handle,
                                 ResourceState from, ResourceState to) override;

    void PatchLastFrameGPUCulledCount(int culled) override {
        draw_executor_.MutableLastFrameStats().gpu_culled_count = culled;
    }

    // --- SSBO（Clustered Forward+ 所需） ---
    // 旧 API 已 deprecated，但后端实现仍需覆写供基类路由调用
#pragma warning(push)
#pragma warning(disable: 4996)
    unsigned int CreateSSBO(size_t size, const void* data) override;
    void UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) override;
    void BindSSBO(unsigned int handle, unsigned int binding_point) override;
    void DeleteSSBO(unsigned int handle) override;
    bool SupportsSSBO() const override { return supports_ssbo_; }

    // --- Compute Shader（GL 4.3+）---
    unsigned int CreateComputeShader(const std::string& source) override;
    void DeleteComputeShader(unsigned int handle) override;
    void DispatchCompute(unsigned int shader_handle, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override;
    void ComputeMemoryBarrier() override;
    void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) override;
    void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                   int mip_level, bool read_only, bool r32f = false) override;
    void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) override;
    bool SupportsCompute() const override { return supports_ssbo_; }
    bool SupportsSSBOCompute() const override { return supports_ssbo_; }
    unsigned int CreateComputeWriteTexture2D(int width, int height) override;

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

    // --- SSBO 读回 ---
    void ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) override;

    // --- Indirect Draw Buffer ---
    unsigned int CreateIndirectBuffer(size_t size, const void* data) override;
    void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) override;
    void DeleteIndirectBuffer(unsigned int handle) override;
#pragma warning(pop)
    void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride, size_t byte_offset = 0) override;
    bool SupportsIndirectDraw() const override { return supports_ssbo_; }

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
    void SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& camera_pos,
                                  const glm::vec3& light_dir, const glm::vec3& light_color,
                                  float light_intensity, float ambient_intensity,
                                  float shadow_strength = 0.0f) override;
    void SetupGPUDrivenShadowShader(const glm::mat4& light_view, const glm::mat4& light_proj) override;
    void BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                                unsigned int metallic_roughness,
                                unsigned int emissive, unsigned int occlusion) override;

    // --- 编辑器场景视图模式 ---
    void SetWireframeMode(bool enable) override;
    void SetForceUnlit(bool enable) override;
    void SetOverdrawMode(bool enable) override;

    // --- 内部方法（供 OpenGLCommandBuffer 直接调用，委托到子系统） ---
    void RealBeginRenderPass(const RenderPassDesc& render_pass);
    void RealEndRenderPass();
    void RealSetPipelineState(unsigned int pipeline_state_handle);
    void RealClearColor(const glm::vec4& color);
    void RealSubmitDrawSpriteBatch(const std::vector<SpriteDrawItem>& items, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawMeshBatch(const std::vector<MeshDrawItem>& items, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawSkybox(unsigned int cubemap_texture_handle, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawPostProcess(const PostProcessRequest& request);
    void RealSubmitDrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection);

    // --- 子系统访问器 ---
    GLResourceManager& resource_mgr() { return resource_mgr_; }
    GLPipelineStateManager& state_mgr() { return state_mgr_; }
    GLShaderManager& shader_mgr() { return shader_mgr_; }
    GLDrawExecutor& draw_executor() { return draw_executor_; }
    UBOManager& ubo_mgr() { return ubo_mgr_; }

private:
    void EnsureInitialized();
    void LogResourceLedger() const;

    /// 子系统实例
    GLResourceManager resource_mgr_;
    GLPipelineStateManager state_mgr_;
    GLShaderManager shader_mgr_;
    GLDrawExecutor draw_executor_{global_render_state_};
    UBOManager ubo_mgr_;

    /// 通过 CreateShaderProgram 外部创建的着色器句柄，需在 Shutdown 中统一清理
    std::unordered_set<unsigned int> external_shader_programs_;

    /// 通过 CreateComputeShader 创建的 compute 程序句柄
    std::unordered_set<unsigned int> compute_programs_;

    /// Hi-Z 纹理信息（pimpl 隐藏实现，避免 WINDOWS_EXPORT_ALL_SYMBOLS 模板泄漏）
    struct HiZImpl;
    std::unique_ptr<HiZImpl> hiz_impl_;

    /// Indirect draw buffer 管理
    std::unordered_map<unsigned int, unsigned int> indirect_buffers_; ///< handle → GL buffer ID
    unsigned int next_indirect_handle_ = 600000;

    bool initialized_ = false;
    bool supports_ssbo_ = true;  ///< GL 4.3+ 支持 SSBO；GL 3.3 fallback 为 false
};

} // namespace render
} // namespace dse

#endif
