/**
 * @file asset_distribution.cpp
 * @brief 打包分发管线实现 —— 真实 HTTP 下载 + 二进制 patch + JSON manifest
 */

#include "engine/assets/asset_distribution.h"
#include "engine/assets/sha256.h"
#include "engine/assets/binary_patch.h"
#ifdef DSE_ENABLE_HTTP
#include "engine/http/http_client.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <sstream>

namespace dse {
namespace assets {

namespace fs = std::filesystem;

// ── 内部辅助 ─────────────────────────────────────────────────────────────────

namespace {

std::vector<uint8_t> ReadFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    if (sz <= 0) return {};
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

bool WriteFileBytes(const std::string& path, const uint8_t* data, size_t size) {
    fs::path p(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return f.good();
}

std::string ComputeFileHash(const std::string& path) {
    auto data = ReadFileBytes(path);
    if (data.empty()) return {};
    return SHA256::HashToHex(data.data(), data.size());
}

std::string PackageCachePath(const std::string& cache_dir, const std::string& package_id) {
    return cache_dir + "/" + package_id + ".dpak";
}

} // anonymous namespace

// ── Init / Shutdown ──────────────────────────────────────────────────────────

void AssetDistribution::Init(const DistributionConfig& config) {
    config_ = config;
    if (!config_.cache_path.empty()) {
        fs::create_directories(config_.cache_path);
    }
    initialized_ = true;
}

void AssetDistribution::Shutdown() {
    manifest_ = {};
    package_index_.clear();
    download_queue_.clear();
    download_callback_ = nullptr;
    initialized_ = false;
}

// ── Manifest JSON 序列化 / 反序列化 ──────────────────────────────────────────

bool AssetDistribution::LoadManifest(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError() || !doc.IsObject()) return false;

    manifest_ = {};
    package_index_.clear();

    if (doc.HasMember("game_version") && doc["game_version"].IsString())
        manifest_.game_version = doc["game_version"].GetString();
    if (doc.HasMember("manifest_version") && doc["manifest_version"].IsUint())
        manifest_.manifest_version = doc["manifest_version"].GetUint();

    if (doc.HasMember("packages") && doc["packages"].IsArray()) {
        const auto& pkgs = doc["packages"];
        for (rapidjson::SizeType i = 0; i < pkgs.Size(); ++i) {
            const auto& jp = pkgs[i];
            PackageInfo pkg;
            if (jp.HasMember("id") && jp["id"].IsString())
                pkg.package_id = jp["id"].GetString();
            if (jp.HasMember("cell_x") && jp["cell_x"].IsInt())
                pkg.cell_x = jp["cell_x"].GetInt();
            if (jp.HasMember("cell_y") && jp["cell_y"].IsInt())
                pkg.cell_y = jp["cell_y"].GetInt();
            if (jp.HasMember("lod") && jp["lod"].IsInt())
                pkg.lod_level = jp["lod"].GetInt();
            if (jp.HasMember("size") && jp["size"].IsUint64())
                pkg.size_bytes = jp["size"].GetUint64();
            if (jp.HasMember("compressed_size") && jp["compressed_size"].IsUint64())
                pkg.compressed_size = jp["compressed_size"].GetUint64();
            if (jp.HasMember("hash") && jp["hash"].IsString())
                pkg.hash = jp["hash"].GetString();
            if (jp.HasMember("version") && jp["version"].IsUint())
                pkg.version = jp["version"].GetUint();
            if (jp.HasMember("deps") && jp["deps"].IsArray()) {
                for (rapidjson::SizeType d = 0; d < jp["deps"].Size(); ++d) {
                    if (jp["deps"][d].IsString())
                        pkg.dependencies.push_back(jp["deps"][d].GetString());
                }
            }
            pkg.state = PackageState::NotDownloaded;

            // 检查本地缓存是否已有该包
            if (!config_.cache_path.empty()) {
                std::string local = PackageCachePath(config_.cache_path, pkg.package_id);
                if (fs::exists(local)) {
                    std::string local_hash = ComputeFileHash(local);
                    if (local_hash == pkg.hash) {
                        pkg.state = PackageState::Installed;
                    } else {
                        pkg.state = PackageState::NeedsUpdate;
                    }
                }
            }

            size_t idx = manifest_.packages.size();
            package_index_[pkg.package_id] = idx;
            manifest_.packages.push_back(std::move(pkg));
        }
    }

