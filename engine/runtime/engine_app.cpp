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

EngineInstance::~EngineInstance() = default;

int EngineInstance::Run() {
    return RunEngine(config_); // Delegating to the original loop for now
}

int RunEngine(const EngineRunConfig& config) {
    std::cout << "Starting DSEngine Runtime..." << std::endl;
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(config.window_width, config.window_height, config.window_title.c_str(), nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetCursorPosCallback(window, CursorPositionCallback);
    FramePipeline pipeline;
    pipeline.SetWindowTitleSetter([window](const std::string& title) {
        glfwSetWindowTitle(window, title.c_str());
    });

    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    core::JobSystem::Init();

    World runtime_world;
    World* active_world = config.world ? config.world : &runtime_world;
    pipeline.SetWorld(active_world);
    pipeline.SetAssetManager(config.asset_manager);
    pipeline.SetBusinessMode(config.business_mode);
    if (config.business_mode == BusinessMode::Lua && !config.startup_lua_script_path.empty()) {
        SetStartupLuaScriptPath(config.startup_lua_script_path);
    }
    std::cout << "Business mode: " << (config.business_mode == BusinessMode::Lua ? "lua" : "cpp") << std::endl;
    if (!pipeline.Init()) {
        std::cerr << "Failed to initialize FramePipeline\n";
        pipeline.Shutdown();
        core::JobSystem::Shutdown();
        glfwTerminate();
        return -1;
    }

    float fixed_time_step = 0.02f;
    float accumulator = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        Screen::set_width_height(width, height);

        Time::Update();
        float dt = Time::delta_time();

        accumulator += dt;
        while (accumulator >= fixed_time_step) {
            pipeline.FixedUpdate(fixed_time_step);
            accumulator -= fixed_time_step;
        }

        pipeline.Update(dt);
        pipeline.Render();
        glfwSwapBuffers(window);
        Input::Update();
    }
    pipeline.Shutdown();
    core::JobSystem::Shutdown();
    glfwTerminate();
    return 0;
}
}
