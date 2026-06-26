/**
 * @file webgpu_resource_manager.cpp
 * @brief WebGPUResourceManager 实现（机械抽自 webgpu_rhi_device.cpp）。
 *
 * 方法体经 out/split_tools/gen.py 机械抽取（两步 sed + live 转发 + 跨界访问器改写），
 * 仅 Shutdown 为手写编排片段（释放本 manager 持有的资源；设备/交换链归 ctx）。
 */

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/webgpu/webgpu_resource_manager.h"
#include "engine/render/rhi/webgpu/webgpu_draw_executor.h"  // exec_->cur_pass()/cur_compute_pass() 完整类型

#include "engine/base/debug.h"

#include <algorithm>
#include <cstring>

namespace dse {
namespace render {

void WebGPUResourceManager::Shutdown() {
    // 释放本 manager 持有的资源对象；交换链/队列/设备由 ctx 释放。
    for (auto& [h, rt] : render_targets_) {
        (void)h;
        for (unsigned int th : rt.color_textures) {
            auto it = textures_.find(th);
            if (it != textures_.end()) { DestroyTextureEntry(it->second); textures_.erase(it); }
        }
        if (rt.depth_texture) {
            auto it = textures_.find(rt.depth_texture);
            if (it != textures_.end()) { DestroyTextureEntry(it->second); textures_.erase(it); }
        }
    }
    render_targets_.clear();
    for (auto& [h, e] : textures_) { (void)h; DestroyTextureEntry(e); }
    textures_.clear();
    for (auto& [h, e] : buffers_) { (void)h; if (e.buffer) wgpuBufferRelease(e.buffer); }
    buffers_.clear();
    hiz_textures_.clear();
    // GPU→CPU 异步双缓冲延迟回读 staging（map 飞行中所有权随 manager 销毁一并释放）。
    for (int i = 0; i < 2; ++i) {
        if (async_rb_staging_[i]) { wgpuBufferRelease(async_rb_staging_[i]); async_rb_staging_[i] = nullptr; }
        async_rb_capacity_[i] = 0;
        async_rb_mapped_[i] = false;
    }
    async_rb_has_pending_ = false;
    async_rb_ready_ = false;
    // UBO / 几何版本环。
    if (ubo_ring_) { wgpuBufferRelease(ubo_ring_); ubo_ring_ = nullptr; ubo_ring_size_ = 0; ubo_ring_cursor_ = 0; }
    if (geom_ring_) { wgpuBufferRelease(geom_ring_); geom_ring_ = nullptr; geom_ring_size_ = 0; geom_ring_cursor_ = 0; }
    ubo_versions_.clear();
    geom_versions_.clear();
}

uint64_t WebGPUResourceManager::AllocUboVersion(const void* data, uint64_t size) {
    if (!device_ || !data || size == 0) return UINT64_MAX;
    constexpr uint64_t kAlign = 256;  // WebGPU minUniformBufferOffsetAlignment 默认 256
    const uint64_t aligned_size = (size + 3) & ~uint64_t(3);
    const uint64_t off = (ubo_ring_cursor_ + kAlign - 1) & ~(kAlign - 1);
    // 环不足以容纳本次分配：本帧已录制的 BindGroup 仍引用现有环缓冲，不能中途重建；
    //   故仅当环尚未创建或在帧首（游标为 0）时按需扩容，运行中溢出则降级（返回失败，回退原存储）。
    if (off + aligned_size > ubo_ring_size_) {
        if (ubo_ring_cursor_ != 0) {
            DEBUG_LOG_WARN("WebGPU: UBO 版本环本帧溢出（需 {} > 容量 {}），该 UBO 回退原存储",
                           static_cast<unsigned long long>(off + aligned_size),
                           static_cast<unsigned long long>(ubo_ring_size_));
            return UINT64_MAX;
        }
        uint64_t new_size = ubo_ring_size_ ? ubo_ring_size_ : (1u << 22);  // 4MB 起步
        while (off + aligned_size > new_size) new_size *= 2;
        if (ubo_ring_) wgpuBufferRelease(ubo_ring_);
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        bd.size = new_size;
        ubo_ring_ = wgpuDeviceCreateBuffer(device_, &bd);
        ubo_ring_size_ = ubo_ring_ ? new_size : 0;
        if (!ubo_ring_) return UINT64_MAX;
    }
    if (aligned_size == size) {
        wgpuQueueWriteBuffer(queue_, ubo_ring_, off, data, size);
    } else {
        std::vector<uint8_t> padded(aligned_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, ubo_ring_, off, padded.data(), aligned_size);
    }
    ubo_ring_cursor_ = off + aligned_size;
    return off;
}

uint64_t WebGPUResourceManager::AllocGeomVersion(const void* data, uint64_t size) {
    if (!device_ || !data || size == 0) return UINT64_MAX;
    // 顶点缓冲偏移须 4 对齐；索引缓冲偏移须为索引元素字节数（2/4）的倍数。取 4 对齐两者均满足。
    constexpr uint64_t kAlign = 4;
    const uint64_t aligned_size = (size + 3) & ~uint64_t(3);
    const uint64_t off = (geom_ring_cursor_ + kAlign - 1) & ~(kAlign - 1);
    // 同 UBO 版本环：本帧已录制的绑定/索引绑定仍引用现有环缓冲，不能中途重建；
    //   故仅在环尚未创建或在帧首（游标为 0）按需扩容，运行中溢出则降级（返回失败，回退原存储）。
    if (off + aligned_size > geom_ring_size_) {
        if (geom_ring_cursor_ != 0) {
            DEBUG_LOG_WARN("WebGPU: 几何版本环本帧溢出（需 {} > 容量 {}），该顶点/索引回退原存储",
                           static_cast<unsigned long long>(off + aligned_size),
                           static_cast<unsigned long long>(geom_ring_size_));
            return UINT64_MAX;
        }
        uint64_t new_size = geom_ring_size_ ? geom_ring_size_ : (1u << 22);  // 4MB 起步
        while (off + aligned_size > new_size) new_size *= 2;
        if (geom_ring_) wgpuBufferRelease(geom_ring_);
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
        bd.size = new_size;
        geom_ring_ = wgpuDeviceCreateBuffer(device_, &bd);
        geom_ring_size_ = geom_ring_ ? new_size : 0;
        if (!geom_ring_) return UINT64_MAX;
    }
    if (aligned_size == size) {
        wgpuQueueWriteBuffer(queue_, geom_ring_, off, data, size);
    } else {
        std::vector<uint8_t> padded(aligned_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, geom_ring_, off, padded.data(), aligned_size);
    }
    geom_ring_cursor_ = off + aligned_size;
    return off;
}

WGPUSampler WebGPUResourceManager::CreateSampler(const TextureSamplerDesc& desc, uint32_t mip_levels) const {
    WGPUSamplerDescriptor sd{};
    const WGPUAddressMode am = ToAddressMode(desc.wrap);
    sd.addressModeU = am; sd.addressModeV = am; sd.addressModeW = am;
    const WGPUFilterMode fm = ToFilterMode(desc.filter);
    sd.magFilter = fm; sd.minFilter = fm;
    sd.mipmapFilter = (desc.filter == TextureFilter::Linear) ? WGPUMipmapFilterMode_Linear
                                                             : WGPUMipmapFilterMode_Nearest;
    sd.lodMinClamp = 0.0f;
    sd.lodMaxClamp = (mip_levels > 1) ? static_cast<float>(mip_levels) : 32.0f;
    sd.maxAnisotropy = 1;
    sd.compare = WGPUCompareFunction_Undefined;  // 比较采样器（阴影 PCF）按需另建
    return wgpuDeviceCreateSampler(device_, &sd);
}

void WebGPUResourceManager::DestroyTextureEntry(TextureEntry& e) {
    if (e.sampler) { wgpuSamplerRelease(e.sampler); e.sampler = nullptr; }
    if (e.view)    { wgpuTextureViewRelease(e.view); e.view = nullptr; }
    if (e.texture && e.owns_texture) { wgpuTextureRelease(e.texture); }
    e.texture = nullptr;
}

unsigned int WebGPUResourceManager::CreateTextureImpl(
        WGPUTextureDimension dim, WGPUTextureViewDimension view_dim,
        uint32_t width, uint32_t height, uint32_t depth_or_layers,
        WGPUTextureFormat format, WGPUTextureUsageFlags usage,
        uint32_t mip_levels, int msaa_samples,
        const std::vector<const unsigned char*>& layer_data,
        const TextureSamplerDesc& sampler) {
    if (!EnsureInitialized() || !device_) return 0;
    width = width > 0 ? width : 1;
    height = height > 0 ? height : 1;
    depth_or_layers = depth_or_layers > 0 ? depth_or_layers : 1;
    mip_levels = mip_levels > 0 ? mip_levels : 1;

    WGPUTextureDescriptor td{};
    td.usage = usage;
    td.dimension = dim;
    td.size = WGPUExtent3D{width, height, depth_or_layers};
    td.format = format;
    td.mipLevelCount = mip_levels;
    td.sampleCount = static_cast<uint32_t>(msaa_samples > 1 ? msaa_samples : 1);
    WGPUTexture tex = wgpuDeviceCreateTexture(device_, &td);
    if (!tex) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateTexture 失败 ({}x{}x{} fmt=0x{:x})",
                        width, height, depth_or_layers, static_cast<unsigned int>(format));
        return 0;
    }

