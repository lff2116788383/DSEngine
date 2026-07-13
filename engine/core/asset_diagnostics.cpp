/**
 * @file asset_diagnostics.cpp
 * @brief 共享资产诊断的版本策略实现。
 */

#include "engine/core/asset_diagnostics.h"

#include <sstream>

namespace dse::assets {

void NoteForwardCompat(int source_version, int current_version,
                       const char* format_name, AssetDiagnostics& diag) {
    if (source_version <= current_version) return;
    std::ostringstream os;
    os << (format_name ? format_name : "asset") << " schema version " << source_version
       << " is newer than supported " << current_version
       << "; loading leniently (forward-compat)";
    diag.warnings.push_back(os.str());
}

}  // namespace dse::assets
