/**
 * @file rhi_storage_buffer.h
 * @brief RHI Storage Buffer (SSBO) 扩展接口
 *
 * 提供 SSBO 创建、更新、绑定、删除、读回能力。
 * Clustered Forward+ 光源列表、GPU 粒子等功能依赖此接口。
 */

#ifndef DSE_RHI_STORAGE_BUFFER_H
#define DSE_RHI_STORAGE_BUFFER_H

#include <cstddef>

namespace dse {
namespace render {

/**
 * @class IRhiStorageBuffer
 * @brief Shader Storage Buffer Object 扩展接口
 *
 * OpenGL: GL_SHADER_STORAGE_BUFFER (GL 4.3+)
 * Vulkan: VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
 * DX11:   StructuredBuffer + SRV
 */
class IRhiStorageBuffer {
public:
    virtual ~IRhiStorageBuffer() = default;

    /// 是否支持 SSBO（OpenGL 4.3+ 支持；GL 3.3 使用 UBO fallback）
    virtual bool SupportsSSBO() const { return true; }

    /// 创建 SSBO 缓冲区
    /// @param size 缓冲区大小（字节）
    /// @param data 初始数据指针（可为 nullptr）
    /// @return 缓冲区句柄，0 表示失败
    [[deprecated("使用 CreateGpuBuffer 替代")]]
    virtual unsigned int CreateSSBO(size_t size, const void* data) { (void)size; (void)data; return 0; }

    /// 更新 SSBO 数据（子区域）
    [[deprecated("使用 UpdateGpuBuffer 替代")]]
    virtual void UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) {
        (void)handle; (void)offset; (void)size; (void)data;
    }

    /// 将 SSBO 绑定到指定绑定点（SSBO 绑定点与 UBO 独立）
    [[deprecated("使用 BindGpuBuffer 替代")]]
    virtual void BindSSBO(unsigned int handle, unsigned int binding_point) {
        (void)handle; (void)binding_point;
    }

    /// 删除 SSBO 缓冲区
    [[deprecated("使用 DeleteGpuBuffer 替代")]]
    virtual void DeleteSSBO(unsigned int handle) { (void)handle; }

    /// 同步读回 SSBO 内容到 CPU
    /// @param handle SSBO 句柄
    /// @param offset 读取起始偏移（字节）
    /// @param size   读取大小（字节）
    /// @param dst    目标缓冲区
    [[deprecated("使用 ReadGpuBuffer 替代")]]
    virtual void ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) {
        (void)handle; (void)offset; (void)size; (void)dst;
    }
};

} // namespace render
} // namespace dse

#endif // DSE_RHI_STORAGE_BUFFER_H
