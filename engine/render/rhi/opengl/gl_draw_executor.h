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

    // --- 通用绘制原语 (A1) ---
    /// 设置后续 Prim* 绘制的图元拓扑（由 BindPipeline 从 PSO desc 推送）。
    void PrimSetTopology(PrimitiveTopology topology);
    void PrimBindShaderProgram(unsigned int program_handle);
    void PrimBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                              const std::vector<VertexAttr>& attrs,
                              VertexInputRate rate = VertexInputRate::PerVertex);
    void PrimPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size);
    void PrimDraw(uint32_t vertex_count, uint32_t first_vertex);

    // --- 通用绘制原语 (B0): 索引 / 2D 纹理 / UBO / 索引绘制 ---
    void PrimBindIndexBuffer(unsigned int buffer_handle, IndexType type);
    void PrimBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim);
    void PrimBindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                               uint32_t offset, uint32_t size);
    void PrimBindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                               uint32_t offset, uint32_t size);
    void PrimDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex);

    // --- 通用绘制原语 (B2b 前置): 实例化索引绘制 ---
    void PrimDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                  uint32_t first_index, int32_t base_vertex,
                                  uint32_t first_instance);

    // --- 通用绘制原语 (B2b-5): GPU-driven 间接索引绘制 ---
    // 间接绘制在 OpenGLRhiDevice::RealDrawIndexedIndirect 里发起（pfn_glMultiDrawElementsIndirect
    // 解析在 device 端），但须复用本执行器记录的 prim VAO（含 VBO 属性 + EBO）与索引元素类型。
    unsigned int PrimVaoHandle() const { return prim_vao_handle_.raw(); }
    unsigned int PrimIndexType() const { return prim_index_type_; }

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

    // 顶点流绑定记录（按 slot 索引）。GLES3.0/WebGL2 与 GLES3.1 无 base-vertex 绘制变体，
    // 用「重设 PerVertex 属性指针偏移 base_vertex*stride」模拟，故须记住各流的 buffer/stride/attrs/rate。
    struct PrimVertexBinding {
        unsigned int buffer = 0;
        uint32_t stride = 0;
        std::vector<VertexAttr> attrs;
        VertexInputRate rate = VertexInputRate::PerVertex;
    };
    std::vector<PrimVertexBinding> prim_vertex_bindings_;
    /// 把所有 PerVertex 流的属性指针整体偏移 base_vertex*stride（base_vertex=0 复位为原始偏移）。
    void ApplyPrimBaseVertex(int32_t base_vertex);
    unsigned int prim_program_ = 0;           ///< 当前绑定的着色器程序
    unsigned int prim_index_type_ = 0x1405;   ///< GL_UNSIGNED_INT，当前索引缓冲元素类型 (B0)
    unsigned int prim_topology_ = 0x0004;     ///< GL_TRIANGLES，当前 PSO 拓扑（BindPipeline 推送）

    // 通用 push constant → push-block UBO 降级支撑（契约 §8.2）。
    // 编译器把 layout(push_constant) 降级为按 stage 命名的 std140 UBO 块（DsePushVS/FS，
    // 无显式 binding——ESSL300 不支持）。后端按块名反射 + glUniformBlockBinding 绑到保留
    // binding（VS=14/FS=15，避开真 UBO 的 0..9）+ 建 backing UBO，PushConstants 按 offset 写。
    static constexpr unsigned int kPushUboBindingVS = 14;
    static constexpr unsigned int kPushUboBindingFS = 15;
    struct PushUboStage {
        unsigned int ubo = 0;       ///< backing UBO（0=该程序此 stage 无 push 块）
        unsigned int binding = 0;   ///< 保留 binding 点
        int size = 0;               ///< 块字节大小（GL_UNIFORM_BLOCK_DATA_SIZE）
    };
    struct PushUboProgram {
        PushUboStage vs;
        PushUboStage fs;
        bool initialized = false;
    };
    std::unordered_map<unsigned int, PushUboProgram> push_ubo_programs_;
    PushUboProgram& EnsurePushUbo(unsigned int program);

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