    // mip0 上传（各层 RGBA8 紧打包；nullptr 层跳过，供 RT 附件 / 后续生成）。
    for (uint32_t z = 0; z < layer_data.size() && z < depth_or_layers; ++z) {
        WriteTextureLayerRGBA8(queue_, tex, 0, width, height, z, layer_data[z]);
    }

    WGPUTextureViewDescriptor vd{};
    vd.format = format;
    vd.dimension = view_dim;
    vd.baseMipLevel = 0;
    vd.mipLevelCount = mip_levels;
    vd.baseArrayLayer = 0;
    vd.arrayLayerCount = (dim == WGPUTextureDimension_3D) ? 1u : depth_or_layers;
    vd.aspect = WGPUTextureAspect_All;
    WGPUTextureView view = wgpuTextureCreateView(tex, &vd);

    TextureEntry e;
    e.texture = tex;
    e.view = view;
    e.sampler = CreateSampler(sampler, mip_levels);
    e.format = format;
    e.view_dim = view_dim;
    e.width = width; e.height = height;
    e.depth = (dim == WGPUTextureDimension_3D) ? depth_or_layers : 1;
    e.array_layers = (dim == WGPUTextureDimension_3D) ? 1 : depth_or_layers;
    e.mip_levels = mip_levels;
    e.msaa_samples = msaa_samples;
    e.owns_texture = true;

