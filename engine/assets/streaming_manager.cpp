/**
 * @file streaming_manager.cpp
 * @brief 资源流式加载管理器实现 — 距离触发 + 异步 IO 优先级队列
 */

#include "engine/assets/streaming_manager.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
#include <algorithm>
#include <cmath>

namespace dse::streaming {

// ============================================================
// 生命周期
// ============================================================

void StreamingManager::Init(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
    initialized_ = true;
    DEBUG_LOG_INFO("[StreamingManager] Initialized (budget={}/frame, max_concurrent={})",
                  load_budget_per_frame_, max_concurrent_loads_);
}

void StreamingManager::Shutdown() {
    if (!initialized_) return;

    std::lock_guard<std::mutex> lock(zones_mutex_);
    for (auto& [id, zone] : zones_) {
        if (zone.state == ZoneState::Loaded || zone.state == ZoneState::Loading) {
            BeginUnloadZone(zone);
        }
    }
    zones_.clear();
    initialized_ = false;
    DEBUG_LOG_INFO("[StreamingManager] Shutdown");
}

// ============================================================
// Zone 管理
// ============================================================

uint32_t StreamingManager::CreateZone(const std::string& name, const glm::vec3& center,
                                       float load_radius, float unload_radius) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    uint32_t id = next_zone_id_++;
    StreamingZone zone;
    zone.id = id;
    zone.name = name;
    zone.center = center;
    zone.load_radius = load_radius;
    zone.unload_radius = std::max(unload_radius, load_radius + 1.0f);
    zones_[id] = std::move(zone);
    DEBUG_LOG_INFO("[StreamingManager] Created zone '{}' id={} center=({},{},{}) load_r={} unload_r={}",
                  name, id, center.x, center.y, center.z, load_radius, unload_radius);
    return id;
}

void StreamingManager::DestroyZone(uint32_t zone_id) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    auto it = zones_.find(zone_id);
    if (it == zones_.end()) return;

    auto& zone = it->second;
    if (zone.state == ZoneState::Loaded || zone.state == ZoneState::Loading) {
        BeginUnloadZone(zone);
    }
    zones_.erase(it);
}

void StreamingManager::AddAsset(uint32_t zone_id, const std::string& path, AssetType type) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    auto it = zones_.find(zone_id);
    if (it == zones_.end()) return;

    StreamingAssetEntry entry;
    entry.path = path;
    entry.type = type;
    it->second.assets.push_back(std::move(entry));
}

void StreamingManager::AddAssets(uint32_t zone_id, const std::vector<std::string>& paths, AssetType type) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    auto it = zones_.find(zone_id);
    if (it == zones_.end()) return;

    for (const auto& path : paths) {
        StreamingAssetEntry entry;
        entry.path = path;
        entry.type = type;
        it->second.assets.push_back(std::move(entry));
    }
}

void StreamingManager::SetZoneCenter(uint32_t zone_id, const glm::vec3& center) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    auto it = zones_.find(zone_id);
    if (it == zones_.end()) return;
    it->second.center = center;
}

void StreamingManager::ForceLoadZone(uint32_t zone_id) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    auto it = zones_.find(zone_id);
    if (it == zones_.end()) return;

    auto& zone = it->second;
    zone.force_loaded = true;
    if (zone.state == ZoneState::Unloaded || zone.state == ZoneState::Unloading) {
        BeginLoadZone(zone);
    }
}

void StreamingManager::ForceUnloadZone(uint32_t zone_id) {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    auto it = zones_.find(zone_id);
    if (it == zones_.end()) return;

    auto& zone = it->second;
    zone.force_loaded = false;
    if (zone.state == ZoneState::Loaded || zone.state == ZoneState::Loading) {
        BeginUnloadZone(zone);
    }
}

// ============================================================
// 每帧更新
// ============================================================

