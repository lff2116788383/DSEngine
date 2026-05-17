/**
 * @file rhi_device.h
 * @brief 渲染硬件接口(RHI)抽象层 — 纯虚基类 RhiDevice + CommandBuffer + OpenGLCommandBuffer
 *
 * 本文件仅包含后端无关的抽象接口和 OpenGL 命令缓冲。
 * 具体后端实现位于各自头文件：
 * - gl_rhi_device.h    (OpenGLRhiDevice)
 * - vulkan/vulkan_rhi_device.h (VulkanRhiDevice)
 * - dx11/dx11_rhi_device.h     (DX11RhiDevice)
 */

#ifndef DSE_RHI_DEVICE_H
#define DSE_RHI_DEVICE_H

#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <string>
#include "engine/render/rhi/rhi_types.h"

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
    virtual void DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) = 0;

    /// 延迟阴影贴图绑定命令（Pass 中调用，Submit 时回放到 Device）
    virtual void DeferSetGlobalShadowMap(unsigned int index, unsigned int texture_handle) = 0;
    virtual void DeferSetGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) = 0;
    virtual void DeferSetGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) = 0;
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
    void DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;
    void DeferSetGlobalShadowMap(unsigned int index, unsigned int texture_handle) override;
    void DeferSetGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) override;
    void DeferSetGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) override;

    void Execute(OpenGLRhiDevice* device);
    void Reset();

    /// 将 other 的所有录制命令追加到当前缓冲（用于合并 secondary buffers）
    void AppendFrom(OpenGLCommandBuffer& other);

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
    struct DeferShadowMapCmd {
        uint64_t order;
        unsigned int index;
        unsigned int texture_handle;
        int shadow_type; // 0=CSM, 1=Spot, 2=Point
    };

    struct DrawHairStrandsCmd {
        uint64_t order;
        std::vector<HairDrawItem> items;
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };

    std::vector<DrawParticles3DCmd> draw_particles3d_cmds_;
    std::vector<DrawHairStrandsCmd> draw_hair_strands_cmds_;
    std::vector<DeferShadowMapCmd> defer_shadow_map_cmds_;
};

/**
 * @class RhiDevice
 * @brief 渲染硬件接口基类，提供创建 GPU 资源（纹理、缓冲、着色器）及命令缓冲的统一抽象
 *
 * 阴影/光源全局状态接口：所有后端（OpenGL、Vulkan）均实现相同的阴影贴图与光源矩阵绑定，
 * 消除上层代码对具体后端的 dynamic_cast 依赖。
 */
class RhiDevice {
public:
    virtual ~RhiDevice() = default;

    void SetInitKeepAlive(std::function<void()> cb) { init_keep_alive_ = std::move(cb); }

    /// 在窗口创建后初始化设备（D3D11/Vulkan 需要 HWND；OpenGL 默认已就绪）
    virtual bool InitDevice(void* window_handle, int width, int height) { (void)window_handle; (void)width; (void)height; return true; }

