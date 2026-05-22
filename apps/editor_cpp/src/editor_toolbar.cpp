#include "editor_toolbar.h"
#include "editor_context.h"

#include <memory>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include "editor_profiler_panel.h"
#include "editor_scene_io.h"
#include "editor_scene_tabs.h"
#include "editor_scene_camera.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/world.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/base/time.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include "editor_locale.h"
#include "editor_preferences_panel.h"

namespace dse::editor {

static std::unique_ptr<entt::registry> s_backup_registry;
static bool s_step_pending = false;

void MarkAllUILabelsDirty(entt::registry& registry) {
    auto view = registry.view<UILabelComponent>();
    for (auto entity : view) {
        auto& label = view.get<UILabelComponent>(entity);
        label.runtime_glyph_entities.clear();
        label.dirty = true;
    }
}

static EditorState s_editor_state = EditorState::Edit;

EditorState GetEditorState() {
    return s_editor_state;
}

bool IsEditorInPlayMode() {
    return s_editor_state == EditorState::Play;
}

static void ResetPlayModeRuntimeState() {
    dse::runtime::ShutdownLuaRuntime();
    dse::runtime::ConfigureLuaApiContext(dse::runtime::LuaApiContext{});
    dse::runtime::SetStartupLuaScriptPath("");
    Time::Init();
}

void EnterPlayMode(entt::registry& registry) {
    if (s_editor_state != EditorState::Edit) {
        return;
    }
    s_backup_registry = std::make_unique<entt::registry>();
    CopyRegistry(*s_backup_registry, registry);

    // SubScene 实例化：将引用的 .dscene 文件加载为子节点
    // 先收集到 vector，避免 LoadSceneAdditive 修改 registry 导致 view 迭代器失效
    std::vector<entt::entity> sub_entities;
    for (auto e : registry.view<dse::SubSceneComponent>()) sub_entities.push_back(e);
    for (auto entity : sub_entities) {
        auto& sub = registry.get<dse::SubSceneComponent>(entity);
        if (!sub.enabled || sub.scene_path.empty()) continue;
        LoadSceneAdditive(registry, sub.scene_path, entity);
        DEBUG_LOG_INFO("[SubScene] Loaded '{}' as child of entity {}", sub.scene_path, static_cast<uint32_t>(entity));
    }

    ResetPlayModeRuntimeState();
    s_editor_state = EditorState::Play;
}

void ExitPlayMode(entt::registry& registry, entt::entity& selected_entity,
                  dse::runtime::EngineInstance* engine) {
    if (s_editor_state == EditorState::Edit) {
        return;
    }
    // 1. Tear down Lua before touching registry
    ResetPlayModeRuntimeState();
    // 2. Release PhysX actors that were created during play
    if (engine) {
        engine->pipeline()->ResetPhysics3D();
    }
    // 3. Restore edit-mode registry snapshot
    if (s_backup_registry) {
        CopyRegistry(registry, *s_backup_registry);
        s_backup_registry.reset();
    }
    // 4. Restored state == clean (nothing to save)
    SceneTabManager::Get().MarkClean();
    MarkAllUILabelsDirty(registry);
    ResetProfilerPanelState();
    selected_entity = entt::null;
    s_step_pending = false;
    s_editor_state = EditorState::Edit;
}

bool IsEditorPaused() {
    return s_editor_state == EditorState::Pause;
}

bool ConsumeStepFrame() {
    if (!s_step_pending) return false;
    s_step_pending = false;
    return true;
}

void DrawEditorToolbar(EditorContext& ctx) {
    auto& registry = ctx.registry;
    auto& selected_entity = ctx.selected_entity;
    auto& gizmo_op = ctx.current_gizmo_operation;
    auto& gizmo_mode = ctx.current_gizmo_mode;
    auto& languages = ctx.editor_languages;
    auto& lang_index = ctx.editor_language_index;

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

    tool_button(MDI_ICON_CURSOR_DEFAULT_OUTLINE, T("Hand Tool (H)"), -1, gizmo_op);
    tool_button(MDI_ICON_ARROW_ALL, T("Translate Tool (W)"), 0, gizmo_op);
    tool_button(MDI_ICON_ROTATE_3D_VARIANT, T("Rotate Tool (E)"), 1, gizmo_op);
    tool_button(MDI_ICON_RESIZE, T("Scale Tool (R)"), 2, gizmo_op);

    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button(gizmo_mode == 0 ? T("Local") : T("World"), ImVec2(48, 24))) { gizmo_mode = 1 - gizmo_mode; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Toggle Gizmo Coordinate Space"));
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::SetCursorPosX(10 + 4 * 36 + 20);
    const bool was_2d = ctx.is_2d;
    if (was_2d) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
    }
    if (ImGui::Button("2D", ImVec2(32, 24))) { ctx.is_2d = !ctx.is_2d; }
    if (was_2d) {
        ImGui::PopStyleColor();
    }

