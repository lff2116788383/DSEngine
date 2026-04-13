/**
 * @file rhi_device.h
 * @brief 渲染硬件接口(RHI)抽象层，提供跨图形API的底层渲染命令封装
 */

#ifndef DSE_RHI_DEVICE_H
#define DSE_RHI_DEVICE_H

#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <string>

struct SpriteDrawItem {
    unsigned int texture_handle = 0;
    unsigned int material_instance_id = 0;
    unsigned int shader_variant_key = 0;
    unsigned int blend_mode = 0;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    int sorting_layer = 0;
    int order_in_layer = 0;
};

struct BatchVertex {
    glm::vec3 pos;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec4 weights = glm::vec4(0.0f);
    glm::vec4 joints = glm::vec4(0.0f);
};

struct MeshDrawItem {
    unsigned int vao_override = 0; // If > 0, use this VAO directly instead of client memory (vertices/indices)
    unsigned int index_count_override = 0;
    
    unsigned int texture_handle = 0;
    unsigned int normal_map_handle = 0;
    unsigned int metallic_roughness_map_handle = 0;
    unsigned int emissive_map_handle = 0;
    unsigned int occlusion_map_handle = 0;
    unsigned int blend_mode = 0;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
    std::vector<BatchVertex> vertices;
    std::vector<unsigned short> indices;
    int sorting_layer = 0;
    int order_in_layer = 0;

    bool lighting_enabled = false;
    glm::vec3 material_albedo = glm::vec3(1.0f);
    float material_metallic = 0.0f;
    float material_roughness = 1.0f;
    float material_ao = 1.0f;
    glm::vec3 material_emissive = glm::vec3(0.0f);
    float material_normal_strength = 1.0f;
    float material_alpha_cutoff = 0.5f;
    bool material_alpha_test = false;
    bool material_double_sided = false;
    bool material_uses_instance_data = false;
    bool receive_shadow = true;

    glm::vec3 light_direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color = glm::vec3(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.2f;
    float shadow_strength = 0.5f;
    
    // Multiple lights
    struct PointLightData {
        glm::vec3 color;
        glm::vec3 position;
        float intensity;
        float radius;
        bool cast_shadow = false;
        int shadow_index = -1;
    };
    std::vector<PointLightData> point_lights;
    
    struct SpotLightData {
        glm::vec3 color;
        glm::vec3 position;
        glm::vec3 direction;
        float intensity;
        float radius;
        float inner_cone;
        float outer_cone;
        bool cast_shadow = false;
        int shadow_index = -1;
    };
    std::vector<SpotLightData> spot_lights;

    bool skinned = false;
    std::vector<glm::mat4> bone_matrices;
    
    // Morph targets
    bool morph_enabled = false;
    std::vector<float> morph_weights;
};

struct Particle3DDrawItem {
    unsigned int texture_handle = 0;
    unsigned int material_instance_id = 0;
    unsigned int shader_variant_key = 0;
    unsigned int blend_mode = 0;
    int particle_count = 0;
    unsigned int instance_vbo = 0; // Contains instance transforms/colors
};

#define CSM_CASCADES 3

using DrawBatchItem = SpriteDrawItem;

struct RenderStats {
    int sprite_count = 0;
    int mesh_count = 0;
    int draw_calls = 0;
    int material_switches = 0;
    int max_batch_sprites = 0;
    int render_passes = 0;
    int shadow_passes = 0;
};

struct RenderTargetDesc {
    int width = 0;
    int height = 0;
    bool has_color = true;
    bool has_depth = false;
    bool generate_mipmaps = false; // Phase 2: Bloom Downsample requires mipmaps
    bool cube_map = false;
};

struct RenderPassDesc {
    unsigned int render_target = 0;
    glm::vec4 clear_color = glm::vec4(0.0f);
    bool clear_color_enabled = false;
};

struct PipelineStateDesc {
    bool blend_enabled = true;
    unsigned int blend_src = 0x0302;
    unsigned int blend_dst = 0x0303;
    bool depth_test_enabled = true;
    bool depth_write_enabled = true;
    bool culling_enabled = true;
    unsigned int cull_face = 0x0405; // GL_BACK
};

class OpenGLRhiDevice;

// Simple Command Buffer abstraction for Phase 1
/**
 * @class CommandBuffer
 * @brief 命令缓冲抽象类，负责收集和记录一帧内所有的渲染指令
 */
class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;
    
