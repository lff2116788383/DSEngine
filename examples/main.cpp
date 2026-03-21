#define GLFW_INCLUDE_NONE

#if defined(DSE_PHASE1_ONLY)
#include "phase1/runtime/frame_pipeline.h"
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <iostream>
#include "utils/time.h"
#include "utils/screen.h"
#include "control/input.h"
#include "render_device/render_task_consumer.h"
#include "render_device/render_task_consumer_standalone.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <thread>
#include <chrono>

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Input::RecordKey(key, action);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    Input::RecordKey(button, action);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    Input::RecordScroll(yoffset);
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    Input::set_mousePosition(xpos, ypos);
}

int main(void){
    std::cout << "Starting DSEngine Phase 1..." << std::endl;
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    std::cout << "GLFW Initialized." << std::endl;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "DSEngine Phase 1", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    
    // ImGui needs to be initialized in the thread where the context is current
    // In DSEngine, the render thread makes the context current.
    // However, some ImGui operations are done on the main thread (like ImGui::NewFrame),
    // and rendering is done on the render thread.
    // For Phase 1 standalone, let's keep it simple: we don't use multi-threaded rendering if it breaks ImGui,
    // but DSEngine forces RenderTaskConsumer to use a separate thread.
    // This is a known issue: ImGui context is thread-local by default.
    // We must call ImGui::SetCurrentContext() on both threads to the same context if we share it,
    // OR we just do ImGui setup in the RenderTaskConsumer (which we just did).
    
    // But ImGui::NewFrame is called in main thread's FramePipeline::Update.
    // So we need to create the context here, and share it.
    IMGUI_CHECKVERSION();
    ImGuiContext* imgui_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_ctx);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    
    glfwMakeContextCurrent(nullptr);
    
    RenderTaskConsumer::Init(new RenderTaskConsumerStandalone(window));
    RenderTaskConsumerStandalone* consumer = static_cast<RenderTaskConsumerStandalone*>(RenderTaskConsumer::Instance());
    consumer->SetImGuiContext(imgui_ctx);
    
    // We need to wait until the render thread has initialized ImGui_ImplOpenGL3_Init.
    // Otherwise, ImGui::NewFrame() might fail an assertion in the main loop.
    // A simple hack for Phase 1:
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    Phase1FramePipeline::Instance().Init();
    
    float fixed_time_step = 0.02f;
    float accumulator = 0.0f;
    
    std::cout << "Starting main loop..." << std::endl;
    int frame_count = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        Screen::set_width_height(width, height);
        
        Time::Update();
        float dt = Time::delta_time();
        
        accumulator += dt;
        while (accumulator >= fixed_time_step) {
            Phase1FramePipeline::Instance().FixedUpdate(fixed_time_step);
            accumulator -= fixed_time_step;
        }
        
        Phase1FramePipeline::Instance().Update(dt);
        Phase1FramePipeline::Instance().Render();
        
        Input::Update();
        
        if (frame_count++ % 60 == 0) {
            std::cout << "Frame " << frame_count << std::endl;
        }
    }
    std::cout << "Main loop exited." << std::endl;
    
    // ImGui Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // glfwDestroyWindow is handled by RenderTaskConsumer
    // glfwTerminate();
    return 0;
}
#else
#include "app/application.h"
#include "app/application_editor.h"
#include "app/application_standalone.h"

int main(void){
    Application::Init(new ApplicationStandalone());
    Application::Run();
    return 0;
}
#endif


