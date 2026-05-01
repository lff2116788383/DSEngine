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
#include <glm/glm.hpp>
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
    GLDrawExecutor() = default;
    ~GLDrawExecutor() = default;

    /// 初始化几何缓冲区（2D 批处理 VAO/VBO/EBO + 3D 网格 VAO/VBO/EBO + 白色纹理）
    using InitCreateVaoFn = std::function<unsigned int()>;
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
    void DrawBatch(const std::vector<SpriteDrawItem>& items,
                    const glm::mat4& view,
                    const glm::mat4& projection,
                    GLPipelineStateManager& state_mgr,
                    GLShaderManager& shader_mgr,
                    UBOManager& ubo_mgr);

    void DrawMeshBatch(const std::vector<MeshDrawItem>& items,
                         const glm::mat4& view,
                         const glm::mat4& projection,
                         GLPipelineStateManager& state_mgr,
                         GLShaderManager& shader_mgr,
                         GLResourceManager& resource_mgr,
                         UBOManager& ubo_mgr);

    void DrawSkybox(unsigned int cubemap_texture_handle,
                      const glm::mat4& view,
                      const glm::mat4& projection,
                      GLShaderManager& shader_mgr);

    void DrawPostProcess(unsigned int source_texture,
                           const std::string& effect_name,
                           const std::vector<float>& params,
                           GLShaderManager& shader_mgr);

    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items,
                           const glm::mat4& view,
                           const glm::mat4& projection,
                           GLShaderManager& shader_mgr);

    // --- 全局阴影/光源矩阵 ---
    void SetGlobalShadowMap(unsigned int index, unsigned int handle) {
        if (index < 3) global_shadow_map_[index] = handle;
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
    void SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
        if (index < 4) global_spot_light_space_matrix_[index] = mat;
    }

    // --- 渲染统计 ---
    void BeginFrame();
    void EndFrame();
    const RenderStats& last_frame_stats() const { return last_frame_stats_; }
    const RenderStats& current_frame_stats() const { return current_frame_stats_; }

    // --- 访问器（供 OpenGLRhiDevice 委托使用） ---
    unsigned int white_texture_handle() const { return white_texture_handle_; }
    unsigned int vao_handle() const { return vao_handle_; }
    unsigned int vbo_handle() const { return vbo_handle_; }
    unsigned int ebo_handle() const { return ebo_handle_; }
    unsigned int mesh_vao_handle() const { return mesh_vao_handle_; }
    unsigned int mesh_vbo_handle() const { return mesh_vbo_handle_; }
    unsigned int mesh_ibo_handle() const { return mesh_ibo_handle_; }
    unsigned int skybox_vao_handle() const { return skybox_vao_handle_; }
    unsigned int skybox_vbo_handle() const { return skybox_vbo_handle_; }

    unsigned int active_render_target() const { return active_render_target_; }

    /// 设置 UpdateBuffer 函数指针（由 OpenGLRhiDevice 注入）
    using UpdateBufferFn = std::function<void(unsigned int, size_t, size_t, const void*, bool)>;
    void set_update_buffer_fn(UpdateBufferFn fn) { update_buffer_fn_ = fn; }

    /// 设置 CreateBuffer/CreateVertexArray 函数指针
    using CreateBufferFn = std::function<unsigned int(size_t, const void*, bool, bool)>;
    using CreateVaoFn = std::function<unsigned int()>;
    void set_create_buffer_fn(CreateBufferFn fn) { create_buffer_fn_ = fn; }
    void set_create_vao_fn(CreateVaoFn fn) { create_vao_fn_ = fn; }

private:
    // 几何缓冲区
    unsigned int vao_handle_ = 0;
    unsigned int vbo_handle_ = 0;
    unsigned int ebo_handle_ = 0;
    unsigned int mesh_vao_handle_ = 0;
    unsigned int mesh_vbo_handle_ = 0;
    unsigned int mesh_ibo_handle_ = 0;
    unsigned int white_texture_handle_ = 0;

    // 天空盒几何
    unsigned int skybox_vao_handle_ = 0;
    unsigned int skybox_vbo_handle_ = 0;

    // 后处理全屏四边形
    unsigned int pp_vao_handle_ = 0;
    unsigned int pp_vbo_handle_ = 0;

    // 3D 粒子四边形
    unsigned int particle_quad_vao_handle_ = 0;
    unsigned int particle_quad_vbo_handle_ = 0;

    // 活跃渲染目标
    unsigned int active_render_target_ = 0;

    // 全局阴影/光源状态
    glm::mat4 global_light_space_matrix_[3];
    glm::mat4 global_spot_light_space_matrix_[4] = {
        glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)
    };
    float global_cascade_splits_[3];
    unsigned int global_shadow_map_[3];
    unsigned int global_spot_shadow_map_[4] = {0, 0, 0, 0};
    unsigned int global_point_shadow_map_[4] = {0, 0, 0, 0};

    // 渲染统计
    RenderStats current_frame_stats_;
    RenderStats last_frame_stats_;

    // 外部函数指针（由 OpenGLRhiDevice 注入）
    UpdateBufferFn update_buffer_fn_ = nullptr;
    CreateBufferFn create_buffer_fn_ = nullptr;
    CreateVaoFn create_vao_fn_ = nullptr;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_GL_DRAW_EXECUTOR_H
