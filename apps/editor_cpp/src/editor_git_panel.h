#pragma once

namespace dse::editor {

/// Draw the Git version control integration panel
void DrawGitPanel();

/// Get the current branch name (cached, updated periodically)
const char* GetGitBranchName();

/// Refresh git status data (call periodically, e.g. every 5 seconds)
void RefreshGitStatus();

} // namespace dse::editor