    const unsigned int h = NextHandle();
    textures_[h] = e;
    return h;
}

const BufferEntry* WebGPUResourceManager::FindBuffer(unsigned int handle) const {
    auto it = buffers_.find(handle);
    return it != buffers_.end() ? &it->second : nullptr;
}

const TextureEntry* WebGPUResourceManager::FindTexture(unsigned int handle) const {
    auto it = textures_.find(handle);
    return it != textures_.end() ? &it->second : nullptr;
}

const RenderTargetEntry* WebGPUResourceManager::FindRenderTarget(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? &it->second : nullptr;
}

unsigned int WebGPUResourceManager::CreateRenderTarget(const RenderTargetDesc& desc) {
    if (!EnsureInitialized() || !device_) return 0;
    RenderTargetEntry rt;
    rt.width = desc.width > 0 ? desc.width : 1;
    rt.height = desc.height > 0 ? desc.height : 1;
    rt.is_cube = desc.cube_map;
    // 注：MSAA 解析（多重采样颜色 → 单采样可采样纹理）在渲染 pass 组装时落地；
    //     资源结构先以单采样附件成形（多重采样纹理不可直接 TextureBinding 采样）。
    rt.msaa_samples = 1;

    const TextureSamplerDesc rt_sampler{TextureFilter::Linear, TextureWrap::ClampToEdge};

    if (desc.has_color) {
        WGPUTextureUsageFlags color_usage = WGPUTextureUsage_RenderAttachment |
                                            WGPUTextureUsage_TextureBinding |
                                            WGPUTextureUsage_CopySrc;
        if (desc.allow_uav) color_usage |= WGPUTextureUsage_StorageBinding;
        // 场景颜色用 HDR RGBA16F，与桌面/GL 后端 color=RGBA16F 一致。
        const WGPUTextureFormat color_fmt = WGPUTextureFormat_RGBA16Float;
        const uint32_t mips = desc.generate_mipmaps ? FullMipCount(rt.width, rt.height) : 1;
        const int n = desc.color_attachment_count > 0 ? desc.color_attachment_count : 1;
        for (int i = 0; i < n; ++i) {
            unsigned int th = desc.cube_map
                ? CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
                                    rt.width, rt.height, 6, color_fmt, color_usage, mips, 1, {}, rt_sampler)
                : CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                                    rt.width, rt.height, 1, color_fmt, color_usage, mips, 1, {}, rt_sampler);
            if (th) rt.color_textures.push_back(th);
        }
    }

    if (desc.has_depth) {
        const WGPUTextureUsageFlags depth_usage = WGPUTextureUsage_RenderAttachment |
                                                  WGPUTextureUsage_TextureBinding;
        const WGPUTextureFormat depth_fmt = WGPUTextureFormat_Depth32Float;
        rt.depth_texture = desc.cube_map
            ? CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
                                rt.width, rt.height, 6, depth_fmt, depth_usage, 1, 1, {}, rt_sampler)
            : CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                                rt.width, rt.height, 1, depth_fmt, depth_usage, 1, 1, {}, rt_sampler);
    }

    const unsigned int h = NextHandle();
    render_targets_[h] = std::move(rt);
    return h;
}

