/**
 * @file lightmap_system.cpp
 * @brief Lightmap 运行时系统实现
 *
 * 通过 RhiDevice 真实创建/释放 GPU 纹理。irradiance 为 HDR RGB，
 * 当前 RHI 仅支持 RGBA8 2D 纹理，故转换为 LDR RGBA8 上传（alpha=1）。
 */

#include "engine/render/gi/lightmap_system.h"
#include "engine/render/gi/lightmap_baker.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_handle.h"

#include <algorithm>

namespace dse {
namespace render {

namespace {

// HDR float 转 LDR RGBA8（clamp [0,1]）
inline unsigned char ToU8(float v) {
    return static_cast<unsigned char>(std::min(1.0f, std::max(0.0f, v)) * 255.0f + 0.5f);
}

} // namespace

void LightmapSystem::Init(RhiDevice* device) {
    device_ = device;
}

uint32_t LightmapSystem::GetTextureHandle(const std::string& path) const {
    auto it = cache_.find(path);
    if (it != cache_.end()) return it->second.texture_handle;
    return 0;
}

uint32_t LightmapSystem::GetAOTextureHandle(const std::string& path) const {
    auto it = cache_.find(path);
    if (it != cache_.end() && it->second.has_ao) return it->second.ao_texture_handle;
    return 0;
}

uint32_t LightmapSystem::LoadLightmap(const std::string& path) {
    // Check cache first
    auto it = cache_.find(path);
    if (it != cache_.end()) {
        it->second.ref_count++;
        return it->second.texture_handle;
    }

    if (!device_) return 0;  // 无 RHI 设备无法创建纹理

    // Load from file
    LightmapResult result;
    if (!LightmapBaker::LoadFromFile(path, result) || !result.success) {
        return 0;
    }

    LightmapEntry entry;
    entry.width = result.width;
    entry.height = result.height;
    entry.ref_count = 1;

    // irradiance → RGBA8 上传
    {
        const size_t texel_count = static_cast<size_t>(result.width) * result.height;
        std::vector<unsigned char> rgba(texel_count * 4, 255);
        const size_t n = std::min(result.irradiance.size(), texel_count);
        for (size_t i = 0; i < n; ++i) {
            rgba[i * 4 + 0] = ToU8(result.irradiance[i].r);
            rgba[i * 4 + 1] = ToU8(result.irradiance[i].g);
            rgba[i * 4 + 2] = ToU8(result.irradiance[i].b);
            rgba[i * 4 + 3] = 255;
        }
        TextureHandle handle = device_->CreateTexture2D(
            static_cast<int>(result.width), static_cast<int>(result.height),
            rgba.data(), true);
        if (!handle) return 0;
        entry.texture_handle = handle.raw();
    }

    // AO 通道 → 灰度 RGBA8 上传
    if (!result.ao.empty()) {
        const size_t texel_count = static_cast<size_t>(result.width) * result.height;
        std::vector<unsigned char> rgba(texel_count * 4, 255);
        const size_t n = std::min(result.ao.size(), texel_count);
        for (size_t i = 0; i < n; ++i) {
            unsigned char v = ToU8(result.ao[i]);
            rgba[i * 4 + 0] = v;
            rgba[i * 4 + 1] = v;
            rgba[i * 4 + 2] = v;
            rgba[i * 4 + 3] = 255;
        }
        TextureHandle handle = device_->CreateTexture2D(
            static_cast<int>(result.width), static_cast<int>(result.height),
            rgba.data(), true);
        if (handle) {
            entry.has_ao = true;
            entry.ao_texture_handle = handle.raw();
        }
    }

    cache_[path] = entry;
    return entry.texture_handle;
}

void LightmapSystem::UnloadLightmap(const std::string& path) {
    auto it = cache_.find(path);
    if (it == cache_.end()) return;

    it->second.ref_count--;
    if (it->second.ref_count == 0) {
        if (device_) {
            if (it->second.texture_handle != 0) {
                device_->DeleteTexture(TextureHandle::from_raw(it->second.texture_handle));
            }
            if (it->second.ao_texture_handle != 0) {
                device_->DeleteTexture(TextureHandle::from_raw(it->second.ao_texture_handle));
            }
        }
        cache_.erase(it);
    }
}

bool LightmapSystem::IsLoaded(const std::string& path) const {
    return cache_.find(path) != cache_.end();
}

void LightmapSystem::Shutdown() {
    if (device_) {
        for (auto& [path, entry] : cache_) {
            (void)path;
            if (entry.texture_handle != 0) {
                device_->DeleteTexture(TextureHandle::from_raw(entry.texture_handle));
            }
            if (entry.ao_texture_handle != 0) {
                device_->DeleteTexture(TextureHandle::from_raw(entry.ao_texture_handle));
            }
        }
    }
    cache_.clear();
    device_ = nullptr;
}

} // namespace render
} // namespace dse