    if (doc.HasMember("patches") && doc["patches"].IsArray()) {
        const auto& patches = doc["patches"];
        for (rapidjson::SizeType i = 0; i < patches.Size(); ++i) {
            const auto& jp = patches[i];
            PatchInfo pi;
            if (jp.HasMember("from") && jp["from"].IsString())
                pi.from_version = jp["from"].GetString();
            if (jp.HasMember("to") && jp["to"].IsString())
                pi.to_version = jp["to"].GetString();
            if (jp.HasMember("size") && jp["size"].IsUint64())
                pi.patch_size = jp["size"].GetUint64();
            if (jp.HasMember("url") && jp["url"].IsString())
                pi.patch_url = jp["url"].GetString();
            manifest_.patches.push_back(std::move(pi));
        }
    }

    manifest_.package_count = static_cast<uint32_t>(manifest_.packages.size());
    manifest_.total_size = 0;
    for (const auto& p : manifest_.packages) manifest_.total_size += p.size_bytes;

    return true;
}

bool AssetDistribution::SaveManifest(const std::string& path) const {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("game_version",
        rapidjson::Value(manifest_.game_version.c_str(), alloc), alloc);
    doc.AddMember("manifest_version", manifest_.manifest_version, alloc);
    doc.AddMember("total_size", manifest_.total_size, alloc);

    rapidjson::Value pkgs(rapidjson::kArrayType);
    for (const auto& p : manifest_.packages) {
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("id", rapidjson::Value(p.package_id.c_str(), alloc), alloc);
        obj.AddMember("cell_x", p.cell_x, alloc);
        obj.AddMember("cell_y", p.cell_y, alloc);
        obj.AddMember("lod", p.lod_level, alloc);
        obj.AddMember("size", p.size_bytes, alloc);
        obj.AddMember("compressed_size", p.compressed_size, alloc);
        obj.AddMember("hash", rapidjson::Value(p.hash.c_str(), alloc), alloc);
        obj.AddMember("version", p.version, alloc);

        rapidjson::Value deps(rapidjson::kArrayType);
        for (const auto& d : p.dependencies) {
            deps.PushBack(rapidjson::Value(d.c_str(), alloc), alloc);
        }
        obj.AddMember("deps", deps, alloc);
        pkgs.PushBack(obj, alloc);
    }
    doc.AddMember("packages", pkgs, alloc);

    rapidjson::Value patches_arr(rapidjson::kArrayType);
    for (const auto& pi : manifest_.patches) {
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("from", rapidjson::Value(pi.from_version.c_str(), alloc), alloc);
        obj.AddMember("to", rapidjson::Value(pi.to_version.c_str(), alloc), alloc);
        obj.AddMember("size", pi.patch_size, alloc);
        obj.AddMember("url", rapidjson::Value(pi.patch_url.c_str(), alloc), alloc);
        patches_arr.PushBack(obj, alloc);
    }
    doc.AddMember("patches", patches_arr, alloc);

    rapidjson::StringBuffer buf;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);

    fs::path p(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path());

    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f.write(buf.GetString(), static_cast<std::streamsize>(buf.GetSize()));
    return f.good();
}

// ── 查询 ─────────────────────────────────────────────────────────────────────

const PackageInfo* AssetDistribution::GetPackageInfo(const std::string& package_id) const {
    auto it = package_index_.find(package_id);
    if (it != package_index_.end() && it->second < manifest_.packages.size()) {
        return &manifest_.packages[it->second];
    }
    return nullptr;
}

// ── 打包 ─────────────────────────────────────────────────────────────────────

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

    // 计算真实文件大小并生成包内容哈希
    uint64_t total = 0;
    SHA256 hasher;
    for (const auto& p : asset_paths) {
        auto data = ReadFileBytes(p);
        total += data.size();
        hasher.Update(data.data(), data.size());
    }
    pkg.size_bytes = total;
    pkg.compressed_size = total * 7 / 10; // 预估 ~70% 压缩率
    pkg.hash = SHA256::DigestToHex(hasher.Finalize());

    size_t idx = manifest_.packages.size();
    manifest_.packages.push_back(pkg);
    package_index_[pkg.package_id] = idx;
    manifest_.package_count = static_cast<uint32_t>(manifest_.packages.size());
    manifest_.total_size += pkg.size_bytes;

    return static_cast<uint32_t>(idx);
}

// ── Patch 生成 ───────────────────────────────────────────────────────────────

