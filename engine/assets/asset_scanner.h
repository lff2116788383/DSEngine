#pragma once

#include <string>
#include <vector>

namespace dse::pak {

/// Scan a .dscene / .json scene file and collect all referenced asset paths.
/// Returns paths relative to the data root (e.g., "models/cube.dmesh").
std::vector<std::string> ScanSceneAssetPaths(const std::string& scene_file_path);

/// Recursively collect all files under a directory (for bulk packing).
std::vector<std::string> CollectDirectoryFiles(const std::string& directory_path);

} // namespace dse::pak
