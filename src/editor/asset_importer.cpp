#include "asset_importer.h"
#include "utils/debug.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

void AssetImporter::Import(const std::string& path) {
    if (!fs::exists(path)) return;

    EnsureMeta(path);
    // Load meta
    AssetMeta meta; // LoadMeta(path + ".meta");
    
    switch (meta.type) {
        case AssetType::Texture:
            ProcessTexture(path, meta);
            break;
        // ...
    }
}

void AssetImporter::EnsureMeta(const std::string& path) {
    std::string meta_path = path + ".meta";
    if (!fs::exists(meta_path)) {
        // Create new meta file with default settings and new GUID
        DEBUG_LOG_INFO("Generating meta file for: {}", path);
        std::ofstream meta_file(meta_path);
        meta_file << "guid: " << "new-guid-here" << "\n";
        meta_file.close();
    }
}

AssetType AssetImporter::DetectType(const std::string& extension) {
    if (extension == ".png" || extension == ".jpg") return AssetType::Texture;
    if (extension == ".wav" || extension == ".mp3") return AssetType::Audio;
    return AssetType::Unknown;
}

void AssetImporter::ProcessTexture(const std::string& path, const AssetMeta& meta) {
    // Convert to internal format, compress, generate atlas, etc.
}

void AssetImporter::ProcessAudio(const std::string& path, const AssetMeta& meta) {
    // Convert to bank or stream format
}
