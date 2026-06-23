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
#include "engine/assets/asset_manager.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/transform.h"
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

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
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
#include "editor_selection.h"
#include "editor_preferences_panel.h"
#include "editor_terrain_panel.h"
#include "editor_audio_panel.h"
#include "editor_scene_tabs.h"
#include "editor_lua_console.h"
#include "editor_lua_debugger.h"
#include "editor_build_game.h"
#include "editor_project.h"
#include "editor_project_hub.h"
#include "editor_undo_panel.h"
#include "editor_asset_importer.h"
#include "editor_os_drop.h"
#include "editor_asset_db.h"
#include "editor_autosave.h"
#include "editor_plugin_api.h"
#include "editor_locale.h"
#include "editor_snapshot.h"
#include "editor_selection_outline.h"
#include "editor_physics_debug.h"
#include "editor_gpu.h"
#include "editor_asset_browser.h"
#include "editor_prefab_override.h"
#include "editor_animation_timeline.h"
#include "editor_navmesh_panel.h"
#include "editor_lighting_gizmos.h"
#include "editor_shader_graph.h"
#include "editor_multi_viewport.h"
#include "editor_scene_view_mode.h"
#include "editor_anim_state_machine.h"
#include "editor_ai_config.h"
#include "editor_streaming_panel.h"
#include "editor_curve_editor.h"
#include "editor_visual_script.h"
#include "editor_anim_retarget.h"
#include "editor_crash.h"



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
    // 处理多级路径：bin/RelWithDebInfo/、bin/Debug/ 等 CMake 配置子目录
    const auto leaf = path.filename().string();
    if (leaf == "Debug" || leaf == "Release" || leaf == "RelWithDebInfo" || leaf == "MinSizeRel") {
        path = path.parent_path(); // bin/RelWithDebInfo → bin
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
    // 常驻 RPC 模式（--automation-api）不按帧数自退，置 -1 表示无限运行，直到收到 quit。
    frames_remaining_ = (headless && !test_config_.automation_api)
                            ? test_config_.max_frames : -1;

    // 尽早安装崩溃处理器，覆盖 glfwInit/ImGui/字体加载等引擎 Init 之前的早期阶段。
    dse::editor::InstallEditorCrashHandler();
    dse::editor::SetEditorCrashMetadata("phase", "startup");
    dse::editor::AddEditorBreadcrumb("editor: init start");

#if defined(_WIN32)
    if (headless) {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    std::cerr << "[Editor] Starting..." << (headless ? " (headless)" : "") << std::endl;

    // 启动 splash：进程一启动即弹出 logo，盖住窗口创建→首帧之间的空白期，带淡入淡出。
    // 自动化/截图/无头模式与 DSE_SPLASH=0 时关闭。
    const bool splash_on = !headless && test_config_.screenshot_path.empty() &&
        frames_remaining_ < 0 &&
        !(std::getenv("DSE_SPLASH") && std::string(std::getenv("DSE_SPLASH")) == "0");
    if (splash_on) {
        dse::platform::SplashConfig splash_cfg;
        splash_cfg.image_path = (GetProjectRootPath() / "data" / "icon" / "dse_icon.png").string();
        splash_cfg.app_name = "DSEngine Editor";
        splash_cfg.initial_status = "正在启动编辑器…";
        dse::platform::ApplySplashEnvOverrides(splash_cfg);
        splash_.Show(splash_cfg);
    }

    if (!glfwInit()) {
        std::cerr << "[Editor] glfwInit() failed!" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (headless) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    } else {
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
        // splash 期间隐藏主窗口，首帧后再 Show（仍以 MAXIMIZED 状态出现）。
        if (splash_on) {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            deferred_window_show_ = true;
        }
    }

    window_ = glfwCreateWindow(1280, 720, "DSEngine Editor", NULL, NULL);
    if (!window_) {
        std::cerr << "Failed to create GLFW window in Editor." << std::endl;
        splash_.Finish();
        glfwTerminate();
        return false;
    }

    std::cout << "Editor window created successfully." << std::endl;

    // 设置窗口图标
    {
        std::filesystem::path icon_path = GetProjectRootPath() / "data" / "icon" / "dse_icon.png";
        int iw, ih, ic;
        unsigned char* icon_pixels = stbi_load(icon_path.string().c_str(), &iw, &ih, &ic, 4);
        if (icon_pixels) {
            GLFWimage icon_image{ iw, ih, icon_pixels };
            glfwSetWindowIcon(window_, 1, &icon_image);
            stbi_image_free(icon_pixels);
        }
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(0); // Editor: no VSync cap, let GPU run freely

    // Drag & Drop: accept file drops from OS — dispatch to panels via OsDropEvent
    glfwSetDropCallback(window_, [](GLFWwindow* win, int count, const char** paths) {
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);
        std::vector<std::string> file_paths;
        file_paths.reserve(count);
        for (int i = 0; i < count; ++i) {
            file_paths.emplace_back(paths[i]);
        }
        dse::editor::PushOsDropEvent(file_paths, static_cast<float>(mx), static_cast<float>(my));
    });

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL (gladLoadGL) in Editor." << std::endl;
        splash_.Finish();
        glfwDestroyWindow(window_);
        glfwTerminate();
        return false;
    }

    // Layout version check: if the stored version doesn't match, delete the old ini
    // so BuildDefaultDockLayout rebuilds from scratch on next launch.
    {
        static constexpr int kLayoutVersion = 4;
        const auto ini_path = GetEditorBinPath() / "editor_layout.ini";
        const auto ver_path = GetEditorBinPath() / "editor_layout.ver";
        bool needs_reset = true;
        if (std::filesystem::exists(ver_path)) {
            std::ifstream vf(ver_path);
            int stored_ver = 0;
            vf >> stored_ver;
            needs_reset = (stored_ver != kLayoutVersion);
        }
        if (needs_reset) {
            std::filesystem::remove(ini_path);
            std::ofstream vf(ver_path);
            vf << kLayoutVersion;
        }
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    static std::string imgui_ini_path = (GetEditorBinPath() / "editor_layout.ini").string();
    io.IniFilename = imgui_ini_path.c_str();

    ImGui::StyleColorsDark();
    dse::editor::SetupEditorStyle();
    dse::editor::InitPreferencesFromSettings();

    {
        namespace fs = std::filesystem;
        // Resolve editor fonts: prefer fonts shipped next to the executable
        // (release package / installed layout), fall back to the source tree.
        fs::path fonts_dir;
#if defined(_WIN32)
        std::wstring exe_path(MAX_PATH, L'\0');
        const DWORD exe_len = GetModuleFileNameW(nullptr, exe_path.data(),
                                                 static_cast<DWORD>(exe_path.size()));
        if (exe_len > 0) {
            exe_path.resize(exe_len);
            const fs::path exe_fonts = fs::path(exe_path).parent_path() / "fonts";
            if (fs::exists(exe_fonts)) fonts_dir = exe_fonts;
        }
#endif
        if (fonts_dir.empty() && fs::exists(GetEditorBinPath() / "fonts"))
            fonts_dir = GetEditorBinPath() / "fonts";
        if (fonts_dir.empty() && fs::exists(GetProjectRootPath() / "fonts"))
            fonts_dir = GetProjectRootPath() / "fonts";
        if (fonts_dir.empty())
            fonts_dir = GetProjectRootPath() / "apps" / "editor_cpp" / "fonts";
        dse::editor::LoadEditorFonts(fonts_dir);
    }

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    splash_.SetStatus("正在初始化渲染引擎…");

    // Initialize DSEngine
    dse::runtime::EngineRunConfig engine_config;
    int fb_w = 1280, fb_h = 720;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    engine_config.window_width = fb_w;
    engine_config.window_height = fb_h;
    engine_config.window_title = "DSEngine Editor";
    engine_config.business_mode = BusinessMode::Lua;
    engine_config.enable_editor = true;
    // Editor mode: no Lua startup script needed; engine will skip bootstrap gracefully.

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
        splash_.Finish();
        return false;
    }
    splash_.SetStatus("正在加载工程与场景…");

    // 把引擎 RHI 设备注入编辑器 GPU 接入层：编辑器自建的零散 GPU 资源（资产缩略图等）
    // 经此走引擎 RHI 抽象，不再直接调用 OpenGL。
    dse::editor::SetEditorRhiDevice(engine_instance_->pipeline()->GetRhiDevice());

    // 引擎 Init 内部以 app_name "DSEngine" 重新安装并重置了面包屑；此处重申编辑器身份，
    // 之后再补充编辑器专属面包屑/元数据，确保崩溃报告标记为编辑器进程且带编辑器上下文。
    dse::editor::InstallEditorCrashHandler();
    dse::editor::SetEditorCrashMetadata("phase", "running");
    dse::editor::AddEditorBreadcrumb("editor: engine initialized");

    EnsureEditorLocalizationData();
    dse::editor::InstallEditorLogSink();

    // Load editor settings and restore state
    dse::editor::EditorSettings editor_settings = dse::editor::LoadEditorSettings();
    current_gizmo_operation_ = editor_settings.default_gizmo_operation;
    current_gizmo_mode_ = editor_settings.default_gizmo_mode;
    dse::editor::SetEditorLocale(editor_settings.editor_ui_locale);

    // Load AI configuration
    dse::editor::AIConfigManager::Instance().Load("bin/editor_ai_config.json");

    // 尝试自动打开上次项目
    if (!editor_settings.last_project_path.empty()) {
        std::filesystem::path dseproj = std::filesystem::path(editor_settings.last_project_path) / "project.dseproj";
        if (std::filesystem::exists(dseproj)) {
            dse::editor::ProjectManager::Get().OpenProject(dseproj);
        }
    }
    // 项目打开后同步 data root 到 AssetManager
    if (dse::editor::ProjectManager::Get().HasOpenProject()) {
        dse::editor::ProjectManager::Get().ApplyDataRoot();
        engine_instance_->asset_manager()->ConfigureDataRoot(
            dse::editor::ProjectManager::Get().GetAssetDir().string());
        dse::editor::AssetDatabase::Get().Refresh();
        // 启动文件监听，支持资源热重载
        engine_instance_->asset_manager()->StartFileWatcher();
    }

    // Restore scene camera
    {
        auto& cam = dse::editor::GetEditorCamera();
        cam.focal_point = glm::vec3(editor_settings.cam_focal_x, editor_settings.cam_focal_y, editor_settings.cam_focal_z);
        cam.distance = editor_settings.cam_distance;
        cam.yaw = editor_settings.cam_yaw;
        cam.pitch = editor_settings.cam_pitch;
    }

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
            dse::editor::SetEditorCrashMetadata("scene", editor_settings.last_scene_path);
            dse::editor::AddEditorBreadcrumb("editor: loaded scene " + editor_settings.last_scene_path);
        }
    }

    // 启动 Control Server (WebSocket JSON-RPC)
    control_server_ = std::make_unique<ControlServer>();
    RegisterBuiltinTools(*control_server_);
    if (!control_server_->Start(test_config_.api_port)) {
        std::cerr << "[Editor] Warning: Control Server failed to start" << std::endl;
    }

    // 初始化 AI Chat Panel bridge 路径 + 历史记录路径
    chat_panel_.SetBridgePath(
        (GetProjectRootPath() / "tools" / "ai_chat_bridge.py").string());
    chat_panel_.LoadHistory("bin/ai_chat_history.json");

    // 注册 @mention 上下文解析器
    chat_panel_.SetMentionResolver([this](const std::string& token) -> std::string {
        if (!engine_instance_ || !engine_instance_->pipeline()) return {};
        entt::registry& registry = engine_instance_->pipeline()->world().registry();

        // @scene — 当前场景基本信息
        if (token == "@scene") {
            const std::string scene_name = dse::editor::SceneTabManager::Get().GetActiveDisplayName();
            int entity_count = 0;
            for (auto e : registry.storage<entt::entity>())
                if (registry.valid(e)) ++entity_count;
            return "[场景上下文]\n场景名: " + scene_name + "\n实体数量: " + std::to_string(entity_count) + "\n";
        }

        // @entity / @selection — 当前选中实体详情
        if (token == "@entity" || token == "@selection") {
            if (selected_entity_ == entt::null || !registry.valid(selected_entity_))
                return "[选中实体]\n（无选中实体）\n";

            std::string name = "Entity " + std::to_string(static_cast<uint32_t>(selected_entity_));
            if (registry.all_of<dse::editor::EditorNameComponent>(selected_entity_))
                name = registry.get<dse::editor::EditorNameComponent>(selected_entity_).name;

            std::string info = "[选中实体]\n名称: " + name + "\n组件:\n";
            if (registry.all_of<TransformComponent>(selected_entity_)) {
                const auto& t = registry.get<TransformComponent>(selected_entity_);
                char buf[128];
                std::snprintf(buf, sizeof(buf), "  Transform: pos=(%.2f,%.2f,%.2f) scale=(%.2f,%.2f,%.2f)\n",
                    t.position.x, t.position.y, t.position.z, t.scale.x, t.scale.y, t.scale.z);
                info += buf;
            }
            if (registry.all_of<dse::MeshRendererComponent>(selected_entity_)) {
                const auto& m = registry.get<dse::MeshRendererComponent>(selected_entity_);
                info += "  MeshRenderer: mesh=" + m.mesh_path + "\n";
            }
            if (registry.all_of<dse::DirectionalLight3DComponent>(selected_entity_)) info += "  DirectionalLight3D\n";
            if (registry.all_of<dse::PointLightComponent>(selected_entity_))         info += "  PointLight\n";
            if (registry.all_of<dse::SpotLightComponent>(selected_entity_))          info += "  SpotLight\n";
            if (registry.all_of<dse::Camera3DComponent>(selected_entity_))           info += "  Camera3D\n";
            return info;
        }

        // @script:relative/path — 脚本文件内容（最多 200 行）
        if (token.rfind("@script:", 0) == 0) {
            std::string rel_path = token.substr(8);
            std::filesystem::path project_root = GetProjectRootPath();
            std::filesystem::path full = project_root / rel_path;
            if (!std::filesystem::exists(full))
                full = project_root / "samples" / "lua" / rel_path;
            // Fix E: 路径穿越防护 — canonical 路径必须以 project_root 开头
            std::error_code ec;
            auto canonical = std::filesystem::weakly_canonical(full, ec);
            auto root_canonical = std::filesystem::weakly_canonical(project_root, ec);
            auto canonical_str = canonical.string();
            auto root_str = root_canonical.string();
            if (canonical_str.rfind(root_str, 0) != 0)
                return "[脚本: " + rel_path + "]\n（路径不在项目目录内）\n";
            std::ifstream f(canonical);
            if (!f) return "[脚本: " + rel_path + "]\n（文件不存在）\n";
            std::string content, line;
            int lines = 0;
            while (std::getline(f, line) && lines < 200) { content += line + "\n"; ++lines; }
            return "[脚本: " + rel_path + "]\n```lua\n" + content + "```\n";
        }

        return {}; // 未识别的 token
    });

    // 扫描插件目录
    plugin_manager_.ScanPlugins(GetProjectRootPath() / "plugins");

    std::cout << "Engine initialized successfully. Entering main loop..." << std::endl;
    return true;
}

