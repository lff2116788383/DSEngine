/**
 * @file engine_app.cpp
 * @brief 引擎应用外壳，负责运行时生命周期与服务装配。
 */

#define GLFW_INCLUDE_NONE
#include "engine/runtime/engine_app.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/scene/scene.h"
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <iostream>
#include "engine/base/debug.h"
#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include "engine/core/event_bus.h"
#include <utility>
#include "engine/base/time.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"
#include <cstdlib>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <string>

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <stb_image_write.h>

namespace dse::runtime {
namespace {
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Input::RecordKey(key, action);
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    Input::RecordKey(button, action);
}

void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Input::RecordMouseScroll(yoffset);
}

void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    Input::RecordMousePosition(xpos, ypos);
}

std::string ReadNonEmptyEnv(const char* name) {
    if (const char* value = std::getenv(name)) {
        if (value[0] != '\0') {
            return value;
        }
    }
    return {};
}

bool IsStartupSceneRegressionDisabled() {
    const char* env = std::getenv("DSE_DISABLE_STARTUP_SCENE_REGRESSION");
    return env && env[0] != '\0' && std::string(env) != "0";
}

void FlipImageRowsRgba8(std::vector<unsigned char>& pixels, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    const std::size_t row_bytes = static_cast<std::size_t>(width) * 4u;
    std::vector<unsigned char> row(row_bytes, 0u);
    for (int y = 0; y < height / 2; ++y) {
        auto* top = pixels.data() + static_cast<std::size_t>(y) * row_bytes;
        auto* bottom = pixels.data() + static_cast<std::size_t>(height - 1 - y) * row_bytes;
        std::copy(top, top + row_bytes, row.begin());
        std::copy(bottom, bottom + row_bytes, top);
        std::copy(row.begin(), row.end(), bottom);
    }
}

bool CaptureRuntimeScreenshot(FramePipeline& pipeline) {
    const std::string screenshot_path = ReadNonEmptyEnv("DSE_SCREENSHOT_PATH");
    if (screenshot_path.empty()) {
        return true;
    }

    const std::string target = ReadNonEmptyEnv("DSE_SCREENSHOT_TARGET");
    RenderTargetReadback readback = (target == "main")
        ? pipeline.ReadMainColorRgba8WithSize()
        : pipeline.ReadSceneColorRgba8WithSize();
    if (readback.pixels.empty() || readback.width <= 0 || readback.height <= 0) {
        std::cerr << "Failed to capture screenshot pixels\n";
        return false;
    }

    const std::size_t expected_size = static_cast<std::size_t>(readback.width) * static_cast<std::size_t>(readback.height) * 4u;
    if (readback.pixels.size() != expected_size) {
        std::cerr << "Unexpected screenshot pixel buffer size\n";
        return false;
    }

    FlipImageRowsRgba8(readback.pixels, readback.width, readback.height);
    std::filesystem::create_directories(std::filesystem::path(screenshot_path).parent_path());
    if (stbi_write_png(screenshot_path.c_str(), readback.width, readback.height, 4, readback.pixels.data(), readback.width * 4) == 0) {
        std::cerr << "Failed to write screenshot png: " << screenshot_path << "\n";
        return false;
    }

    std::cout << "DSE_SCREENSHOT_WRITTEN path=" << screenshot_path << " target=" << (target == "main" ? "main" : "scene")
              << " size=" << readback.width << "x" << readback.height << std::endl;
    return true;
}
}



EngineInstance::EngineInstance(const EngineRunConfig& config)
    : config_(config)
    , services_(config.services) {
    if (services_.world == nullptr && config_.world != nullptr) {
        services_.world = config_.world;
    }
    if (services_.asset_manager == nullptr && config_.asset_manager != nullptr) {
        services_.asset_manager = config_.asset_manager;
    }
    if (services_.job_system == nullptr && config_.job_system != nullptr) {
        services_.job_system = config_.job_system;
    }
    if (services_.world == nullptr) {
        default_world_ = std::make_unique<World>();
        services_.world = default_world_.get();
    }
    if (services_.asset_manager == nullptr) {
        default_asset_manager_ = std::make_unique<AssetManager>();
        services_.asset_manager = default_asset_manager_.get();
    }
    if (services_.job_system == nullptr) {
        default_job_system_ = std::make_unique<core::JobSystem>();
        services_.job_system = default_job_system_.get();
    }

    // Keep backward-compatible mirrors in sync for older call sites.
    config_.world = services_.world;
    config_.asset_manager = services_.asset_manager;
    config_.job_system = services_.job_system;
    config_.services = services_;

    pipeline_ = std::make_unique<FramePipeline>();
}

