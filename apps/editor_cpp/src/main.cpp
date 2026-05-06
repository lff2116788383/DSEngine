#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#include <Windows.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "ImGuizmo.h"

#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "modules/gameplay_3d/camera/free_camera_controller_system.h"
#include "modules/gameplay_3d/animation/animator_system.h"
#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/memory_profiler.h"
#include "engine/profiler/render_profiler.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include "editor_aux_panels.h"
#include "editor_hierarchy_panel.h"
#include "editor_inspector_panel.h"
#include "editor_profiler_panel.h"
#include "editor_scene_io.h"
#include "editor_shell.h"
#include "editor_toolbar.h"
#include "editor_viewport_panel.h"

#include "editor_shared_components.h"
#include "editor_theme.h"
#include "editor_icons.h"
#include "editor_shortcuts.h"
#include "editor_console_panel.h"
#include "editor_scene_camera.h"
#include "editor_status_bar.h"
#include "editor_settings.h"
#include "editor_material_panel.h"
#include "editor_prefab.h"
#include "editor_preferences_panel.h"
#include "editor_terrain_panel.h"

// Theme & font setup moved to editor_theme.cpp (SetupEditorStyle / LoadEditorFonts)

extern std::vector<std::string> g_editor_languages;
extern int g_editor_language_index;
extern int g_current_gizmo_operation;
extern int g_current_gizmo_mode;

dse::profiler::CPUProfiler g_cpu_profiler;
dse::profiler::MemoryProfiler g_memory_profiler;
dse::profiler::RenderProfiler g_render_profiler;

constexpr int kProfilerHistoryMaxSamples = 180;

namespace {

std::filesystem::path GetProjectRootPath() {
    std::filesystem::path path;
#if defined(_WIN32)
    std::wstring module_path(MAX_PATH, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (size > 0) {
        module_path.resize(size);
        path = std::filesystem::path(module_path).parent_path();
    }
#endif
    if (path.empty()) {
        path = std::filesystem::current_path();
    }
    if (path.filename() == "bin" || path.filename() == "build_vs2022") {
        path = path.parent_path();
    }
    return path.lexically_normal();
}

std::filesystem::path GetEditorBinPath() {
    const std::filesystem::path path = GetProjectRootPath() / "bin";
    std::filesystem::create_directories(path);
    return path;
}

void PushHistorySample(std::vector<float>& history, float value) {
    history.push_back(value);
    if (static_cast<int>(history.size()) > kProfilerHistoryMaxSamples) {
        history.erase(history.begin(), history.begin() + (history.size() - kProfilerHistoryMaxSamples));
    }
}

bool BuildActiveCameraMatrices(entt::registry& registry, float aspect_ratio, glm::mat4& out_view, glm::mat4& out_proj) {
    entt::entity selected_camera3d = entt::null;
    int selected_priority = -2147483647;
    std::uint32_t selected_id = 0xFFFFFFFFu;
    auto camera3d_view = registry.view<dse::Camera3DComponent>();
    for (auto entity : camera3d_view) {
        const auto& camera = camera3d_view.get<dse::Camera3DComponent>(entity);
        if (!camera.enabled) {
            continue;
        }
        const std::uint32_t entity_id = static_cast<std::uint32_t>(entity);
        if (selected_camera3d == entt::null ||
            camera.priority > selected_priority ||
            (camera.priority == selected_priority && entity_id < selected_id)) {
            selected_camera3d = entity;
            selected_priority = camera.priority;
            selected_id = entity_id;
        }
    }

    if (selected_camera3d != entt::null) {
        const auto& camera = camera3d_view.get<dse::Camera3DComponent>(selected_camera3d);
        const float near_clip = std::max(0.001f, camera.near_clip);
        const float far_clip = std::max(near_clip + 0.001f, camera.far_clip);
        const float safe_aspect = std::max(0.001f, aspect_ratio);
        out_proj = glm::perspective(glm::radians(camera.fov), safe_aspect, near_clip, far_clip);
        if (registry.all_of<TransformComponent>(selected_camera3d)) {
            const auto& transform = registry.get<TransformComponent>(selected_camera3d);
            glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            out_view = glm::lookAt(transform.position, transform.position + front, up);
        } else {
            out_view = glm::mat4(1.0f);
        }
        return true;
    }

    auto camera2d_view = registry.view<CameraComponent>();
    entt::entity selected_camera2d = entt::null;
    selected_priority = -2147483647;
    selected_id = 0xFFFFFFFFu;
    for (auto entity : camera2d_view) {
        const auto& camera = camera2d_view.get<CameraComponent>(entity);
        if (!camera.enabled) {
            continue;
        }
        const std::uint32_t entity_id = static_cast<std::uint32_t>(entity);
        if (selected_camera2d == entt::null ||
            camera.priority > selected_priority ||
            (camera.priority == selected_priority && entity_id < selected_id)) {
            selected_camera2d = entity;
            selected_priority = camera.priority;
            selected_id = entity_id;
        }
    }
    if (selected_camera2d != entt::null) {
        const auto& camera = camera2d_view.get<CameraComponent>(selected_camera2d);
        out_view = camera.view;
        out_proj = camera.projection;
        return true;
    }
    return false;
}

std::filesystem::path GetEditorExportDirectory() {
    std::filesystem::path path = GetEditorBinPath() / "editor_exports";
    std::filesystem::create_directories(path);
    return path;
}

void ExportTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::trunc);
    if (ofs.is_open()) {
        ofs << content;
    }
}

