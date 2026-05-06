#include "editor_toolbar.h"

#include <memory>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include "editor_profiler_panel.h"
#include "engine/runtime/engine_app.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/world.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/base/time.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include "editor_scene_io.h"

int g_current_gizmo_operation = 0;
int g_current_gizmo_mode = 0;
std::vector<std::string> g_editor_languages;
int g_editor_language_index = 0;
std::unique_ptr<entt::registry> g_backup_registry;

void MarkAllUILabelsDirty(entt::registry& registry) {
    auto view = registry.view<UILabelComponent>();
    for (auto entity : view) {
        auto& label = view.get<UILabelComponent>(entity);
        label.runtime_glyph_entities.clear();
        label.dirty = true;
    }
}

EditorState g_editor_state = EditorState::Edit;

EditorState GetEditorState() {
    return g_editor_state;
}

bool IsEditorInPlayMode() {
    return g_editor_state == EditorState::Play;
}

namespace {

void ResetPlayModeRuntimeState() {
    dse::runtime::ShutdownLuaRuntime();
    dse::runtime::ConfigureLuaApiContext(dse::runtime::LuaApiContext{});
    dse::runtime::SetStartupLuaScriptPath("");
    Time::Init();
}

void EnterPlayMode(entt::registry& registry) {
    if (g_editor_state != EditorState::Edit) {
        return;
    }
    g_backup_registry = std::make_unique<entt::registry>();
    CopyRegistry(*g_backup_registry, registry);
    ResetPlayModeRuntimeState();
    g_editor_state = EditorState::Play;
}

void ExitPlayMode(entt::registry& registry, entt::entity& selected_entity) {
    if (g_editor_state == EditorState::Edit) {
        return;
    }
    ResetPlayModeRuntimeState();
    if (g_backup_registry) {
        CopyRegistry(registry, *g_backup_registry);
        g_backup_registry.reset();
    }
    MarkAllUILabelsDirty(registry);
    dse::editor::ResetProfilerPanelState();
    selected_entity = entt::null;
    g_editor_state = EditorState::Edit;
}

} // namespace

void DrawEditorToolbar(dse::runtime::EngineInstance& engine,
                       entt::registry& registry,
                       entt::entity& selected_entity) {
    (void)engine;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4));
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);
    float avail_width = ImGui::GetContentRegionAvail().x;

    ImGui::SetCursorPosX(10);

    auto tool_button = [](const char* icon, const char* tooltip, int op_id, int& current_op) {
        ImGui::PushStyleColor(ImGuiCol_Button, current_op == op_id ? ImGui::GetStyle().Colors[ImGuiCol_ButtonActive] : ImGui::GetStyle().Colors[ImGuiCol_Button]);
        if (ImGui::Button(icon, ImVec2(32, 24))) { current_op = op_id; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        ImGui::PopStyleColor();
        ImGui::SameLine();
    };

    tool_button(MDI_ICON_CURSOR_DEFAULT_OUTLINE, "Hand Tool (H)", -1, g_current_gizmo_operation);
    tool_button(MDI_ICON_ARROW_ALL, "Translate Tool (W)", 0, g_current_gizmo_operation);
    tool_button(MDI_ICON_ROTATE_3D_VARIANT, "Rotate Tool (E)", 1, g_current_gizmo_operation);
    tool_button(MDI_ICON_RESIZE, "Scale Tool (R)", 2, g_current_gizmo_operation);

    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button(g_current_gizmo_mode == 0 ? "Local" : "World", ImVec2(48, 24))) { g_current_gizmo_mode = 1 - g_current_gizmo_mode; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Gizmo Coordinate Space");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::SetCursorPosX(10 + 4 * 36 + 20);
    static bool is2D = false;
    if (is2D) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
    }
    if (ImGui::Button("2D", ImVec2(32, 24))) { is2D = !is2D; }
    if (is2D) {
        ImGui::PopStyleColor();
    }

    // Vertical separator between tool group and play controls
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::SetCursorPosX((avail_width / 2.0f) - 60);

    if (g_editor_state == EditorState::Edit) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.18f, 1.0f));
        if (ImGui::Button(MDI_ICON_PLAY "##play", ImVec2(32, 24))) {
            EnterPlayMode(registry);
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(MDI_ICON_STOP "##stop", ImVec2(32, 24))) {
            ExitPlayMode(registry, selected_entity);
        }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::Button(MDI_ICON_PAUSE "##pause", ImVec2(32, 24))) {}
    ImGui::SameLine();
    if (ImGui::Button(MDI_ICON_SKIP_NEXT "##step", ImVec2(32, 24))) {}

    ImGui::SameLine();
    ImGui::SetCursorPosX(avail_width - 320);
    if (!g_editor_languages.empty()) {
        std::vector<const char*> language_items;
        language_items.reserve(g_editor_languages.size());
        for (const auto& lang : g_editor_languages) {
            language_items.push_back(lang.c_str());
        }
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##LanguagePreview", &g_editor_language_index, language_items.data(), static_cast<int>(language_items.size()))) {
            auto& localization = dse::gameplay2d::LocalizationSystem::GetInstance();
            localization.SetCurrentLanguage(g_editor_languages[g_editor_language_index]);
            MarkAllUILabelsDirty(registry);
        }
        ImGui::SameLine();
    }
    ImGui::Button("Collab", ImVec2(60, 24));
    ImGui::SameLine();
    ImGui::Button("Layers", ImVec2(60, 24));

    ImGui::End();
    ImGui::PopStyleVar();
}