EngineInstance::~EngineInstance() {
    Shutdown();
}

bool EngineInstance::RunStartupSceneRegressionChecks() {
    if (IsStartupSceneRegressionDisabled()) {
        DEBUG_LOG_INFO("EngineInstance init: startup scene regression checks skipped by env");
        return true;
    }

    DEBUG_LOG_INFO("EngineInstance init: startup scene regression begin");
    const bool scene_round_trip_ok = scene::RunSceneRoundTripRegressionSample("bin/scene_roundtrip_regression.json");
    DEBUG_LOG_INFO("Scene round-trip regression: {}", scene_round_trip_ok ? "PASSED" : "FAILED");
    const bool scene_backward_compat_ok = scene::RunSceneBackwardCompatibilityRegressionSample("bin/scene_backward_compat_regression.json");
    DEBUG_LOG_INFO("Scene backward-compat regression: {}", scene_backward_compat_ok ? "PASSED" : "FAILED");
    return scene_round_trip_ok && scene_backward_compat_ok;
}

void EngineInstance::RegisterRuntimeServices() {
    auto pipeline_shared = std::shared_ptr<FramePipeline>(pipeline_.get(), [](FramePipeline*) {});
    service_locator().Register<FramePipeline, FramePipeline>(pipeline_shared);

    if (services_.world) {
        auto world_shared = std::shared_ptr<World>(services_.world, [](World*) {});
        service_locator().Register<World, World>(world_shared);
    }

    event_bus_ = std::make_shared<core::EventBus>(&service_locator());
    service_locator().Register<core::EventBus, core::EventBus>(event_bus_);

    if (services_.job_system) {
        auto job_system_shared = std::shared_ptr<core::JobSystem>(services_.job_system, [](core::JobSystem*) {});
        service_locator().Register<core::JobSystem, core::JobSystem>(job_system_shared);
    }

    service_locator().BridgeTo<FramePipeline>(core::ServiceLocator::Instance());
    service_locator().BridgeTo<World>(core::ServiceLocator::Instance());
    service_locator().BridgeTo<core::EventBus>(core::ServiceLocator::Instance());
    service_locator().BridgeTo<core::JobSystem>(core::ServiceLocator::Instance());
}

void EngineInstance::ResetRuntimeServices() {
    service_locator().Reset<core::JobSystem>();
    service_locator().Reset<core::EventBus>();
    service_locator().Reset<FramePipeline>();
    service_locator().Reset<World>();
    event_bus_.reset();

    core::ServiceLocator::Instance().Reset<core::JobSystem>();
    core::ServiceLocator::Instance().Reset<FramePipeline>();
    core::ServiceLocator::Instance().Reset<World>();
    core::ServiceLocator::Instance().Reset<core::EventBus>();
}

void EngineInstance::CleanupOnInitFailure() {
    ResetRuntimeServices();
    pipeline_->Shutdown();
    if (services_.job_system) {
        services_.job_system->Shutdown();
    }
    Debug::ShutDown();
    if (!config_.enable_editor) glfwTerminate();
}