void EnsureEditorLocalizationData() {
    auto& localization = dse::gameplay2d::LocalizationSystem::GetInstance();
    localization.Clear();

    const std::filesystem::path dir = GetEditorBinPath() / "editor_localization";
    std::filesystem::create_directories(dir);

    const std::array<std::pair<const char*, const char*>, 3> seeds = {{
        {"en", R"({"editor":{"preview":{"title":"Editor Preview","status":"Language: {lang}","selection":"Selected: {entity}"}}})"},
        {"zh", R"({"editor":{"preview":{"title":"\u7f16\u8f91\u5668\u9884\u89c8","status":"\u5f53\u524d\u8bed\u8a00\uff1a{lang}","selection":"\u5f53\u524d\u9009\u4e2d\uff1a{entity}"}}})"},
        {"ja", R"({"editor":{"preview":{"title":"\u30a8\u30c7\u30a3\u30bf\u30fc\u30d7\u30ec\u30d3\u30e5\u30fc","status":"\u73fe\u5728\u306e\u8a00\u8a9e: {lang}","selection":"\u9078\u629e\u4e2d: {entity}"}}})"}
    }};

    g_editor_languages.clear();
    for (const auto& seed : seeds) {
        const std::filesystem::path file_path = dir / (std::string(seed.first) + ".json");
        ExportTextFile(file_path, seed.second);
        if (localization.LoadLanguage(seed.first, file_path.string())) {
            g_editor_languages.emplace_back(seed.first);
        }
    }

    if (!g_editor_languages.empty()) {
        localization.SetCurrentLanguage(g_editor_languages.front());
        g_editor_language_index = 0;
    }
}

} // namespace