    /**
     * @brief 开始一个新的渲染通道（Render Pass）
     * @param render_pass 渲染通道描述符，包含目标RT和清理颜色配置
     */
    virtual void BeginRenderPass(const RenderPassDesc& render_pass) = 0;
    
    /**
     * @brief 结束当前的渲染通道
     */
    virtual void EndRenderPass() = 0;
    
    /**
     * @brief 绑定当前的管线状态（如混合模式、深度测试状态等）
     * @param pipeline_state_handle 管线状态句柄
     */
    virtual void SetPipelineState(unsigned int pipeline_state_handle) = 0;
    
    /**
     * @brief 设置当前渲染通道使用的摄像机矩阵
     * @param view 视图矩阵
     * @param projection 投影矩阵
     */
    virtual void SetCamera(const glm::mat4& view, const glm::mat4& projection) = 0;
    
    /**
     * @brief 提交一个通用的渲染批次
     * @param items 待渲染的批次元素集合
     */
    virtual void DrawBatch(const std::vector<DrawBatchItem>& items) = 0;

    virtual void DrawMeshBatch(const std::vector<MeshDrawItem>& items) = 0;
    
    /**
     * @brief 提交一个 2D 精灵图的渲染批次
     * @param items 待渲染的 Sprite 元素集合
     */
    virtual void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) = 0;
    
    /**
     * @brief 执行清屏操作（填充指定颜色）
     * @param color 用于清屏的 RGBA 颜色向量
     */
    virtual void ClearColor(const glm::vec4& color) = 0;

    virtual void SetGlobalMat4(const std::string& name, const glm::mat4& value) = 0;
    
    /**
     * @brief 设置全局矩阵数组参数（主要用于传递 CSM 的多级矩阵）
     */
    virtual void SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) = 0;
    
    /**
     * @brief 设置全局浮点数组参数（主要用于传递 CSM 的分割距离）
     */
    virtual void SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) = 0;

    virtual void DrawSkybox(unsigned int cubemap_texture_handle) = 0;

    /**
     * @brief 提交一个全屏后处理绘制批次
     * @param source_texture 输入纹理
     * @param shader_variant_key 后处理类型 (e.g. bloom, ssao, color_grading)
     * @param params 附加参数
     */
    virtual void DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) = 0;

    virtual void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) = 0;
};

/**
 * @class OpenGLCommandBuffer
 * @brief OpenGL 命令缓冲实现，负责在前端收集命令，并在 Execute 阶段通过底层的 OpenGLRhiDevice 提交到 GPU
 */
class OpenGLCommandBuffer final : public CommandBuffer {
public:
    /**
     * @brief 记录开始渲染通道的指令
     * @param render_pass 渲染通道配置
     */
    void BeginRenderPass(const RenderPassDesc& render_pass) override;

    /**
     * @brief 记录结束渲染通道的指令
     */
    void EndRenderPass() override;

    /**
     * @brief 记录设置渲染管线状态的指令
     * @param pipeline_state_handle 状态句柄
     */
    void SetPipelineState(unsigned int pipeline_state_handle) override;

    /**
     * @brief 记录摄像机矩阵更新的指令
     * @param view 视图矩阵
     * @param projection 投影矩阵
     */
    void SetCamera(const glm::mat4& view, const glm::mat4& projection) override;

    /**
     * @brief 记录通用批次绘制的指令
     * @param items 渲染项列表
     */
    void DrawBatch(const std::vector<DrawBatchItem>& items) override;

    void DrawMeshBatch(const std::vector<MeshDrawItem>& items) override;

