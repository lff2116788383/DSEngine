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

    // Play controls centered
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
    float right_section_width = 32 + 8 + 110 + 8 + 60 + 8 + 60 + 10;
    float right_x = avail_width - right_section_width;
    float play_end = (avail_width / 2.0f) + 60;
    if (right_x < play_end + 16) right_x = play_end + 16;
    ImGui::SetCursorPosX(right_x);

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
            // 同步更新编辑器 UI 语言（即时生效，无需重启）
            const std::string& lang = languages[lang_index];
            if (lang == "zh" || lang == "zh-CN" || lang == "zh_CN")
                SetEditorLocale("zh-CN");
            else
                SetEditorLocale("en");
        }
        ImGui::SameLine();
    }
    ImGui::Button("Collab", ImVec2(60, 24));
    ImGui::SameLine();
    ImGui::Button("Layers", ImVec2(60, 24));

    ImGui::PopStyleVar();
    ImGui::End();
}

} // namespace dse::editor