void WebGPUResourceManager::DeleteRenderTarget(unsigned int render_target_handle) {
    auto it = render_targets_.find(render_target_handle);
    if (it == render_targets_.end()) return;
    for (unsigned int th : it->second.color_textures) {
        auto te = textures_.find(th);
        if (te != textures_.end()) { DestroyTextureEntry(te->second); textures_.erase(te); }
    }
    if (it->second.depth_texture) {
        auto te = textures_.find(it->second.depth_texture);
        if (te != textures_.end()) { DestroyTextureEntry(te->second); textures_.erase(te); }
    }
    render_targets_.erase(it);
}

unsigned int WebGPUResourceManager::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    return GetRenderTargetColorTexture(render_target_handle, 0);
}

unsigned int WebGPUResourceManager::GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const {
    const RenderTargetEntry* rt = FindRenderTarget(render_target_handle);
    if (!rt || index < 0 || static_cast<size_t>(index) >= rt->color_textures.size()) return 0;
    return rt->color_textures[index];
}

unsigned int WebGPUResourceManager::GetRenderTargetDepthTexture(unsigned int render_target_handle) const {
    const RenderTargetEntry* rt = FindRenderTarget(render_target_handle);
    return rt ? rt->depth_texture : 0;
}

std::vector<unsigned char> WebGPUResourceManager::ReadRenderTargetColorRgba8(unsigned int render_target_handle) const {
    return ReadRenderTargetColorRgba8WithSize(render_target_handle).pixels;
}

RenderTargetReadback WebGPUResourceManager::ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const {
    // WebGPU 的 GPU→CPU 回读是异步的（texture→staging buffer copy + mapAsync）。在浏览器
    // 主线程同步返回需 ASYNCIFY，本路径不启用。回读供桌面编辑器/CI 像素校验用，Web 运行期渲染
    // 不依赖它。
    (void)render_target_handle;
    return {};
}

unsigned int WebGPUResourceManager::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    return CreateTexture2D(width, height, rgba8_data, TextureSamplerDesc::FromLinearFlag(linear_filter));
}

unsigned int WebGPUResourceManager::CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                              const TextureSamplerDesc& sampler) {
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding |
                                        WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;
    return CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                             static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1,
                             WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, {rgba8_data}, sampler);
}

