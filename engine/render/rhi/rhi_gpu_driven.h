/**
 * @file rhi_gpu_driven.h
 * @brief RHI GPU-Driven 渲染扩展接口
 *
 * 提供 Indirect Draw、Mega Buffer、Hi-Z Occlusion Culling、Static Mesh VAO 能力。
 * GPU-Driven 渲染管线的核心基础设施。
 */

#ifndef DSE_RHI_GPU_DRIVEN_H
#define DSE_RHI_GPU_DRIVEN_H

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
    virtual unsigned int CreateIndirectBuffer(size_t size, const void* data) {
        (void)size; (void)data; return 0;
    }

    /// 更新 Indirect Draw Buffer 子区域
    virtual void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) {
        (void)handle; (void)offset; (void)size; (void)data;
    }

    /// 删除 Indirect Draw Buffer
    virtual void DeleteIndirectBuffer(unsigned int handle) { (void)handle; }

    /// 绑定 indirect buffer 并发起 Multi-Draw Indexed Indirect
    virtual void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride) {
        (void)indirect_buffer; (void)draw_count; (void)stride;
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

    // --- Static Mesh VAO ---

    /// 创建静态网格 VAO（含 VBO + 多个 EBO），使用 BatchVertex 属性布局
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
};

} // namespace render
} // namespace dse

#endif // DSE_RHI_GPU_DRIVEN_H