// ─── Run ────────────────────────────────────────────────────────────────────

void EditorApp::Run() {
    ImGuiIO& io = ImGui::GetIO();
    int frame_counter = 0;
    bool screenshot_taken = false;

    dse::editor::AutoSaveManager::Get().CheckRecovery();

    // 上次会话若异常退出（存在崩溃报告），与 AutoSave 恢复一并提示，并把路径写入控制台日志。
    if (const std::string prev = dse::editor::GetPreviousSessionCrashReport(); !prev.empty()) {
        dse::editor::EditorLog(dse::editor::LogLevel::Warning,
            "上次会话异常退出，崩溃报告: " + prev);
        dse::editor::AddEditorBreadcrumb("editor: previous session crash report found");
    }
    dse::editor::AddEditorBreadcrumb("editor: entering main loop");

    if (!test_config_.replay_path.empty()) {
        dse::editor::EditorLog(dse::editor::LogLevel::Warning,
            "--replay is not yet implemented, path ignored: " + test_config_.replay_path);
        test_config_.replay_path.clear();
    }

    while (!glfwWindowShouldClose(window_) && !dse::editor::IsExitRequested() && frames_remaining_ != 0) {
        if (frames_remaining_ > 0) --frames_remaining_;
        ++frame_counter;
        cpu_profiler_.BeginFrame();
        render_profiler_.BeginFrame();
        memory_profiler_.Reset();

        glfwPollEvents();

        // Process Control Server requests (JSON-RPC)
        if (control_server_ && control_server_->IsRunning()) {
            control_server_->Poll(*engine_instance_);
        }

        // Poll plugin process status
        plugin_manager_.PollStatus();

        // Update editor camera (Edit mode only)
        if (dse::editor::GetEditorState() == dse::editor::EditorState::Edit) {
            float aspect = dse::editor::GetCachedSceneViewportAspect();
            auto& editor_cam = dse::editor::GetEditorCamera();
            engine_instance_->pipeline()->SetEditorCamera(
                editor_cam.GetViewMatrix(),
                editor_cam.GetProjectionMatrix(aspect));
            // 同步编辑器场景背景色（light / dark 主题）
            if (dse::editor::GetCurrentThemeIndex() == 1) {
                engine_instance_->pipeline()->SetEditorBgColor(glm::vec4(0.78f, 0.78f, 0.82f, 1.0f));
            } else {
                engine_instance_->pipeline()->SetEditorBgColor(glm::vec4(0.17f, 0.17f, 0.21f, 1.0f));
            }
        } else {
            engine_instance_->pipeline()->DisableEditorCamera();
        }

        // Set scene view mode for rendering pipeline
        engine_instance_->pipeline()->SetSceneViewMode(
            static_cast<int>(dse::editor::GetCurrentSceneViewMode()));

        // Tick Engine
        {
            dse::profiler::ScopedCPUProfile scope(cpu_profiler_, "EngineTick");
            const auto editor_state = dse::editor::GetEditorState();
            if (editor_state == dse::editor::EditorState::Edit) {
                Time::Update();
                dse::runtime::PumpLuaScriptHotReloads();
                if (engine_instance_->asset_manager()->PumpHotReloads() > 0) {
                    dse::editor::InvalidateThumbnailCache();
                }
                engine_instance_->pipeline()->Render();
                Input::Update();
            } else if (editor_state == dse::editor::EditorState::Pause) {
                // Paused: step one frame if requested, then render-only to keep view alive
                if (dse::editor::ConsumeStepFrame()) {
                    engine_instance_->Tick();
                } else {
                    engine_instance_->pipeline()->Render();
                }
            } else {
                engine_instance_->Tick();
            }
        }

        unsigned int scene_texture = engine_instance_->pipeline()->GetSceneTextureId();
        unsigned int game_texture = engine_instance_->pipeline()->GetMainTextureId();

        // Metrics
        {
            dse::profiler::ScopedCPUProfile scope(cpu_profiler_, "EditorMetrics");
            World& profiler_world = engine_instance_->pipeline()->world();
            auto& profiler_registry = profiler_world.registry();
            const int entity_count = static_cast<int>(profiler_registry.storage<entt::entity>().size());
            const ImGuiIO& imgui_io = ImGui::GetIO();
            const int render_width = static_cast<int>(imgui_io.DisplaySize.x);
            const int render_height = static_cast<int>(imgui_io.DisplaySize.y);
            const size_t render_target_bytes =
                static_cast<size_t>(render_width > 0 ? render_width : 1280) *
                static_cast<size_t>(render_height > 0 ? render_height : 720) * 4u;
            const size_t imgui_buffer_bytes =
                static_cast<size_t>(std::max(imgui_io.MetricsRenderIndices, 0)) * sizeof(ImDrawIdx) + 256u * 1024u;

            memory_profiler_.RecordAlloc("World.Entities", static_cast<size_t>(std::max(entity_count, 0)) * sizeof(entt::entity));
            memory_profiler_.RecordAlloc("Render.SceneTexture", render_target_bytes);
            memory_profiler_.RecordAlloc("Render.GameTexture", render_target_bytes);
            memory_profiler_.RecordAlloc("UI.ImGui", imgui_buffer_bytes);

            {
                const auto rhi_stats = engine_instance_->pipeline()->GetRhiFrameStats();
                render_profiler_.UpdateFromRhi(
                    rhi_stats.draw_calls,
                    0,  // vertex_count — RHI 未单独统计
                    rhi_stats.triangle_count,
                    rhi_stats.sprite_count,
                    rhi_stats.texture_binds,
                    rhi_stats.shader_switches);
            }
            render_profiler_.SetTextureMemory(render_target_bytes * 2u);
        }

        // ImGui frame
        {
            dse::profiler::ScopedCPUProfile scope(cpu_profiler_, "ImGuiFrame");
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();
        }

        // Draw Editor UI
        {
            dse::profiler::ScopedCPUProfile scope(cpu_profiler_, "DrawEditorUI");
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
        // 根据主题设置 GL 清屏色，避免 light 模式下面板缝隙处显示黑色
        if (dse::editor::GetCurrentThemeIndex() == 1) {
            glClearColor(0.94f, 0.94f, 0.96f, 1.0f);  // 浅色主题
        } else {
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);  // 深色主题
        }
        glClear(GL_COLOR_BUFFER_BIT);

        {
            dse::profiler::ScopedCPUProfile scope(cpu_profiler_, "ImGuiRender");
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

        // 首帧已上屏：显示主窗口并淡出 splash（满足最短显示时长后才真正消失）。
        if (!first_frame_shown_) {
            first_frame_shown_ = true;
            if (deferred_window_show_) glfwShowWindow(window_);
            splash_.Finish();
        }

        // Auto-screenshot: 在指定帧或退出前最后一帧截图
        if (!test_config_.screenshot_path.empty() && !screenshot_taken) {
            bool should_capture = false;
            if (test_config_.screenshot_frame >= 0 && frame_counter >= test_config_.screenshot_frame) {
                should_capture = true;
            } else if (test_config_.screenshot_frame < 0 && frames_remaining_ == 0) {
                should_capture = true;
            }
            if (should_capture) {
                auto readback = engine_instance_->pipeline()->ReadSceneColorRgba8WithSize();
                if (!readback.pixels.empty() && readback.width > 0 && readback.height > 0) {
                    if (engine_instance_->pipeline()->NeedsReadbackYFlip()) {
                        const int stride = readback.width * 4;
                        std::vector<unsigned char> row(stride);
                        for (int y = 0; y < readback.height / 2; ++y) {
                            unsigned char* top = readback.pixels.data() + y * stride;
                            unsigned char* bot = readback.pixels.data() + (readback.height - 1 - y) * stride;
                            std::memcpy(row.data(), top, stride);
                            std::memcpy(top, bot, stride);
                            std::memcpy(bot, row.data(), stride);
                        }
                    }
                    std::filesystem::path ss_path(test_config_.screenshot_path);
                    ss_path.parent_path().empty() || std::filesystem::create_directories(ss_path.parent_path());
                    stbi_write_png(test_config_.screenshot_path.c_str(),
                                   readback.width, readback.height, 4,
                                   readback.pixels.data(), readback.width * 4);
                    std::cout << "[EditorApp] Screenshot saved: " << test_config_.screenshot_path
                              << " (" << readback.width << "x" << readback.height << ")" << std::endl;
                } else {
                    std::cerr << "[EditorApp] Screenshot FAILED: no framebuffer data" << std::endl;
                }
                screenshot_taken = true;
            }
        }

        // Test harness verify: export snapshot and compare against expected on last frame
        if (!test_config_.verify_path.empty() && frames_remaining_ == 0) {
            auto& verify_world = engine_instance_->pipeline()->world();
            const std::string actual_json = dse::editor::test::ExportRegistrySnapshot(verify_world.registry());

            std::string expected_json;
            {
                std::ifstream f(test_config_.verify_path);
                if (f.is_open()) {
                    expected_json.assign(std::istreambuf_iterator<char>(f),
                                        std::istreambuf_iterator<char>());
                }
            }

            if (expected_json.empty()) {
                // No expected file found – write actual as the new baseline
                {
                    auto parent = std::filesystem::path(test_config_.verify_path).parent_path();
                    if (!parent.empty()) std::filesystem::create_directories(parent);
                }
                std::ofstream f(test_config_.verify_path);
                f << actual_json;
                std::cout << "[EditorApp][Verify] Baseline written: " << test_config_.verify_path << std::endl;
            } else {
                const auto diffs = dse::editor::test::CompareSnapshot(actual_json, expected_json);
                if (diffs.empty()) {
                    std::cout << "[EditorApp][Verify] PASSED: " << test_config_.verify_path << std::endl;
                } else {
                    std::cerr << "[EditorApp][Verify] FAILED: " << diffs.size() << " difference(s) vs "
                              << test_config_.verify_path << ":" << std::endl;
                    for (const auto& d : diffs) {
                        std::cerr << "  " << d << std::endl;
                    }
                    render_profiler_.EndFrame();
                    cpu_profiler_.EndFrame();
                    std::exit(1);
                }
            }
        }

        render_profiler_.EndFrame();
        cpu_profiler_.EndFrame();
    }
}

// ─── Shutdown ───────────────────────────────────────────────────────────────

void EditorApp::Shutdown() {
    splash_.Finish();  // 若主循环未走到首帧（异常早退），确保 splash 线程被回收

    // Save editor settings
    dse::editor::EditorSettings editor_settings = dse::editor::LoadEditorSettings();
    editor_settings.last_scene_path = dse::editor::SceneTabManager::Get().GetActiveFilePath();
    if (editor_settings.last_scene_path.empty()) editor_settings.last_scene_path = dse::editor::GetCurrentScenePath();
    editor_settings.default_gizmo_operation = current_gizmo_operation_;
    editor_settings.default_gizmo_mode = current_gizmo_mode_;
    // Save scene camera
    {
        auto& cam = dse::editor::GetEditorCamera();
        editor_settings.cam_focal_x = cam.focal_point.x;
        editor_settings.cam_focal_y = cam.focal_point.y;
        editor_settings.cam_focal_z = cam.focal_point.z;
        editor_settings.cam_distance = cam.distance;
        editor_settings.cam_yaw = cam.yaw;
        editor_settings.cam_pitch = cam.pitch;
    }
    dse::editor::AddRecentFile(editor_settings, dse::editor::GetCurrentScenePath());
    dse::editor::SaveEditorSettings(editor_settings);

    dse::editor::AutoSaveManager::Get().OnExit();

    // 关闭当前项目（释放 .lock）
    dse::editor::ProjectManager::Get().CloseProject();

    // 停止所有插件
    plugin_manager_.StopAll();

    // 停止 Control Server
    if (control_server_) {
        control_server_->Stop();
        control_server_.reset();
    }

    if (engine_instance_) {
        engine_instance_->asset_manager()->StopFileWatcher();
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
    // 无项目打开时显示 Project Hub
    if (!dse::editor::ProjectManager::Get().HasOpenProject()) {
        dse::editor::DrawProjectHub();
        // Hub 中可能刚打开了项目，此时同步 data root 并加载默认场景
        if (dse::editor::ProjectManager::Get().HasOpenProject()) {
            auto& mgr = dse::editor::ProjectManager::Get();
            mgr.ApplyDataRoot();
            engine_instance_->asset_manager()->ConfigureDataRoot(
                mgr.GetAssetDir().string());
            dse::editor::AssetDatabase::Get().Refresh();
            engine_instance_->asset_manager()->StartFileWatcher();

            // 加载项目默认场景
            World& world = engine_instance_->pipeline()->world();
            auto& registry = world.registry();
            registry.clear();
            selected_entity_ = entt::null;

            std::string default_scene;
            const auto& desc = mgr.GetDescriptor();
            if (!desc.default_scene.empty()) {
                std::filesystem::path scene_file = mgr.GetProjectRoot() / desc.default_scene;
                if (std::filesystem::exists(scene_file))
                    default_scene = scene_file.string();
            }
            if (default_scene.empty()) {
                std::filesystem::path fallback = mgr.GetSceneDir() / "main.json";
                if (std::filesystem::exists(fallback))
                    default_scene = fallback.string();
            }

            if (!default_scene.empty()) {
                LoadScene(registry, default_scene);
                dse::editor::SetCurrentScenePath(default_scene);
                dse::editor::SceneTabManager::Get().Init(default_scene);
            } else {
                dse::editor::SetCurrentScenePath("");
                dse::editor::SceneTabManager::Get().Init("Untitled");
            }
        }
        return;
    }

    World& world = engine_instance_->pipeline()->world();
    auto& registry = world.registry();
    const bool is_play = (dse::editor::GetEditorState() == dse::editor::EditorState::Play);

    if (static_cast<int>(last_editor_state_) != static_cast<int>(dse::editor::GetEditorState()) && dse::editor::GetEditorState() == dse::editor::EditorState::Edit) {
        selected_entity_ = entt::null;
        SelectionManager::Get().Clear();
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
        cpu_profiler_, memory_profiler_, render_profiler_,
        inspector_active_, inspector_static_,
        current_gizmo_operation_, current_gizmo_mode_,
        editor_languages_, editor_language_index_
    };

    dse::editor::BeginEditorShell();
    dse::editor::PanelVisibility panel_vis{
        &show_localization_preview_, &show_profiler_, &show_animation_,
        &show_tile_palette_, &show_terrain_editor_, &show_lua_console_,
        &show_undo_history_,
        &show_asset_browser_, &show_animation_timeline_, &show_navmesh_,
        &show_shader_graph_, &show_git_, &show_multi_viewport_,
        &show_anim_state_machine_,
        &show_lua_debugger_,
        &show_streaming_debug_, &show_curve_editor_, &show_visual_script_,
        &show_anim_retarget_
    };
    dse::editor::DrawEditorMainMenu(ctx, &show_preferences_, &show_plugins_panel_, &show_chat_panel_, &panel_vis);

    if (!is_play) {
        dse::editor::DrawSceneTabBar(ctx);
    }

    dse::editor::ProcessShortcuts(ctx);

    dse::editor::DrawEditorToolbar(ctx);

    dse::editor::DrawHierarchyPanel(ctx);

    dse::editor::DrawInspectorPanel(ctx);

    dse::editor::DrawProjectPanel();
    dse::editor::DrawConsolePanel();

    if (show_localization_preview_) {
        dse::editor::DrawLocalizationPreviewPanel(ctx,
            localization_preview_key_, sizeof(localization_preview_key_),
            localization_preview_fallback_, sizeof(localization_preview_fallback_));
    }

    if (show_profiler_)       dse::editor::DrawProfilerPanel(ctx);
    if (show_animation_)      dse::editor::DrawAnimationPanel(ctx);
    dse::editor::DrawMaterialPanel(ctx);
    if (show_tile_palette_)   dse::editor::DrawTilePalettePanel(ctx);
    if (show_terrain_editor_) dse::editor::DrawTerrainEditorPanel(ctx);
    if (show_lua_console_)    dse::editor::DrawLuaConsolePanel();
    if (show_lua_debugger_)   dse::editor::DrawLuaDebuggerPanel(ctx);
    dse::editor::DrawBuildGameDialog();
    dse::editor::DrawAssetImporterDialog(ctx);

    dse::editor::DrawPreferencesPanel(&show_preferences_);
    dse::editor::DrawUndoHistoryPanel(&show_undo_history_);
    
    // AI Configuration window
    dse::editor::AIConfigManager::Instance().DrawConfigWindow();

    // New panels
    if (show_asset_browser_)        dse::editor::DrawAssetBrowserPanel();
    if (show_animation_timeline_)   dse::editor::DrawAnimationTimelinePanel(ctx);
    if (show_navmesh_)              dse::editor::DrawNavMeshPanel(ctx);
    if (show_shader_graph_)         dse::editor::DrawShaderGraphPanel(ctx);
    if (show_multi_viewport_)       dse::editor::DrawMultiViewportConfigPanel();
    if (show_anim_state_machine_)   dse::editor::DrawAnimStateMachinePanel(ctx);

    // Streaming Zone debug panel
    if (show_streaming_debug_) {
        ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Streaming Debug", &show_streaming_debug_)) {
            dse::editor::DrawStreamingDebugPanel(ctx);
        }
        ImGui::End();
    }

    // Curve Editor panel
    if (show_curve_editor_) {
        ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Curve Editor", &show_curve_editor_)) {
            static dse::editor::CurveEditorState s_curve_state;
            static bool s_curve_init = false;
            if (!s_curve_init) {
                s_curve_state.curves.push_back(dse::editor::MakeDefaultCurve("Alpha", 0.0f, 1.0f));
                s_curve_state.curves.back().color = IM_COL32(100, 200, 255, 255);
                s_curve_state.curves.push_back(dse::editor::MakeDefaultCurve("Scale", 1.0f, 0.0f));
                s_curve_state.curves.back().color = IM_COL32(255, 150, 80, 255);
                s_curve_init = true;
            }
            dse::editor::DrawCurveEditor("##main_curve", s_curve_state, ImVec2(0, 0));
        }
        ImGui::End();
    }

    // Visual Script editor panel
    if (show_visual_script_) {
        ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Visual Script", &show_visual_script_)) {
            dse::editor::DrawVisualScriptEditor(ctx);
        }
        ImGui::End();
    }

    // Animation Retargeting panel
    if (show_anim_retarget_) {
        ImGui::SetNextWindowSize(ImVec2(720, 560), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Anim Retarget", &show_anim_retarget_)) {
            dse::editor::DrawAnimRetargetPanel(ctx);
        }
        ImGui::End();
    }

    if (show_git_) {
        ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Git", &show_git_)) {
            ImGui::TextDisabled("Git integration is not yet available.");
            ImGui::Spacing();
            ImGui::Text("Planned features:");
            ImGui::BulletText("Repository status");
            ImGui::BulletText("Commit / push / pull");
            ImGui::BulletText("Branch management");
            ImGui::BulletText("Diff viewer");
        }
        ImGui::End();
    }

    // Plugin Manager 面板
    if (show_plugins_panel_) {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Plugins", &show_plugins_panel_)) {
            dse::editor::DrawPluginManagerPanel(plugin_manager_);
        }
        ImGui::End();
    }

    // Plugin API: update and draw custom panels
    dse::editor::EditorPluginManager::Instance().UpdateAll(ctx, ImGui::GetIO().DeltaTime);
    dse::editor::EditorPluginManager::Instance().DrawAllPanels(ctx);

    // AI Chat 面板
    if (show_chat_panel_) {
        ImGui::SetNextWindowSize(ImVec2(420, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("AI Chat", &show_chat_panel_)) {
            chat_panel_.Draw(*control_server_, *engine_instance_);
        }
        ImGui::End();
    }

    dse::editor::DrawSceneViewportPanel(ctx, scene_texture, BuildActiveCameraMatrices,
                                        engine_instance_->pipeline());
    dse::editor::DrawGameViewportPanel(game_texture);

    dse::editor::AutoSaveManager::Get().Tick(registry);
    dse::editor::AutoSaveManager::Get().DrawRecoveryDialog(registry);

    dse::editor::DrawStatusBar(ctx);

    // Fallback: if OS drop wasn't consumed by any panel, open Asset Importer
    {
        dse::editor::OsDropEvent unclaimed;
        if (dse::editor::ConsumeOsDropEvent(unclaimed) && !unclaimed.paths.empty()) {
            dse::editor::OpenAssetImporter();
            dse::editor::SetAssetImporterSourcePath(unclaimed.paths[0].c_str());
        }
    }

    dse::editor::EndEditorShell();
}

