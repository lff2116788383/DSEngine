/**
 * @file rhi_device.h
 * @brief 渲染硬件接口(RHI)抽象层，提供跨图形API的底层渲染命令封装
 *
 * 架构：OpenGLRhiDevice 作为协调器，持有五个子系统并委托调用：
 * - GLResourceManager：GPU 资源创建/销毁/查询
 * - GLPipelineStateManager：渲染状态缓存与应用
 * - GLShaderManager：着色器编译/链接/Uniform 缓存
 * - GLDrawExecutor：绘制命令执行
 * - UBOManager：Uniform Buffer Object 生命周期与数据更新
 */

#ifndef DSE_RHI_DEVICE_H
#define DSE_RHI_DEVICE_H

#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <string>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/gl_resource_manager.h"
#include "engine/render/rhi/gl_pipeline_state_manager.h"
#include "engine/render/rhi/gl_shader_manager.h"
#include "engine/render/rhi/gl_draw_executor.h"
#include "engine/render/rhi/ubo_manager.h"

class OpenGLRhiDevice;

/**
 * @class CommandBuffer
 * @brief 命令缓冲抽象类，负责收集和记录一帧内所有的渲染指令
 */
class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;
    virtual void BeginRenderPass(const RenderPassDesc& render_pass) = 0;
    virtual void EndRenderPass() = 0;
    virtual void SetPipelineState(unsigned int pipeline_state_handle) = 0;
    virtual void SetCamera(const glm::mat4& view, const glm::mat4& projection) = 0;
    virtual void DrawBatch(const std::vector<DrawBatchItem>& items) = 0;
    virtual void DrawMeshBatch(const std::vector<MeshDrawItem>& items) = 0;
    virtual void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) = 0;
    virtual void ClearColor(const glm::vec4& color) = 0;
    virtual void SetGlobalMat4(const std::string& name, const glm::mat4& value) = 0;
    virtual void SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) = 0;
    virtual void SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) = 0;
    virtual void DrawSkybox(unsigned int cubemap_texture_handle) = 0;
    virtual void DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) = 0;
    virtual void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) = 0;
};

/**
 * @class OpenGLCommandBuffer
 * @brief OpenGL 命令缓冲实现，负责在前端收集命令，并在 Execute 阶段通过底层的 OpenGLRhiDevice 提交到 GPU
 */
class OpenGLCommandBuffer final : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc& render_pass) override;
    void EndRenderPass() override;
    void SetPipelineState(unsigned int pipeline_state_handle) override;
    void SetCamera(const glm::mat4& view, const glm::mat4& projection) override;
    void DrawBatch(const std::vector<DrawBatchItem>& items) override;
    void DrawMeshBatch(const std::vector<MeshDrawItem>& items) override;
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) override;
    void ClearColor(const glm::vec4& color) override;
    void SetGlobalMat4(const std::string& name, const glm::mat4& value) override;
    void SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) override;
    void SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) override;
    void DrawSkybox(unsigned int cubemap_texture_handle) override;
    void DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) override;
    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;

    void Execute(OpenGLRhiDevice* device);
    void Reset();

private:
    struct ClearCmd { uint64_t order; glm::vec4 color; };
    struct BeginRenderPassCmd { uint64_t order; RenderPassDesc render_pass; };
    struct EndRenderPassCmd { uint64_t order; };
    struct SetPipelineStateCmd { uint64_t order; unsigned int pipeline_state_handle; };
    struct DrawBatchCmd {
        uint64_t order;
        std::vector<SpriteDrawItem> items;
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };
    struct DrawMeshBatchCmd {
        uint64_t order;
        std::vector<MeshDrawItem> items;
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };
    struct SetGlobalMat4Cmd {
        uint64_t order;
        std::string name;
        glm::mat4 value;
    };
    struct SetGlobalMat4ArrayCmd {
        uint64_t order;
        std::string name;
        std::vector<glm::mat4> values;
    };
    struct SetGlobalFloatArrayCmd {
        uint64_t order;
        std::string name;
        std::vector<float> values;
    };
    struct DrawSkyboxCmd {
        uint64_t order;
        unsigned int cubemap_texture_handle;
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };
    struct DrawPostProcessCmd {
        uint64_t order;
        unsigned int source_texture;
        std::string effect_name;
        std::vector<float> params;
    };
    struct DrawParticles3DCmd {
        uint64_t order;
        std::vector<Particle3DDrawItem> items;
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };
    struct CommandRef {
        uint64_t order = 0;
        int type = 0;
        size_t index = 0;
    };

    glm::mat4 view_ = glm::mat4(1.0f);
    glm::mat4 projection_ = glm::mat4(1.0f);
    uint64_t next_cmd_order_ = 0;
    std::vector<BeginRenderPassCmd> begin_render_pass_cmds_;
    std::vector<EndRenderPassCmd> end_render_pass_cmds_;
    std::vector<SetPipelineStateCmd> set_pipeline_state_cmds_;
    std::vector<SetGlobalMat4Cmd> set_global_mat4_cmds_;
    std::vector<SetGlobalMat4ArrayCmd> set_global_mat4_array_cmds_;
    std::vector<SetGlobalFloatArrayCmd> set_global_float_array_cmds_;
    std::vector<ClearCmd> clear_cmds_;
    std::vector<DrawBatchCmd> draw_batch_cmds_;
    std::vector<DrawMeshBatchCmd> draw_mesh_batch_cmds_;
    std::vector<DrawSkyboxCmd> draw_skybox_cmds_;
    std::vector<DrawPostProcessCmd> draw_post_process_cmds_;
    std::vector<DrawParticles3DCmd> draw_particles3d_cmds_;
};