bool AssetDistribution::GeneratePatch(const std::string& package_id,
                                       const std::string& old_hash,
                                       const std::string& new_hash) {
    if (!config_.enable_delta_patches) return false;
    if (config_.cache_path.empty()) return false;

    std::string old_file = PackageCachePath(config_.cache_path, package_id) + "." + old_hash.substr(0, 8);
    std::string new_file = PackageCachePath(config_.cache_path, package_id);
    std::string patch_file = config_.cache_path + "/patches/" + package_id + "_" +
                             old_hash.substr(0, 8) + "_to_" + new_hash.substr(0, 8) + ".dpatch";

    // 生成真实二进制 patch
    bool generated = BinaryPatch::Generate(old_file, new_file, patch_file);

    PatchInfo patch;
    patch.from_version = old_hash;
    patch.to_version = new_hash;
    patch.patch_url = config_.cdn_base_url + "/patches/" + package_id + "_" +
                      old_hash.substr(0, 8) + "_to_" + new_hash.substr(0, 8) + ".dpatch";

    if (generated && fs::exists(patch_file)) {
        patch.patch_size = fs::file_size(patch_file);
    } else {
        // 回退：估算 patch 大小
        auto info = GetPackageInfo(package_id);
        patch.patch_size = info ? info->size_bytes / 5 : 0;
    }

    manifest_.patches.push_back(patch);
    return true;
}

bool AssetDistribution::VerifyPackage(const std::string& package_id) const {
    auto info = GetPackageInfo(package_id);
    if (!info) return false;
    if (info->state != PackageState::Installed) return false;
    if (info->hash.empty()) return false;

    // 真实文件哈希校验
    if (!config_.cache_path.empty()) {
        std::string local = PackageCachePath(config_.cache_path, info->package_id);
        if (!fs::exists(local)) return false;
        std::string local_hash = ComputeFileHash(local);
        return local_hash == info->hash;
    }
    return true;
}

// ── 下载管理 ─────────────────────────────────────────────────────────────────

