#include "engine/assets/asset_scanner.h"
#include "engine/base/debug.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_set>
#include <rapidjson/document.h>

namespace dse::pak {

namespace {

void CollectStringField(const rapidjson::Value& obj, const char* field,
                        std::unordered_set<std::string>& out) {
    if (obj.HasMember(field) && obj[field].IsString()) {
        std::string val = obj[field].GetString();
        if (!val.empty()) {
            out.insert(val);
        }
    }
}

void ScanEntityComponents(const rapidjson::Value& components,
                          std::unordered_set<std::string>& out) {
    // ScriptComponent
    if (components.HasMember("ScriptComponent") && components["ScriptComponent"].IsObject()) {
        CollectStringField(components["ScriptComponent"], "script_path", out);
    }

    // MeshRendererComponent
    if (components.HasMember("MeshRendererComponent") && components["MeshRendererComponent"].IsObject()) {
        CollectStringField(components["MeshRendererComponent"], "mesh_path", out);
    }

    // SkyboxComponent
    if (components.HasMember("SkyboxComponent") && components["SkyboxComponent"].IsObject()) {
        CollectStringField(components["SkyboxComponent"], "cubemap_path", out);
    }

    // Animator3DComponent
    if (components.HasMember("Animator3DComponent") && components["Animator3DComponent"].IsObject()) {
        const auto& anim = components["Animator3DComponent"];
        CollectStringField(anim, "dskel_path", out);
        CollectStringField(anim, "danim_path", out);
        // Blend nodes
        if (anim.HasMember("blend_nodes") && anim["blend_nodes"].IsArray()) {
            for (const auto& node : anim["blend_nodes"].GetArray()) {
                if (node.IsObject()) {
                    CollectStringField(node, "danim_path", out);
                }
            }
        }
    }

    // TerrainComponent
    if (components.HasMember("TerrainComponent") && components["TerrainComponent"].IsObject()) {
        CollectStringField(components["TerrainComponent"], "heightmap_path", out);
    }

    // AudioSourceComponent
    if (components.HasMember("AudioSourceComponent") && components["AudioSourceComponent"].IsObject()) {
        CollectStringField(components["AudioSourceComponent"], "clip_path", out);
    }
}

} // namespace

std::vector<std::string> ScanSceneAssetPaths(const std::string& scene_file_path) {
    std::unordered_set<std::string> paths;

    std::ifstream in(scene_file_path);
    if (!in.is_open()) {
        DEBUG_LOG_ERROR("[AssetScanner] Cannot open scene: {}", scene_file_path);
        return {};
    }

    std::stringstream buf;
    buf << in.rdbuf();
    std::string json_str = buf.str();

    rapidjson::Document doc;
    if (doc.Parse(json_str.c_str()).HasParseError() || !doc.IsObject()) {
        DEBUG_LOG_ERROR("[AssetScanner] Parse error in: {}", scene_file_path);
        return {};
    }

    // Scan entities
    if (doc.HasMember("entities") && doc["entities"].IsArray()) {
        for (const auto& entity : doc["entities"].GetArray()) {
            if (!entity.IsObject()) continue;
            if (entity.HasMember("components") && entity["components"].IsObject()) {
                ScanEntityComponents(entity["components"], paths);
            }
        }
    }

    // Include the scene file itself
    paths.insert(std::filesystem::path(scene_file_path).filename().generic_string());

    std::vector<std::string> result(paths.begin(), paths.end());
    std::sort(result.begin(), result.end());

    DEBUG_LOG_INFO("[AssetScanner] Found {} asset paths in {}", result.size(), scene_file_path);
    return result;
}

std::vector<std::string> CollectDirectoryFiles(const std::string& directory_path) {
    namespace fs = std::filesystem;
    std::vector<std::string> result;

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(directory_path, ec)) {
        if (entry.is_regular_file()) {
            result.push_back(entry.path().generic_string());
        }
    }
    if (ec) {
        DEBUG_LOG_WARN("[AssetScanner] Error iterating directory: {}", directory_path);
    }

    std::sort(result.begin(), result.end());
    return result;
}

} // namespace dse::pak
