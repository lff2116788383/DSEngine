#include "editor_asset_db.h"

#include <algorithm>
#include <fstream>
#include <cstdint>
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

// NOTE: AssetTypeToString / AssetTypeFromExtension are defined in
// editor_asset_db_core.cpp (pure logic, headless-testable).

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

// ─── Binary DB cache helpers ─────────────────────────────────────────────────

static constexpr uint32_t kDbCacheMagic   = 0x42445344u; // 'DSDB'
static constexpr uint32_t kDbCacheVersion = 1u;

static void DbW32(std::ofstream& f, uint32_t v)  { f.write(reinterpret_cast<const char*>(&v), 4); }
static void DbW64(std::ofstream& f, int64_t v)   { f.write(reinterpret_cast<const char*>(&v), 8); }
static void DbWStr(std::ofstream& f, const std::string& s) {
    DbW32(f, static_cast<uint32_t>(s.size()));
    if (!s.empty()) f.write(s.data(), s.size());
}
static bool DbR32(std::ifstream& f, uint32_t& v) { return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), 4)); }
static bool DbR64(std::ifstream& f, int64_t& v)  { return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), 8)); }
static bool DbRStr(std::ifstream& f, std::string& s) {
    uint32_t len = 0;
    if (!DbR32(f, len) || len > 65536u) return false;
    s.resize(len);
    return len == 0 || static_cast<bool>(f.read(s.data(), len));
}
static int64_t DbGetMtime(const std::filesystem::path& p) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(p, ec);
    return ec ? -1LL : static_cast<int64_t>(t.time_since_epoch().count());
}

void AssetDatabase::ScanDirectory(const std::filesystem::path& dir,
                                   const std::unordered_map<std::string, CachedDbEntry>& cache_map) {
    std::error_code ec;
    for (auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!entry.is_regular_file(ec)) continue;

        const auto& path = entry.path();
        if (IsMeta(path) || IsHidden(path.filename())) continue;

        std::string ext = path.extension().string();
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string rel_path = std::filesystem::relative(path, asset_root_, ec).string();
        std::replace(rel_path.begin(), rel_path.end(), '\\', '/');

        const int64_t asset_mtime = DbGetMtime(path);
        const int64_t meta_mtime  = DbGetMtime(std::filesystem::path(path.string() + ".meta"));

        AssetInfo info;
        info.absolute_path = path.string();
        info.relative_path = rel_path;
        info.extension     = ext;
        info.display_name  = path.stem().string();
        info.file_size     = static_cast<int64_t>(entry.file_size(ec));

        auto it = cache_map.find(rel_path);
        if (it != cache_map.end() &&
            it->second.asset_mtime == asset_mtime &&
            it->second.meta_mtime  == meta_mtime) {
            info.guid = it->second.guid;
            info.type = it->second.type;
        } else {
            info.guid = ReadOrCreateMeta(path);
            info.type = AssetTypeFromExtension(ext);
        }

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

    const std::filesystem::path cache_path =
        asset_root_.parent_path() / "Cache" / "asset_db.bin";
    std::unordered_map<std::string, CachedDbEntry> cache_map;
    LoadDbCache(cache_path, cache_map);

    ScanDirectory(asset_root_, cache_map);
    SaveDbCache(cache_path);

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

void AssetDatabase::LoadDbCache(const std::filesystem::path& cache_path,
                                std::unordered_map<std::string, CachedDbEntry>& out) const {
    out.clear();
    std::ifstream f(cache_path, std::ios::binary);
    if (!f.is_open()) return;
    uint32_t magic = 0, version = 0, count = 0;
    if (!DbR32(f, magic) || magic != kDbCacheMagic) return;
    if (!DbR32(f, version) || version != kDbCacheVersion) return;
    if (!DbR32(f, count)) return;
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        std::string rel_path;
        CachedDbEntry e;
        uint8_t type_u8 = 0;
        if (!DbRStr(f, rel_path)) return;
        if (!DbRStr(f, e.guid)) return;
        if (!DbRStr(f, e.extension)) return;
        if (!DbRStr(f, e.display_name)) return;
        if (!f.read(reinterpret_cast<char*>(&type_u8), 1)) return;
        e.type = static_cast<AssetType>(type_u8);
        if (!DbR64(f, e.file_size)) return;
        if (!DbR64(f, e.asset_mtime)) return;
        if (!DbR64(f, e.meta_mtime)) return;
        out[rel_path] = std::move(e);
    }
}

void AssetDatabase::SaveDbCache(const std::filesystem::path& cache_path) const {
    std::error_code ec;
    std::filesystem::create_directories(cache_path.parent_path(), ec);
    std::ofstream f(cache_path, std::ios::binary);
    if (!f.is_open()) return;
    DbW32(f, kDbCacheMagic);
    DbW32(f, kDbCacheVersion);
    DbW32(f, static_cast<uint32_t>(assets_.size()));
    for (const auto& info : assets_) {
        std::filesystem::path abs(info.absolute_path);
        const int64_t asset_mt = DbGetMtime(abs);
        const int64_t meta_mt  = DbGetMtime(std::filesystem::path(info.absolute_path + ".meta"));
        DbWStr(f, info.relative_path);
        DbWStr(f, info.guid);
        DbWStr(f, info.extension);
        DbWStr(f, info.display_name);
        const uint8_t type_u8 = static_cast<uint8_t>(info.type);
        f.write(reinterpret_cast<const char*>(&type_u8), 1);
        DbW64(f, info.file_size);
        DbW64(f, asset_mt);
        DbW64(f, meta_mt);
    }
}

std::vector<const AssetInfo*> AssetDatabase::GetByType(AssetType type) const {
    std::vector<const AssetInfo*> result;
    for (const auto& a : assets_) {
        if (a.type == type) result.push_back(&a);
    }
    return result;
}

} // namespace dse::editor