bool EngineInstance::Init() {

    if (is_initialized_) return true;

    // 如果未启用编辑器模式，则初始化系统环境
    if (!config_.enable_editor) {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW\n";
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* window = glfwCreateWindow(config_.window_width, config_.window_height, config_.window_title.c_str(), nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window\n";
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window);
        if (!gladLoadGL(glfwGetProcAddress)) {
            std::cerr << "Failed to initialize OpenGL (glad)\n";
            glfwDestroyWindow(window);
            glfwTerminate();
            return false;
        }
    } else {
        // 在编辑器模式下，gladLoadGL 通常已经在编辑器主程序中初始化过了，
        // 但由于引擎是 DLL，exe 中的 glad 初始化不会影响 DLL 中的 glad 函数指针，
        // 所以我们必须在 DLL 中再次调用 gladLoadGL。
        gladLoadGL(glfwGetProcAddress);
        
        // 确保 Screen 大小被正确初始化
        Screen::set_width_height(config_.window_width, config_.window_height);
    }

    GLFWwindow* current_window = glfwGetCurrentContext();
    if (current_window) {
        if (!config_.enable_editor) {
            glfwSetKeyCallback(current_window, KeyCallback);
            glfwSetMouseButtonCallback(current_window, MouseButtonCallback);
            glfwSetScrollCallback(current_window, ScrollCallback);
            glfwSetCursorPosCallback(current_window, CursorPositionCallback);
            
            pipeline_->SetWindowTitleSetter([current_window](const std::string& title) {
                glfwSetWindowTitle(current_window, title.c_str());
            });
        } else {
            // 在编辑器模式下，忽略来自业务逻辑（Lua/C++）对窗口标题的修改，
            // 保持编辑器窗口自身的标题不受干扰
            pipeline_->SetWindowTitleSetter([](const std::string&) {
                // Do nothing
            });
        }
    }

    Debug::Init();
    if (services_.job_system) {
        services_.job_system->Init();
    }

    // 先登记 EngineInstance 级服务容器，再桥接到兼容全局入口。
    RegisterRuntimeServices();

    pipeline_->EnableEditorMode(config_.enable_editor);
    pipeline_->SetWorld(services_.world);
    pipeline_->SetAssetManager(services_.asset_manager);
    if (services_.asset_manager) {
        services_.asset_manager->SetEventBus(event_bus_.get());
        services_.asset_manager->SetJobSystem(services_.job_system);
    }
    pipeline_->SetBusinessMode(config_.business_mode);
    
    if (config_.business_mode == BusinessMode::Lua && !config_.startup_lua_script_path.empty()) {
        SetStartupLuaScriptPath(config_.startup_lua_script_path);
    }
    
    std::cout << "Business mode: " << (config_.business_mode == BusinessMode::Lua ? "lua" : "cpp") << std::endl;
    
    if (!pipeline_->Init()) {
        std::cerr << "Failed to initialize FramePipeline\n";
        CleanupOnInitFailure();
        return false;
    }

    if (!RunStartupSceneRegressionChecks()) {
        std::cerr << "Startup scene regression checks failed\n";
        CleanupOnInitFailure();
        return false;
    }

    is_initialized_ = true;
    return true;
}


void EngineInstance::Tick() {
    if (!is_initialized_) return;

    Time::Update();
    float dt = Time::delta_time();

    // Clamp accumulator to prevent spiral-of-death when dt is very large
    // (e.g. after a loading stall or breakpoint). Allow at most 10 fixed steps per frame.
    constexpr float kMaxAccumulator = 0.2f; // 10 * 0.02s
    accumulator_ += dt;
    if (accumulator_ > kMaxAccumulator) {
        accumulator_ = kMaxAccumulator;
    }
    while (accumulator_ >= fixed_time_step_) {
        pipeline_->FixedUpdate(fixed_time_step_);
        accumulator_ -= fixed_time_step_;
    }

    pipeline_->Update(dt);
    pipeline_->Render();
    Input::Update();
}

void EngineInstance::Shutdown() {
    if (!is_initialized_) return;
    
    pipeline_->Shutdown();
    if (services_.job_system) {
        services_.job_system->Shutdown();
    }

    if (services_.asset_manager) {
        services_.asset_manager->SetEventBus(nullptr);
        services_.asset_manager->SetJobSystem(nullptr);
    }

    // 清理实例级/兼容级 ServiceLocator 中的服务引用（不销毁 World / FramePipeline 本身，由 EngineInstance 管理）
    ResetRuntimeServices();
    
    Debug::ShutDown();
    
    if (!config_.enable_editor) {
        glfwTerminate();
    }
    
    is_initialized_ = false;
}

int EngineInstance::Run() {
    if (!Init()) return -1;

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        std::cerr << "No current GLFW context found\n";
        return -1;
    }

    int max_frames = 0;
    if (const char* env_max_frames = std::getenv("DSE_MAX_FRAMES")) {
        max_frames = std::max(0, std::atoi(env_max_frames));
    }
    int frame_counter = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        Screen::set_width_height(width, height);

        Tick();

        glfwSwapBuffers(window);
        frame_counter += 1;
        if (max_frames > 0 && frame_counter >= max_frames) {
            std::cout << "DSE_MAX_FRAMES reached: " << frame_counter << std::endl;
            break;
        }
    }

    const bool screenshot_ok = CaptureRuntimeScreenshot(*pipeline_);


    Shutdown();
    return screenshot_ok ? 0 : -2;
}

int RunEngine(const EngineRunConfig& config) {
    std::cout << "Starting DSEngine Runtime..." << std::endl;
    EngineInstance instance(config);
    return instance.Run();
}
}
