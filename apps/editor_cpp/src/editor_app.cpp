#include "editor_app.h"

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
#include "engine/base/time.h"
#include "engine/input/input.h"
#include "engine/scripting/lua/lua_runtime.h"
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
#include "editor_context.h"
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
#include "editor_audio_panel.h"
#include "editor_scene_tabs.h"
#include "editor_lua_console.h"
#include "editor_build_game.h"

extern std::vector<std::string> g_editor_languages;
extern int g_editor_language_index;
extern int g_current_gizmo_operation;
extern int g_current_gizmo_mode;

// 全局 profiler 实例（供其他面板通过 extern 访问）
dse::profiler::CPUProfiler g_cpu_profiler;
dse::profiler::MemoryProfiler g_memory_profiler;
dse::profiler::RenderProfiler g_render_profiler;

namespace {

constexpr int kProfilerHistoryMaxSamples = 180;

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
        if (!camera.enabled) continue;
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
        if (!camera.enabled) continue;
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

void ExportTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::trunc);
    if (ofs.is_open()) {
        ofs << content;
    }
}

// UI Layout Inspector (passed as callback to DrawInspectorPanel)
void DrawUILayoutInspector(entt::registry& registry, entt::entity entity) {
    const bool read_only = dse::editor::GetEditorState() == dse::editor::EditorState::Play;

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

    dse::editor::DrawAudioSection(registry, entity);
}

} // anonymous namespace

