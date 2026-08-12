#include "editor_undo_panel.h"
#include "editor_shortcuts.h"
#include "editor_undo.h"
#include "imgui.h"

#include "editor_panel_registry.h"

namespace dse::editor {

void DrawUndoHistoryPanel(bool* p_open) {
    if (!p_open || !*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Undo History", p_open)) {
        ImGui::End();
        return;
    }
    PanelRegistry::Get().DrawMaximizeRestoreButton();

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
                // 点击条目直接跳转到该历史状态（撤销 i+1 步）
                if (ImGui::Selectable(undo_history[i].c_str(), is_top)) {
                    mgr.JumpToUndo(i + 1);
                }
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
                // 点击条目直接跳转（重做 i+1 步）
                if (ImGui::Selectable(redo_history[i].c_str(), is_top)) {
                    mgr.JumpToRedo(i + 1);
                }
                if (is_top) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

// P0-6 self-registration: data-driven; editor_app binds visibility by id.
DSE_EDITOR_PANEL([](dse::editor::PanelRegistry& reg) {
    dse::editor::PanelEntry e;
    e.id = "undo_history";
    e.display_name = "Undo History";
    e.category = "Debug";
    e.order = 140;
    e.draw = [](dse::editor::EditorContext&) {
        auto* self = dse::editor::PanelRegistry::Get().Find("undo_history");
        DrawUndoHistoryPanel(self ? self->visible : nullptr);
    };
    reg.Register(std::move(e));
});

} // namespace dse::editor