    // Vertical separator between tool group and utility tools
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // 对齐工具
    if (ImGui::Button(MDI_ICON_AXIS_ARROW "##align_left", ImVec2(32, 24))) {
        // TODO: 实现对齐功能
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Align Left"));
    ImGui::SameLine();

    if (ImGui::Button(MDI_ICON_AXIS_ARROW "##align_center", ImVec2(32, 24))) {
        // TODO: 实现对齐功能
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Align Center"));
    ImGui::SameLine();

    if (ImGui::Button(MDI_ICON_AXIS_ARROW "##align_right", ImVec2(32, 24))) {
        // TODO: 实现对齐功能
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Align Right"));
    ImGui::SameLine();

    // 网格吸附
    static bool grid_snap = false;
    if (grid_snap) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    if (ImGui::Button(MDI_ICON_VIEW_GRID "##grid_snap", ImVec2(32, 24))) {
        grid_snap = !grid_snap;
    }
    if (grid_snap) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Grid Snap"));
    ImGui::SameLine();

    // 视图重置
    if (ImGui::Button(MDI_ICON_CAMERA "##view_reset", ImVec2(32, 24))) {
        EditorCamera& cam = GetEditorCamera();
        cam.focal_point = glm::vec3(0.0f);
        cam.distance = 10.0f;
        cam.yaw = 0.0f;
        cam.pitch = 0.3f;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Reset View"));

    // Vertical separator between tool group and play controls
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::SetCursorPosX((avail_width / 2.0f) - 60);

    if (s_editor_state == EditorState::Edit) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.18f, 1.0f));
        if (ImGui::Button(MDI_ICON_PLAY "##play", ImVec2(32, 24))) {
            EnterPlayMode(registry);
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(MDI_ICON_STOP "##stop", ImVec2(32, 24))) {
            ExitPlayMode(registry, selected_entity, &ctx.engine);
        }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    {
        const bool is_paused = (s_editor_state == EditorState::Pause);
        if (is_paused) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        const bool can_pause = (s_editor_state == EditorState::Play || s_editor_state == EditorState::Pause);
        if (!can_pause) ImGui::BeginDisabled();
        if (ImGui::Button(MDI_ICON_PAUSE "##pause", ImVec2(32, 24))) {
            s_editor_state = is_paused ? EditorState::Play : EditorState::Pause;
        }
        if (!can_pause) ImGui::EndDisabled();
        if (is_paused) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", is_paused ? T("Resume") : T("Pause"));
    }
    ImGui::SameLine();
    {
        const bool can_step = (s_editor_state == EditorState::Pause);
        if (!can_step) ImGui::BeginDisabled();
        if (ImGui::Button(MDI_ICON_SKIP_NEXT "##step", ImVec2(32, 24))) {
            s_step_pending = true;
        }
        if (!can_step) ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Step one fixed-update frame"));
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(avail_width - 360);

    // ─── Theme toggle ───────────────────────────────────────────────────────
    {
        const bool is_light = (GetCurrentThemeIndex() == 1);
        const char* icon    = is_light ? MDI_ICON_MOON : MDI_ICON_WEATHER_SUNNY;
        const char* tip     = is_light ? T("Switch to Dark Theme") : T("Switch to Light Theme");
        if (ImGui::Button(icon, ImVec2(32, 24))) {
            ToggleEditorTheme();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        ImGui::SameLine();
    }

    if (!languages.empty()) {
        std::vector<const char*> language_items;
        language_items.reserve(languages.size());
        for (const auto& lang : languages) {
            language_items.push_back(lang.c_str());
        }
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##LanguagePreview", &lang_index, language_items.data(), static_cast<int>(language_items.size()))) {
            auto& localization = dse::gameplay2d::LocalizationSystem::GetInstance();
            localization.SetCurrentLanguage(languages[lang_index]);
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

} // namespace dse::editor