unsigned int WebGPUResourceManager::CreateComputeWriteTexture2D(int width, int height) {
    // 可供 compute 写入的 storage image。usage 含 StorageBinding（compute textureStore）
    // + CopySrc（回读/拷贝至下一级金字塔）+ TextureBinding（随后采样）。rgba8unorm 为 WebGPU
    // storage 纹理保证支持的格式。无初始数据。
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_StorageBinding |
                                        WGPUTextureUsage_CopySrc |
                                        WGPUTextureUsage_CopyDst |
                                        WGPUTextureUsage_TextureBinding;
    return CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                             static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1,
                             WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, {nullptr},
                             TextureSamplerDesc::FromLinearFlag(false));
}

unsigned int WebGPUResourceManager::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    std::vector<const unsigned char*> faces(6, nullptr);
    if (rgba8_faces) for (int i = 0; i < 6; ++i) faces[i] = rgba8_faces[i];
    return CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
                             static_cast<uint32_t>(width), static_cast<uint32_t>(height), 6,
                             WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, faces,
                             TextureSamplerDesc::FromLinearFlag(linear_filter));
}

unsigned int WebGPUResourceManager::CreateTextureCubeWithMips(const std::vector<CubeMipLevel>& mips, bool linear_filter) {
    if (mips.empty()) return 0;
    const uint32_t mip_count = static_cast<uint32_t>(mips.size());
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    // 先建带完整 mip 链的 cube（不在 Impl 内上传），再逐 mip 逐面写入。
    const unsigned int h = CreateTextureImpl(
        WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
        static_cast<uint32_t>(mips[0].width), static_cast<uint32_t>(mips[0].height), 6,
        WGPUTextureFormat_RGBA8Unorm, usage, mip_count, 1, {},
        TextureSamplerDesc::FromLinearFlag(linear_filter));
    const TextureEntry* e = FindTexture(h);
    if (!e || !e->texture) return h;
    for (uint32_t m = 0; m < mip_count; ++m) {
        const uint32_t w = static_cast<uint32_t>(mips[m].width > 0 ? mips[m].width : 1);
        const uint32_t ht = static_cast<uint32_t>(mips[m].height > 0 ? mips[m].height : 1);
        for (uint32_t f = 0; f < 6; ++f) {
            WriteTextureLayerRGBA8(queue_, e->texture, m, w, ht, f, mips[m].faces[f]);
        }
    }
    return h;
}

unsigned int WebGPUResourceManager::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) {
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    const unsigned int h = CreateTextureImpl(
        WGPUTextureDimension_3D, WGPUTextureViewDimension_3D,
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(depth),
        WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, {},
        TextureSamplerDesc::FromLinearFlag(linear_filter));
    const TextureEntry* e = FindTexture(h);
    if (e && e->texture && rgba8_data && width > 0 && height > 0 && depth > 0) {
        // 3D 体一次性整卷上传（rowsPerImage=height，writeSize.depth=depth）。
        WGPUImageCopyTexture dst{};
        dst.texture = e->texture;
        dst.mipLevel = 0;
        dst.origin = WGPUOrigin3D{0, 0, 0};
        dst.aspect = WGPUTextureAspect_All;
        WGPUTextureDataLayout layout{};
        layout.offset = 0;
        layout.bytesPerRow = static_cast<uint32_t>(width) * 4u;
        layout.rowsPerImage = static_cast<uint32_t>(height);
        WGPUExtent3D extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                            static_cast<uint32_t>(depth)};
        wgpuQueueWriteTexture(queue_, &dst, rgba8_data,
                              static_cast<size_t>(width) * height * depth * 4u, &layout, &extent);
    }
    return h;
}

void WebGPUResourceManager::DeleteTexture(unsigned int texture_handle) {
    auto it = textures_.find(texture_handle);
    if (it == textures_.end()) return;
    exec_->InvalidateComputeMipViews(texture_handle);  // 先释放该句柄缓存的单 mip 视图
    DestroyTextureEntry(it->second);
    textures_.erase(it);
}

