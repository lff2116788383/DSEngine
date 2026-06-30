/**
 * @file lightmap_system.cpp
 * @brief Lightmap 运行时系统实现
 */

#include "engine/render/gi/lightmap_system.h"
#include "engine/render/gi/lightmap_baker.h"
#include "engine/ecs/world.h"

namespace dse {
namespace render {

void LightmapSystem::Init(RhiDevice* device) {
    device_ = device;
}

void LightmapSystem::Update(::World& world) {
    (void)world;
    if (!device_) return;

    // Scan all entities with LightmapComponent, ensure textures are loaded
    auto view = world.registry().view<LightmapComponent>();
    for (auto entity : view) {
        auto& lmc = view.get<LightmapComponent>(entity);
        if (lmc.lightmap_path.empty()) continue;

        // Already loaded - update handle reference
        if (lmc.lightmap_handle != 0) continue;

        // Try to load
        uint32_t handle = LoadLightmap(lmc.lightmap_path);
        lmc.lightmap_handle = handle;
    }
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

    // Load from file
    LightmapResult result;
    if (!LightmapBaker::LoadFromFile(path, result) || !result.success) {
        return 0;
    }

    // Create GPU texture (RGB32F irradiance)
    LightmapEntry entry;
    entry.width = result.width;
    entry.height = result.height;
    entry.ref_count = 1;

    // Simulate GPU texture creation via RHI
    // In production this would call device_->CreateTexture2D(...)
    // For now, use a simple incrementing handle
    static uint32_t s_next_handle = 10000;
    entry.texture_handle = ++s_next_handle;

    // AO channel
    if (!result.ao.empty()) {
        entry.has_ao = true;
        entry.ao_texture_handle = ++s_next_handle;
    }

    cache_[path] = entry;
    return entry.texture_handle;
}

void LightmapSystem::UnloadLightmap(const std::string& path) {
    auto it = cache_.find(path);
    if (it == cache_.end()) return;

    it->second.ref_count--;
    if (it->second.ref_count == 0) {
        // Release GPU texture
        // In production: device_->DestroyTexture(it->second.texture_handle);
        if (it->second.has_ao) {
            // device_->DestroyTexture(it->second.ao_texture_handle);
        }
        cache_.erase(it);
    }
}

bool LightmapSystem::IsLoaded(const std::string& path) const {
    return cache_.find(path) != cache_.end();
}

void LightmapSystem::Shutdown() {
    // Release all GPU textures
    for (auto& [path, entry] : cache_) {
        // In production: device_->DestroyTexture(entry.texture_handle);
        if (entry.has_ao) {
            // device_->DestroyTexture(entry.ao_texture_handle);
        }
    }
    cache_.clear();
    device_ = nullptr;
}

} // namespace render
} // namespace dse
