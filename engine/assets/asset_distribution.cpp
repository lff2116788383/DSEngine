/**
 * @file asset_distribution.cpp
 * @brief 打包分发管线实现
 */

#include "engine/assets/asset_distribution.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <numeric>

namespace dse {
namespace assets {

void AssetDistribution::Init(const DistributionConfig& config) {
    config_ = config;
    initialized_ = true;
}

void AssetDistribution::Shutdown() {
    manifest_ = {};
    package_index_.clear();
    download_queue_.clear();
    download_callback_ = nullptr;
    initialized_ = false;
}

bool AssetDistribution::LoadManifest(const std::string& path) {
    // Simplified: in production would parse binary/JSON manifest from file
    // For now, supports programmatic building
    (void)path;
    manifest_.manifest_version++;
    return true;
}

bool AssetDistribution::SaveManifest(const std::string& path) const {
    (void)path;
    // In production: serialize manifest to file
    return true;
}

const PackageInfo* AssetDistribution::GetPackageInfo(const std::string& package_id) const {
    auto it = package_index_.find(package_id);
    if (it != package_index_.end() && it->second < manifest_.packages.size()) {
        return &manifest_.packages[it->second];
    }
    return nullptr;
}

uint32_t AssetDistribution::PackageCell(int cell_x, int cell_y, int lod_level,
                                         const std::vector<std::string>& asset_paths) {
    PackageInfo pkg;
    pkg.package_id = "cell_" + std::to_string(cell_x) + "_" + std::to_string(cell_y) +
                     "_lod_" + std::to_string(lod_level);
    pkg.cell_x = cell_x;
    pkg.cell_y = cell_y;
    pkg.lod_level = lod_level;
    pkg.version = manifest_.manifest_version;
    pkg.state = PackageState::NotDownloaded;

    // Calculate size from asset paths
    uint64_t total = 0;
    for (const auto& p : asset_paths) {
        total += p.size() * 1024; // Simulated size
    }
    pkg.size_bytes = total;
    pkg.compressed_size = total * 7 / 10; // ~70% compression ratio

    // Simple hash
    std::hash<std::string> hasher;
    pkg.hash = std::to_string(hasher(pkg.package_id + std::to_string(pkg.version)));

    size_t idx = manifest_.packages.size();
    manifest_.packages.push_back(pkg);
    package_index_[pkg.package_id] = idx;
    manifest_.package_count = static_cast<uint32_t>(manifest_.packages.size());
    manifest_.total_size += pkg.size_bytes;

    return static_cast<uint32_t>(idx);
}

bool AssetDistribution::GeneratePatch(const std::string& package_id,
                                       const std::string& old_hash,
                                       const std::string& new_hash) {
    if (!config_.enable_delta_patches) return false;

    PatchInfo patch;
    patch.from_version = old_hash;
    patch.to_version = new_hash;
    patch.patch_url = config_.cdn_base_url + "/patches/" + package_id + "_" +
                      old_hash.substr(0, 8) + "_to_" + new_hash.substr(0, 8) + ".dpatch";

    auto info = GetPackageInfo(package_id);
    if (info) {
        patch.patch_size = info->size_bytes / 5; // ~20% of full size for delta
    }

    manifest_.patches.push_back(patch);
    return true;
}

bool AssetDistribution::VerifyPackage(const std::string& package_id) const {
    auto info = GetPackageInfo(package_id);
    if (!info) return false;
    return info->state == PackageState::Installed && !info->hash.empty();
}

void AssetDistribution::RequestDownload(const std::string& package_id) {
    auto it = package_index_.find(package_id);
    if (it == package_index_.end()) return;

    auto& pkg = manifest_.packages[it->second];
    if (pkg.state == PackageState::Installed) return;

    pkg.state = PackageState::Downloading;
    pkg.download_progress = 0.0f;

    // Also resolve and request dependencies
    std::vector<std::string> deps;
    ResolveDependencies(package_id, deps);
    for (const auto& dep : deps) {
        auto dep_it = package_index_.find(dep);
        if (dep_it != package_index_.end()) {
            auto& dep_pkg = manifest_.packages[dep_it->second];
            if (dep_pkg.state == PackageState::NotDownloaded) {
                dep_pkg.state = PackageState::Downloading;
            }
        }
    }
}

void AssetDistribution::CancelDownload(const std::string& package_id) {
    auto it = package_index_.find(package_id);
    if (it == package_index_.end()) return;
    auto& pkg = manifest_.packages[it->second];
    if (pkg.state == PackageState::Downloading) {
        pkg.state = PackageState::NotDownloaded;
        pkg.download_progress = 0.0f;
    }
}

void AssetDistribution::UpdatePriorities(const glm::vec3& player_pos) {
    download_queue_.clear();

    for (auto& pkg : manifest_.packages) {
        if (pkg.state == PackageState::Downloading || pkg.state == PackageState::NotDownloaded) {
            DownloadPriority dp;
            dp.package_id = pkg.package_id;
            dp.priority = CalculatePriority(pkg, player_pos);
            dp.distance = std::sqrt(
                static_cast<float>((pkg.cell_x * config_.cell_size - player_pos.x) *
                                   (pkg.cell_x * config_.cell_size - player_pos.x) +
                                   (pkg.cell_y * config_.cell_size - player_pos.z) *
                                   (pkg.cell_y * config_.cell_size - player_pos.z)));
            download_queue_.push_back(dp);
        }
    }

    std::sort(download_queue_.begin(), download_queue_.end(),
        [](const DownloadPriority& a, const DownloadPriority& b) {
            return a.priority > b.priority;
        });
}

void AssetDistribution::Tick(float dt) {
    if (!initialized_) return;

    // Simulate download progress
    int active_downloads = 0;
    float simulated_speed = 10 * 1024 * 1024; // 10 MB/s simulated

    for (auto& pkg : manifest_.packages) {
        if (pkg.state != PackageState::Downloading) continue;
        if (active_downloads >= config_.max_concurrent_downloads) break;

        active_downloads++;
        float bytes_this_frame = simulated_speed * dt / active_downloads;
        pkg.download_progress += bytes_this_frame / static_cast<float>(pkg.compressed_size);
        total_downloaded_ += static_cast<uint64_t>(bytes_this_frame);

        if (pkg.download_progress >= 1.0f) {
            pkg.download_progress = 1.0f;
            pkg.state = PackageState::Installed;
            if (download_callback_) download_callback_(pkg.package_id, true);
        }
    }

    download_speed_ = simulated_speed;
}

std::vector<DownloadPriority> AssetDistribution::GetDownloadQueue() const {
    return download_queue_;
}

std::vector<std::string> AssetDistribution::GetRequiredPackages(const glm::vec3& position, float radius) const {
    std::vector<std::string> result;
    int min_cx = static_cast<int>(std::floor((position.x - radius) / config_.cell_size));
    int max_cx = static_cast<int>(std::ceil((position.x + radius) / config_.cell_size));
    int min_cy = static_cast<int>(std::floor((position.z - radius) / config_.cell_size));
    int max_cy = static_cast<int>(std::ceil((position.z + radius) / config_.cell_size));

    for (const auto& pkg : manifest_.packages) {
        if (pkg.cell_x >= min_cx && pkg.cell_x <= max_cx &&
            pkg.cell_y >= min_cy && pkg.cell_y <= max_cy) {
            result.push_back(pkg.package_id);
        }
    }
    return result;
}

std::vector<std::string> AssetDistribution::GetMissingPackages(const glm::vec3& position, float radius) const {
    auto required = GetRequiredPackages(position, radius);
    std::vector<std::string> missing;
    for (const auto& id : required) {
        if (!IsPackageInstalled(id)) missing.push_back(id);
    }
    return missing;
}

bool AssetDistribution::IsPackageInstalled(const std::string& package_id) const {
    auto info = GetPackageInfo(package_id);
    return info && info->state == PackageState::Installed;
}

DistributionStats AssetDistribution::GetStats() const {
    DistributionStats stats{};
    stats.total_packages = static_cast<uint32_t>(manifest_.packages.size());

    uint64_t remaining = 0;
    for (const auto& pkg : manifest_.packages) {
        switch (pkg.state) {
            case PackageState::Installed: stats.installed_packages++; break;
            case PackageState::Downloading: stats.downloading_packages++; remaining += pkg.compressed_size; break;
            default: stats.pending_packages++; remaining += pkg.compressed_size; break;
        }
    }

    stats.total_downloaded_bytes = total_downloaded_;
    stats.total_remaining_bytes = remaining;
    stats.download_speed_bps = download_speed_;
    return stats;
}

uint64_t AssetDistribution::GetDiskUsage() const {
    uint64_t usage = 0;
    for (const auto& pkg : manifest_.packages) {
        if (pkg.state == PackageState::Installed || pkg.state == PackageState::Downloaded) {
            usage += pkg.size_bytes;
        }
    }
    return usage;
}

uint64_t AssetDistribution::PurgeOldCache(uint32_t max_age_days) {
    (void)max_age_days;
    // In production: remove packages not accessed in max_age_days
    return 0;
}

void AssetDistribution::ResolveDependencies(const std::string& package_id,
                                             std::vector<std::string>& out) const {
    auto info = GetPackageInfo(package_id);
    if (!info) return;
    for (const auto& dep : info->dependencies) {
        if (std::find(out.begin(), out.end(), dep) == out.end()) {
            out.push_back(dep);
            ResolveDependencies(dep, out); // Recursive
        }
    }
}

float AssetDistribution::CalculatePriority(const PackageInfo& pkg, const glm::vec3& player_pos) const {
    float cell_center_x = pkg.cell_x * config_.cell_size + config_.cell_size * 0.5f;
    float cell_center_z = pkg.cell_y * config_.cell_size + config_.cell_size * 0.5f;

    float dist = std::sqrt((cell_center_x - player_pos.x) * (cell_center_x - player_pos.x) +
                           (cell_center_z - player_pos.z) * (cell_center_z - player_pos.z));

    // Higher priority for closer cells and lower LOD levels
    float priority = config_.priority_radius / (dist + 1.0f);
    priority *= 1.0f / (1.0f + pkg.lod_level * 0.5f);
    return priority;
}

} // namespace assets
} // namespace dse
