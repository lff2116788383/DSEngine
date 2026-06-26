/**
 * @file webgpu_resource_manager.h
 * @brief WebGPU 资源管理器（manager 拆分：依赖 ctx）。
 *
 * 持有缓冲/纹理/渲染目标资源表、每帧 UBO/几何版本环（res 写、draw 读）、GPU→CPU 异步
 * 双缓冲延迟回读状态、Hi-Z 纹理登记表。缓存同名稳定句柄 device_/queue_（OnDeviceAcquired 设、
 * Shutdown 清）；每帧瞬态经 ctx_ live 访问器读取（不缓存）。
 *
 * 仅在 Emscripten + DSE_ENABLE_WEBGPU 下编入（与各 webgpu 实现文件一致）。
 */

#ifndef DSE_WEBGPU_RESOURCE_MANAGER_H
#define DSE_WEBGPU_RESOURCE_MANAGER_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/webgpu/webgpu_common.h"
#include "engine/render/rhi/webgpu/webgpu_context.h"

#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dse {
namespace render {

/// 录制/帧态归 draw；res 的 BeginGpuReadback 仅只读取其当前 pass 活动态（见下 exec_）。
class WebGPUDrawExecutor;

/**
 * @class WebGPUResourceManager
 * @brief 缓冲/纹理/RT 资源表 + 版本环 + 异步回读 + Hi-Z 的持有者。
 */
class WebGPUResourceManager {
public:
    /// 注入依赖：ctx（设备/帧态/句柄发号）+ exec（仅读其当前 pass 活动态，回读门控用）。
    void Init(WebGPUContext* ctx, WebGPUDrawExecutor* exec) { ctx_ = ctx; exec_ = exec; }
    /// 同名稳定句柄缓存：device 生命周期内不变（AcquireDevice 成功时设，Shutdown 时以空清）。
    void OnDeviceAcquired(WGPUDevice device, WGPUQueue queue) { device_ = device; queue_ = queue; }
    /// orchestrator Shutdown 调用（逆序释放）：释放 RT/纹理/缓冲 + 异步回读 staging + 版本环。
    void Shutdown();

    // --- 版本环跨界访问器（draw 读，见 WEBGPU_MANAGER_SPLIT_PLAN「版本环跨界」）---
    const UboVersion* FindUboVersion(unsigned int handle) const {
        auto it = ubo_versions_.find(handle);
        return it == ubo_versions_.end() ? nullptr : &it->second;
    }
    const UboVersion* FindGeomVersion(unsigned int handle) const {
        auto it = geom_versions_.find(handle);
        return it == geom_versions_.end() ? nullptr : &it->second;
    }
    WGPUBuffer ubo_ring() const { return ubo_ring_; }
    /// orchestrator BeginFrame 调用：环游标归零 + 清当帧版本图（buffer 跨帧复用）。
    void BeginFrameResetVersions() {
        ubo_ring_cursor_ = 0; ubo_versions_.clear();
        geom_ring_cursor_ = 0; geom_versions_.clear();
    }

    // --- 查表（找不到返回 nullptr）；draw 经 res_ 转发于此 ---
    const BufferEntry*       FindBuffer(unsigned int handle) const;
    const TextureEntry*      FindTexture(unsigned int handle) const;
    const RenderTargetEntry* FindRenderTarget(unsigned int handle) const;

    // --- 版本环分配（res 写）；draw 经 res_ 转发于此 ---
    uint64_t AllocUboVersion(const void* data, uint64_t size);
    uint64_t AllocGeomVersion(const void* data, uint64_t size);

    // --- 纹理创建内部助手（draw 亦经 res_ 转发 CreateTextureImpl/CreateSampler）---
    unsigned int CreateTextureImpl(WGPUTextureDimension dim, WGPUTextureViewDimension view_dim,
                                   uint32_t width, uint32_t height, uint32_t depth_or_layers,
                                   WGPUTextureFormat format, WGPUTextureUsageFlags usage,
                                   uint32_t mip_levels, int msaa_samples,
                                   const std::vector<const unsigned char*>& layer_data,
                                   const TextureSamplerDesc& sampler);
    WGPUSampler CreateSampler(const TextureSamplerDesc& desc, uint32_t mip_levels) const;
    void DestroyTextureEntry(TextureEntry& e);

    // --- 渲染目标 ---
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc);
    void DeleteRenderTarget(unsigned int render_target_handle);
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const;
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const;

