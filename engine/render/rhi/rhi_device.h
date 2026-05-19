/**
 * @file rhi_device.h
 * @brief 渲染硬件接口(RHI)抽象层 — 纯虚基类 RhiDevice + CommandBuffer
 *
 * 本文件仅包含后端无关的抽象接口。
 * 具体后端实现位于各自头文件：
 * - gl_command_buffer.h + gl_rhi_device.h    (OpenGL)
 * - vulkan/vulkan_rhi_device.h (Vulkan)
 * - dx11/dx11_rhi_device.h     (DX11)
 */

#ifndef DSE_RHI_DEVICE_H
#define DSE_RHI_DEVICE_H

#include <vector>
#include <functional>
#include <unordered_map>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/render/rhi/rhi_compute.h"
#include "engine/render/rhi/rhi_storage_buffer.h"
#include "engine/render/rhi/rhi_gpu_driven.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

namespace dse {
namespace render {

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
    virtual void DrawMeshBatch(const std::vector<MeshDrawItem>& items) = 0;
    virtual void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) = 0;
    virtual void ClearColor(const glm::vec4& color) = 0;
    virtual void SetGlobalMat4(const std::string& name, const glm::mat4& value) = 0;
    virtual void SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) = 0;
    virtual void SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) = 0;
    virtual void DrawSkybox(unsigned int cubemap_texture_handle) = 0;
    virtual void DrawPostProcess(PostProcessRequest request) = 0;
    virtual void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) = 0;
    virtual void DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) = 0;

    /// 阴影贴图绑定命令（Pass 中调用，直接委托到 RhiDevice）
    virtual void BindGlobalShadowMap(unsigned int index, unsigned int texture_handle) = 0;
    virtual void BindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) = 0;
    virtual void BindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) = 0;
};

/**
 * @class RhiDevice
 * @brief 渲染硬件接口基类，提供创建 GPU 资源（纹理、缓冲、着色器）及命令缓冲的统一抽象
 *
 * 阴影/光源全局状态接口：所有后端（OpenGL、Vulkan）均实现相同的阴影贴图与光源矩阵绑定，
 * 消除上层代码对具体后端的 dynamic_cast 依赖。
 */
class RhiDevice : public IRhiCompute, public IRhiStorageBuffer, public IRhiGpuDriven {
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

    // --- 统一 GPU Buffer API（Phase 2）---
    // 默认实现转发到旧 API，后端可覆写以获取完整 usage 信息。
    // 旧 CreateBuffer/CreateSSBO/CreateIndirectBuffer 将在全量迁移后标记 deprecated。

    virtual BufferHandle CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) {
        BufferHandle h;
        if (has(desc.usage, GpuBufferUsage::kStorage)) {
            h = BufferHandle{CreateSSBO(desc.size, initial_data)};
        } else if (has(desc.usage, GpuBufferUsage::kIndirect)) {
            h = BufferHandle{CreateIndirectBuffer(desc.size, initial_data)};
        } else {
            bool is_index = has(desc.usage, GpuBufferUsage::kIndex);
            h = BufferHandle{CreateBuffer(desc.size, initial_data, desc.is_dynamic, is_index)};
        }
        if (h) gpu_buffer_usage_map_[h.raw()] = desc.usage;
        return h;
    }

    virtual void UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) {
        auto usage = GetBufferUsage_(handle);
        if (has(usage, GpuBufferUsage::kStorage)) {
            UpdateSSBO(handle.raw(), offset, size, data);
        } else if (has(usage, GpuBufferUsage::kIndirect)) {
            UpdateIndirectBuffer(handle.raw(), offset, size, data);
        } else {
            bool is_index = has(usage, GpuBufferUsage::kIndex);
            UpdateBuffer(handle.raw(), offset, size, data, is_index);
        }
    }

    virtual void DeleteGpuBuffer(BufferHandle handle) {
        auto usage = GetBufferUsage_(handle);
        if (has(usage, GpuBufferUsage::kStorage)) {
            DeleteSSBO(handle.raw());
        } else if (has(usage, GpuBufferUsage::kIndirect)) {
            DeleteIndirectBuffer(handle.raw());
        } else {
            DeleteBuffer(handle.raw());
        }
        gpu_buffer_usage_map_.erase(handle.raw());
    }

    virtual void BindGpuBuffer(BufferHandle handle, uint32_t binding_point) {
        BindSSBO(handle.raw(), binding_point);
    }

    virtual void ReadGpuBuffer(BufferHandle handle, size_t offset, size_t size, void* dst) {
        ReadSSBO(handle.raw(), offset, size, dst);
    }
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

    // --- 扩展能力查询（委托到继承的接口，此处仅保留 PatchLastFrameGPUCulledCount）---

protected:
    std::function<void()> init_keep_alive_;
    void KeepAlive() { if (init_keep_alive_) init_keep_alive_(); }

    GpuBufferUsage GetBufferUsage_(BufferHandle handle) const {
        auto it = gpu_buffer_usage_map_.find(handle.raw());
        return it != gpu_buffer_usage_map_.end() ? it->second : GpuBufferUsage::kVertex;
    }

    std::unordered_map<unsigned int, GpuBufferUsage> gpu_buffer_usage_map_;
};

} // namespace render
} // namespace dse

// 兼容性 using：大量上层代码仍以全局命名空间形式引用这两个类
using dse::render::CommandBuffer;
using dse::render::RhiDevice;

#endif
