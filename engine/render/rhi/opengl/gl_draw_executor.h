/**
 * @file gl_draw_executor.h
 * @brief OpenGL 绘制执行器 - 负责执行所有渲染命令和绘制调用
 *
 * 从 OpenGLRhiDevice 中提取的第四个子系统：
 * - 2D 精灵批处理绘制
 * - 3D PBR 网格绘制
 * - 天空盒绘制
 * - 后处理绘制
 * - 3D 粒子绘制
 * - RenderPass 管理（FBO 绑定/清屏）
 * - 全局阴影贴图/光源矩阵管理
 */

#ifndef DSE_RENDER_GL_DRAW_EXECUTOR_H
#define DSE_RENDER_GL_DRAW_EXECUTOR_H

#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/draw_executor_common.h"
#include "engine/render/rhi/postprocess_common.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

namespace dse {
namespace render {

class GLPipelineStateManager;
class GLShaderManager;
class GLResourceManager;
class UBOManager;

/**
 * @class GLDrawExecutor
 * @brief OpenGL 绘制执行器
 *
 * 职责：
 * 1. 管理几何缓冲区（VAO/VBO/EBO）
 * 2. 执行各类绘制命令
 * 3. 管理全局阴影贴图和光源空间矩阵
 * 4. 追踪渲染统计数据
 *
 * @note 执行绘制时需要引用 ShaderManager 和 PipelineStateManager 获取状态
 */
class GLDrawExecutor {
public:
    explicit GLDrawExecutor(DrawExecutorGlobalState& shared_state)
        : global_state_(shared_state) {}
    ~GLDrawExecutor() = default;

    /// 初始化几何缓冲区（2D 批处理 VAO/VBO/EBO + 3D 网格 VAO/VBO/EBO + 白色纹理）
    using InitCreateVaoFn = std::function<VertexArrayHandle()>;
    using InitCreateVboFn = std::function<unsigned int(size_t, const void*, bool, bool)>;
    using InitUpdateVboFn = std::function<void(unsigned int, size_t, size_t, const void*, bool)>;
    void InitGeometryBuffers(InitCreateVaoFn create_vao_fn,
                              InitCreateVboFn create_vbo_fn,
                              InitUpdateVboFn update_vbo_fn);

    /// 清理所有几何缓冲区资源
    void ShutdownGeometryBuffers();

    // --- RenderPass ---
    void BeginRenderPass(const RenderPassDesc& render_pass,
                          GLResourceManager& resource_mgr);
    void EndRenderPass(GLResourceManager& resource_mgr);
    void ClearColor(const glm::vec4& color);

    // --- 绘制命令 ---
    void DrawMeshBatch(const std::vector<MeshDrawItem>& items,
                         const glm::mat4& view,
                         const glm::mat4& projection,
                         GLPipelineStateManager& state_mgr,
                         GLShaderManager& shader_mgr,
                         GLResourceManager& resource_mgr,
                         UBOManager& ubo_mgr);

    void DrawPostProcess(const dse::render::PostProcessRequest& request,
                           GLShaderManager& shader_mgr);

    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items,
                           const glm::mat4& view,
                           const glm::mat4& projection,
                           GLShaderManager& shader_mgr);

    void DrawHairStrands(const std::vector<HairDrawItem>& items,
                          const glm::mat4& view,
                          const glm::mat4& projection,
                          GLShaderManager& shader_mgr);

    // --- 通用绘制原语 (A1) ---
    void PrimBindShaderProgram(unsigned int program_handle);
    void PrimBindVertexBuffer(unsigned int buffer_handle, uint32_t stride,
                              const std::vector<VertexAttr>& attrs);
    void PrimBindTextureCube(unsigned int slot, unsigned int cubemap_handle);
    void PrimPushConstantsMat4(const glm::mat4& value);
    void PrimDraw(uint32_t vertex_count, uint32_t first_vertex);