void StreamingManager::Tick(const glm::vec3& camera_position) {
    if (!initialized_ || !asset_manager_) return;

    std::lock_guard<std::mutex> lock(zones_mutex_);

    // 收集需要处理的 zone，按距离排序
    struct ZoneDistEntry {
        uint32_t id;
        float distance;
        int priority_bias;
    };
    std::vector<ZoneDistEntry> load_candidates;

    for (auto& [id, zone] : zones_) {
        const glm::vec3 diff = camera_position - zone.center;
        const float dist_sq = glm::dot(diff, diff);
        const float load_r_sq = zone.load_radius * zone.load_radius;
        const float unload_r_sq = zone.unload_radius * zone.unload_radius;

        switch (zone.state) {
        case ZoneState::Unloaded:
            if (zone.force_loaded || dist_sq <= load_r_sq) {
                load_candidates.push_back({id, dist_sq, zone.priority_bias});
            }
            break;

        case ZoneState::Loading:
            if (!zone.force_loaded && dist_sq > unload_r_sq) {
                BeginUnloadZone(zone);
            }
            break;

        case ZoneState::Loaded:
            if (!zone.force_loaded && dist_sq > unload_r_sq) {
                BeginUnloadZone(zone);
            }
            break;

        case ZoneState::Unloading:
            zone.state = ZoneState::Unloaded;
            break;
        }
    }

    if (load_candidates.empty()) return;

    // 按距离平方升序排列（最近的先加载，priority_bias 权重用 10000 代表一个半径级差异）
    std::sort(load_candidates.begin(), load_candidates.end(),
        [](const ZoneDistEntry& a, const ZoneDistEntry& b) {
            float score_a = a.distance - static_cast<float>(a.priority_bias) * 10000.0f;
            float score_b = b.distance - static_cast<float>(b.priority_bias) * 10000.0f;
            return score_a < score_b;
        });

    // 受限于每帧预算和全局并发上限
    int budget_remaining = load_budget_per_frame_;
    int current_active = active_load_count_.load(std::memory_order_relaxed);

    for (const auto& entry : load_candidates) {
        if (budget_remaining <= 0) break;
        if (current_active >= max_concurrent_loads_) break;

        auto it = zones_.find(entry.id);
        if (it == zones_.end()) continue;

        auto& zone = it->second;
        if (zone.state != ZoneState::Unloaded) continue;

        BeginLoadZone(zone);
        int assets_to_load = static_cast<int>(zone.assets.size());
        current_active += assets_to_load;
        --budget_remaining;
    }
}

// ============================================================
// 查询
// ============================================================

ZoneState StreamingManager::GetZoneState(uint32_t zone_id) const {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    auto it = zones_.find(zone_id);
    if (it == zones_.end()) return ZoneState::Unloaded;
    return it->second.state;
}

float StreamingManager::GetZoneProgress(uint32_t zone_id) const {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    auto it = zones_.find(zone_id);
    if (it == zones_.end()) return 0.0f;

    const auto& zone = it->second;
    if (zone.assets.empty()) return 1.0f;
    if (zone.state == ZoneState::Loaded) return 1.0f;
    if (zone.state == ZoneState::Unloaded) return 0.0f;

    int total = static_cast<int>(zone.assets.size());
    int loaded = total - zone.assets_pending;
    return static_cast<float>(loaded) / static_cast<float>(total);
}

std::size_t StreamingManager::GetZoneCount() const {
    std::lock_guard<std::mutex> lock(zones_mutex_);
    return zones_.size();
}

// ============================================================
// 内部：加载/卸载
// ============================================================