    virtual void Shutdown() = 0;
    virtual void BeginFrame() = 0;
    virtual unsigned int CreateRenderTarget(const RenderTargetDesc& desc) = 0;
    virtual unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const = 0;
    virtual unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const {
        (void)index; return GetRenderTargetColorTexture(render_target_handle);
    }
    virtual unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const = 0;
    virtual std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const = 0;
    virtual RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const = 0;
    virtual unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) = 0;
    virtual unsigned int CreateCompressedTexture2D(CompressedTextureFormat format,
                                                   const std::vector<CompressedMipLevel>& mips,
                                                   bool linear_filter) { (void)format; (void)mips; (void)linear_filter; return 0; }
    virtual unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) = 0;
    virtual unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) = 0;
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

    /// 在 EndFrame 之后补写 GPU Driven 剔除统计（因 readback 在 EndFrame 后发生）
    virtual void PatchLastFrameGPUCulledCount(int culled) { (void)culled; }

    // --- 阴影/光源全局状态接口（所有后端统一） ---
    virtual void SetGlobalShadowMap(unsigned int index, unsigned int handle) = 0;
    void SetGlobalSpotShadowMap(unsigned int handle) { SetGlobalSpotShadowMap(0, handle); }
    virtual void SetGlobalSpotShadowMap(unsigned int index, unsigned int handle) = 0;
    virtual void SetGlobalPointShadowMap(unsigned int index, unsigned int handle) = 0;
    virtual void SetGlobalLightSpaceMatrix(unsigned int index, const glm::mat4& mat) = 0;
    virtual void SetGlobalCascadeSplit(unsigned int index, float split) = 0;
    void SetGlobalSpotLightSpaceMatrix(const glm::mat4& mat) { SetGlobalSpotLightSpaceMatrix(0, mat); }
    virtual void SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) = 0;
    virtual void SetGlobalLightProbeSH(const glm::vec4 sh[9], bool enabled) = 0;

    // --- DDGI 全局状态 ---
    virtual void SetGlobalDDGI(bool enabled, unsigned int irradiance_atlas,
                                const glm::vec3& grid_origin, const glm::vec3& grid_spacing,
                                const glm::ivec3& grid_resolution, int irradiance_texels,
                                float gi_intensity, float normal_bias) {
        (void)enabled; (void)irradiance_atlas;
        (void)grid_origin; (void)grid_spacing;
        (void)grid_resolution; (void)irradiance_texels;
        (void)gi_intensity; (void)normal_bias;
    }

    // --- GBuffer / Deferred 管线状态 ---
    virtual void SetGlobalGBufferTexture(unsigned int index, unsigned int texture_handle) {
        (void)index; (void)texture_handle;
    }
    virtual void SetGBufferRenderingMode(bool enabled) { (void)enabled; }

    /// OpenGL textures need Y-flip on load (bottom-left origin); D3D11/Vulkan don't
    virtual bool NeedsTextureYFlip() const { return true; }
    /// OpenGL readback is bottom-up and needs flip; D3D11/Vulkan readback is top-down
    virtual bool NeedsReadbackYFlip() const { return true; }

    /// Clip-space correction matrix to convert from OpenGL NDC convention
    /// (Y-up, Z∈[-1,1]) to the target API convention.
    /// OpenGL: identity. Vulkan: Y-flip + Z remap. DX11: Z remap only.
    virtual glm::mat4 GetProjectionCorrection() const { return glm::mat4(1.0f); }

    /// Shadow-sampling correction: same as GetProjectionCorrection() but WITHOUT
    /// Z remap, so the shader can consistently remap Z from [-1,1] to [0,1].
    /// OpenGL: identity (same).  Vulkan: Y-flip only.  DX11: identity.
    virtual glm::mat4 GetShadowSampleCorrection() const { return glm::mat4(1.0f); }

    // --- SSBO (Shader Storage Buffer Object) 接口 ---
    // Clustered Forward+ 所需：光源列表 + Cluster 映射表
    // OpenGL: GL_SHADER_STORAGE_BUFFER (GL 4.3+)
    // Vulkan: VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    // DX11:   StructuredBuffer + SRV

    /// 创建 SSBO 缓冲区
    /// @param size 缓冲区大小（字节）
    /// @param data 初始数据指针（可为 nullptr）
    /// @return 缓冲区句柄，0 表示失败
    virtual unsigned int CreateSSBO(size_t size, const void* data) { (void)size; (void)data; return 0; }

    /// 更新 SSBO 数据（子区域）
    virtual void UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) {
        (void)handle; (void)offset; (void)size; (void)data;
    }

    /// 将 SSBO 绑定到指定绑定点（SSBO 绑定点与 UBO 独立）
    virtual void BindSSBO(unsigned int handle, unsigned int binding_point) {
        (void)handle; (void)binding_point;
    }

    /// 删除 SSBO 缓冲区
    virtual void DeleteSSBO(unsigned int handle) { (void)handle; }

    /// 是否支持 SSBO（OpenGL 4.3+ 支持；GL 3.3 使用 UBO fallback）
    virtual bool SupportsSSBO() const { return true; }

    // --- Compute Shader 管线 ---
    // OpenGL: GL_COMPUTE_SHADER (GL 4.3+)
    // Vulkan: VkPipeline (compute)
    // DX11:   ID3D11ComputeShader

    /// 创建 compute shader（source 为后端原生语言：GLSL/SPIR-V binary/HLSL）
    /// @return shader 句柄，0 表示失败
    virtual unsigned int CreateComputeShader(const std::string& source) { (void)source; return 0; }

    /// 删除 compute shader
    virtual void DeleteComputeShader(unsigned int handle) { (void)handle; }

    /// 调度 compute shader 执行
    virtual void DispatchCompute(unsigned int shader_handle,
                                 unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
        (void)shader_handle; (void)groups_x; (void)groups_y; (void)groups_z;
    }

    /// 插入内存屏障（保证 compute 写入对后续图形管线可见）
    virtual void ComputeMemoryBarrier() {}

    /// 开始 compute pass（批量录制多个 dispatch，一次提交）
    /// 在 BeginComputePass/EndComputePass 之间的 DispatchCompute 调用会被
    /// 录制到同一 command buffer 中，ComputeMemoryBarrier 插入 pipeline barrier。
    /// 若后端不支持或未调用 BeginComputePass，DispatchCompute 退化为单次提交。
    virtual void BeginComputePass() {}

    /// 结束 compute pass 并提交所有录制的 dispatch
    virtual void EndComputePass() {}

    /// 将纹理绑定到 compute shader 的 image 单元（image load/store）
    virtual void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) {
        (void)binding; (void)texture_handle; (void)read_only;
    }

    /// 将纹理的指定 mip level 绑定到 compute shader 的 image 单元
    /// @param binding  image unit 绑定点
    /// @param texture_handle 纹理句柄（Hi-Z 纹理或其他 R32F 纹理）
    /// @param mip_level 要绑定的 mip level
    /// @param read_only true=GL_READ_ONLY, false=GL_WRITE_ONLY/GL_READ_WRITE
    /// @param r32f      true 使用 R32F 格式，false 使用 RGBA32F
    virtual void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                           int mip_level, bool read_only, bool r32f = false) {
        (void)binding; (void)texture_handle; (void)mip_level; (void)read_only; (void)r32f;
    }

    /// 将纹理绑定到 compute shader 的采样器单元（用于 textureLod 采样）
    /// @param unit    纹理单元编号
    /// @param texture_handle 纹理句柄
    virtual void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) {
        (void)unit; (void)texture_handle;
    }

    // --- Hi-Z Occlusion Culling 纹理 ---

    /// 创建 Hi-Z 纹理（R32F 格式，完整 mip chain，nearest 过滤）
    /// @param width  基础 mip 宽度（通常等于屏幕宽度）
    /// @param height 基础 mip 高度（通常等于屏幕高度）
    /// @return 纹理句柄，0 表示失败
    virtual unsigned int CreateHiZTexture(int width, int height) { (void)width; (void)height; return 0; }

    /// 删除 Hi-Z 纹理
    virtual void DeleteHiZTexture(unsigned int handle) { (void)handle; }

    /// 获取 Hi-Z 纹理的 mip 级数
    virtual int GetHiZMipCount(unsigned int handle) const { (void)handle; return 0; }

    /// 获取 Hi-Z 纹理的 GPU 原生句柄（OpenGL 为 GLuint texture，其他后端为内部 ID）
    virtual unsigned int GetHiZGpuTexture(unsigned int handle) const { (void)handle; return 0; }

    // --- Compute Uniform 设置（在 DispatchCompute 之前调用）---

    /// 设置 compute shader 的 int uniform
    virtual void SetComputeUniformInt(unsigned int shader, const char* name, int value) {
        (void)shader; (void)name; (void)value;
    }
    /// 设置 compute shader 的 float uniform
    virtual void SetComputeUniformFloat(unsigned int shader, const char* name, float value) {
        (void)shader; (void)name; (void)value;
    }
    /// 设置 compute shader 的 ivec2 uniform
    virtual void SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
        (void)shader; (void)name; (void)x; (void)y;
    }
    /// 设置 compute shader 的 vec2 uniform
    virtual void SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
        (void)shader; (void)name; (void)x; (void)y;
    }
    /// 设置 compute shader 的 vec3 uniform
    virtual void SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) {
        (void)shader; (void)name; (void)x; (void)y; (void)z;
    }
    /// 设置 compute shader 的 ivec3 uniform
    virtual void SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) {
        (void)shader; (void)name; (void)x; (void)y; (void)z;
    }
    /// 设置 compute shader 的 vec4 uniform
    virtual void SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
        (void)shader; (void)name; (void)x; (void)y; (void)z; (void)w;
    }
    /// 设置 compute shader 的 mat4 uniform
    virtual void SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
        (void)shader; (void)name; (void)data;
    }

    // --- SSBO 读回 ---

    /// 同步读回 SSBO 内容到 CPU
    /// @param handle SSBO 句柄
    /// @param offset 读取起始偏移（字节）
    /// @param size   读取大小（字节）
    /// @param dst    目标缓冲区
    virtual void ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) {
        (void)handle; (void)offset; (void)size; (void)dst;
    }

    // --- Indirect Draw Buffer (GL_DRAW_INDIRECT_BUFFER) ---

    /// 创建 Indirect Draw Buffer
    /// @param size  缓冲区大小（字节）
    /// @param data  初始数据（可为 nullptr）
    /// @return 句柄，0 表示失败
    virtual unsigned int CreateIndirectBuffer(size_t size, const void* data) {
        (void)size; (void)data; return 0;
    }

    /// 更新 Indirect Draw Buffer 子区域
    virtual void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) {
        (void)handle; (void)offset; (void)size; (void)data;
    }

    /// 删除 Indirect Draw Buffer
    virtual void DeleteIndirectBuffer(unsigned int handle) {
        (void)handle;
    }

    /// 绑定 indirect buffer 并发起 Multi-Draw Indexed Indirect
    /// @param indirect_buffer  indirect buffer 句柄
    /// @param draw_count       draw command 条数
    /// @param stride           每条 command 的字节步长
    virtual void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride) {
        (void)indirect_buffer; (void)draw_count; (void)stride;
    }

    /// 是否支持 indirect draw (需要 GL 4.3+ / VK / DX11.1)
    virtual bool SupportsIndirectDraw() const { return false; }

    /// 是否支持 compute shader
    virtual bool SupportsCompute() const { return false; }

    /// 是否支持 SSBO compute + 同步读回（GPU 草地风场使用）
    virtual bool SupportsSSBOCompute() const { return false; }

    // --- Mega Buffer (GPU Driven) ---

    /// 创建 Mega VAO（BatchVertex 布局），同时创建 VBO 和 IBO
    /// @param vbo_size_bytes  VBO 初始大小
    /// @param ibo_size_bytes  IBO 初始大小
    /// @param out_vbo  输出 VBO GL handle
    /// @param out_ibo  输出 IBO GL handle
    /// @return VAO handle，0 表示失败
    virtual unsigned int CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                       unsigned int& out_vbo, unsigned int& out_ibo) {
        (void)vbo_size_bytes; (void)ibo_size_bytes; out_vbo = 0; out_ibo = 0; return 0;
    }

    /// 更新 Mega VBO 子区域数据
    virtual void UpdateMegaVBO(unsigned int vbo, size_t offset, size_t size, const void* data) {
        (void)vbo; (void)offset; (void)size; (void)data;
    }

    /// 更新 Mega IBO 子区域数据
    virtual void UpdateMegaIBO(unsigned int ibo, size_t offset, size_t size, const void* data) {
        (void)ibo; (void)offset; (void)size; (void)data;
    }

    /// 删除 Mega VAO + VBO + IBO
    virtual void DeleteMegaVAO(unsigned int vao, unsigned int vbo, unsigned int ibo) {
        (void)vao; (void)vbo; (void)ibo;
    }

    /// 绑定 Mega VAO 供 indirect draw 使用
    virtual void BindMegaVAO(unsigned int vao) { (void)vao; }

    /// 解绑 VAO
    virtual void UnbindVAO() {}

    // --- Static Mesh VAO (BatchVertex 布局, GL_STATIC_DRAW, GL_UNSIGNED_INT 索引) ---

    /// 创建静态网格 VAO（含 VBO + 多个 EBO），使用 BatchVertex 属性布局
    /// @param vertex_data   顶点数据 (BatchVertex[])
    /// @param vertex_bytes  顶点数据字节数
    /// @param ebo_datas     各 LOD 级别索引数据 (uint32_t[])
    /// @param ebo_sizes     各 LOD 级别索引字节数
    /// @param out_vbo       输出 VBO handle
    /// @param out_ebos      输出各 LOD EBO handle
    /// @return VAO handle，0 表示失败
    virtual unsigned int CreateStaticMeshVAO(
        const void* vertex_data, size_t vertex_bytes,
        const std::vector<const void*>& ebo_datas,
        const std::vector<size_t>& ebo_sizes,
        unsigned int& out_vbo,
        std::vector<unsigned int>& out_ebos) {
        (void)vertex_data; (void)vertex_bytes;
        (void)ebo_datas; (void)ebo_sizes;
        out_vbo = 0; out_ebos.clear();
        return 0;
    }

    /// 删除静态网格 VAO + VBO + 所有 EBO
    virtual void DeleteStaticMeshVAO(unsigned int vao, unsigned int vbo,
                                      const std::vector<unsigned int>& ebos) {
        (void)vao; (void)vbo; (void)ebos;
    }

    /// 绑定 VAO 并切换到指定 EBO 进行绘制
    virtual void BindVAOWithEBO(unsigned int vao, unsigned int ebo) {
        (void)vao; (void)ebo;
    }

protected:
    std::function<void()> init_keep_alive_;
    void KeepAlive() { if (init_keep_alive_) init_keep_alive_(); }
};

#endif
