#pragma once

#include "imgui.h"
#include <filesystem>

namespace dse::editor {

// Font slots available after LoadEditorFonts()
struct EditorFonts {
    ImFont* regular = nullptr;   // Inter-Regular 16px + CJK + Icons merged
    ImFont* bold    = nullptr;   // Inter-Bold 16px (headings)
};

// Call AFTER ImGui::CreateContext() and BEFORE the first NewFrame().
// |fonts_dir| should point to apps/editor_cpp/fonts/
// Returns false if no custom fonts could be loaded (falls back to default).
bool LoadEditorFonts(const std::filesystem::path& fonts_dir);

// Apply the DSEngine dark theme (Hazel-inspired).
// Call AFTER ImGui::CreateContext().
void SetupEditorStyle();

// Access the loaded fonts (valid after LoadEditorFonts succeeds).
const EditorFonts& GetEditorFonts();

// Helper: push bold font for a scope.  Call PopFont() when done.
void PushBoldFont();

} // namespace dse::editor