    /**
     * @brief 记录精灵批次绘制的指令
     * @param items 精灵渲染项列表
     */
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) override;

    /**
     * @brief 记录清屏指令
     * @param color 颜色
     */
    void ClearColor(const glm::vec4& color) override;
    
    void SetGlobalMat4(const std::string& name, const glm::mat4& value) override;
    void SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) override;
    void SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) override;
    void DrawSkybox(unsigned int cubemap_texture_handle) override;
    void DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) override;
    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;

    // For internal use by OpenGLRhiDevice
    /**
     * @brief 执行所有已记录的渲染命令
     * @param device 实际执行图形调用的 OpenGL RHI 设备指针
     */
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

    /**
     * @brief 销毁所有已分配的底层 RHI 资源
     */
    virtual void Shutdown() = 0;
    
    /**
     * @brief 标记一帧渲染的开始，重置统计数据和缓存
     */
    virtual void BeginFrame() = 0;
    virtual unsigned int CreateRenderTarget(const RenderTargetDesc& desc) = 0;
    virtual unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const = 0;
    virtual unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const = 0;
    virtual unsigned int GetRenderTargetDepthTextureFace(unsigned int render_target_handle, unsigned int face) const = 0;
    virtual unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) = 0;
    virtual void DeleteTexture(unsigned int texture_handle) = 0;
    virtual unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) = 0;
    virtual void DeleteShaderProgram(unsigned int program_handle) = 0;
    virtual unsigned int CreatePipelineState(const PipelineStateDesc& desc) = 0;
    
    virtual unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) = 0;
    /**
     * @brief 更新现有的 GPU 缓冲数据
     * @param handle 缓冲句柄
     * @param offset 偏移量（字节）
     * @param size 更新大小（字节）
     * @param data 源数据指针
     * @param is_index 是否为索引缓冲
     */
    virtual void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) = 0;
    
    /**
     * @brief 删除 GPU 缓冲
     * @param handle 缓冲句柄
     */
    virtual void DeleteBuffer(unsigned int handle) = 0;
    virtual unsigned int CreateVertexArray() = 0;
    
    /**
     * @brief 删除顶点数组对象 (VAO)
     * @param handle VAO句柄
     */
    virtual void DeleteVertexArray(unsigned int handle) = 0;

    /**
     * @brief 创建一个新的命令缓冲
     * @return 智能指针包装的 CommandBuffer 实例
     * @example
     * // auto cmd_buffer = rhi_device->CreateCommandBuffer();
     */
    virtual std::shared_ptr<CommandBuffer> CreateCommandBuffer() = 0;
    
    /**
     * @brief 提交命令缓冲到 GPU 执行
     * @param cmd_buffer 记录好指令的命令缓冲
     */
    virtual void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) = 0;
    
    /**
     * @brief 标记一帧渲染结束，可能会执行缓冲交换或清理
     */
    virtual void EndFrame() = 0;
    virtual const RenderStats& LastFrameStats() const = 0;
};

/**
 * @class OpenGLRhiDevice
 * @brief RHI 的 OpenGL 实现，管理真实的 GL 对象和状态
 */
class OpenGLRhiDevice final : public RhiDevice {
public:
    /**
     * @brief 销毁 OpenGL 设备，释放所有 FBO、纹理和缓冲
     */
    void Shutdown() override;

    /**
     * @brief 开始新的一帧，重置 OpenGL 状态和统计数据
     */
    void BeginFrame() override;

    /**
     * @brief 创建一个帧缓冲对象 (FBO) 作为渲染目标
     * @param desc 包含宽高和是否需要深度缓冲的描述符
     * @return FBO 资源的内部句柄
     */
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override;

    /**
     * @brief 获取渲染目标挂载的颜色纹理句柄，用于后续采样或显示
     * @param render_target_handle FBO 句柄
     * @return 颜色纹理句柄
     */
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override;
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override;
    unsigned int GetRenderTargetDepthTextureFace(unsigned int render_target_handle, unsigned int face) const override;
    