void DrawUILayoutInspector(entt::registry& registry, entt::entity entity) {
    const bool read_only = GetEditorState() == EditorState::Play;

    if (registry.all_of<UIAnchorComponent>(entity)) {
        if (ImGui::CollapsingHeader(MDI_ICON_IMAGE "  UI Anchor", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& anchor = registry.get<UIAnchorComponent>(entity);
            const char* anchor_types[] = {
                "Center", "TopLeft", "TopCenter", "TopRight",
                "MiddleLeft", "MiddleRight", "BottomLeft", "BottomCenter",
                "BottomRight", "StretchAll", "StretchHorizontal", "StretchVertical"
            };
            int current_anchor = static_cast<int>(anchor.anchor);
            ImGui::BeginDisabled(read_only);
            if (ImGui::Combo("Anchor Preset", &current_anchor, anchor_types, IM_ARRAYSIZE(anchor_types))) {
                anchor.anchor = current_anchor;
            }
            ImGui::DragFloat2("Offset", glm::value_ptr(anchor.offset), 1.0f);
            if (ImGui::Button("Remove UI Anchor", ImVec2(-1, 0))) {
                registry.remove<UIAnchorComponent>(entity);
            }
            ImGui::EndDisabled();
        }
    } else {
        ImGui::BeginDisabled(read_only);
        if (ImGui::Button("Add UI Anchor", ImVec2(-1, 0))) {
            registry.emplace<UIAnchorComponent>(entity);
        }
        ImGui::EndDisabled();
    }

    if (registry.all_of<UIGridLayoutComponent>(entity)) {
        if (ImGui::CollapsingHeader(MDI_ICON_SHAPE "  UI Grid Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& grid = registry.get<UIGridLayoutComponent>(entity);
            ImGui::BeginDisabled(read_only);
            ImGui::DragInt("Columns", &grid.columns, 0.1f, 0, 100);
            ImGui::DragInt("Rows", &grid.rows, 0.1f, 0, 100);
            ImGui::DragFloat2("Cell Size", glm::value_ptr(grid.cell_size), 1.0f);
            ImGui::DragFloat2("Spacing", glm::value_ptr(grid.spacing), 1.0f);

            const char* align_types[] = {
                "TopLeft", "TopCenter", "TopRight",
                "MiddleLeft", "MiddleCenter", "MiddleRight",
                "BottomLeft", "BottomCenter", "BottomRight"
            };
            int current_align = static_cast<int>(grid.alignment);
            if (ImGui::Combo("Alignment", &current_align, align_types, IM_ARRAYSIZE(align_types))) {
                grid.alignment = current_align;
            }

            if (ImGui::Button("Remove UI Grid Layout", ImVec2(-1, 0))) {
                registry.remove<UIGridLayoutComponent>(entity);
            }
            ImGui::EndDisabled();
        }
    } else {
        ImGui::BeginDisabled(read_only);
        if (ImGui::Button("Add UI Grid Layout", ImVec2(-1, 0))) {
            registry.emplace<UIGridLayoutComponent>(entity);
        }
        ImGui::EndDisabled();
    }

    if (registry.all_of<UICanvasScalerComponent>(entity)) {
        if (ImGui::CollapsingHeader(MDI_ICON_RESIZE "  UI Canvas Scaler", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& scaler = registry.get<UICanvasScalerComponent>(entity);
            ImGui::BeginDisabled(read_only);
            ImGui::DragFloat2("Reference Resolution", glm::value_ptr(scaler.reference_resolution), 1.0f);
            ImGui::Checkbox("Match Width Or Height", &scaler.match_width_or_height);
            if (scaler.match_width_or_height) {
                ImGui::SliderFloat("Match (0=Width, 1=Height)", &scaler.scale_factor, 0.0f, 1.0f);
            }

            if (ImGui::Button("Remove UI Canvas Scaler", ImVec2(-1, 0))) {
                registry.remove<UICanvasScalerComponent>(entity);
            }
            ImGui::EndDisabled();
        }
    } else {
        ImGui::BeginDisabled(read_only);
        if (ImGui::Button("Add UI Canvas Scaler", ImVec2(-1, 0))) {
            registry.emplace<UICanvasScalerComponent>(entity);
        }
        ImGui::EndDisabled();
    }

    if (registry.all_of<UIAnimationComponent>(entity)) {
        if (ImGui::CollapsingHeader(MDI_ICON_ANIMATION "  UI Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& anim = registry.get<UIAnimationComponent>(entity);
            ImGui::BeginDisabled(read_only);

            if (anim.playing) {
                if (ImGui::Button("Stop##UIAnim", ImVec2(-1, 0))) anim.playing = false;
            } else {
                if (ImGui::Button("Play##UIAnim", ImVec2(-1, 0))) {
                    anim.playing = true;
                    anim.elapsed = 0.0f;
                    anim.delay_remaining = anim.delay;
                    anim.reverse = false;
                }
            }

            ImGui::Separator();
            ImGui::Text("Animation Properties");
            ImGui::DragFloat("Duration (s)", &anim.duration, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Delay (s)", &anim.delay, 0.01f, 0.0f, 10.0f);

            ImGui::Checkbox("Loop", &anim.loop);
            ImGui::SameLine();
            ImGui::Checkbox("Ping Pong", &anim.ping_pong);

            const char* easing_types[] = { "Linear", "Ease-In", "Ease-Out", "Ease-In-Out" };
            ImGui::Combo("Easing", &anim.easing, easing_types, IM_ARRAYSIZE(easing_types));

            ImGui::Separator();
            ImGui::Text("Targets (Enable to animate)");
            ImGui::Checkbox("##anim_pos", &anim.animate_position); ImGui::SameLine();
            if (anim.animate_position) {
                ImGui::DragFloat2("Target Position", glm::value_ptr(anim.target_position), 1.0f);
            } else { ImGui::TextDisabled("Target Position"); }

            ImGui::Checkbox("##anim_scale", &anim.animate_scale); ImGui::SameLine();
            if (anim.animate_scale) {
                ImGui::DragFloat2("Target Scale", glm::value_ptr(anim.target_scale), 0.05f);
            } else { ImGui::TextDisabled("Target Scale"); }

            ImGui::Checkbox("##anim_alpha", &anim.animate_alpha); ImGui::SameLine();
            if (anim.animate_alpha) {
                ImGui::DragFloat("Target Alpha", &anim.target_alpha, 0.05f, 0.0f, 1.0f);
            } else { ImGui::TextDisabled("Target Alpha"); }

            ImGui::Checkbox("##anim_color", &anim.animate_color); ImGui::SameLine();
            if (anim.animate_color) {
                ImGui::ColorEdit4("Target Color", glm::value_ptr(anim.target_color));
            } else { ImGui::TextDisabled("Target Color"); }

            if (ImGui::Button("Remove UI Animation", ImVec2(-1, 0))) {
                registry.remove<UIAnimationComponent>(entity);
            }
            ImGui::EndDisabled();
        }
    } else {
        ImGui::BeginDisabled(read_only);
        if (ImGui::Button("Add UI Animation", ImVec2(-1, 0))) {
            registry.emplace<UIAnimationComponent>(entity);
        }
        ImGui::EndDisabled();
    }

    if (read_only) {
        ImGui::TextDisabled("Play 模式下已禁用 UI Layout Inspector 编辑。请退出 Play 后修改 UI 布局组件。");
    }
}

void DrawEditorUI(dse::runtime::EngineInstance& engine, unsigned int scene_texture, unsigned int game_texture) {
    static entt::entity selected_entity = entt::null;
    static EditorState last_editor_state = EditorState::Edit;
    World& world = engine.pipeline()->world();
    auto& registry = world.registry();
    static bool is2D = false;

    static bool inspector_active = true;
    static bool inspector_static = false;
    static bool sprite_flip_x = false;
    static bool sprite_flip_y = false;
    static bool collider_is_trigger = false;
    static char localization_preview_key[128] = "editor.preview.status";
    static char localization_preview_fallback[128] = "Language: {lang}";

    if (last_editor_state != GetEditorState() && GetEditorState() == EditorState::Edit) {
        selected_entity = entt::null;
        inspector_active = true;
        inspector_static = false;
        sprite_flip_x = false;
        sprite_flip_y = false;
        collider_is_trigger = false;
        std::strncpy(localization_preview_key, "editor.preview.status", sizeof(localization_preview_key) - 1);
        localization_preview_key[sizeof(localization_preview_key) - 1] = '\0';
        std::strncpy(localization_preview_fallback, "Language: {lang}", sizeof(localization_preview_fallback) - 1);
        localization_preview_fallback[sizeof(localization_preview_fallback) - 1] = '\0';
    }
    last_editor_state = GetEditorState();

    static bool s_show_preferences = false;

    dse::editor::BeginEditorShell();
    dse::editor::EditorShellContext shell_context{engine, registry, selected_entity, GetEditorState() == EditorState::Play, &s_show_preferences};
    dse::editor::DrawEditorMainMenu(shell_context);

    // Process global keyboard shortcuts
    dse::editor::ShortcutContext shortcut_ctx{world, registry, selected_entity, GetEditorState() == EditorState::Play};
    dse::editor::ProcessShortcuts(shortcut_ctx);

    DrawEditorToolbar(engine, registry, selected_entity);

    // Panels (Unity-style layout)
    dse::editor::EditorHierarchyPanelContext hierarchy_context{world, registry, selected_entity, GetEditorState() == EditorState::Play};
    dse::editor::DrawHierarchyPanel(hierarchy_context);

    dse::editor::EditorInspectorPanelContext inspector_context{
        registry,
        selected_entity,
        is2D,
        inspector_active,
        inspector_static,
        GetEditorState() == EditorState::Play
    };
    dse::editor::DrawInspectorPanel(inspector_context, DrawUILayoutInspector);

    dse::editor::DrawProjectPanel();
    dse::editor::DrawConsolePanel();

    dse::editor::EditorAuxPanelsContext aux_panels_context{
        registry,
        selected_entity,
        is2D,
        GetEditorState() == EditorState::Play,
        localization_preview_key,
        sizeof(localization_preview_key),
        localization_preview_fallback,
        sizeof(localization_preview_fallback)
    };
    dse::editor::DrawLocalizationPreviewPanel(aux_panels_context);

    dse::editor::EditorProfilerContext profiler_context{g_cpu_profiler, g_memory_profiler, g_render_profiler};
    dse::editor::DrawProfilerPanel(profiler_context);
    dse::editor::DrawAnimationPanel(registry, selected_entity);
    dse::editor::DrawMaterialPanel(registry, selected_entity);
    dse::editor::DrawTilePalettePanel(aux_panels_context);
    dse::editor::DrawTerrainEditorPanel(registry, selected_entity);

    // Preferences panel (toggled from Window menu)
    dse::editor::DrawPreferencesPanel(&s_show_preferences);

    dse::editor::EditorViewportPanelContext scene_viewport_context{registry, selected_entity, scene_texture};
    dse::editor::DrawSceneViewportPanel(scene_viewport_context, g_current_gizmo_operation, g_current_gizmo_mode, BuildActiveCameraMatrices);
    dse::editor::DrawGameViewportPanel(game_texture);

    dse::editor::EditorStatusBarContext status_bar_context{g_cpu_profiler, g_render_profiler, registry, g_current_gizmo_operation, g_current_gizmo_mode};
    dse::editor::DrawStatusBar(status_bar_context);

    dse::editor::EndEditorShell();
}

int main() {
    // Allocate a console for WIN32 app so we can see crash output
    #if defined(_WIN32)
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    #endif
    
    std::cerr << "[Editor] Starting..." << std::endl;
    
    if (!glfwInit()) {
        std::cerr << "[Editor] glfwInit() failed!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "DSEngine Editor", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window in Editor." << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "Editor window created successfully." << std::endl;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL (gladLoadGL) in Editor." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    // NOTE: Multi-viewport on this editor build has been causing CRT heap assertions
    // on Windows during runtime/teardown. Keep docking enabled, but disable platform
    // viewports for stability until the backend lifetime issue is fully resolved.
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Redirect imgui.ini to bin folder so it doesn't clutter the project root
    static std::string imgui_ini_path = (GetEditorBinPath() / "editor_layout.ini").string();
    io.IniFilename = imgui_ini_path.c_str();

    // Setup Dear ImGui style (Hazel-inspired dark theme)
    ImGui::StyleColorsDark();
    dse::editor::SetupEditorStyle();

    // Load custom fonts (Inter + NotoSansSC + MDI icons)
    {
        std::filesystem::path fonts_dir = GetProjectRootPath() / "apps" / "editor_cpp" / "fonts";
        dse::editor::LoadEditorFonts(fonts_dir);
    }

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize DSEngine
    dse::runtime::EngineRunConfig engine_config;
    engine_config.window_width = 1280;
    engine_config.window_height = 720;
    engine_config.window_title = "DSEngine Editor";
    engine_config.business_mode = BusinessMode::Lua;
    engine_config.enable_editor = true;
    engine_config.startup_lua_script_path = "samples/lua/main.lua";
    
    // We need to set DSE_DATA_ROOT before initializing the engine
    if (std::getenv("DSE_DATA_ROOT") == nullptr) {
#if defined(_WIN32)
        const std::string data_root = (GetProjectRootPath() / "samples" / "lua" / "data").string();
        _putenv_s("DSE_DATA_ROOT", data_root.c_str());
#else
        const std::string data_root = (GetProjectRootPath() / "samples" / "lua" / "data").string();
        setenv("DSE_DATA_ROOT", data_root.c_str(), 1);
#endif
    }

    {
        dse::runtime::EngineInstance engine_instance(engine_config);
        if (!engine_instance.Init()) {
            return -1;
        }

        EnsureEditorLocalizationData();
        dse::editor::InstallEditorLogSink();

        // Load editor settings and restore state
        dse::editor::EditorSettings editor_settings = dse::editor::LoadEditorSettings();
        g_current_gizmo_operation = editor_settings.default_gizmo_operation;
        g_current_gizmo_mode = editor_settings.default_gizmo_mode;

        // Auto-load last opened scene
        if (!editor_settings.last_scene_path.empty() && editor_settings.last_scene_path != "Untitled") {
            if (std::filesystem::exists(editor_settings.last_scene_path)) {
                World& world = engine_instance.pipeline()->world();
                LoadScene(world.registry(), editor_settings.last_scene_path);
                dse::editor::SetCurrentScenePath(editor_settings.last_scene_path);
            }
        }

        std::cout << "Engine initialized successfully. Entering main loop..." << std::endl;

        // Main loop
        while (!glfwWindowShouldClose(window)) {
            g_cpu_profiler.BeginFrame();
            g_render_profiler.BeginFrame();
            g_memory_profiler.Reset();

            glfwPollEvents();

            // Update editor camera in render pipeline (Edit mode only)
            if (GetEditorState() == EditorState::Edit) {
                int fb_w, fb_h;
                glfwGetFramebufferSize(window, &fb_w, &fb_h);
                float aspect = (fb_h > 0) ? static_cast<float>(fb_w) / static_cast<float>(fb_h) : 1.7777f;
                auto& editor_cam = dse::editor::GetEditorCamera();
                engine_instance.pipeline()->SetEditorCamera(
                    editor_cam.GetViewMatrix(),
                    editor_cam.GetProjectionMatrix(aspect));
            } else {
                engine_instance.pipeline()->DisableEditorCamera();
            }

            // Tick Engine
            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "EngineTick");
                engine_instance.Tick();
            }

            unsigned int scene_texture = engine_instance.pipeline()->GetSceneTextureId();
            unsigned int game_texture = engine_instance.pipeline()->GetMainTextureId();

            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "EditorMetrics");
                World& profiler_world = engine_instance.pipeline()->world();
                auto& profiler_registry = profiler_world.registry();
                const int entity_count = static_cast<int>(profiler_registry.storage<entt::entity>().size());
                auto sprite_view = profiler_registry.view<SpriteRendererComponent>();
                const int sprite_count = static_cast<int>(std::distance(sprite_view.begin(), sprite_view.end()));

                g_memory_profiler.RecordAlloc("World.Entities", static_cast<size_t>(std::max(entity_count, 0)) * sizeof(entt::entity));
                g_memory_profiler.RecordAlloc("Render.SceneTexture", static_cast<size_t>(1280 * 720 * 4));
                g_memory_profiler.RecordAlloc("Render.GameTexture", static_cast<size_t>(1280 * 720 * 4));
                g_memory_profiler.RecordAlloc("UI.ImGui", static_cast<size_t>(256 * 1024));

                g_render_profiler.RecordSpriteBatch(std::max(sprite_count, 0));
                g_render_profiler.RecordDrawCall(6, 2);
                g_render_profiler.RecordTextureBind();
                g_render_profiler.RecordShaderSwitch();
                g_render_profiler.SetTextureMemory(static_cast<size_t>(1280 * 720 * 4 * 2));
            }

            // Start the Dear ImGui frame
            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "ImGuiFrame");
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                ImGuizmo::BeginFrame();
            }

            // Draw Editor UI
            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "DrawEditorUI");
                DrawEditorUI(engine_instance, scene_texture, game_texture);
            }

            // Update window title with scene name and editor state
            {
                std::string title = "DSEngine Editor - " + dse::editor::GetCurrentScenePath();
                if (GetEditorState() == EditorState::Play) {
                    title += " [PLAYING]";
                } else if (GetEditorState() == EditorState::Pause) {
                    title += " [PAUSED]";
                }
                glfwSetWindowTitle(window, title.c_str());
            }

            // Rendering
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "ImGuiRender");
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }

            // Update and Render additional Platform Windows
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                GLFWwindow* backup_current_context = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(backup_current_context);
            }

            glfwSwapBuffers(window);

            g_render_profiler.EndFrame();
            g_cpu_profiler.EndFrame();
        }

        // Save editor settings before shutdown
        editor_settings.last_scene_path = dse::editor::GetCurrentScenePath();
        editor_settings.default_gizmo_operation = g_current_gizmo_operation;
        editor_settings.default_gizmo_mode = g_current_gizmo_mode;
        dse::editor::AddRecentFile(editor_settings, dse::editor::GetCurrentScenePath());
        dse::editor::SaveEditorSettings(editor_settings);

        engine_instance.Shutdown();
    }

    // Cleanup
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::DestroyPlatformWindows();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