    // --- 通用绘制原语 (B0): 索引 / 2D 纹理 / UBO / 索引绘制 ---
    void PrimBindIndexBuffer(unsigned int buffer_handle, IndexType type);
    void PrimBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim);
    void PrimBindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                               uint32_t offset, uint32_t size);
    void PrimDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex);

    // --- 渲染统计 ---
    void BeginFrame();
    void EndFrame();
    const RenderStats& last_frame_stats() const { return global_state_.last_frame_stats; }
    const RenderStats& current_frame_stats() const { return global_state_.current_frame_stats; }
    RenderStats& MutableCurrentStats() { return global_state_.current_frame_stats; }
    RenderStats& MutableLastFrameStats() { return global_state_.last_frame_stats; }
    const DrawExecutorGlobalState& global_state() const { return global_state_; }

    // --- 访问器（供 OpenGLRhiDevice 委托使用） ---
    unsigned int white_texture_handle() const { return white_texture_handle_; }
    VertexArrayHandle vao_handle() const { return vao_handle_; }
    unsigned int vbo_handle() const { return vbo_handle_; }
    unsigned int ebo_handle() const { return ebo_handle_; }
    VertexArrayHandle mesh_vao_handle() const { return mesh_vao_handle_; }
    unsigned int mesh_vbo_handle() const { return mesh_vbo_handle_; }
    unsigned int mesh_ibo_handle() const { return mesh_ibo_handle_; }
    VertexArrayHandle skybox_vao_handle() const { return skybox_vao_handle_; }
    unsigned int skybox_vbo_handle() const { return skybox_vbo_handle_; }

    unsigned int active_render_target() const { return active_render_target_; }

    /// 设置 UpdateBuffer 函数指针（由 OpenGLRhiDevice 注入）
    using UpdateBufferFn = std::function<void(unsigned int, size_t, size_t, const void*, bool)>;
    void set_update_buffer_fn(UpdateBufferFn fn) { update_buffer_fn_ = fn; }

    /// 设置 CreateBuffer/CreateVertexArray 函数指针
    using CreateBufferFn = std::function<unsigned int(size_t, const void*, bool, bool)>;
    using CreateVaoFn = std::function<VertexArrayHandle()>;
    void set_create_buffer_fn(CreateBufferFn fn) { create_buffer_fn_ = fn; }
    void set_create_vao_fn(CreateVaoFn fn) { create_vao_fn_ = fn; }

    /// 设置 DeleteVertexArray/DeleteBuffer/DeleteTexture 函数指针（供 Shutdown 走账本）
    using DeleteVaoFn = std::function<void(VertexArrayHandle)>;
    using DeleteBufferFn = std::function<void(unsigned int)>;
    using DeleteTextureFn = std::function<void(unsigned int)>;
    void set_delete_vao_fn(DeleteVaoFn fn) { delete_vao_fn_ = fn; }
    void set_delete_buffer_fn(DeleteBufferFn fn) { delete_buffer_fn_ = fn; }
    void set_delete_texture_fn(DeleteTextureFn fn) { delete_texture_fn_ = fn; }

    /// 设置 CreateTexture 函数指针（供白色纹理走账本）
    using CreateTextureFn = std::function<unsigned int(int, int, const unsigned char*, bool)>;
    void set_create_texture_fn(CreateTextureFn fn) { create_texture_fn_ = fn; }

