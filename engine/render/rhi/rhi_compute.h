/**
 * @file rhi_compute.h
 * @brief RHI Compute Shader 扩展接口
 *
 * 提供 Compute Shader 创建、调度、Uniform 设置、Texture Image 绑定等能力。
 * RhiDevice 通过多继承获得此接口，后端按需 override。
 */

#ifndef DSE_RHI_COMPUTE_H
#define DSE_RHI_COMPUTE_H

#include <string>

namespace dse {
namespace render {

/**
 * @class IRhiCompute
 * @brief Compute Shader 管线扩展接口
 *
 * 默认实现为 no-op（返回 0 或不执行），后端按能力覆写。
 * OpenGL 4.3+ / Vulkan / DX11 均可选实现。
 */
class IRhiCompute {
public:
    virtual ~IRhiCompute() = default;

    /// 是否支持 compute shader
    virtual bool SupportsCompute() const { return false; }

    /// 是否支持 SSBO compute + 同步读回（GPU 草地风场使用）
    virtual bool SupportsSSBOCompute() const { return false; }

    // --- Compute Shader 生命周期 ---

    /// 创建 compute shader（source 为后端原生语言：GLSL/SPIR-V binary/HLSL）
    /// @return shader 句柄，0 表示失败
    virtual unsigned int CreateComputeShader(const std::string& source) { (void)source; return 0; }

    /// 删除 compute shader
    virtual void DeleteComputeShader(unsigned int handle) { (void)handle; }

    // --- Compute Shader 调度 ---

    /// 调度 compute shader 执行
    virtual void DispatchCompute(unsigned int shader_handle,
                                 unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
        (void)shader_handle; (void)groups_x; (void)groups_y; (void)groups_z;
    }

    /// 插入内存屏障（保证 compute 写入对后续图形管线可见）
    virtual void ComputeMemoryBarrier() {}

    /// 开始 compute pass（批量录制多个 dispatch，一次提交）
    virtual void BeginComputePass() {}

    /// 结束 compute pass 并提交所有录制的 dispatch
    virtual void EndComputePass() {}

    // --- Compute Texture 绑定 ---

    /// 将纹理绑定到 compute shader 的 image 单元（image load/store）
    virtual void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) {
        (void)binding; (void)texture_handle; (void)read_only;
    }

    /// 将纹理的指定 mip level 绑定到 compute shader 的 image 单元
    virtual void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                           int mip_level, bool read_only, bool r32f = false) {
        (void)binding; (void)texture_handle; (void)mip_level; (void)read_only; (void)r32f;
    }

    /// 将纹理绑定到 compute shader 的采样器单元（用于 textureLod 采样）
    virtual void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) {
        (void)unit; (void)texture_handle;
    }

    // --- Compute Uniform 设置 ---

    virtual void SetComputeUniformInt(unsigned int shader, const char* name, int value) {
        (void)shader; (void)name; (void)value;
    }
    virtual void SetComputeUniformFloat(unsigned int shader, const char* name, float value) {
        (void)shader; (void)name; (void)value;
    }
    virtual void SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
        (void)shader; (void)name; (void)x; (void)y;
    }
    virtual void SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
        (void)shader; (void)name; (void)x; (void)y;
    }
    virtual void SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) {
        (void)shader; (void)name; (void)x; (void)y; (void)z;
    }
    virtual void SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) {
        (void)shader; (void)name; (void)x; (void)y; (void)z;
    }
    virtual void SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
        (void)shader; (void)name; (void)x; (void)y; (void)z; (void)w;
    }
    virtual void SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
        (void)shader; (void)name; (void)data;
    }
};

} // namespace render
} // namespace dse

#endif // DSE_RHI_COMPUTE_H