// ─── EnsureEditorLocalizationData ───────────────────────────────────────────

void EditorApp::EnsureEditorLocalizationData() {
    auto& localization = dse::gameplay2d::LocalizationSystem::GetInstance();
    localization.Clear();

    const std::filesystem::path dir = GetEditorBinPath() / "editor_localization";
    std::filesystem::create_directories(dir);

    const std::array<std::pair<const char*, const char*>, 2> seeds = {{
        {"en", R"({"editor":{"preview":{"title":"Editor Preview","status":"Language: {lang}","selection":"Selected: {entity}"}}})"},
        {"zh", R"({"editor":{"preview":{"title":"\u7f16\u8f91\u5668\u9884\u89c8","status":"\u5f53\u524d\u8bed\u8a00\uff1a{lang}","selection":"\u5f53\u524d\u9009\u4e2d\uff1a{entity}"}}})"}
    }};

    editor_languages_.clear();
    for (const auto& seed : seeds) {
        const std::filesystem::path file_path = dir / (std::string(seed.first) + ".json");
        ExportTextFile(file_path, seed.second);
        if (localization.LoadLanguage(seed.first, file_path.string())) {
            editor_languages_.emplace_back(seed.first);
        }
    }

    if (!editor_languages_.empty()) {
        // 根据已保存的编辑器 UI 语言设置初始选中项
        const std::string& saved_locale = dse::editor::GetEditorLocale();
        int init_idx = 0;
        for (int i = 0; i < static_cast<int>(editor_languages_.size()); ++i) {
            const auto& lang = editor_languages_[i];
            if (lang == "zh" && (saved_locale == "zh-CN" || saved_locale == "zh_CN" || saved_locale == "zh")) {
                init_idx = i;
                break;
            }
            if (lang == saved_locale) {
                init_idx = i;
                break;
            }
        }
        editor_language_index_ = init_idx;
        localization.SetCurrentLanguage(editor_languages_[init_idx]);
    }
}

} // namespace dse::editor