private:
    // 几何缓冲区
    VertexArrayHandle vao_handle_;
    unsigned int vbo_handle_ = 0;
    unsigned int ebo_handle_ = 0;
    VertexArrayHandle mesh_vao_handle_;
    unsigned int mesh_vbo_handle_ = 0;
    unsigned int mesh_ibo_handle_ = 0;
    unsigned int instance_vbo_handle_ = 0;
    size_t instance_vbo_capacity_ = 0;  ///< 当前 instance VBO 容量（字节）
    unsigned int white_texture_handle_ = 0;

    // Shared mesh template: 同 pass 内跳过重复 VBO 上传
    const BatchVertex* last_shared_vtx_ptr_ = nullptr;
    size_t last_shared_vtx_count_ = 0;

    // Static Mesh VBO 缓存：shared_vertex_ptr → 持久化 VAO/VBO/IBO
    struct StaticMeshEntry {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ibo = 0;
        size_t vtx_count = 0;
        size_t idx_count = 0;
    };
    std::unordered_map<const void*, StaticMeshEntry> static_mesh_cache_;
    StaticMeshEntry CreateStaticMeshVAO(const BatchVertex* vtx_data, size_t vtx_count,
                                         const uint32_t* idx_data, size_t idx_count);

    // Bone SSBO: 所有蒙皮实例的骨骼矩阵打包为一个 SSBO
    unsigned int bone_ssbo_ = 0;
    size_t bone_ssbo_capacity_ = 0;  ///< 当前 SSBO 容量（字节）
    bool bone_ssbo_uploaded_this_frame_ = false;

    // Skinned Instance SSBO (binding 10): per-instance model + bone_offset
    unsigned int skinned_inst_ssbo_ = 0;
    size_t skinned_inst_ssbo_capacity_ = 0;
    bool inst_ssbo_uploaded_this_frame_ = false;

    // 天空盒几何
    VertexArrayHandle skybox_vao_handle_;
    unsigned int skybox_vbo_handle_ = 0;

    // 通用绘制原语状态 (A1)
    VertexArrayHandle prim_vao_handle_;       ///< 通用原语复用的 VAO
    unsigned int prim_program_ = 0;           ///< 当前绑定的着色器程序
    unsigned int prim_index_type_ = 0x1405;   ///< GL_UNSIGNED_INT，当前索引缓冲元素类型 (B0)

    // 后处理全屏四边形
    VertexArrayHandle pp_vao_handle_;
    unsigned int pp_vbo_handle_ = 0;
    unsigned int pp_param_ubo_ = 0;  ///< gen.h PP shader 参数 UBO (binding=2)

    // 3D 粒子四边形
    VertexArrayHandle particle_quad_vao_handle_;
    unsigned int particle_quad_vbo_handle_ = 0;

    // 毛发 (Hair strands)
    VertexArrayHandle hair_vao_handle_;
    unsigned int hair_shader_handle_ = 0;
    // 缓存 uniform locations (在 shader 编译后填充)
    int hair_loc_model_ = -1, hair_loc_view_ = -1, hair_loc_proj_ = -1;
    int hair_loc_cam_ = -1, hair_loc_ldir_ = -1, hair_loc_lcol_ = -1;
    int hair_loc_lint_ = -1, hair_loc_ambient_ = -1;
    int hair_loc_root_ = -1, hair_loc_tip_ = -1, hair_loc_opacity_ = -1;
    int hair_loc_spec1_ = -1, hair_loc_spec2_ = -1;
    int hair_loc_sstr1_ = -1, hair_loc_sstr2_ = -1, hair_loc_scol_ = -1;

    // 活跃渲染目标
    unsigned int active_render_target_ = 0;
    bool is_depth_only_pass_ = false;

    // 全局渲染状态（引用 RhiDevice::global_render_state_）
    DrawExecutorGlobalState& global_state_;

    // 外部函数指针（由 OpenGLRhiDevice 注入）
    UpdateBufferFn update_buffer_fn_ = nullptr;
    CreateBufferFn create_buffer_fn_ = nullptr;
    CreateVaoFn create_vao_fn_ = nullptr;
    DeleteVaoFn delete_vao_fn_ = nullptr;
    DeleteBufferFn delete_buffer_fn_ = nullptr;
    DeleteTextureFn delete_texture_fn_ = nullptr;
    CreateTextureFn create_texture_fn_ = nullptr;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_GL_DRAW_EXECUTOR_H