    // --- 纹理 ---
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter);
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                 const TextureSamplerDesc& sampler);
    unsigned int CreateComputeWriteTexture2D(int width, int height);
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter);
    unsigned int CreateTextureCubeWithMips(const std::vector<CubeMipLevel>& mips, bool linear_filter);
    unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter);
    void DeleteTexture(unsigned int texture_handle);

    // --- 缓冲 ---
    unsigned int CreateBufferRaw(size_t logical_size, const void* data,
                                 WGPUBufferUsageFlags usage, bool is_index);
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index);
    BufferHandle CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data);
    void UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data);
    void DeleteGpuBuffer(BufferHandle handle);
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index);
    void DeleteBuffer(unsigned int handle);

    // --- GPU→CPU 异步双缓冲延迟回读 ---
    bool BeginGpuReadback(BufferHandle handle, size_t offset, size_t size);
    const void* GetLastReadbackResult(size_t* out_size = nullptr) const;
    /// orchestrator EndFrame（QueueSubmit 后）调用：对 pending staging 发 mapAsync。
    void KickDeferredReadback();

    // --- Hi-Z 纹理 ---
    unsigned int CreateHiZTexture(int width, int height);
    void DeleteHiZTexture(unsigned int handle);
    int GetHiZMipCount(unsigned int handle) const;
    unsigned int GetHiZGpuTexture(unsigned int handle) const;

private:
    // ctx 转发的同名 shim（迁移方法体内 bare 调用文本不变）。
    bool EnsureInitialized() { return ctx_->EnsureInitialized(); }
    unsigned int NextHandle() { return ctx_->NextHandle(); }

    WebGPUContext* ctx_ = nullptr;
    WebGPUDrawExecutor* exec_ = nullptr;  ///< 只读：BeginGpuReadback 门控当前 pass 活动态
    WGPUDevice device_ = nullptr;
    WGPUQueue queue_ = nullptr;

    // 资源表（句柄 → 原生对象）
    std::unordered_map<unsigned int, BufferEntry>       buffers_;
    std::unordered_map<unsigned int, TextureEntry>      textures_;
    std::unordered_map<unsigned int, RenderTargetEntry> render_targets_;

    // --- 每帧 UBO 版本环（res 写、draw 读：每 draw UpdateGpuBuffer 后绑定，故环内独立切片）---
    WGPUBuffer ubo_ring_ = nullptr;
    uint64_t ubo_ring_size_ = 0;
    uint64_t ubo_ring_cursor_ = 0;
    std::unordered_map<unsigned int, UboVersion> ubo_versions_;  ///< 句柄 → 当帧最近版本切片（每帧清）

    // --- 每帧几何版本环（同 UBO 版本环，但服务顶点/索引缓冲）---
    WGPUBuffer geom_ring_ = nullptr;
    uint64_t geom_ring_size_ = 0;
    uint64_t geom_ring_cursor_ = 0;
    std::unordered_map<unsigned int, UboVersion> geom_versions_;  ///< 顶点/索引句柄 → 当帧最近版本切片（每帧清）

    // --- GPU→CPU 异步双缓冲延迟回读状态 ---
    WGPUBuffer async_rb_staging_[2] = {nullptr, nullptr};  ///< 轮换 staging 缓冲
    size_t     async_rb_capacity_[2] = {0, 0};             ///< 各 staging 当前容量（按需扩容）
    int        async_rb_write_idx_ = 0;                    ///< 本帧写入的 staging 下标（0/1 轮换）
    bool       async_rb_has_pending_ = false;              ///< 是否有一帧拷贝待 map 回读
    size_t     async_rb_pending_size_ = 0;                 ///< 待 map 的字节数
    int        async_rb_pending_idx_ = 0;                  ///< 待 map 的 staging 下标
    bool       async_rb_mapped_[2] = {false, false};       ///< 各 staging 是否正处于异步 map 飞行中
    bool       async_rb_ready_ = false;                    ///< async_rb_result_ 是否含有效（上帧）数据
    std::vector<uint8_t> async_rb_result_;                 ///< 最近一次成功 map 的回读数据
    struct DeferredReadbackCtx { WebGPUResourceManager* dev; int idx; size_t size; };
    static void OnDeferredReadbackMapped(WGPUBufferMapAsyncStatus status, void* userdata);

    // --- Hi-Z 纹理登记表（hiz 句柄 → 引擎纹理句柄，R32Float 完整 mip 链）---
    std::unordered_map<unsigned int, unsigned int> hiz_textures_;  ///< hiz_handle → 引擎纹理句柄
};

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif  // DSE_WEBGPU_RESOURCE_MANAGER_H
