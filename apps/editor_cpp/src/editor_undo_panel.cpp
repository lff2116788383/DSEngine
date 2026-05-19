#include "editor_undo_panel.h"
#include "editor_shortcuts.h"
#include "editor_undo.h"
#include "imgui.h"

namespace dse::editor {

void DrawUndoHistoryPanel(bool* p_open) {
    if (!p_open || !*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Undo History", p_open)) {
        ImGui::End();
        return;
    }

    auto& mgr = GetUndoRedoManager();

    // Undo section
    auto undo_history = mgr.GetUndoHistory();
    auto redo_history = mgr.GetRedoHistory();

    ImGui::Text("Undo Stack (%d)", mgr.GetUndoCount());
    ImGui::Separator();
    if (undo_history.empty()) {
        ImGui::TextDisabled("(empty)");
    } else {
        if (ImGui::BeginChild("##undo_list", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.5f - 20), true)) {
            for (int i = 0; i < static_cast<int>(undo_history.size()); ++i) {
                bool is_top = (i == 0);
                if (is_top) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.4f, 1.0f));
                ImGui::Selectable(undo_history[i].c_str(), is_top);
                if (is_top) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    }

    ImGui::Spacing();
    ImGui::Text("Redo Stack (%d)", mgr.GetRedoCount());
    ImGui::Separator();
    if (redo_history.empty()) {
        ImGui::TextDisabled("(empty)");
    } else {
        if (ImGui::BeginChild("##redo_list", ImVec2(0, 0), true)) {
            for (int i = 0; i < static_cast<int>(redo_history.size()); ++i) {
                bool is_top = (i == 0);
                if (is_top) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                ImGui::Selectable(redo_history[i].c_str(), is_top);
                if (is_top) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

} // namespace dse::editor
