/**
 * @file rhi_gpu_driven.h
 * @brief RHI GPU-Driven 渲染扩展接口
 *
 * 提供 Indirect Draw、Mega Buffer、Hi-Z Occlusion Culling、Static Mesh VAO 能力。
 * GPU-Driven 渲染管线的核心基础设施。
 */

#ifndef DSE_RHI_GPU_DRIVEN_H
#define DSE_RHI_GPU_DRIVEN_H

#include "engine/render/rhi/rhi_handle.h"
#include <glm/glm.hpp>
#include <cstddef>
#include <vector>

namespace dse {
namespace render {

/**
 * @class IRhiGpuDriven
 * @brief GPU-Driven 渲染扩展接口（IndirectDraw + MegaBuffer + Hi-Z + StaticMeshVAO）
 *
 * 默认实现为 no-op，后端按能力覆写。
 */
class IRhiGpuDriven {
public:
    virtual ~IRhiGpuDriven() = default;

    // --- Indirect Draw Buffer ---

    /// 是否支持 indirect draw (需要 GL 4.3+ / VK / DX11.1)
    virtual bool SupportsIndirectDraw() const { return false; }

    /// 创建 Indirect Draw Buffer
    [[deprecated("使用 CreateGpuBuffer(kIndirect) 替代")]]
    virtual unsigned int CreateIndirectBuffer(size_t size, const void* data) {
        (void)size; (void)data; return 0;
    }

    /// 更新 Indirect Draw Buffer 子区域
    [[deprecated("使用 UpdateGpuBuffer 替代")]]
    virtual void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) {
        (void)handle; (void)offset; (void)size; (void)data;
    }

    /// 删除 Indirect Draw Buffer
    [[deprecated("使用 DeleteGpuBuffer 替代")]]
    virtual void DeleteIndirectBuffer(unsigned int handle) { (void)handle; }

    /// 绑定 indirect buffer 并发起 Multi-Draw Indexed Indirect
    /// @param byte_offset  indirect buffer 内的字节偏移（用于纹理桶分段绘制）
    virtual void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride, size_t byte_offset = 0) {
        (void)indirect_buffer; (void)draw_count; (void)stride; (void)byte_offset;
    }

    // --- Hi-Z Occlusion Culling ---

    /// 创建 Hi-Z 纹理（R32F 格式，完整 mip chain，nearest 过滤）
    virtual unsigned int CreateHiZTexture(int width, int height) { (void)width; (void)height; return 0; }

    /// 删除 Hi-Z 纹理
    virtual void DeleteHiZTexture(unsigned int handle) { (void)handle; }

    /// 获取 Hi-Z 纹理的 mip 级数
    virtual int GetHiZMipCount(unsigned int handle) const { (void)handle; return 0; }

    /// 获取 Hi-Z 纹理的 GPU 原生句柄
    virtual unsigned int GetHiZGpuTexture(unsigned int handle) const { (void)handle; return 0; }

    // --- Mega Buffer (GPU Driven) ---

