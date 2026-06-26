/**
 * @file editor_asset_db_core.cpp
 * @brief 资产类型分类纯逻辑（无 ProjectManager / ImGui 依赖）：扩展名 → AssetType、
 *        AssetType → 字符串。从 editor_asset_db.cpp 抽出，供无头 gtest 直接链接测试。
 */

#include "editor_asset_db.h"

namespace dse::editor {

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

} // namespace dse::editor