    /**
     * @brief 创建 VBO 或 EBO 数据缓冲
     * @param size 缓冲大小（字节）
     * @param data 初始数据指针（可为 nullptr）
     * @param is_dynamic 是否为动态更新（GL_DYNAMIC_DRAW）
     * @param is_index 是否为索引缓冲（GL_ELEMENT_ARRAY_BUFFER）
     * @return 缓冲句柄
     */
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) override;

    /**
     * @brief 使用 glBufferSubData 更新缓冲的部分或全部数据
     * @param handle 目标缓冲句柄
     * @param offset 起始偏移（字节）
     * @param size 更新长度（字节）
     * @param data 数据源指针
     * @param is_index 是否为索引缓冲
     */
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override;

    /**
     * @brief 删除 GPU 缓冲对象
     * @param handle 缓冲句柄
     */
    void DeleteBuffer(unsigned int handle) override;

    /**
     * @brief 创建一个顶点数组对象 (VAO)
     * @return VAO 句柄
     */
    unsigned int CreateVertexArray() override;

    /**
     * @brief 删除顶点数组对象
     * @param handle VAO 句柄
     */
    void DeleteVertexArray(unsigned int handle) override;

    /**
     * @brief 上传并创建 2D 纹理
     * @param width 宽度
     * @param height 高度
     * @param rgba8_data 图像像素数据
     * @param linear_filter 是否使用双线性过滤（否则为临近采样）
     * @return 纹理句柄
     */
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override;
    void DeleteTexture(unsigned int texture_handle) override;

    /**
     * @brief 编译并链接着色器程序
     * @param vert_src 顶点着色器源码
     * @param frag_src 片段着色器源码
     * @return Shader Program 句柄
     */
    unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) override;
    void DeleteShaderProgram(unsigned int program_handle) override;

    /**
     * @brief 缓存并记录一种管线状态（主要是混合模式）
     * @param desc 状态描述符
     * @return 状态字典的内部键值句柄
     */
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override;

    /**
     * @brief 创建一个专属于 OpenGL 渲染后端的命令缓冲
     * @return 命令缓冲实例
     */
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;

    /**
     * @brief 解析并执行命令缓冲中记录的所有 OpenGL 渲染指令
     * @param cmd_buffer 待执行的命令缓冲
     */
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;

    /**
     * @brief 结束当前帧，执行状态收尾
     */
    void EndFrame() override;

    /**
     * @brief 获取上一帧的渲染统计数据（DrawCall、顶点数等）
     * @return 统计数据结构体
     */
    const RenderStats& LastFrameStats() const override;

    void SetGlobalShadowMap(unsigned int index, unsigned int handle) {
        if (index < 3) global_shadow_map_[index] = handle;
    }
    void SetGlobalSpotShadowMap(unsigned int handle) {
        SetGlobalSpotShadowMap(0, handle);
    }
    void SetGlobalSpotShadowMap(unsigned int index, unsigned int handle) {
        if (index < 4) global_spot_shadow_map_[index] = handle;
    }
    void SetGlobalPointShadowMap(unsigned int index, unsigned int handle) {
        if (index < 4) global_point_shadow_map_[index] = handle;
    }
    void SetGlobalLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
        if (index < 3) global_light_space_matrix_[index] = mat;
    }
    void SetGlobalCascadeSplit(unsigned int index, float split) {
        if (index < 3) global_cascade_splits_[index] = split;
    }
    void SetGlobalSpotLightSpaceMatrix(const glm::mat4& mat) {
        SetGlobalSpotLightSpaceMatrix(0, mat);
    }
    void SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
        if (index < 4) global_spot_light_space_matrix_[index] = mat;
    }
    
    // These are kept public temporarily for the OpenGLCommandBuffer to use
    /**
     * @brief 内部方法：绑定真实的 FBO 并清屏
     * @param render_pass 渲染通道配置
     */
    void RealBeginRenderPass(const RenderPassDesc& render_pass);
    
    /**
     * @brief 内部方法：解绑 FBO
     */
    void RealEndRenderPass();
    
    /**
     * @brief 内部方法：应用 OpenGL 状态机设置（混合、深度等）
     * @param pipeline_state_handle 管线状态句柄
     */
    void RealSetPipelineState(unsigned int pipeline_state_handle);
    
    /**
     * @brief 内部方法：调用 glClear
     * @param color 颜色
     */
    void RealClearColor(const glm::vec4& color);
    
    /**
     * @brief 内部方法：执行实际的实例化绘制或批处理调用
     * @param items 渲染项集合
     * @param view 视图矩阵
     * @param projection 投影矩阵
     */
    void RealSubmitDrawBatch(const std::vector<DrawBatchItem>& items, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawMeshBatch(const std::vector<MeshDrawItem>& items, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawSkybox(unsigned int cubemap_texture_handle, const glm::mat4& view, const glm::mat4& projection);
    void RealSubmitDrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params);
    void RealSubmitDrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection);
    