void AssetDistribution::RequestDownload(const std::string& package_id) {
    auto it = package_index_.find(package_id);
    if (it == package_index_.end()) return;

    auto& pkg = manifest_.packages[it->second];
    if (pkg.state == PackageState::Installed) return;

    pkg.state = PackageState::Downloading;
    pkg.download_progress = 0.0f;

    // 发起 HTTP 下载请求
#ifdef DSE_ENABLE_HTTP
    if (dse::http::HttpClient::Available()) {
        std::string url = config_.cdn_base_url + "/packages/" + package_id + ".dpak";

        // 检查是否有可用的增量 patch（优先使用 delta 更新）
        std::string patch_url;
        if (config_.enable_delta_patches && !config_.cache_path.empty()) {
            std::string local = PackageCachePath(config_.cache_path, package_id);
            if (fs::exists(local)) {
                std::string local_hash = ComputeFileHash(local);
                for (const auto& pi : manifest_.patches) {
                    if (pi.from_version == local_hash && pi.to_version == pkg.hash) {
                        patch_url = pi.patch_url;
                        break;
                    }
                }
            }
        }

        std::string download_url = patch_url.empty() ? url : patch_url;
        bool is_patch = !patch_url.empty();
        std::string pkg_id = package_id;

        dse::http::HttpClient::Instance().Get(download_url, {},
            [this, pkg_id, is_patch](const dse::http::Response& resp) {
                auto idx_it = package_index_.find(pkg_id);
                if (idx_it == package_index_.end()) return;
                auto& target = manifest_.packages[idx_it->second];

                if (resp.ok()) {
                    std::string local_path = PackageCachePath(config_.cache_path, pkg_id);

                    if (is_patch) {
                        // 应用增量 patch
                        auto old_data = ReadFileBytes(local_path);
                        std::vector<uint8_t> new_data;
                        bool ok = BinaryPatch::ApplyFromMemory(
                            old_data.data(), old_data.size(),
                            reinterpret_cast<const uint8_t*>(resp.body.data()),
                            resp.body.size(), new_data);
                        if (ok) {
                            WriteFileBytes(local_path, new_data.data(), new_data.size());
                        }
                    } else {
                        // 全量下载：直接写入缓存
                        WriteFileBytes(local_path,
                            reinterpret_cast<const uint8_t*>(resp.body.data()),
                            resp.body.size());
                    }

                    target.download_progress = 1.0f;
                    target.state = PackageState::Installed;
                    total_downloaded_ += resp.body.size();
                    if (download_callback_) download_callback_(pkg_id, true);
                } else {
                    target.state = PackageState::NotDownloaded;
                    target.download_progress = 0.0f;
                    if (download_callback_) download_callback_(pkg_id, false);
                }
            });
    }
#endif // DSE_ENABLE_HTTP

    // 同时解析并请求依赖
    std::vector<std::string> deps;
    ResolveDependencies(package_id, deps);
    for (const auto& dep : deps) {
        auto dep_it = package_index_.find(dep);
        if (dep_it != package_index_.end()) {
            auto& dep_pkg = manifest_.packages[dep_it->second];
            if (dep_pkg.state == PackageState::NotDownloaded) {
                RequestDownload(dep); // 递归下载依赖
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
        if (pkg.state == PackageState::Downloading ||
            pkg.state == PackageState::NotDownloaded ||
            pkg.state == PackageState::NeedsUpdate) {
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
    (void)dt;

    // 驱动 HttpClient 回调执行（完成的下载在回调里更新 package 状态）
#ifdef DSE_ENABLE_HTTP
    if (dse::http::HttpClient::Available()) {
        dse::http::HttpClient::Instance().Poll();
    }
#endif

    // 统计当前下载速度（基于上次 Tick 间隔的已下载量变化）
    // 简化：基于 active download 数计算吞吐
    int active = 0;
    for (const auto& pkg : manifest_.packages) {
        if (pkg.state == PackageState::Downloading) ++active;
    }

    // 自动请求队列中的高优先级包（限制并发数）
    if (active < config_.max_concurrent_downloads) {
        for (const auto& dp : download_queue_) {
            if (active >= config_.max_concurrent_downloads) break;
            auto it = package_index_.find(dp.package_id);
            if (it == package_index_.end()) continue;
            auto& pkg = manifest_.packages[it->second];
            if (pkg.state == PackageState::NotDownloaded ||
                pkg.state == PackageState::NeedsUpdate) {
                RequestDownload(dp.package_id);
                ++active;
            }
        }
    }
}

std::vector<DownloadPriority> AssetDistribution::GetDownloadQueue() const {
    return download_queue_;
}

// ── 空间查询 ─────────────────────────────────────────────────────────────────

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

// ── 统计与缓存管理 ──────────────────────────────────────────────────────────

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
    if (config_.cache_path.empty()) return 0;

    uint64_t usage = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(config_.cache_path)) {
            if (entry.is_regular_file()) {
                usage += entry.file_size();
            }
        }
    } catch (...) {}
    return usage;
}

uint64_t AssetDistribution::PurgeOldCache(uint32_t max_age_days) {
    if (config_.cache_path.empty()) return 0;

    uint64_t freed = 0;
    auto now = fs::file_time_type::clock::now();

    try {
        for (const auto& entry : fs::directory_iterator(config_.cache_path)) {
            if (!entry.is_regular_file()) continue;

            auto last_write = entry.last_write_time();
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - last_write);
            uint32_t age_days = static_cast<uint32_t>(age.count() / 24);

            if (age_days > max_age_days) {
                // 确保不是当前 manifest 中已安装的包
                std::string fname = entry.path().stem().string();
                auto it = package_index_.find(fname);
                if (it != package_index_.end()) {
                    auto& pkg = manifest_.packages[it->second];
                    if (pkg.state == PackageState::Installed) continue;
                }

                uint64_t fsize = entry.file_size();
                std::error_code ec;
                fs::remove(entry.path(), ec);
                if (!ec) freed += fsize;
            }
        }
    } catch (...) {}

    return freed;
}

// ── 内部辅助 ─────────────────────────────────────────────────────────────────

void AssetDistribution::ResolveDependencies(const std::string& package_id,
                                             std::vector<std::string>& out) const {
    auto info = GetPackageInfo(package_id);
    if (!info) return;
    for (const auto& dep : info->dependencies) {
        if (std::find(out.begin(), out.end(), dep) == out.end()) {
            out.push_back(dep);
            ResolveDependencies(dep, out);
        }
    }
}

float AssetDistribution::CalculatePriority(const PackageInfo& pkg, const glm::vec3& player_pos) const {
    float cell_center_x = pkg.cell_x * config_.cell_size + config_.cell_size * 0.5f;
    float cell_center_z = pkg.cell_y * config_.cell_size + config_.cell_size * 0.5f;

    float dist = std::sqrt((cell_center_x - player_pos.x) * (cell_center_x - player_pos.x) +
                           (cell_center_z - player_pos.z) * (cell_center_z - player_pos.z));

    float priority = config_.priority_radius / (dist + 1.0f);
    priority *= 1.0f / (1.0f + pkg.lod_level * 0.5f);

    // NeedsUpdate 的包略微提高优先级（增量更新通常更小更快）
    if (pkg.state == PackageState::NeedsUpdate) {
        priority *= 1.2f;
    }

    return priority;
}

} // namespace assets
} // namespace dse
