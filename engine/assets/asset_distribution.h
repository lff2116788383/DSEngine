/**
 * @file asset_distribution.h
 * @brief 打包分发管线 — Cell分包 + 增量更新 + Manifest
 *
 * 功能：
 * - Cell分包：按 World Partition Cell 边界切分资源为独立包
 * - 增量更新：基于文件哈希的增量 patch 生成与应用
 * - Manifest：版本化资源清单，记录每个包的版本/大小/依赖
 * - 依赖解析：自动追踪跨 Cell 引用，确保依赖完整
 * - 优先级下载：基于玩家位置的包下载优先级排序
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace assets {

/// 包状态
enum class PackageState : uint8_t {
    NotDownloaded = 0,
    Downloading = 1,
    Downloaded = 2,
    Installed = 3,
    NeedsUpdate = 4,
    Corrupt = 5
};

/// 资源包描述
struct PackageInfo {
    std::string package_id;       ///< 唯一标识（通常 "cell_X_Y_lod_L"）
    int cell_x = 0;              ///< Cell X 坐标
    int cell_y = 0;              ///< Cell Y 坐标
    int lod_level = 0;           ///< LOD 层级
    uint64_t size_bytes = 0;     ///< 包大小
    uint64_t compressed_size = 0;///< 压缩后大小
    std::string hash;            ///< SHA256 哈希
    uint32_t version = 0;        ///< 版本号
    std::vector<std::string> dependencies; ///< 依赖的其他包
    PackageState state = PackageState::NotDownloaded;
    float download_progress = 0.0f;
};

/// Patch（增量更新）描述
struct PatchInfo {
    std::string from_version;    ///< 源版本哈希
    std::string to_version;      ///< 目标版本哈希
    uint64_t patch_size = 0;     ///< Patch 大小
    std::string patch_url;       ///< 下载地址
};

/// Manifest（版本清单）
struct DistributionManifest {
    std::string game_version;
    uint32_t manifest_version = 0;
    uint64_t total_size = 0;
    uint32_t package_count = 0;
    std::vector<PackageInfo> packages;
    std::vector<PatchInfo> patches;
};

/// 下载优先级条目
struct DownloadPriority {
    std::string package_id;
    float priority = 0.0f;       ///< 越高越优先
    float distance = 0.0f;       ///< 到玩家的距离
};

/// 分发统计
struct DistributionStats {
    uint32_t total_packages;
    uint32_t installed_packages;
    uint32_t downloading_packages;
    uint32_t pending_packages;
    uint64_t total_downloaded_bytes;
    uint64_t total_remaining_bytes;
    float download_speed_bps;     ///< 当前下载速度 (bytes/sec)
};

/// 分发配置
struct DistributionConfig {
    float cell_size = 256.0f;           ///< Cell 世界尺寸
    int max_concurrent_downloads = 4;   ///< 最大并行下载数
    float priority_radius = 512.0f;     ///< 高优先级下载半径
    bool enable_delta_patches = true;   ///< 启用增量更新
    std::string cache_path;             ///< 本地缓存路径
    std::string cdn_base_url;           ///< CDN 基础 URL
    uint32_t max_cache_size_mb = 4096;  ///< 最大缓存大小 (MB)
};

/// 下载完成回调
using DownloadCompleteFunc = std::function<void(const std::string& package_id, bool success)>;

/// 资源分发系统
class DSE_EXPORT AssetDistribution {
public:
    AssetDistribution() = default;
    ~AssetDistribution() = default;

    void Init(const DistributionConfig& config);
    void Shutdown();

    // === Manifest 管理 ===

    /// 加载 manifest（从本地缓存或网络）
    bool LoadManifest(const std::string& path);

    /// 保存当前 manifest 到指定路径
    bool SaveManifest(const std::string& path) const;

    /// 获取 manifest 信息
    const DistributionManifest& GetManifest() const { return manifest_; }

    /// 获取指定包的信息
    const PackageInfo* GetPackageInfo(const std::string& package_id) const;

    // === 打包 ===

    /// 将资源按 Cell 打包（离线工具调用）
    uint32_t PackageCell(int cell_x, int cell_y, int lod_level,
                         const std::vector<std::string>& asset_paths);

    /// 生成增量 patch
    bool GeneratePatch(const std::string& package_id,
                       const std::string& old_hash, const std::string& new_hash);

    /// 验证包完整性
    bool VerifyPackage(const std::string& package_id) const;

    // === 下载管理 ===

    /// 请求下载指定包
    void RequestDownload(const std::string& package_id);

    /// 取消下载
    void CancelDownload(const std::string& package_id);

    /// 根据玩家位置更新下载优先级
    void UpdatePriorities(const glm::vec3& player_pos);

    /// 每帧更新下载进度
    void Tick(float dt);

    /// 获取下载队列
    std::vector<DownloadPriority> GetDownloadQueue() const;

    /// 设置下载完成回调
    void SetDownloadCallback(DownloadCompleteFunc func) { download_callback_ = std::move(func); }

    // === 查询 ===

    /// 获取指定位置所需的包列表
    std::vector<std::string> GetRequiredPackages(const glm::vec3& position, float radius) const;

    /// 获取所有未安装但需要的包
    std::vector<std::string> GetMissingPackages(const glm::vec3& position, float radius) const;

    /// 包是否已安装
    bool IsPackageInstalled(const std::string& package_id) const;

    /// 获取统计信息
    DistributionStats GetStats() const;

    /// 获取总磁盘使用量
    uint64_t GetDiskUsage() const;

    /// 清理过期缓存
    uint64_t PurgeOldCache(uint32_t max_age_days);

    /// 获取配置
    const DistributionConfig& GetConfig() const { return config_; }

private:
    void ResolveDependencies(const std::string& package_id, std::vector<std::string>& out) const;
    float CalculatePriority(const PackageInfo& pkg, const glm::vec3& player_pos) const;

    DistributionConfig config_;
    DistributionManifest manifest_;
    std::unordered_map<std::string, size_t> package_index_; // id → index in manifest
    std::vector<DownloadPriority> download_queue_;
    DownloadCompleteFunc download_callback_;

    uint64_t total_downloaded_ = 0;
    float download_speed_ = 0.0f;
    bool initialized_ = false;
};

} // namespace assets
} // namespace dse
