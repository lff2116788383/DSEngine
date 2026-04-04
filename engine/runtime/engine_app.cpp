/**
 * @file engine_app.cpp
 * @brief 引擎核心模块，提供基础功能支持
 */

#define GLFW_INCLUDE_NONE
#include "engine/runtime/engine_app.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/assets/asset_manager.h"
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <iostream>
#include "engine/core/job_system.h"
#include "engine/base/time.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"
#include <vector>
#include <filesystem>

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
}

EngineInstance::EngineInstance(const EngineRunConfig& config) : config_(config) {
    if (!config_.world) {
        default_world_ = std::make_unique<World>();
        config_.world = default_world_.get();
    }
    if (!config_.asset_manager) {
        // Since AssetManager is still a singleton globally, we use it for now, 
        // but mark the path to instance-based in Phase 2
        config_.asset_manager = &AssetManager::Instance();
    }
    pipeline_ = std::make_unique<FramePipeline>();
}

EngineInstance::~EngineInstance() {
    Shutdown();
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

    core::JobSystem::Init();

    World* active_world = config_.world ? config_.world : default_world_.get();
    pipeline_->EnableEditorMode(config_.enable_editor);
    pipeline_->SetWorld(active_world);
    pipeline_->SetAssetManager(config_.asset_manager);
    pipeline_->SetBusinessMode(config_.business_mode);
    
    if (config_.business_mode == BusinessMode::Lua && !config_.startup_lua_script_path.empty()) {
        SetStartupLuaScriptPath(config_.startup_lua_script_path);
    }
    
    std::cout << "Business mode: " << (config_.business_mode == BusinessMode::Lua ? "lua" : "cpp") << std::endl;
    
    if (!pipeline_->Init()) {
        std::cerr << "Failed to initialize FramePipeline\n";
        pipeline_->Shutdown();
        core::JobSystem::Shutdown();
        if (!config_.enable_editor) glfwTerminate();
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
    core::JobSystem::Shutdown();
    
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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        Screen::set_width_height(width, height);

        Tick();

        glfwSwapBuffers(window);
    }

    Shutdown();
    return 0;
}

int RunEngine(const EngineRunConfig& config) {
    std::cout << "Starting DSEngine Runtime..." << std::endl;
    EngineInstance instance(config);
    return instance.Run();
}
}
