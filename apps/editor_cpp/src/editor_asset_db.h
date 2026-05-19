#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace dse::editor {

/// Asset type inferred from file extension
enum class AssetType {
    Unknown,
    Mesh,       // .dmesh
    Material,   // .dmat
    Animation,  // .danim
    Skeleton,   // .dskel
    Texture,    // .png .jpg .hdr .tga .bmp .ktx
    Audio,      // .wav .ogg .mp3 .flac
    Scene,      // .dscene
    Prefab,     // .dprefab
    Script,     // .lua
    Pak,        // .dpak
};

const char* AssetTypeToString(AssetType type);
AssetType AssetTypeFromExtension(const std::string& ext);

struct AssetInfo {
    std::string guid;           // 32-char lowercase hex (128-bit)
    std::string absolute_path;
    std::string relative_path;  // relative to project asset root
    AssetType   type = AssetType::Unknown;
    std::string extension;
    std::string display_name;   // filename without extension
    int64_t     file_size = 0;
};

/// Lightweight asset registry. Scans the project asset directory,
/// generates/reads per-file .meta GUIDs, and provides O(1) lookup.
class AssetDatabase {
public:
    static AssetDatabase& Get();

    /// Scan (or re-scan) the project asset directory.
    /// Creates .meta files for new assets, reads existing ones.
    /// Call after opening/changing a project.
    void Refresh();

    /// True after at least one successful Refresh with a project open.
    bool IsValid() const { return is_valid_; }

    /// Total number of tracked assets (excluding .meta files)
    size_t Count() const { return assets_.size(); }

    /// Find asset by GUID. Returns nullptr if not found.
    const AssetInfo* FindByGUID(const std::string& guid) const;

    /// Find asset by relative path (forward slashes, no leading slash).
    const AssetInfo* FindByPath(const std::string& rel_path) const;

    /// All assets in scan order.
    const std::vector<AssetInfo>& GetAll() const { return assets_; }

    /// All assets of a specific type.
    std::vector<const AssetInfo*> GetByType(AssetType type) const;

    /// Root directory used for the last Refresh.
    const std::filesystem::path& GetAssetRoot() const { return asset_root_; }

private:
    AssetDatabase() = default;

    void ScanDirectory(const std::filesystem::path& dir);
    std::string ReadOrCreateMeta(const std::filesystem::path& file_path);
    static std::string GenerateGUID();

    std::filesystem::path asset_root_;
    std::vector<AssetInfo> assets_;
    std::unordered_map<std::string, size_t> by_guid_;   // guid -> index into assets_
    std::unordered_map<std::string, size_t> by_path_;   // rel_path -> index into assets_
    bool is_valid_ = false;
};

} // namespace dse::editor
