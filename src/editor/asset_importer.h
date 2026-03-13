#ifndef DSE_ASSET_IMPORTER_H
#define DSE_ASSET_IMPORTER_H

#include <string>
#include <vector>

enum class AssetType {
    Texture,
    Audio,
    Model,
    Script,
    Unknown
};

struct AssetMeta {
    std::string guid;
    AssetType type;
    // Import settings
    bool compress = false;
    bool generate_mipmaps = false;
};

class AssetImporter {
public:
    static void Import(const std::string& path);
    static void Reimport(const std::string& path);
    
    // Check if .meta file exists, if not create it
    static void EnsureMeta(const std::string& path);

private:
    static AssetType DetectType(const std::string& extension);
    static void ProcessTexture(const std::string& path, const AssetMeta& meta);
    static void ProcessAudio(const std::string& path, const AssetMeta& meta);
};

#endif // DSE_ASSET_IMPORTER_H
