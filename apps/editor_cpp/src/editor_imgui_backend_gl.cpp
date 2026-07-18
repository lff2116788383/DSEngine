#include "editor_imgui_backend_gl.h"
#include <glad/gl.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace dse::editor {

bool ImGuiBackendGL::Init(GLFWwindow* window, dse::render::RhiDevice*) {
    return ImGui_ImplGlfw_InitForOpenGL(window, true) &&
           ImGui_ImplOpenGL3_Init("#version 330");
}

void ImGuiBackendGL::NewFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}

void ImGuiBackendGL::PrepareFrame(int width, int height, const float clear_color[4]) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ImGuiBackendGL::RenderDrawData(ImDrawData* draw_data) {
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
}

ImTextureID ImGuiBackendGL::GetTextureId(unsigned int texture_handle) {
    return static_cast<ImTextureID>(texture_handle);
}

void ImGuiBackendGL::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
}

} // namespace dse::editor
