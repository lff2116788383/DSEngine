/**
 * @file editor_streaming_panel.cpp
 * @brief Streaming Zone 调试面板 — 显示所有 Zone 的状态、进度和参数
 */

#include "editor_streaming_panel.h"
#include "editor_context.h"
#include "engine/assets/streaming_manager.h"
#include "engine/core/service_locator.h"
#include "imgui.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace dse::editor {

namespace {

const char* ZoneStateLabel(dse::streaming::ZoneState s) {
    switch (s) {
        case dse::streaming::ZoneState::Unloaded:  return "Unloaded";
        case dse::streaming::ZoneState::Loading:   return "Loading";
        case dse::streaming::ZoneState::Loaded:    return "Loaded";
        case dse::streaming::ZoneState::Unloading: return "Unloading";
        default: return "Unknown";
    }
}

ImVec4 ZoneStateColor(dse::streaming::ZoneState s) {
    switch (s) {
        case dse::streaming::ZoneState::Unloaded:  return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        case dse::streaming::ZoneState::Loading:   return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        case dse::streaming::ZoneState::Loaded:    return ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
        case dse::streaming::ZoneState::Unloading: return ImVec4(0.9f, 0.5f, 0.2f, 1.0f);
        default: return ImVec4(1, 1, 1, 1);
    }
}

const char* AssetTypeLabel(dse::streaming::AssetType t) {
    switch (t) {
        case dse::streaming::AssetType::Texture:   return "Tex";
        case dse::streaming::AssetType::Mesh:      return "Mesh";
        case dse::streaming::AssetType::Animation: return "Anim";
        case dse::streaming::AssetType::Skeleton:  return "Skel";
        case dse::streaming::AssetType::Audio:     return "Audio";
        case dse::streaming::AssetType::Material:  return "Mat";
        default: return "?";
    }
}

} // anonymous namespace

void DrawStreamingDebugPanel(EditorContext& /*ctx*/) {
    auto* sm = dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
    if (!sm) {
        ImGui::TextDisabled("StreamingManager not available.");
        return;
    }

    // Summary bar
    int zone_count = static_cast<int>(sm->GetZoneCount());
    int active_loads = sm->GetActiveLoadCount();
    ImGui::Text("Zones: %d  |  Active Loads: %d", zone_count, active_loads);
    ImGui::Separator();

    if (zone_count == 0) {
        ImGui::TextDisabled("No streaming zones registered.");
        ImGui::TextDisabled("Use ecs.create_streaming_zone() in Lua to create zones.");
        return;
    }

    auto zones = sm->GetZoneSnapshot();

    // Zone table
    if (ImGui::BeginTable("##streaming_zones", 7,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, ImGui::GetContentRegionAvail().y - 30))) {

        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Center", ImGuiTableColumnFlags_WidthFixed, 180);
        ImGui::TableSetupColumn("Load R", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        for (auto& z : zones) {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(z.id));

            // ID
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", z.id);

            // Name
            ImGui::TableSetColumnIndex(1);
            bool tree_open = ImGui::TreeNodeEx(z.name.empty() ? "(unnamed)" : z.name.c_str(),
                                                ImGuiTreeNodeFlags_SpanFullWidth);

            // State
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(ZoneStateColor(z.state), "%s", ZoneStateLabel(z.state));

            // Progress
            ImGui::TableSetColumnIndex(3);
            float total = static_cast<float>(z.assets.size());
            float loaded = total > 0 ? total - static_cast<float>(z.assets_pending) : 0;
            float progress = total > 0 ? loaded / total : 1.0f;
            ImGui::ProgressBar(progress, ImVec2(-1, 0));

            // Center
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("(%.0f, %.0f, %.0f)", z.center.x, z.center.y, z.center.z);

            // Radii
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.0f/%.0f", z.load_radius, z.unload_radius);

            // Asset count
            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%d", static_cast<int>(z.assets.size()));

            // Expanded: show asset list
            if (tree_open) {
                for (auto& a : z.assets) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(1);
                    const char* icon = a.loaded ? "[OK]" : "[..]";
                    ImGui::TextColored(a.loaded ? ImVec4(0.3f, 0.9f, 0.3f, 1) : ImVec4(0.6f, 0.6f, 0.6f, 1),
                                       "  %s %s %s", icon, AssetTypeLabel(a.type), a.path.c_str());
                }
                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Tip: Use ForceLoadZone(id) / ForceUnloadZone(id) from Lua console to test.");
}

} // namespace dse::editor