    /// 创建 Mega VAO（BatchVertex 布局），同时创建 VBO 和 IBO
    virtual VertexArrayHandle CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                       BufferHandle& out_vbo, BufferHandle& out_ibo) {
        (void)vbo_size_bytes; (void)ibo_size_bytes; out_vbo = {}; out_ibo = {}; return {};
    }

    /// 更新 Mega VBO 子区域数据
    virtual void UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) {
        (void)vbo; (void)offset; (void)size; (void)data;
    }

    /// 更新 Mega IBO 子区域数据
    virtual void UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) {
        (void)ibo; (void)offset; (void)size; (void)data;
    }

    /// 删除 Mega VAO + VBO + IBO
    virtual void DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) {
        (void)vao; (void)vbo; (void)ibo;
    }

    /// 绑定 Mega VAO 供 indirect draw 使用
    virtual void BindMegaVAO(VertexArrayHandle vao) { (void)vao; }

    /// 解绑 VAO
    virtual void UnbindVAO() {}

    // --- Static Mesh VAO ---

    /// 创建静态网格 VAO（含 VBO + 多个 EBO），使用 BatchVertex 属性布局
    virtual VertexArrayHandle CreateStaticMeshVAO(
        const void* vertex_data, size_t vertex_bytes,
        const std::vector<const void*>& ebo_datas,
        const std::vector<size_t>& ebo_sizes,
        BufferHandle& out_vbo,
        std::vector<BufferHandle>& out_ebos) {
        (void)vertex_data; (void)vertex_bytes;
        (void)ebo_datas; (void)ebo_sizes;
        out_vbo = {}; out_ebos.clear();
        return {};
    }

    /// 删除静态网格 VAO + VBO + 所有 EBO
    virtual void DeleteStaticMeshVAO(VertexArrayHandle vao, BufferHandle vbo,
                                      const std::vector<BufferHandle>& ebos) {
        (void)vao; (void)vbo; (void)ebos;
    }

    /// 绑定 VAO 并切换到指定 EBO 进行绘制
    virtual void BindVAOWithEBO(VertexArrayHandle vao, BufferHandle ebo) {
        (void)vao; (void)ebo;
    }


    // --- GPU-Driven 纹理绑定（Phase 5: 每桶绑定纹理） ---

    /// 绑定 GPU-Driven 路径的 PBR 纹理（albedo/normal/MR/emissive/occlusion）
    /// handle=0 时绑定默认白色/平坦纹理。ForwardScenePass 每桶调用一次。
    virtual void BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                                        unsigned int metallic_roughness,
                                        unsigned int emissive, unsigned int occlusion) {
        (void)albedo; (void)normal; (void)metallic_roughness; (void)emissive; (void)occlusion;
    }

    // --- GPU-Driven Shader 可用性查询 ---

    /// GPU-Driven PBR 着色器是否编译成功（cull shader 通过但 PBR/shadow shader 可能失败）
    virtual bool HasGPUDrivenPBRShader() const { return false; }

    // --- GPU-Driven PBR Shader Setup ---

    /// 激活 GPU-Driven PBR 着色器并上传 PerFrame/PerScene UBO（indirect draw 前调用）
    /// csm_shadow_textures[0..cascade_count-1]：已绑定全局阴影贴图句柄（CSMShadowPass 负责绑定）
    virtual void SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                          const glm::vec3& camera_pos,
                                          const glm::vec3& light_dir, const glm::vec3& light_color,
                                          float light_intensity, float ambient_intensity,
                                          float shadow_strength = 0.0f) {
        (void)view; (void)proj; (void)camera_pos;
        (void)light_dir; (void)light_color; (void)light_intensity; (void)ambient_intensity;
        (void)shadow_strength;
    }

    // --- GPU-Driven Shadow Shader Setup ---

    /// 激活 GPU-Driven Shadow（depth-only）着色器并上传 PerFrame UBO（shadow indirect draw 前调用）
    virtual void SetupGPUDrivenShadowShader(const glm::mat4& light_view, const glm::mat4& light_proj) {
        (void)light_view; (void)light_proj;
    }

    // --- GPU-Driven Per-Bucket Material 更新 ---

    /// 更新 GPU-Driven PerMaterial cbuffer/UBO（per-bucket 调用，DX11/Vulkan 用）
    /// @param mat_data  GPUMaterialData 结构指针（128 bytes，与 PerMaterial cbuffer layout 一致）
    virtual void UpdateGPUDrivenMaterial(const void* mat_data) { (void)mat_data; }

    // --- GPU-Driven 实例数据缓存（DX11/Vulkan per-draw model 更新用） ---

    /// 缓存 CPU 侧 GPU-Driven 实例模型矩阵和 draw commands（GL 无需，DX11/Vulkan per-draw 更新用）
    /// @param models  GPUInstanceData 数组起始（80 bytes/entry, model 在 offset 0）
    /// @param cmds    DrawElementsIndirectCommand 数组起始（20 bytes/entry）
    /// @param count   条目数量
    virtual void CacheGPUDrivenInstanceData(const void* models, const void* cmds, int count) {
        (void)models; (void)cmds; (void)count;
    }
};

} // namespace render
} // namespace dse

#endif // DSE_RHI_GPU_DRIVEN_H