namespace dse::editor {

// ─── Static helpers ─────────────────────────────────────────────────────────

std::filesystem::path EditorApp::GetProjectRootPath() {
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

std::filesystem::path EditorApp::GetEditorBinPath() {
    const std::filesystem::path path = GetProjectRootPath() / "bin";
    std::filesystem::create_directories(path);
    return path;
}

// ─── Init ───────────────────────────────────────────────────────────────────

bool EditorApp::Init(int argc, char* argv[]) {
    test_config_ = dse::editor::test::ParseEditorTestArgs(argc, argv);
    const bool headless = test_config_.headless;
    frames_remaining_ = headless ? test_config_.max_frames : -1;

#if defined(_WIN32)
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    std::cerr << "[Editor] Starting..." << (headless ? " (headless)" : "") << std::endl;

    if (!glfwInit()) {
        std::cerr << "[Editor] glfwInit() failed!" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (headless) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    window_ = glfwCreateWindow(1280, 720, "DSEngine Editor", NULL, NULL);
    if (!window_) {
        std::cerr << "Failed to create GLFW window in Editor." << std::endl;
        glfwTerminate();
        return false;
    }

    std::cout << "Editor window created successfully." << std::endl;

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL (gladLoadGL) in Editor." << std::endl;
        glfwDestroyWindow(window_);
        glfwTerminate();
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // NOTE: Multi-viewport disabled due to CRT heap assertions on Windows.
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    static std::string imgui_ini_path = (GetEditorBinPath() / "editor_layout.ini").string();
    io.IniFilename = imgui_ini_path.c_str();

    ImGui::StyleColorsDark();
    dse::editor::SetupEditorStyle();

    {
        std::filesystem::path fonts_dir = GetProjectRootPath() / "apps" / "editor_cpp" / "fonts";
        dse::editor::LoadEditorFonts(fonts_dir);
    }

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize DSEngine
    dse::runtime::EngineRunConfig engine_config;
    engine_config.window_width = 1280;
    engine_config.window_height = 720;
    engine_config.window_title = "DSEngine Editor";
    engine_config.business_mode = BusinessMode::Lua;
    engine_config.enable_editor = true;
    engine_config.startup_lua_script_path = "samples/lua/main.lua";

    if (std::getenv("DSE_DATA_ROOT") == nullptr) {
#if defined(_WIN32)
        const std::string data_root = (GetProjectRootPath() / "samples" / "lua" / "data").string();
        _putenv_s("DSE_DATA_ROOT", data_root.c_str());
#else
        const std::string data_root = (GetProjectRootPath() / "samples" / "lua" / "data").string();
        setenv("DSE_DATA_ROOT", data_root.c_str(), 1);
#endif
    }

    engine_instance_ = new dse::runtime::EngineInstance(engine_config);
    if (!engine_instance_->Init()) {
        delete engine_instance_;
        engine_instance_ = nullptr;
        return false;
    }

    EnsureEditorLocalizationData();
    dse::editor::InstallEditorLogSink();

    // Load editor settings and restore state
    dse::editor::EditorSettings editor_settings = dse::editor::LoadEditorSettings();
    g_current_gizmo_operation = editor_settings.default_gizmo_operation;
    g_current_gizmo_mode = editor_settings.default_gizmo_mode;

    if (!test_config_.scene_path.empty()) {
        editor_settings.last_scene_path = test_config_.scene_path;
    }

    dse::editor::SceneTabManager::Get().Init(
        editor_settings.last_scene_path.empty() ? "Untitled" : editor_settings.last_scene_path);

    if (!editor_settings.last_scene_path.empty() && editor_settings.last_scene_path != "Untitled") {
        if (std::filesystem::exists(editor_settings.last_scene_path)) {
            World& world = engine_instance_->pipeline()->world();
            LoadScene(world.registry(), editor_settings.last_scene_path);
            dse::editor::SetCurrentScenePath(editor_settings.last_scene_path);
        }
    }

    // 启动 Control Server (WebSocket JSON-RPC)
    control_server_ = std::make_unique<ControlServer>();
    RegisterBuiltinTools(*control_server_);
    if (!control_server_->Start(9527)) {
        std::cerr << "[Editor] Warning: Control Server failed to start" << std::endl;
    }

    // 扫描插件目录
    plugin_manager_.ScanPlugins(GetProjectRootPath() / "plugins");

    std::cout << "Engine initialized successfully. Entering main loop..." << std::endl;
    return true;
}

// ─── Run ────────────────────────────────────────────────────────────────────

void EditorApp::Run() {
    ImGuiIO& io = ImGui::GetIO();

    while (!glfwWindowShouldClose(window_) && frames_remaining_ != 0) {
        if (frames_remaining_ > 0) --frames_remaining_;
        g_cpu_profiler.BeginFrame();
        g_render_profiler.BeginFrame();
        g_memory_profiler.Reset();

        glfwPollEvents();

        // Process Control Server requests (JSON-RPC)
        if (control_server_ && control_server_->IsRunning()) {
            control_server_->Poll(*engine_instance_);
        }

        // Poll plugin process status
        plugin_manager_.PollStatus();

        // Update editor camera (Edit mode only)
        if (dse::editor::GetEditorState() == dse::editor::EditorState::Edit) {
            int fb_w, fb_h;
            glfwGetFramebufferSize(window_, &fb_w, &fb_h);
            float aspect = (fb_h > 0) ? static_cast<float>(fb_w) / static_cast<float>(fb_h) : 1.7777f;
            auto& editor_cam = dse::editor::GetEditorCamera();
            engine_instance_->pipeline()->SetEditorCamera(
                editor_cam.GetViewMatrix(),
                editor_cam.GetProjectionMatrix(aspect));
        } else {
            engine_instance_->pipeline()->DisableEditorCamera();
        }

        // Tick Engine
        {
            dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "EngineTick");
            if (dse::editor::GetEditorState() == dse::editor::EditorState::Edit) {
                Time::Update();
                dse::runtime::PumpLuaScriptHotReloads();
                engine_instance_->pipeline()->Render();
                Input::Update();
            } else {
                engine_instance_->Tick();
            }
        }

        unsigned int scene_texture = engine_instance_->pipeline()->GetSceneTextureId();
        unsigned int game_texture = engine_instance_->pipeline()->GetMainTextureId();

        // Metrics
        {
            dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "EditorMetrics");
            World& profiler_world = engine_instance_->pipeline()->world();
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

        // ImGui frame
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
            DrawEditorUI(scene_texture, game_texture);
        }

        // Update window title
        {
            auto& tab_mgr = dse::editor::SceneTabManager::Get();
            const std::string scene_name = tab_mgr.GetActiveDisplayName();
            const int tab_count = tab_mgr.GetTabCount();
            std::string title = "DSEngine Editor - " + scene_name;
            if (tab_mgr.GetActiveTab().dirty) title += " *";
            if (tab_count > 1) title += " [" + std::to_string(tab_mgr.GetActiveIndex() + 1) + "/" + std::to_string(tab_count) + "]";
            if (dse::editor::GetEditorState() == dse::editor::EditorState::Play) {
                title += " [PLAYING]";
            } else if (dse::editor::GetEditorState() == dse::editor::EditorState::Pause) {
                title += " [PAUSED]";
            }
            glfwSetWindowTitle(window_, title.c_str());
        }

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        {
            dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "ImGuiRender");
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window_);

        g_render_profiler.EndFrame();
        g_cpu_profiler.EndFrame();
    }
}

// ─── Shutdown ───────────────────────────────────────────────────────────────

