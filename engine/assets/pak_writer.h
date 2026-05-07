#pragma once

#include "engine/assets/pak_format.h"
#include <string>
#include <vector>

namespace dse::pak {

/// Packs a list of files into a .dpak archive.
/// @param output_path  Path of the output .dpak file.
/// @param base_dir     Base directory; file paths stored relative to this.
/// @param file_paths   Absolute or relative-to-cwd paths of files to pack.
/// @return true on success.
bool WriteDpak(const std::string& output_path,
               const std::string& base_dir,
               const std::vector<std::string>& file_paths);

} // namespace dse::pak