private:
    struct ResourceLedger {
        std::size_t textures_created = 0;
        std::size_t textures_destroyed = 0;
        std::size_t framebuffers_created = 0;
        std::size_t framebuffers_destroyed = 0;
        std::size_t shader_programs_created = 0;
        std::size_t shader_programs_destroyed = 0;
        std::size_t vertex_arrays_created = 0;
        std::size_t vertex_arrays_destroyed = 0;
        std::size_t buffers_created = 0;
        std::size_t buffers_destroyed = 0;
        std::size_t render_targets_created = 0;
        std::size_t render_targets_destroyed = 0;
        std::size_t pipeline_states_created = 0;
        std::size_t pipeline_states_destroyed = 0;
    };

    /**
     * @brief 内部方法：打印资源账本统计信息，用于检查内存泄漏
     */
    void LogResourceLedger() const;
    
    struct RenderTargetResource {
        RenderTargetDesc desc;
        unsigned int fbo_handle = 0;
        unsigned int color_texture_handle = 0;
        unsigned int depth_texture_handle = 0;
    };

    /**
     * @brief 内部方法：确保基础 OpenGL 状态和默认资源已初始化
     */
    void EnsureInitialized();
    unsigned int next_render_target_handle_ = 320000;
    unsigned int next_texture_handle_ = 340000;
    unsigned int next_fbo_handle_ = 350000;
    unsigned int next_pipeline_state_handle_ = 330000;
    std::unordered_map<unsigned int, RenderTargetResource> render_targets_;
    std::unordered_map<unsigned int, PipelineStateDesc> pipeline_states_;
    unsigned int mesh_vbo_handle_ = 0;
    unsigned int mesh_ibo_handle_ = 0;
    unsigned int mesh_vao_handle_ = 0;

    unsigned int active_pipeline_state_ = 0;
    unsigned int active_render_target_ = 0;
    unsigned int shader_handle_ = 0;
    unsigned int vao_handle_ = 0;
    unsigned int vbo_handle_ = 0;
    unsigned int ebo_handle_ = 0;
    unsigned int white_texture_handle_ = 0;
    unsigned int skybox_shader_handle_ = 0;
    unsigned int skybox_vao_handle_ = 0;
    unsigned int skybox_vbo_handle_ = 0;
    int skybox_view_loc_ = -1;
    int skybox_proj_loc_ = -1;
    int skybox_tex_loc_ = -1;
    int uniform_texture_loc_ = -1;
    int uniform_tint_loc_ = -1;
    int uniform_vp_loc_ = -1;

    int uniform_camera_pos_loc_ = -1;
    int uniform_light_space_matrix_loc_[3];
    int uniform_cascade_splits_loc_ = -1;
    int uniform_shadow_map_loc_[3];
    int uniform_normal_map_loc_ = -1;
    int uniform_has_normal_map_loc_ = -1;
    int uniform_metallic_roughness_map_loc_ = -1;
    int uniform_has_metallic_roughness_map_loc_ = -1;
    int uniform_emissive_map_loc_ = -1;
    int uniform_has_emissive_map_loc_ = -1;
    int uniform_occlusion_map_loc_ = -1;
    int uniform_has_occlusion_map_loc_ = -1;
    int uniform_lighting_enabled_loc_ = -1;
    int uniform_light_direction_loc_ = -1;
    int uniform_light_color_loc_ = -1;
    int uniform_light_intensity_loc_ = -1;
    int uniform_ambient_intensity_loc_ = -1;
    int uniform_shadow_strength_loc_ = -1;
    int uniform_material_albedo_loc_ = -1;
    int uniform_material_metallic_loc_ = -1;
    int uniform_material_roughness_loc_ = -1;
    int uniform_material_ao_loc_ = -1;
    int uniform_material_emissive_loc_ = -1;
    int uniform_material_normal_strength_loc_ = -1;
    int uniform_material_alpha_cutoff_loc_ = -1;
    int uniform_receive_shadow_loc_ = -1;
    int uniform_skinned_loc_ = -1;
    int uniform_bone_matrices_loc_ = -1;

    int uniform_morph_enabled_loc_ = -1;
    int uniform_morph_weights_loc_ = -1;

    int uniform_point_light_count_loc_ = -1;
    struct PointLightLoc {
        int color, position, intensity, radius, cast_shadow, shadow_index;
    } uniform_point_lights_loc_[4];

    int uniform_spot_light_count_loc_ = -1;
    struct SpotLightLoc {
        int color, position, direction, intensity, radius, inner_cone, outer_cone, cast_shadow, shadow_index;
    } uniform_spot_lights_loc_[4];
    int uniform_spot_shadow_map_loc_[4] = {-1, -1, -1, -1};
    int uniform_spot_light_space_matrix_loc_[4] = {-1, -1, -1, -1};

    // Particle 3D support
    unsigned int particle_shader_handle_ = 0;
    int particle_uniform_vp_loc_ = -1;
    int particle_uniform_texture_loc_ = -1;

    glm::mat4 global_light_space_matrix_[3];
    glm::mat4 global_spot_light_space_matrix_[4] = {
        glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)
    };
    float global_cascade_splits_[3];
    unsigned int global_shadow_map_[3];
    unsigned int global_spot_shadow_map_[4] = {0, 0, 0, 0};
    unsigned int global_point_shadow_map_[4] = {0, 0, 0, 0};

    bool initialized_ = false;
    RenderStats current_frame_stats_;
    RenderStats last_frame_stats_;
    ResourceLedger resource_ledger_;
};


#endif