void EditorApp::Shutdown() {
    // Save editor settings
    dse::editor::EditorSettings editor_settings = dse::editor::LoadEditorSettings();
    editor_settings.last_scene_path = dse::editor::SceneTabManager::Get().GetActiveFilePath();
    if (editor_settings.last_scene_path.empty()) editor_settings.last_scene_path = dse::editor::GetCurrentScenePath();
    editor_settings.default_gizmo_operation = g_current_gizmo_operation;
    editor_settings.default_gizmo_mode = g_current_gizmo_mode;
    dse::editor::AddRecentFile(editor_settings, dse::editor::GetCurrentScenePath());
    dse::editor::SaveEditorSettings(editor_settings);

    // 停止所有插件
    plugin_manager_.StopAll();

    // 停止 Control Server
    if (control_server_) {
        control_server_->Stop();
        control_server_.reset();
    }

    if (engine_instance_) {
        engine_instance_->Shutdown();
        delete engine_instance_;
        engine_instance_ = nullptr;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::DestroyPlatformWindows();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

// ─── DrawEditorUI ───────────────────────────────────────────────────────────

void EditorApp::DrawEditorUI(unsigned int scene_texture, unsigned int game_texture) {
    World& world = engine_instance_->pipeline()->world();
    auto& registry = world.registry();
    const bool is_play = (dse::editor::GetEditorState() == dse::editor::EditorState::Play);

    if (static_cast<int>(last_editor_state_) != static_cast<int>(dse::editor::GetEditorState()) && dse::editor::GetEditorState() == dse::editor::EditorState::Edit) {
        selected_entity_ = entt::null;
        inspector_active_ = true;
        inspector_static_ = false;
        sprite_flip_x_ = false;
        sprite_flip_y_ = false;
        collider_is_trigger_ = false;
        std::strncpy(localization_preview_key_, "editor.preview.status", sizeof(localization_preview_key_) - 1);
        localization_preview_key_[sizeof(localization_preview_key_) - 1] = '\0';
        std::strncpy(localization_preview_fallback_, "Language: {lang}", sizeof(localization_preview_fallback_) - 1);
        localization_preview_fallback_[sizeof(localization_preview_fallback_) - 1] = '\0';
    }
    last_editor_state_ = static_cast<int>(dse::editor::GetEditorState());

    // ─── 统一上下文 ─────────────────────────────────────────────────────────
    dse::editor::EditorContext ctx{
        *engine_instance_, world, registry, selected_entity_,
        is_play, is_2d_,
        g_cpu_profiler, g_memory_profiler, g_render_profiler,
        inspector_active_, inspector_static_,
        g_current_gizmo_operation, g_current_gizmo_mode
    };

    dse::editor::BeginEditorShell();
    dse::editor::DrawEditorMainMenu(ctx, &show_preferences_, &show_plugins_panel_);

    if (!is_play) {
        dse::editor::DrawSceneTabBar(ctx);
    }

    dse::editor::ProcessShortcuts(ctx);

    dse::editor::DrawEditorToolbar(ctx);

    dse::editor::DrawHierarchyPanel(ctx);

    dse::editor::DrawInspectorPanel(ctx, DrawUILayoutInspector);

    dse::editor::DrawProjectPanel();
    dse::editor::DrawConsolePanel();

    dse::editor::DrawLocalizationPreviewPanel(ctx,
        localization_preview_key_, sizeof(localization_preview_key_),
        localization_preview_fallback_, sizeof(localization_preview_fallback_));

    dse::editor::DrawProfilerPanel(ctx);
    dse::editor::DrawAnimationPanel(ctx);
    dse::editor::DrawMaterialPanel(ctx);
    dse::editor::DrawTilePalettePanel(ctx);
    dse::editor::DrawTerrainEditorPanel(ctx);
    dse::editor::DrawLuaConsolePanel();
    dse::editor::DrawBuildGameDialog();

    dse::editor::DrawPreferencesPanel(&show_preferences_);

    // Plugin Manager 面板
    if (show_plugins_panel_) {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Plugins", &show_plugins_panel_)) {
            dse::editor::DrawPluginManagerPanel(plugin_manager_);
        }
        ImGui::End();
    }

    dse::editor::DrawSceneViewportPanel(ctx, scene_texture, BuildActiveCameraMatrices);
    dse::editor::DrawGameViewportPanel(game_texture);

    dse::editor::DrawStatusBar(ctx);

    dse::editor::EndEditorShell();
}

// ─── EnsureEditorLocalizationData ───────────────────────────────────────────

void EditorApp::EnsureEditorLocalizationData() {
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

} // namespace dse::editor