unsigned int WebGPUResourceManager::CreateBufferRaw(size_t logical_size, const void* data,
                                              WGPUBufferUsageFlags usage, bool is_index) {
    if (logical_size == 0 || !EnsureInitialized() || !device_) return 0;
    const uint64_t alloc = AlignUp4(logical_size);
    WGPUBufferDescriptor bd{};
    bd.usage = usage;
    bd.size = alloc;
    bd.mappedAtCreation = (data != nullptr);
    WGPUBuffer buf = wgpuDeviceCreateBuffer(device_, &bd);
    if (!buf) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateBuffer 失败 (size={})", static_cast<unsigned long long>(alloc));
        return 0;
    }
    if (data) {
        void* mapped = wgpuBufferGetMappedRange(buf, 0, alloc);
        if (mapped) std::memcpy(mapped, data, logical_size);
        wgpuBufferUnmap(buf);
    }

    BufferEntry e;
    e.buffer = buf;
    e.size = alloc;
    e.logical_size = logical_size;
    e.usage = usage;
    e.is_index = is_index;
    const unsigned int h = NextHandle();
    buffers_[h] = e;
    return h;
}

unsigned int WebGPUResourceManager::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    (void)is_dynamic;  // WebGPU 缓冲无静/动态区分；动态更新经 wgpuQueueWriteBuffer。
    WGPUBufferUsageFlags usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    if (is_index) {
        usage |= WGPUBufferUsage_Index;
    } else {
        // 非索引缓冲可能用作顶点流或 uniform，同时授予两种 usage（WebGPU 允许组合）。
        usage |= WGPUBufferUsage_Vertex | WGPUBufferUsage_Uniform;
    }
    return CreateBufferRaw(size, data, usage, is_index);
}

BufferHandle WebGPUResourceManager::CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) {
    if (desc.size == 0) return BufferHandle{};
    // 按 GpuBufferUsage 授予 WGPU usage 位（始终带 CopyDst|CopySrc 以支持更新/回读拷贝）。
    WGPUBufferUsageFlags usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    const bool is_index = has(desc.usage, GpuBufferUsage::kIndex);
    if (is_index)                                   usage |= WGPUBufferUsage_Index;
    if (has(desc.usage, GpuBufferUsage::kVertex))   usage |= WGPUBufferUsage_Vertex;
    if (has(desc.usage, GpuBufferUsage::kUniform))  usage |= WGPUBufferUsage_Uniform;
    if (has(desc.usage, GpuBufferUsage::kStorage))  usage |= WGPUBufferUsage_Storage;
    if (has(desc.usage, GpuBufferUsage::kIndirect)) usage |= WGPUBufferUsage_Indirect;
    // 无任何用途位（仅 CopyDst|CopySrc）：退化为顶点+uniform，兼容默认 GpuBufferDesc。
    if ((usage & ~static_cast<WGPUBufferUsageFlags>(WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc)) == 0)
        usage |= WGPUBufferUsage_Vertex | WGPUBufferUsage_Uniform;
    return BufferHandle{CreateBufferRaw(desc.size, initial_data, usage, is_index)};
}

void WebGPUResourceManager::UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) {
    auto it = buffers_.find(handle.raw());
    if (it == buffers_.end() || !data || size == 0) return;
    const BufferEntry& e = it->second;
    // 顶点/索引/uniform 缓冲必须经 UpdateBuffer：引擎前向路径每 draw 全量重写共享 vbo_/ibo_ 与
    //   逐材质 UBO（offset=0），需 geom/UBO 版本环避免 wgpuQueueWriteBuffer 合并丢失（见 UpdateBuffer）。
    // 仅 storage/indirect（设备级生命周期、不逐 draw 重写）走直写，不入版本环。
    const bool storage_or_indirect =
        (e.usage & (WGPUBufferUsage_Storage | WGPUBufferUsage_Indirect)) != 0;
    if (!storage_or_indirect) {
        UpdateBuffer(handle.raw(), offset, size, data, e.is_index);
        return;
    }
    if (offset % 4 != 0) {
        DEBUG_LOG_WARN("WebGPU UpdateGpuBuffer: offset {} 非 4 对齐，跳过更新", static_cast<unsigned long long>(offset));
        return;
    }
    const uint64_t write_size = AlignUp4(size);
    if (offset + write_size > e.size) return;
    if (write_size == size) {
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, data, size);
    } else {
        std::vector<uint8_t> padded(write_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, padded.data(), write_size);
    }
}

