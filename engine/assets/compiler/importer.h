#pragma once

#include "engine/assets/compiler/raw_scene_data.h"
#include <string>

namespace dse {
namespace asset {
namespace compiler {

class GltfImporter {
public:
    bool Import(const std::string& file_path, RawSceneData& out_scene);
};

class FbxImporter {
public:
    bool Import(const std::string& file_path, RawSceneData& out_scene);
};

class MeshCooker {

public:
    bool CookToDmesh(const RawSceneData& scene, const std::string& output_path);
    bool CookToDmat(const RawSceneData& scene, const std::string& output_dir, const std::string& base_name);
    bool CookToDanim(const RawSceneData& scene, const std::string& output_dir, const std::string& base_name);
    bool CookToDskel(const RawSceneData& scene, const std::string& output_dir, const std::string& base_name);
};

} // namespace compiler
} // namespace asset
} // namespace dse