/**
 * @class RhiDevice
 * @brief 渲染硬件接口基类，提供创建 GPU 资源（纹理、缓冲、着色器）及命令缓冲的统一抽象
 */
class RhiDevice {
public:
    virtual ~RhiDevice() = default;
    virtual void Shutdown() = 0;
    virtual void BeginFrame() = 0;
    virtual unsigned int CreateRenderTarget(const RenderTargetDesc& desc) = 0;
    virtual unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const = 0;
    virtual unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const = 0;
    virtual std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const = 0;
    virtual RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const = 0;
    virtual unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) = 0;
    virtual unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) = 0;
    virtual void DeleteTexture(unsigned int texture_handle) = 0;
    virtual unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) = 0;
    virtual void DeleteShaderProgram(unsigned int program_handle) = 0;
    virtual unsigned int CreatePipelineState(const PipelineStateDesc& desc) = 0;
    virtual unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) = 0;
    virtual void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) = 0;
    virtual void DeleteBuffer(unsigned int handle) = 0;
    virtual unsigned int CreateVertexArray() = 0;
    virtual void DeleteVertexArray(unsigned int handle) = 0;
    virtual std::shared_ptr<CommandBuffer> CreateCommandBuffer() = 0;
    virtual void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) = 0;
    virtual void EndFrame() = 0;
    virtual const RenderStats& LastFrameStats() const = 0;
};

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
    void Shutdown() override;
    void BeginFrame() override;
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override;
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const override;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const override;
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) override;
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override;
    void DeleteBuffer(unsigned int handle) override;
    unsigned int CreateVertexArray() override;
    void DeleteVertexArray(unsigned int handle) override;
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override;
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) override;
    void DeleteTexture(unsigned int texture_handle) override;
    unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) override;
    void DeleteShaderProgram(unsigned int program_handle) override;
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override;
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;
    void EndFrame() override;
    const RenderStats& LastFrameStats() const override;

    // --- 全局阴影/光源矩阵（委托到 draw_executor_） ---
    void SetGlobalShadowMap(unsigned int index, unsigned int handle) {
        draw_executor_.SetGlobalShadowMap(index, handle);
    }
    void SetGlobalSpotShadowMap(unsigned int handle) {
        SetGlobalSpotShadowMap(0, handle);
    }
    void SetGlobalSpotShadowMap(unsigned int index, unsigned int handle) {
        draw_executor_.SetGlobalSpotShadowMap(index, handle);
    }
    void SetGlobalPointShadowMap(unsigned int index, unsigned int handle) {
        draw_executor_.SetGlobalPointShadowMap(index, handle);
    }
    void SetGlobalLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
        draw_executor_.SetGlobalLightSpaceMatrix(index, mat);
    }
    void SetGlobalCascadeSplit(unsigned int index, float split) {
        draw_executor_.SetGlobalCascadeSplit(index, split);
    }
    void SetGlobalSpotLightSpaceMatrix(const glm::mat4& mat) {
        SetGlobalSpotLightSpaceMatrix(0, mat);
    }
    void SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
        draw_executor_.SetGlobalSpotLightSpaceMatrix(index, mat);
    }

    // --- 内部方法（供 OpenGLCommandBuffer::Execute 调用，委托到子系统） ---
    void RealBeginRenderPass(const RenderPassDesc& render_pass);
    void RealEndRenderPass();
    void RealSetPipelineState(unsigned int pipeline_state_handle);
    void RealClearColor(const glm::vec4& color);
    void RealSubmitDrawBatch(const std::vector<DrawBatchItem>& items, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawMeshBatch(const std::vector<MeshDrawItem>& items, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawSkybox(unsigned int cubemap_texture_handle, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params);
    void RealSubmitDrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection);

    // --- 子系统访问器 ---
    dse::render::GLResourceManager& resource_mgr() { return resource_mgr_; }
    dse::render::GLPipelineStateManager& state_mgr() { return state_mgr_; }
    dse::render::GLShaderManager& shader_mgr() { return shader_mgr_; }
    dse::render::GLDrawExecutor& draw_executor() { return draw_executor_; }
    dse::render::UBOManager& ubo_mgr() { return ubo_mgr_; }

private:
    void EnsureInitialized();
    void LogResourceLedger() const;

    /// 子系统实例
    dse::render::GLResourceManager resource_mgr_;
    dse::render::GLPipelineStateManager state_mgr_;
    dse::render::GLShaderManager shader_mgr_;
    dse::render::GLDrawExecutor draw_executor_;
    dse::render::UBOManager ubo_mgr_;

    /// 通过 CreateShaderProgram 外部创建的着色器句柄，需在 Shutdown 中统一清理
    std::unordered_set<unsigned int> external_shader_programs_;

    bool initialized_ = false;
};


#endif
