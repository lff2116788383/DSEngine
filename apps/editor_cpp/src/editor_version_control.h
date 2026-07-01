#pragma once

#include "editor_context.h"
#include <string>
#include <vector>

namespace dse::editor {

/// Draw the Version Control (Git) integration panel
void DrawVersionControlPanel(EditorContext& ctx);

// ─── Test accessors ─────────────────────────────────────────────────────
enum class VcTab { Changes = 0, History = 1, Branches = 2, Conflicts = 3 };

struct VcTestFile {
    std::string path;
    bool staged = false;
};

struct VcTestBranch {
    std::string name;
    bool is_current = false;
};

struct VersionControlTestState {
    std::vector<VcTestFile> files;
    std::vector<VcTestBranch> branches;
    VcTab active_tab = VcTab::Changes;
};

VersionControlTestState& GetVersionControlState();

} // namespace dse::editor