void WebGPUResourceManager::DeleteGpuBuffer(BufferHandle handle) {
    DeleteBuffer(handle.raw());
}

bool WebGPUResourceManager::BeginGpuReadback(BufferHandle handle, size_t offset, size_t size) {
    if (!device_ || !ctx_->frame_encoder() || exec_->cur_pass() || exec_->cur_compute_pass() || size == 0) {
        return async_rb_ready_;  // 无法在本帧录制拷贝（pass 内/无 encoder）→ 仅返回既有就绪态
    }
    const BufferEntry* be = FindBuffer(handle.raw());
    if (!be || !be->buffer) return async_rb_ready_;

    const int w = async_rb_write_idx_;
    if (async_rb_mapped_[w]) {
        // 目标 staging 仍在上一轮 map 飞行中（罕见，map 慢于 2 帧）→ 本帧跳过拷贝，避免写入已映射缓冲
        return async_rb_ready_;
    }
    // 按需扩容 staging[w]（MapRead|CopyDst）。WGPU copy size 须 4 对齐（mat4 自然满足）。
    const size_t need = (size + 3u) & ~size_t(3);
    if (async_rb_capacity_[w] < need) {
        if (async_rb_staging_[w]) { wgpuBufferRelease(async_rb_staging_[w]); async_rb_staging_[w] = nullptr; }
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        bd.size  = need;
        async_rb_staging_[w] = wgpuDeviceCreateBuffer(device_, &bd);
        async_rb_capacity_[w] = async_rb_staging_[w] ? need : 0;
    }
    if (!async_rb_staging_[w]) return async_rb_ready_;

    wgpuCommandEncoderCopyBufferToBuffer(ctx_->frame_encoder(), be->buffer, offset, async_rb_staging_[w], 0, need);
    async_rb_has_pending_  = true;
    async_rb_pending_idx_  = w;
    async_rb_pending_size_ = size;
    async_rb_write_idx_    = 1 - w;
    return async_rb_ready_;
}

const void* WebGPUResourceManager::GetLastReadbackResult(size_t* out_size) const {
    if (out_size) *out_size = async_rb_ready_ ? async_rb_result_.size() : 0;
    return async_rb_ready_ ? async_rb_result_.data() : nullptr;
}

void WebGPUResourceManager::OnDeferredReadbackMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<DeferredReadbackCtx*>(userdata);
    WebGPUResourceManager* dev = ctx->dev;
    const int idx = ctx->idx;
    WGPUBuffer buf = dev->async_rb_staging_[idx];
    if (status == WGPUBufferMapAsyncStatus_Success && buf) {
        const void* m = wgpuBufferGetConstMappedRange(buf, 0, ctx->size);
        if (m) {
            dev->async_rb_result_.resize(ctx->size);
            std::memcpy(dev->async_rb_result_.data(), m, ctx->size);
            dev->async_rb_ready_ = true;
        }
        wgpuBufferUnmap(buf);
    }
    dev->async_rb_mapped_[idx] = false;
    delete ctx;
}

void WebGPUResourceManager::KickDeferredReadback() {
    if (!async_rb_has_pending_) return;
    async_rb_has_pending_ = false;
    const int p = async_rb_pending_idx_;
    if (!async_rb_staging_[p]) return;
    async_rb_mapped_[p] = true;
    auto* ctx = new DeferredReadbackCtx{this, p, async_rb_pending_size_};
    wgpuBufferMapAsync(async_rb_staging_[p], WGPUMapMode_Read, 0, async_rb_pending_size_,
                       OnDeferredReadbackMapped, ctx);
}

