#include <iostream>
#include <string>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_internal.h"

// TODO: Replace with engine app initialization
// #include "engine/runtime/engine_app.h"

void SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Deep tech dark theme based on Editor_UI_Architecture.md
    colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.10f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 0.66f, 1.00f, 1.00f); // Tech Blue
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 0.66f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.00f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.00f, 0.66f, 1.00f, 0.80f); // Tech Blue hover
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.00f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 0.66f, 1.00f, 0.40f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.00f, 0.66f, 1.00f, 0.80f);
    colors[ImGuiCol_Separator]              = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.00f, 0.66f, 1.00f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.00f, 0.66f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.66f, 1.00f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.00f, 0.66f, 1.00f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.00f, 0.66f, 1.00f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.00f, 0.66f, 1.00f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.54f, 0.17f, 0.89f, 0.70f); // Neon Purple for docking preview
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
    
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
}

void DrawEditorUI() {
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        static bool first_time = true;
        if (first_time) {
            first_time = false;

            ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            // Split the dockspace into nodes -- Unity Style
            auto dock_id_main = dockspace_id;
            
            // 1. Split top for toolbar
            auto dock_id_top = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Up, 0.05f, nullptr, &dock_id_main);
            
            // 2. Split bottom for Project/Console/Animation
            auto dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.30f, nullptr, &dock_id_main);
            
            // 3. Split left for Hierarchy
            auto dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, nullptr, &dock_id_main);
            
            // 4. Split right for Inspector
            auto dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, nullptr, &dock_id_main);

            // We now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow("Toolbar", dock_id_top);
            ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Project", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Animation", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Tile Palette", dock_id_bottom); // New 2D panel
            ImGui::DockBuilderDockWindow("Scene", dock_id_main);
            ImGui::DockBuilderDockWindow("Game", dock_id_main);
            
            // Set some nodes to hide their tab bar if needed (e.g., Toolbar)
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id_top);
            if (node) {
                node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeY;
            }
            
            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {}
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {}
            if (ImGui::MenuItem("Save As...")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Toolbar Panel
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4));
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);
    float avail_width = ImGui::GetContentRegionAvail().x;
    
    // Tools (Left)
    ImGui::SetCursorPosX(10);
    if (ImGui::Button("[H]", ImVec2(32, 24))) {} // Hand
    ImGui::SameLine();
    if (ImGui::Button("[M]", ImVec2(32, 24))) {} // Move
    ImGui::SameLine();
    if (ImGui::Button("[R]", ImVec2(32, 24))) {} // Rotate
    ImGui::SameLine();
    if (ImGui::Button("[S]", ImVec2(32, 24))) {} // Scale
    ImGui::SameLine();
    
    // 2D/3D Toggle
    ImGui::SetCursorPosX(10 + 4 * 36 + 20); // Spacing after transform tools
    static bool is2D = false;
    if (is2D) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
    }
    if (ImGui::Button("2D", ImVec2(32, 24))) { is2D = !is2D; }
    if (is2D) {
        ImGui::PopStyleColor();
    }

    // Play Controls (Center)
    ImGui::SameLine();
    ImGui::SetCursorPosX((avail_width / 2.0f) - 60);
    if (ImGui::Button(">", ImVec2(32, 24))) {} // Play
    ImGui::SameLine();
    if (ImGui::Button("||", ImVec2(32, 24))) {} // Pause
    ImGui::SameLine();
    if (ImGui::Button(">|", ImVec2(32, 24))) {} // Step

    // Account/Settings (Right)
    ImGui::SameLine();
    ImGui::SetCursorPosX(avail_width - 130);
    ImGui::Button("Collab", ImVec2(60, 24));
    ImGui::SameLine();
    ImGui::Button("Layers", ImVec2(60, 24));

    ImGui::End();
    ImGui::PopStyleVar();

    // Panels (Unity-style layout)
    ImGui::Begin("Hierarchy");
    if (ImGui::TreeNode("Scene")) {
        ImGui::BulletText("Main Camera");
        ImGui::BulletText("Directional Light");
        
        if (!is2D) {
            ImGui::BulletText("Cube");
        } else {
            ImGui::BulletText("Sprite");
            ImGui::BulletText("Tilemap");
            ImGui::BulletText("Spine_Character");
        }
        ImGui::TreePop();
    }
    ImGui::End();

    ImGui::Begin("Inspector");
    
    // Header component
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
    ImGui::AlignTextToFramePadding();
    ImGui::Checkbox("##Active", new bool(true)); ImGui::SameLine();
    
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 70);
    char nameBuf[64] = "Selected Object";
    ImGui::InputText("##Name", nameBuf, sizeof(nameBuf)); 
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    ImGui::Checkbox("Static", new bool(false));
    ImGui::Separator();

    // Helper macro for left-aligned labels in Inspector
    #define INSPECTOR_PROPERTY(label, code) \
        ImGui::AlignTextToFramePadding(); \
        ImGui::Text(label); \
        ImGui::NextColumn(); \
        ImGui::SetNextItemWidth(-1); \
        code; \
        ImGui::NextColumn();

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "transform_cols", false);
        ImGui::SetColumnWidth(0, 80.0f);
        
        float pos[3] = {0, 0, 0};
        INSPECTOR_PROPERTY("Position", ImGui::DragFloat3("##pos", pos, 0.1f));
        
        float rot[3] = {0, 0, 0};
        if (is2D) {
            INSPECTOR_PROPERTY("Rotation", ImGui::DragFloat("##rotZ", &rot[2], 0.1f));
        } else {
            INSPECTOR_PROPERTY("Rotation", ImGui::DragFloat3("##rot", rot, 0.1f));
        }
        
        float scale[3] = {1, 1, 1};
        INSPECTOR_PROPERTY("Scale", ImGui::DragFloat3("##scale", scale, 0.1f));
        
        ImGui::Columns(1);
    }
    
    if (!is2D) {
        if (ImGui::CollapsingHeader("Mesh Filter", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "meshfilter_cols", false);
            ImGui::SetColumnWidth(0, 80.0f);
            INSPECTOR_PROPERTY("Mesh", ImGui::Button("Cube", ImVec2(-1, 0)));
            ImGui::Columns(1);
        }

        if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "meshrenderer_cols", false);
            ImGui::SetColumnWidth(0, 80.0f);
            INSPECTOR_PROPERTY("Material", ImGui::Button("Default-Material", ImVec2(-1, 0)));
            ImGui::Columns(1);
        }
    } else {
        if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "spriterenderer_cols", false);
            ImGui::SetColumnWidth(0, 80.0f);
            INSPECTOR_PROPERTY("Sprite", ImGui::Button("hero_idle", ImVec2(-1, 0)));
            
            float color[4] = {1, 1, 1, 1};
            INSPECTOR_PROPERTY("Color", ImGui::ColorEdit4("##color", color));
            
            INSPECTOR_PROPERTY("Flip", {
                ImGui::Checkbox("X", new bool(false)); ImGui::SameLine();
                ImGui::Checkbox("Y", new bool(false));
            });
            ImGui::Columns(1);
        }

        if (ImGui::CollapsingHeader("Box Collider 2D", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "boxcol2d_cols", false);
            ImGui::SetColumnWidth(0, 80.0f);
            
            float offset[2] = {0, 0};
            INSPECTOR_PROPERTY("Offset", ImGui::DragFloat2("##offset", offset, 0.1f));
            
            float size[2] = {1, 1};
            INSPECTOR_PROPERTY("Size", ImGui::DragFloat2("##size", size, 0.1f));
            
            INSPECTOR_PROPERTY("Is Trigger", ImGui::Checkbox("##trigger", new bool(false)));
            ImGui::Columns(1);
        }
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 60);
    ImGui::Button("Add Component", ImVec2(120, 30));
    
    ImGui::PopStyleColor();
    ImGui::End();

    ImGui::Begin("Project");
    ImGui::Text("Assets");
    ImGui::Separator();
    ImGui::Button("Materials"); ImGui::SameLine();
    ImGui::Button("Models"); ImGui::SameLine();
    ImGui::Button("Scripts");
    ImGui::End();

    ImGui::Begin("Console");
    ImGui::Text("[Info] Engine initialized successfully.");
    ImGui::Text("[Info] Loaded default scene.");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[Warning] Missing texture 'skybox_diffuse'.");
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Error] Failed to load shader 'standard_pbr.glsl'.");
    ImGui::End();
    
    ImGui::Begin("Animation");
    ImGui::Text("No animated object selected.");
    ImGui::End();

    ImGui::Begin("Tile Palette");
    if (!is2D) {
        ImGui::TextDisabled("Tile Palette is only available in 2D mode.");
    } else {
        ImGui::Button("Active Brush", ImVec2(120, 24)); ImGui::SameLine();
        ImGui::Button("Paint", ImVec2(60, 24)); ImGui::SameLine();
        ImGui::Button("Erase", ImVec2(60, 24));
        
        ImGui::Separator();
        ImGui::Text("Select a tilemap to start painting.");
        
        // Mock tile palette grid
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 6; x++) {
                draw_list->AddRectFilled(
                    ImVec2(p.x + x * 32, p.y + y * 32), 
                    ImVec2(p.x + x * 32 + 30, p.y + y * 32 + 30), 
                    IM_COL32(80 + (x+y)*10, 100 + x*10, 120 + y*10, 255)
                );
            }
        }
    }
    ImGui::End();

    // The Scene Viewport Panel (Where engine editor rendering would happen)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Scene");
    ImVec2 scenePanelSize = ImGui::GetContentRegionAvail();
    // Here we would typically render an FBO texture containing our engine output
    // For now, just a placeholder dark rect
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetCursorScreenPos();
    ImVec2 p_max = ImVec2(p_min.x + scenePanelSize.x, p_min.y + scenePanelSize.y);
    draw_list->AddRectFilled(p_min, p_max, IM_COL32(40, 40, 40, 255));
    
    // Draw a mock grid
    for (float i = 0; i < scenePanelSize.x; i += 50) {
        draw_list->AddLine(ImVec2(p_min.x + i, p_min.y), ImVec2(p_min.x + i, p_max.y), IM_COL32(60, 60, 60, 255));
    }
    for (float i = 0; i < scenePanelSize.y; i += 50) {
        draw_list->AddLine(ImVec2(p_min.x, p_min.y + i), ImVec2(p_max.x, p_min.y + i), IM_COL32(60, 60, 60, 255));
    }
    
    draw_list->AddText(ImVec2(p_min.x + scenePanelSize.x/2 - 50, p_min.y + scenePanelSize.y/2), IM_COL32(200, 200, 200, 255), "Scene View");
    ImGui::End();
    ImGui::PopStyleVar();

    // The Game Viewport Panel (Independent Game Camera view)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Game");
    ImVec2 gamePanelSize = ImGui::GetContentRegionAvail();
    draw_list = ImGui::GetWindowDrawList();
    p_min = ImGui::GetCursorScreenPos();
    p_max = ImVec2(p_min.x + gamePanelSize.x, p_min.y + gamePanelSize.y);
    draw_list->AddRectFilled(p_min, p_max, IM_COL32(20, 20, 20, 255));
    draw_list->AddText(ImVec2(p_min.x + gamePanelSize.x/2 - 40, p_min.y + gamePanelSize.y/2), IM_COL32(150, 150, 150, 255), "Game View");
    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::End(); // End DockSpace window
}

int main() {
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "DSEngine Editor", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGL(glfwGetProcAddress)) {
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    
    // Redirect imgui.ini to bin folder so it doesn't clutter the project root
    io.IniFilename = "bin/editor_layout.ini";

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    SetupImGuiStyle();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw Editor UI
        DrawEditorUI();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
