#include "editor_prefab_override.h"
#include "editor_prefab.h"
#include "editor_context.h"
#include "editor_icons.h"

#include "imgui.h"

namespace dse::editor {

// NOTE: ComputePrefabOverrides / RevertPrefabOverride / RevertAllPrefabOverrides /
// ApplyOverridesToPrefab (and their JSON-compare helpers) are defined in
// editor_prefab_override_core.cpp (pure entt/JSON logic, headless-testable).
// This file keeps only the ImGui DrawPrefabOverrideSection panel.

void DrawPrefabOverrideSection(EditorContext& ctx) {
    if (ctx.selected_entity == entt::null) return;
    if (!ctx.registry.valid(ctx.selected_entity)) return;
    if (!IsPrefabInstance(ctx.registry, ctx.selected_entity)) return;

    auto info = ComputePrefabOverrides(ctx.registry, ctx.selected_entity);

    ImGui::Separator();
    ImU32 prefab_color = info.overrides.empty()
        ? IM_COL32(100, 200, 100, 255)
        : IM_COL32(255, 180, 80, 255);

    ImGui::PushStyleColor(ImGuiCol_Text, ImColor(prefab_color).Value);
    bool open = ImGui::CollapsingHeader(MDI_ICON_CONTENT_COPY "  Prefab Instance",
                                         ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor();

    if (!open) return;

    ImGui::TextDisabled("Source: %s", info.prefab_source_path.c_str());

    if (info.overrides.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "No overrides (matches prefab)");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%d override(s)",
                          static_cast<int>(info.overrides.size()));

        ImGui::Columns(3, "prefab_overrides", true);
        ImGui::SetColumnWidth(0, 140.0f);
        ImGui::SetColumnWidth(1, 100.0f);
        ImGui::Text("Property"); ImGui::NextColumn();
        ImGui::Text("Original"); ImGui::NextColumn();
        ImGui::Text("Current"); ImGui::NextColumn();
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(info.overrides.size()); i++) {
            auto& ov = info.overrides[i];
            ImGui::PushID(i);

            // Property name with component prefix
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s.%s",
                              ov.component_name.c_str(), ov.property_name.c_str());
            ImGui::NextColumn();

            // Original value
            ImGui::TextDisabled("%s", ov.original_value.c_str());
            ImGui::NextColumn();

            // Current value + revert button
            ImGui::Text("%s", ov.current_value.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Revert")) {
                RevertPrefabOverride(ctx.registry, ctx.selected_entity, ov);
            }
            ImGui::NextColumn();

            ImGui::PopID();
        }
        ImGui::Columns(1);
    }

    ImGui::Spacing();
    if (!info.overrides.empty()) {
        if (ImGui::Button("Revert All", ImVec2(100, 0))) {
            RevertAllPrefabOverrides(ctx.registry, ctx.selected_entity);
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply to Prefab", ImVec2(120, 0))) {
            ApplyOverridesToPrefab(ctx.registry, ctx.selected_entity);
        }
    }
}

} // namespace dse::editor
