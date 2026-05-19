#include "editor_asset_db.h"

#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include "editor_project.h"
#include "editor_console_panel.h"

namespace dse::editor {

// ─── Type helpers ────────────────────────────────────────────────────────────

const char* AssetTypeToString(AssetType type) {
    switch (type) {
    case AssetType::Mesh:      return "Mesh";
    case AssetType::Material:  return "Material";
    case AssetType::Animation: return "Animation";
    case AssetType::Skeleton:  return "Skeleton";
    case AssetType::Texture:   return "Texture";
    case AssetType::Audio:     return "Audio";
    case AssetType::Scene:     return "Scene";
    case AssetType::Prefab:    return "Prefab";
    case AssetType::Script:    return "Script";
    case AssetType::Pak:       return "Pak";
    default:                   return "Unknown";
    }
}

AssetType AssetTypeFromExtension(const std::string& ext) {
    if (ext == ".dmesh")                               return AssetType::Mesh;
    if (ext == ".dmat")                                return AssetType::Material;
    if (ext == ".danim")                               return AssetType::Animation;
    if (ext == ".dskel")                               return AssetType::Skeleton;
    if (ext == ".dscene")                              return AssetType::Scene;
    if (ext == ".dprefab")                             return AssetType::Prefab;
    if (ext == ".lua")                                 return AssetType::Script;
    if (ext == ".dpak")                                return AssetType::Pak;
    if (ext == ".png"  || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".hdr"  || ext == ".tga" || ext == ".bmp"  ||
        ext == ".ktx"  || ext == ".dds" || ext == ".exr")  return AssetType::Texture;
    if (ext == ".wav"  || ext == ".ogg" || ext == ".mp3"  ||
        ext == ".flac" || ext == ".aac")                   return AssetType::Audio;
    return AssetType::Unknown;
}

// ─── Singleton ───────────────────────────────────────────────────────────────

AssetDatabase& AssetDatabase::Get() {
    static AssetDatabase instance;
    return instance;
}

// ─── GUID generation ─────────────────────────────────────────────────────────

std::string AssetDatabase::GenerateGUID() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t hi = dist(rng);
    uint64_t lo = dist(rng);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << hi
        << std::setw(16) << lo;
    return oss.str();
}

// ─── .meta file I/O ──────────────────────────────────────────────────────────

std::string AssetDatabase::ReadOrCreateMeta(const std::filesystem::path& file_path) {
    std::filesystem::path meta_path = file_path.string() + ".meta";

    // Try reading existing meta
    if (std::filesystem::exists(meta_path)) {
        std::ifstream ifs(meta_path);
        if (ifs) {
            std::string content((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
            rapidjson::Document doc;
            if (!doc.Parse(content.c_str()).HasParseError() && doc.IsObject()) {
                if (doc.HasMember("guid") && doc["guid"].IsString()) {
                    std::string guid = doc["guid"].GetString();
                    if (guid.size() == 32) return guid;
                }
            }
        }
    }

    // Create new meta
    std::string new_guid = GenerateGUID();

    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();
    doc.AddMember("guid", rapidjson::Value(new_guid.c_str(), alloc), alloc);
    doc.AddMember("type", rapidjson::Value(
        AssetTypeToString(AssetTypeFromExtension(file_path.extension().string())), alloc), alloc);

    rapidjson::StringBuffer buf;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);

    std::ofstream ofs(meta_path, std::ios::trunc);
    if (ofs) {
        ofs << buf.GetString();
    }

    return new_guid;
}

// ─── Directory scanning ──────────────────────────────────────────────────────

static bool IsMeta(const std::filesystem::path& p) {
    return p.extension() == ".meta";
}

static bool IsHidden(const std::filesystem::path& p) {
    std::string name = p.filename().string();
    return !name.empty() && name[0] == '.';
}

void AssetDatabase::ScanDirectory(const std::filesystem::path& dir) {
    std::error_code ec;
    for (auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!entry.is_regular_file(ec)) continue;

        const auto& path = entry.path();
        if (IsMeta(path) || IsHidden(path.filename())) continue;

        std::string ext = path.extension().string();
        // Lowercase extension
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string guid = ReadOrCreateMeta(path);

        AssetInfo info;
        info.guid          = guid;
        info.absolute_path = path.string();
        info.relative_path = std::filesystem::relative(path, asset_root_, ec).string();
        // Normalize to forward slashes
        std::replace(info.relative_path.begin(), info.relative_path.end(), '\\', '/');
        info.type          = AssetTypeFromExtension(ext);
        info.extension     = ext;
        info.display_name  = path.stem().string();
        info.file_size     = static_cast<int64_t>(entry.file_size(ec));

        size_t idx = assets_.size();
        assets_.push_back(std::move(info));
        by_guid_[assets_[idx].guid]          = idx;
        by_path_[assets_[idx].relative_path] = idx;
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

void AssetDatabase::Refresh() {
    auto& proj = ProjectManager::Get();
    if (!proj.HasOpenProject()) {
        is_valid_ = false;
        return;
    }

    assets_.clear();
    by_guid_.clear();
    by_path_.clear();

    asset_root_ = proj.GetAssetDir();

    if (!std::filesystem::exists(asset_root_)) {
        std::filesystem::create_directories(asset_root_);
    }

    ScanDirectory(asset_root_);

    is_valid_ = true;
    EditorLog(LogLevel::Info,
        "AssetDatabase refreshed: " + std::to_string(assets_.size()) + " assets in " + asset_root_.string());
}

const AssetInfo* AssetDatabase::FindByGUID(const std::string& guid) const {
    auto it = by_guid_.find(guid);
    if (it == by_guid_.end()) return nullptr;
    return &assets_[it->second];
}

const AssetInfo* AssetDatabase::FindByPath(const std::string& rel_path) const {
    // Normalize separators
    std::string normalized = rel_path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    auto it = by_path_.find(normalized);
    if (it == by_path_.end()) return nullptr;
    return &assets_[it->second];
}

std::vector<const AssetInfo*> AssetDatabase::GetByType(AssetType type) const {
    std::vector<const AssetInfo*> result;
    for (const auto& a : assets_) {
        if (a.type == type) result.push_back(&a);
    }
    return result;
}

} // namespace dse::editor