void StreamingManager::BeginLoadZone(StreamingZone& zone) {
    if (zone.assets.empty()) {
        zone.state = ZoneState::Loaded;
        return;
    }

    zone.state = ZoneState::Loading;
    zone.assets_pending = 0;

    for (auto& asset : zone.assets) {
        if (asset.loaded) continue;
        ++zone.assets_pending;
    }

    if (zone.assets_pending == 0) {
        zone.state = ZoneState::Loaded;
        return;
    }

    active_load_count_.fetch_add(zone.assets_pending, std::memory_order_relaxed);

    uint32_t zone_id = zone.id;
    for (auto& asset : zone.assets) {
        if (asset.loaded) continue;

        const std::string path = asset.path;
        AssetType type = asset.type;

        switch (type) {
        case AssetType::Texture:
            asset_manager_->LoadTextureAsync(path,
                [this, zone_id, path](std::shared_ptr<TextureAsset> tex) {
                    OnAssetLoadedWithResource(zone_id, path, tex != nullptr, tex);
                });
            break;

        case AssetType::Mesh:
            asset_manager_->LoadDmeshAsync(path,
                [this, zone_id, path](std::shared_ptr<DmeshAsset> mesh) {
                    OnAssetLoadedWithResource(zone_id, path, mesh != nullptr, mesh);
                });
            break;

        case AssetType::Animation:
            asset_manager_->LoadDanimAsync(path,
                [this, zone_id, path](std::shared_ptr<DanimAsset> anim) {
                    OnAssetLoadedWithResource(zone_id, path, anim != nullptr, anim);
                });
            break;

        case AssetType::Skeleton:
            asset_manager_->LoadDskelAsync(path,
                [this, zone_id, path](std::shared_ptr<DskelAsset> skel) {
                    OnAssetLoadedWithResource(zone_id, path, skel != nullptr, skel);
                });
            break;

        case AssetType::Audio:
            asset_manager_->LoadAudioClipAsync(path,
                [this, zone_id, path](std::shared_ptr<AudioClipAsset> clip) {
                    OnAssetLoadedWithResource(zone_id, path, clip != nullptr, clip);
                });
            break;

        case AssetType::Material:
            asset_manager_->LoadMaterialAsync(path, 0,
                [this, zone_id, path](std::shared_ptr<MaterialAsset> mat) {
                    OnAssetLoadedWithResource(zone_id, path, mat != nullptr, mat);
                });
            break;
        }
    }

    DEBUG_LOG_INFO("[StreamingManager] Begin loading zone '{}' ({} assets)",
                  zone.name, zone.assets_pending);
}

void StreamingManager::BeginUnloadZone(StreamingZone& zone) {
    zone.state = ZoneState::Unloading;

    for (auto& asset : zone.assets) {
        asset.loaded = false;
        asset.retained_resource.reset();  // 释放资源引用，允许 AssetManager LRU 淘汰
    }
    zone.assets_pending = 0;

    // 在通中的回调仍会到达，OnAssetLoadedWithResource 会检查 zone.state 并忽略
    zone.state = ZoneState::Unloaded;
    DEBUG_LOG_INFO("[StreamingManager] Unloaded zone '{}'", zone.name);
}

void StreamingManager::OnAssetLoaded(uint32_t zone_id, const std::string& path, bool success) {
    OnAssetLoadedWithResource(zone_id, path, success, std::any{});
}

void StreamingManager::OnAssetLoadedWithResource(uint32_t zone_id, const std::string& path,
                                                  bool success, std::any resource) {
    active_load_count_.fetch_sub(1, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(zones_mutex_);
    auto it = zones_.find(zone_id);
    if (it == zones_.end()) return;

    auto& zone = it->second;
    // Zone 已被卸载或销毁，忽略回调
    if (zone.state != ZoneState::Loading) return;

    // 标记对应 asset 为已加载，并持有引用
    for (auto& asset : zone.assets) {
        if (asset.path == path && !asset.loaded) {
            asset.loaded = success;
            if (success) {
                asset.retained_resource = std::move(resource);
            }
            break;
        }
    }

    if (zone.assets_pending > 0) {
        --zone.assets_pending;
    }

    if (zone.assets_pending == 0) {
        zone.state = ZoneState::Loaded;
        DEBUG_LOG_INFO("[StreamingManager] Zone '{}' fully loaded", zone.name);
    }
}

} // namespace dse::streaming