void WebGPUResourceManager::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    (void)is_index;
    auto it = buffers_.find(handle);
    if (it == buffers_.end() || !data || size == 0) return;
    const BufferEntry& e = it->second;
    // wgpuQueueWriteBuffer 要求 offset 与 size 均为 4 的倍数。常规调用方已 4 对齐；
    // 否则把 size 向上取整到 4（缓冲分配已对齐，越界由下方 clamp 兜底）。
    if (offset % 4 != 0) {
        DEBUG_LOG_WARN("WebGPU UpdateBuffer: offset {} 非 4 对齐，跳过更新", static_cast<unsigned long long>(offset));
        return;
    }
    const uint64_t write_size = AlignUp4(size);
    if (offset + write_size > e.size) return;
    if (write_size == size) {
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, data, size);
    } else {
        std::vector<uint8_t> padded(write_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, padded.data(), write_size);
    }
    // 同时登记 UBO 版本切片：仅全量（offset=0）且 uniform 量级（≤64KB，WebGPU uniform 绑定上限）
    //   的更新参与——大顶点/存储缓冲不在此列。该缓冲若稍后以 UBO 绑定（group1），将改用此切片，
    //   使每 draw 的逐材质数据互不覆盖（见 CollectGroupBindings group1）。
    if (offset == 0 && size <= 65536) {
        const uint64_t voff = AllocUboVersion(data, size);
        if (voff != UINT64_MAX) ubo_versions_[handle] = {ubo_ring_, voff, size};
    }
    // 几何版本切片：引擎前向路径每 draw 把世界空间顶点/索引重写进共享 vbo_/ibo_（offset=0）。
    //   同 UBO 合并问题——故全量更新（offset=0）在 geom 环内分配独立切片，绑定时改用当帧最近版本，
    //   使各 draw 的顶点/索引互不覆盖（见 IssueDraw 的 SetVertexBuffer/SetIndexBuffer）。
    if (offset == 0) {
        const uint64_t goff = AllocGeomVersion(data, size);
        if (goff != UINT64_MAX) geom_versions_[handle] = {geom_ring_, goff, AlignUp4(size)};
    }
}

void WebGPUResourceManager::DeleteBuffer(unsigned int handle) {
    auto it = buffers_.find(handle);
    if (it == buffers_.end()) return;
    if (it->second.buffer) wgpuBufferRelease(it->second.buffer);
    buffers_.erase(it);
}

unsigned int WebGPUResourceManager::CreateHiZTexture(int width, int height) {
    if (!EnsureInitialized() || !device_ || width <= 0 || height <= 0) return 0;

    int mip_count = 1;
    {
        int w = width, h = height;
        while (w > 1 || h > 1) {
            w = std::max(1, w / 2);
            h = std::max(1, h / 2);
            ++mip_count;
        }
    }

    const unsigned int tex = CreateTextureImpl(
        WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1,
        WGPUTextureFormat_R32Float,
        WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding |
            WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst,
        static_cast<uint32_t>(mip_count), 1, {nullptr},
        TextureSamplerDesc::FromLinearFlag(false));
    if (!tex) return 0;

    const unsigned int handle = NextHandle();
    hiz_textures_[handle] = tex;
    DEBUG_LOG_INFO("WebGPU Hi-Z texture created: handle={} tex={} {}x{} mips={}",
                   handle, tex, width, height, mip_count);
    return handle;
}

void WebGPUResourceManager::DeleteHiZTexture(unsigned int handle) {
    auto it = hiz_textures_.find(handle);
    if (it == hiz_textures_.end()) return;
    DeleteTexture(it->second);
    hiz_textures_.erase(it);
}

int WebGPUResourceManager::GetHiZMipCount(unsigned int handle) const {
    auto it = hiz_textures_.find(handle);
    if (it == hiz_textures_.end()) return 0;
    const TextureEntry* e = FindTexture(it->second);
    return e ? static_cast<int>(e->mip_levels) : 0;
}

unsigned int WebGPUResourceManager::GetHiZGpuTexture(unsigned int handle) const {
    auto it = hiz_textures_.find(handle);
    return it != hiz_textures_.end() ? it->second : 0;
}

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
