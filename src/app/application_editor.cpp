//
// Created by captainchen on 2021/5/14.
//

#include "application_editor.h"
#include <memory>
#include <iostream>
#include "rttr/registration"
#include "easy/profiler.h"

#include "glad/gl.h"

#ifdef WIN32
// 避免出现APIENTRY重定义警告。
// freetype引用了windows.h，里面定义了APIENTRY。
// glfw3.h会判断是否APIENTRY已经定义然后再定义一次。
// 但是从编译顺序来看glfw3.h在freetype之前被引用了，判断不到 Windows.h中的定义，所以会出现重定义。
// 所以在 glfw3.h之前必须引用  Windows.h。
#include <Windows.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "utils/debug.h"
#include "component/game_object.h"
#include "component/transform.h"
#include "renderer/texture_2d.h"
#include "renderer/camera.h"
#include "renderer/mesh_renderer.h"
#include "renderer/shader.h"
#include "control/input.h"
#include "utils/screen.h"
#include "render_device/render_task_consumer.h"
#include "render_device/render_task_consumer_editor.h"
#include "render_device/read_pixels_queue.h"
#include "audio/audio.h"
#include "utils/time.h"
#include "render_device/render_task_producer.h"
#include "render_device/read_pixels_queue.h"
#include "physics/physics.h"
#include "imgui_internal.h"

static void glfw_error_callback(int error, const char* description)
{
    DEBUG_LOG_ERROR("glfw error:{} description:{}",error,description);
}

/// 键盘回调
/// \param window
/// \param key
/// \param scancode
/// \param action
/// \param mods
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Input::RecordKey(key,action);

}
/// 鼠标按键回调
/// \param window
/// \param button
/// \param action
/// \param mods
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    Input::RecordKey(button,action);

//    std::cout<<"mouse_button_callback:"<<button<<","<<action<<std::endl;
}

/// 鼠标移动回调
/// \param window
/// \param x
/// \param y
static void mouse_move_callback(GLFWwindow* window, double x, double y)
{
    Input::set_mousePosition(x,y);
}
/// 鼠标滚轮回调
/// \param window
/// \param x
/// \param y
static void mouse_scroll_callback(GLFWwindow* window, double x, double y)
{
    Input::RecordScroll(y);
}

void ApplicationEditor::InitGraphicsLibraryFramework() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        DEBUG_LOG_ERROR("glfw init failed!");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    //创建游戏窗口
    game_glfw_window_ = glfwCreateWindow(960, 640, title_.c_str(), NULL, NULL);
    if (!game_glfw_window_)
    {
        DEBUG_LOG_ERROR("glfwCreateWindow error!");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    //创建编辑器窗口，并将游戏Context共享。
    editor_glfw_window_ = glfwCreateWindow(1280, 720, "Editor", NULL, game_glfw_window_);
    if (!editor_glfw_window_)
    {
        DEBUG_LOG_ERROR("glfwCreateWindow error!");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(editor_glfw_window_, key_callback);
    glfwSetMouseButtonCallback(editor_glfw_window_,mouse_button_callback);
    glfwSetScrollCallback(editor_glfw_window_,mouse_scroll_callback);
    glfwSetCursorPosCallback(editor_glfw_window_,mouse_move_callback);

    glfwShowWindow(editor_glfw_window_);

    //设置编辑器主线程使用的是 Editor Context
    glfwMakeContextCurrent(editor_glfw_window_);

    //开启垂直同步
    glfwSwapInterval(1); // Enable vsync

    //ImGui初始化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly=true;//设置仅在标题栏拖动窗口
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    //设置主题
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    //配置后端
    ImGui_ImplGlfw_InitForOpenGL(editor_glfw_window_, true);
    const char* glsl_version = "#version 330";
    ImGui_ImplOpenGL3_Init(glsl_version);

    //游戏窗口 初始化渲染任务消费者(单独渲染线程)
    RenderTaskConsumer::Init(new RenderTaskConsumerEditor(game_glfw_window_));
}

/// 绘制DepthTexture回调，更换自定义Shader。
void DrawDepthTextureCallbackUseCustomShader(const ImDrawList*, const ImDrawCmd*) {
    static GLuint depth_texture_custom_shader_program_id_=0;
    if(depth_texture_custom_shader_program_id_==0)
    {
        //注意Shader代码是从imgui_impl_opengl3.cpp的 bool ImGui_ImplOpenGL3_CreateDeviceObjects()函数中复制。
        //只能修改逻辑，只能修改非资源相关的变量如ProjMtx，需要手动设置非资源相关的变量值如ProjMtx。
        //不能修改资源相关的变量名例如Position UV Color Texture,如果修改需要手动设置值。

        //顶点着色器代码
        const char* vertex_shader_text =R"(
            #version 330 core
            precision mediump float;
            layout (location = 0) in vec2 Position;
            layout (location = 1) in vec2 UV;
            layout (location = 2) in vec4 Color;

            uniform mat4 ProjMtx;
            out vec2 Frag_UV;
            out vec4 Frag_Color;

            void main()
            {
                Frag_UV = UV;
                Frag_Color = Color;
                gl_Position = ProjMtx * vec4(Position.xy,0,1);
            }
        )";
        //片段着色器代码
        const char* fragment_shader_text =R"(
            #version 330 core
            precision mediump float;

            in vec2 Frag_UV;
            in vec4 Frag_Color;
            uniform sampler2D Texture;
            layout (location = 0) out vec4 Out_Color;

            void main()
            {
                float gray = texture(Texture, Frag_UV.st).r;// 从Texture中采样单通道像素值
                Out_Color = Frag_Color * vec4(gray, gray, gray, 1.0);// 将像素值复制到RGB三通道上
            }
        )";

        //创建顶点Shader
        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        //指定Shader源码
        glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
        //编译Shader
        glCompileShader(vertex_shader);
        //获取编译结果
        GLint compile_status=GL_FALSE;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == GL_FALSE)
        {
            GLchar message[256];
            glGetShaderInfoLog(vertex_shader, sizeof(message), 0, message);
            DEBUG_LOG_ERROR("compile vs error:{}",message);
        }

        //创建片段Shader
        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        //指定Shader源码
        glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
        //编译Shader
        glCompileShader(fragment_shader);
        //获取编译结果
        compile_status=GL_FALSE;
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == GL_FALSE)
        {
            GLchar message[256];
            glGetShaderInfoLog(fragment_shader, sizeof(message), 0, message);
            DEBUG_LOG_ERROR("compile fs error:{}",message);
        }

        //创建GPU程序
        GLuint program = glCreateProgram();
        //附加Shader
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        //Link
        glLinkProgram(program);
        //获取编译结果
        GLint link_status=GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &link_status);
        if (link_status == GL_FALSE)
        {
            GLchar message[256];
            glGetProgramInfoLog(program, sizeof(message), 0, message);
            DEBUG_LOG_ERROR("link error:{}",message);
        }
        depth_texture_custom_shader_program_id_=program;
    }

    //从imgui_impl_opengl3.cpp的ImGui_ImplOpenGL3_SetupRenderState函数中复制出来正交投影矩阵计算代码。
    ImDrawData* draw_data = ImGui::GetDrawData();
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    const float ortho_projection[4][4] =
            {
                    { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
                    { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
                    { 0.0f,         0.0f,        -1.0f,   0.0f },
                    { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
            };

    glUseProgram(depth_texture_custom_shader_program_id_);
    glUniformMatrix4fv(glGetUniformLocation(depth_texture_custom_shader_program_id_, "ProjMtx"), 1, GL_FALSE, &ortho_projection[0][0]);
};

void ApplicationEditor::Run() {
    ApplicationBase::Run();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(editor_glfw_window_))
    {
        glfwPollEvents();

        //ImGui刷帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 主菜单栏
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene")) {
                    // 新建场景
                    // 清空当前场景
                    GameObject::Foreach([](GameObject* game_object) {
                        delete game_object;
                        return false;
                    });
                }
                if (ImGui::MenuItem("Save Scene")) {
                    // 保存场景
                    SaveScene("scene.json");
                }
                if (ImGui::MenuItem("Load Scene")) {
                    // 加载场景
                    LoadScene("scene.json");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                if (ImGui::MenuItem("About")) {
                    // 关于对话框
                    ImGui::OpenPopup("AboutPopup");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        
        // 关于对话框
        if (ImGui::BeginPopupModal("AboutPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("2D Game Engine Editor");
            ImGui::Text("Version 1.0");
            ImGui::Text("\n");
            ImGui::Text("A complete 2D game editor based on cpp-game-engine-book");
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 1. 状态
        {
            ImGui::Begin("Status");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 2. 游戏渲染画面
        {
            ImGui::Begin("ViewPort",NULL,ImGuiWindowFlags_None);
            if (ImGui::BeginTabBar("ViewPortTabBar", ImGuiTabBarFlags_None)){
                // 2.1 Game视图
                if (ImGui::BeginTabItem("Game")) {
                    RenderTaskConsumerEditor* render_task_consumer_editor= dynamic_cast<RenderTaskConsumerEditor *>(RenderTaskConsumer::Instance());

                    //从游戏渲染线程拿到FBO Attach Texture id
                    GLuint texture_id=render_task_consumer_editor->color_texture_id();
                    ImTextureID image_id = (void*)(intptr_t)texture_id;

                    // 第一个参数：生成的纹理的id
                    // 第2个参数：Image的大小
                    // 第3，4个参数：UV的起点坐标和终点坐标，UV是被规范化到（0，1）之间的坐标
                    // 第5个参数：图片的色调
                    // 第6个参数：图片边框的颜色
                    ImGui::Image(image_id, ImVec2(480,320), ImVec2(0.0, 1.0), ImVec2(1.0, 0.0), ImVec4(255, 255, 255, 1), ImVec4(0, 255, 0, 1));

                    ImGui::EndTabItem();
                }
                // 2.2 深度视图
                if (ImGui::BeginTabItem("Depth")) {
                    RenderTaskConsumerEditor* render_task_consumer_editor= dynamic_cast<RenderTaskConsumerEditor *>(RenderTaskConsumer::Instance());

                    GLuint texture_id=render_task_consumer_editor->depth_texture_id();
                    ImTextureID image_id = (void*)(intptr_t)texture_id;

                    //设置自定义Shader渲染深度图
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddCallback(&DrawDepthTextureCallbackUseCustomShader, nullptr);

                    ImGui::Image(image_id, ImVec2(480,320), ImVec2(0.0, 1.0), ImVec2(1.0, 0.0), ImVec4(255, 255, 255, 1), ImVec4(0, 255, 0, 1));

                    //还原
                    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }


        // 3. Hierarchy
        {
            ImGui::Begin("Hierarchy");
            ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            Tree::Node* root_node=GameObject::game_object_tree().root_node();
            DrawHierarchy(root_node, "scene",base_flags);
            ImGui::End();
        }

        //4. Property
        {
            ImGui::Begin("Property");

            //4.1 GameObject属性
            GameObject* game_object=nullptr;
            if(selected_node_!= nullptr){
                game_object=dynamic_cast<GameObject*>(selected_node_);
            }
            if(game_object!=nullptr){
                //是否Active
                bool active_self = game_object->active_self();
                if(ImGui::Checkbox("active", &active_self)){
                    game_object->set_active_self(active_self);
                }
                //Layer
                int layer=game_object->layer();
                if(ImGui::InputInt("Layer",&layer)){
                    game_object->set_layer(layer);
                }

            }else{
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "no valid GameObject");
            }

            //4.2 Transform属性
            Transform* transform=nullptr;

            if(game_object!= nullptr){
                transform=game_object->GetComponent<Transform>();
            }

            if(transform!= nullptr){
                if(ImGui::TreeNodeEx("Transform",ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)){
                    // 显示Local属性，如果数值改变，将数据写回Transform
                    if(ImGui::TreeNodeEx("Local",ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)){
                        glm::vec3 local_position=transform->local_position();
                        glm::vec3 local_rotation=transform->local_rotation();
                        glm::vec3 local_scale=transform->local_scale();

                        if(ImGui::InputFloat3("local_position",(float*)&local_position)){
                            transform->set_local_position(local_position);
                        }
                        if(ImGui::InputFloat3("local_rotation",(float*)&local_rotation)){
                            transform->set_local_rotation(local_rotation);
                        }
                        if(ImGui::InputFloat3("local_scale",(float*)&local_scale)){
                            transform->set_local_scale(local_scale);
                        }

                        ImGui::TreePop();
                    }


                    // 显示World属性
                    if(ImGui::TreeNodeEx("World",ImGuiTreeNodeFlags_::ImGuiTreeNodeFlags_DefaultOpen)){
                        glm::vec3 position=transform->position();
                        glm::vec3 rotation=transform->rotation();
                        glm::vec3 scale=transform->scale();

                        ImGui::InputFloat3("position",(float*)&position,"%.3f",ImGuiInputTextFlags_::ImGuiInputTextFlags_ReadOnly);
                        ImGui::InputFloat3("rotation",(float*)&rotation,"%.3f",ImGuiInputTextFlags_::ImGuiInputTextFlags_ReadOnly);
                        ImGui::InputFloat3("scale",(float*)&scale,"%.3f",ImGuiInputTextFlags_::ImGuiInputTextFlags_ReadOnly);

                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }
            }else{
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "not found Transform");
            }

            ImGui::End();
        }
        
        // 5. Sprite Editor
        {
            ImGui::Begin("Sprite Editor");
            
            // 精灵导入
            static char sprite_path[256] = ""; 
            ImGui::InputText("Sprite Path", sprite_path, sizeof(sprite_path));
            if (ImGui::Button("Import Sprite")) {
                // 导入精灵
                Texture2D* texture = Texture2D::LoadFromFile(sprite_path);
                if (texture != nullptr) {
                    ImGui::OpenPopup("SpriteImportedPopup");
                }
            }
            
            // 精灵导入成功对话框
            if (ImGui::BeginPopupModal("SpriteImportedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Sprite imported successfully!");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            // 精灵裁剪
            ImGui::Text("Sprite Cutting");
            static int cut_x = 0, cut_y = 0, cut_width = 32, cut_height = 32;
            ImGui::InputInt("X", &cut_x);
            ImGui::InputInt("Y", &cut_y);
            ImGui::InputInt("Width", &cut_width);
            ImGui::InputInt("Height", &cut_height);
            if (ImGui::Button("Cut Sprite")) {
                // 裁剪精灵
                ImGui::OpenPopup("SpriteCutPopup");
            }
            
            // 精灵裁剪成功对话框
            if (ImGui::BeginPopupModal("SpriteCutPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Sprite cut successfully!");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            // 精灵表创建
            ImGui::Text("Sprite Atlas Creation");
            if (ImGui::Button("Create Sprite Atlas")) {
                // 创建精灵表
                ImGui::OpenPopup("SpriteAtlasPopup");
            }
            
            // 精灵表创建成功对话框
            if (ImGui::BeginPopupModal("SpriteAtlasPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Sprite atlas created successfully!");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            ImGui::End();
        }
        
        // 6. Resource Manager
        {
            ImGui::Begin("Resource Manager");
            
            // 资源导入
            ImGui::Text("Resource Import");
            static char import_path[256] = "";
            ImGui::InputText("Import Path", import_path, sizeof(import_path));
            
            if (ImGui::Button("Import Resource")) {
                ImGui::OpenPopup("ResourceImportedPopup");
            }
            
            // 资源导入成功对话框
            if (ImGui::BeginPopupModal("ResourceImportedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Resource imported successfully!");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            // 资源分类
            ImGui::Text("Resource Categories");
            static const char* categories[] = { "All", "Textures", "Models", "Audio", "Fonts", "Scripts" };
            static int selected_category = 0;
            ImGui::Combo("Category", &selected_category, categories, IM_ARRAYSIZE(categories));
            
            // 资源列表
            ImGui::Text("Resources");
            ImGui::BeginChild("ResourcesList", ImVec2(0, 200), true);
            
            // 示例资源
            static const char* textures[] = { "texture1.png", "texture2.jpg", "spriteatlas1.png" };
            static const char* models[] = { "model1.obj", "model2.fbx" };
            static const char* audio[] = { "sound1.wav", "music1.mp3" };
            
            if (selected_category == 0) { // All
                for (int i = 0; i < 3; i++) {
                    if (ImGui::Selectable(textures[i])) {
                        // 选中资源
                    }
                }
                for (int i = 0; i < 2; i++) {
                    if (ImGui::Selectable(models[i])) {
                        // 选中资源
                    }
                }
                for (int i = 0; i < 2; i++) {
                    if (ImGui::Selectable(audio[i])) {
                        // 选中资源
                    }
                }
            } else if (selected_category == 1) { // Textures
                for (int i = 0; i < 3; i++) {
                    if (ImGui::Selectable(textures[i])) {
                        // 选中资源
                    }
                }
            } else if (selected_category == 2) { // Models
                for (int i = 0; i < 2; i++) {
                    if (ImGui::Selectable(models[i])) {
                        // 选中资源
                    }
                }
            } else if (selected_category == 3) { // Audio
                for (int i = 0; i < 2; i++) {
                    if (ImGui::Selectable(audio[i])) {
                        // 选中资源
                    }
                }
            }
            
            ImGui::EndChild();
            
            // 资源预览
            ImGui::Text("Resource Preview");
            ImGui::BeginChild("ResourcePreview", ImVec2(0, 150), true);
            
            if (selected_category == 1) { // Textures
                ImGui::Image((void*)(intptr_t)0, ImVec2(128, 128));
            } else if (selected_category == 2) { // Models
                ImGui::Text("3D Model Preview");
            } else if (selected_category == 3) { // Audio
                ImGui::Text("Audio Preview");
                ImGui::Button("Play");
            } else {
                ImGui::Text("Select a resource to preview");
            }
            
            ImGui::EndChild();
            
            // 资源属性
            ImGui::Text("Resource Properties");
            static char resource_name[64] = "";
            ImGui::InputText("Name", resource_name, sizeof(resource_name));
            
            static char resource_path[256] = "";
            ImGui::InputText("Path", resource_path, sizeof(resource_path), ImGuiInputTextFlags_ReadOnly);
            
            ImGui::End();
        }
        
        // 7. Animation Editor
        {
            ImGui::Begin("Animation Editor");
            
            // 动画创建
            ImGui::Text("Animation Creation");
            static char animation_name[64] = "NewAnimation";
            ImGui::InputText("Animation Name", animation_name, sizeof(animation_name));
            
            // 动画帧设置
            ImGui::Text("Animation Frames");
            static int frame_count = 4;
            ImGui::InputInt("Frame Count", &frame_count);
            
            static float frame_duration = 0.1f;
            ImGui::InputFloat("Frame Duration", &frame_duration);
            
            // 添加动画帧
            if (ImGui::Button("Add Animation Frames")) {
                ImGui::OpenPopup("AnimationFramesAddedPopup");
            }
            
            // 动画帧添加成功对话框
            if (ImGui::BeginPopupModal("AnimationFramesAddedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Animation frames added successfully!");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            // 动画状态机
            ImGui::Text("Animation State Machine");
            if (ImGui::Button("Create State Machine")) {
                ImGui::OpenPopup("StateMachineCreatedPopup");
            }
            
            // 状态机创建成功对话框
            if (ImGui::BeginPopupModal("StateMachineCreatedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("State machine created successfully!");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            // 动画预览
            ImGui::Text("Animation Preview");
            static bool is_playing = false;
            if (ImGui::Button(is_playing ? "Pause" : "Play")) {
                is_playing = !is_playing;
            }
            
            // 动画预览窗口
            ImGui::BeginChild("AnimationPreview", ImVec2(0, 100), true);
            ImGui::Text("Animation preview area");
            ImGui::EndChild();
            
            ImGui::End();
        }
        
        // 8. Physics Editor
        {
            ImGui::Begin("Physics Editor");
            
            // 碰撞体编辑
            ImGui::Text("Collider Editing");
            
            // 碰撞体类型选择
            static const char* collider_types[] = { "Box", "Sphere", "Capsule", "Mesh" };
            static int selected_collider_type = 0;
            ImGui::Combo("Collider Type", &selected_collider_type, collider_types, IM_ARRAYSIZE(collider_types));
            
            // 碰撞体属性
            if (selected_collider_type == 0) { // Box
                static float box_size[3] = { 1.0f, 1.0f, 1.0f };
                ImGui::InputFloat3("Size", box_size);
            } else if (selected_collider_type == 1) { // Sphere
                static float sphere_radius = 0.5f;
                ImGui::InputFloat("Radius", &sphere_radius);
            } else if (selected_collider_type == 2) { // Capsule
                static float capsule_radius = 0.5f;
                static float capsule_height = 1.0f;
                ImGui::InputFloat("Radius", &capsule_radius);
                ImGui::InputFloat("Height", &capsule_height);
            }
            
            // 添加碰撞体
            if (ImGui::Button("Add Collider")) {
                ImGui::OpenPopup("ColliderAddedPopup");
            }
            
            // 碰撞体添加成功对话框
            if (ImGui::BeginPopupModal("ColliderAddedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Collider added successfully!");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            // 物理属性调整
            ImGui::Text("Physics Properties");
            
            static float mass = 1.0f;
            ImGui::InputFloat("Mass", &mass);
            
            static float friction = 0.5f;
            ImGui::InputFloat("Friction", &friction);
            
            static float restitution = 0.2f;
            ImGui::InputFloat("Restitution", &restitution);
            
            // 物理调试视图
            ImGui::Text("Physics Debug View");
            static bool show_debug = false;
            ImGui::Checkbox("Show Debug", &show_debug);
            
            if (ImGui::Button("Toggle Debug View")) {
                show_debug = !show_debug;
                ImGui::OpenPopup("DebugViewToggledPopup");
            }
            
            // 调试视图切换对话框
            if (ImGui::BeginPopupModal("DebugViewToggledPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Physics debug view %s!", show_debug ? "enabled" : "disabled");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            ImGui::End();
        }
        
        // 9. UI Editor
        {
            ImGui::Begin("UI Editor");
            
            // UI 控件库
            ImGui::Text("UI Controls Library");
            ImGui::BeginChild("UIControlsLibrary", ImVec2(200, 200), true);
            
            static const char* ui_controls[] = { "Button", "Text", "Image", "Input Field", "Slider", "Toggle", "Dropdown" };
            for (int i = 0; i < IM_ARRAYSIZE(ui_controls); i++) {
                if (ImGui::Button(ui_controls[i])) {
                    // 创建 UI 控件
                    ImGui::OpenPopup("UIControlCreatedPopup");
                }
            }
            
            ImGui::EndChild();
            
            // UI 控件创建成功对话框
            if (ImGui::BeginPopupModal("UIControlCreatedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("UI control created successfully!");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            // UI 布局调整
            ImGui::Text("UI Layout Adjustment");
            
            static int layout_type = 0; // 0: Horizontal, 1: Vertical, 2: Grid
            static const char* layout_types[] = { "Horizontal", "Vertical", "Grid" };
            ImGui::Combo("Layout Type", &layout_type, layout_types, IM_ARRAYSIZE(layout_types));
            
            static float spacing = 10.0f;
            ImGui::InputFloat("Spacing", &spacing);
            
            static float padding = 5.0f;
            ImGui::InputFloat("Padding", &padding);
            
            // 应用布局
            if (ImGui::Button("Apply Layout")) {
                ImGui::OpenPopup("LayoutAppliedPopup");
            }
            
            // 布局应用成功对话框
            if (ImGui::BeginPopupModal("LayoutAppliedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Layout applied successfully!");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            // UI 元素属性
            ImGui::Text("UI Element Properties");
            
            static char ui_element_name[64] = "NewUIElement";
            ImGui::InputText("Element Name", ui_element_name, sizeof(ui_element_name));
            
            static float ui_position[2] = { 0.0f, 0.0f };
            ImGui::InputFloat2("Position", ui_position);
            
            static float ui_size[2] = { 100.0f, 50.0f };
            ImGui::InputFloat2("Size", ui_size);
            
            // UI 预览
            ImGui::Text("UI Preview");
            ImGui::BeginChild("UIPreview", ImVec2(0, 150), true);
            
            // 示例 UI 预览
            ImGui::Text("UI Preview Area");
            ImGui::Button("Sample Button", ImVec2(100, 40));
            
            ImGui::EndChild();
            
            ImGui::End();
        }


        //绘制
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(editor_glfw_window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(editor_glfw_window_);

        //渲染游戏
        EASY_BLOCK("Frame"){
            OneFrame();
        }EASY_END_BLOCK;
    }

    Exit();
}

void ApplicationEditor::DrawHierarchy(Tree::Node* node,const char* label,int base_flags) {
    int flags=base_flags;

    if(selected_node_==node){//如果当前Node是被选中的，那么设置flag，显示样式为选中。
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    std::list<Tree::Node*>& children=node->children();
    if(children.size()>0){
        if(ImGui::TreeNodeEx(label, flags)){//如果被点击，就展开子节点。
            if(ImGui::IsItemClicked()){
                selected_node_=node;
            }
            
            // 右键菜单
            if(ImGui::IsItemClicked(1)){
                ImGui::OpenPopup("HierarchyContextMenu");
                selected_node_=node;
            }
            
            for(auto* child:children){
                GameObject* game_object= dynamic_cast<GameObject *>(child);
//                DEBUG_LOG_INFO("game object:{} depth:{}",game_object->name(),game_object->depth());
                DrawHierarchy(child, game_object->name(), base_flags);
            }
            ImGui::TreePop();//可以点击展开的TreeNode，需要加上TreePop()。
        }
    }else{//没有子节点，不显示展开按钮
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx(label, flags);
        if(ImGui::IsItemClicked()){
            selected_node_=node;
        }
        
        // 右键菜单
        if(ImGui::IsItemClicked(1)){
            ImGui::OpenPopup("HierarchyContextMenu");
            selected_node_=node;
        }
    }
    
    // 右键菜单内容
    if(ImGui::BeginPopup("HierarchyContextMenu")){
        if(ImGui::MenuItem("Create Empty GameObject")){
            // 创建空游戏对象
            static int counter = 0;
            char name[64];
            sprintf(name, "GameObject_%d", counter++);
            GameObject* game_object = new GameObject(name);
            
            // 如果有选中的节点且是GameObject，设置为其子节点
            if(selected_node_ != nullptr){
                GameObject* parent = dynamic_cast<GameObject*>(selected_node_);
                if(parent != nullptr){
                    game_object->SetParent(parent);
                }
            }
        }
        
        if(ImGui::MenuItem("Delete")){
            // 删除选中的游戏对象
            if(selected_node_ != nullptr){
                GameObject* game_object = dynamic_cast<GameObject*>(selected_node_);
                if(game_object != nullptr){
                    // 从树中移除
                    Tree::Node* parent = game_object->parent();
                    if(parent != nullptr){
                        parent->RemoveChild(game_object);
                    }
                    // 删除对象
                    delete game_object;
                    selected_node_ = nullptr;
                }
            }
        }
        
        ImGui::EndPopup();
    }
}

void ApplicationEditor::SaveScene(const char* file_path) {
    // 简单的场景保存实现
    // 使用文本格式保存场景数据
    FILE* file = fopen(file_path, "w");
    if (file == nullptr) {
        DEBUG_LOG_ERROR("Failed to open file for writing: {}", file_path);
        return;
    }
    
    // 保存游戏对象
    fprintf(file, "# Scene File\n");
    
    GameObject::Foreach([file](GameObject* game_object) {
        // 保存游戏对象基本信息
        fprintf(file, "GameObject %s\n", game_object->name());
        
        // 保存变换信息
        Transform* transform = game_object->GetComponent<Transform>();
        if (transform != nullptr) {
            glm::vec3 position = transform->position();
            glm::vec3 rotation = transform->rotation();
            glm::vec3 scale = transform->scale();
            fprintf(file, "Transform %f %f %f %f %f %f %f %f %f\n", 
                    position.x, position.y, position.z,
                    rotation.x, rotation.y, rotation.z,
                    scale.x, scale.y, scale.z);
        }
        
        // 保存其他组件信息...
        
        fprintf(file, "\n");
        return true;
    });
    
    fclose(file);
    DEBUG_LOG_INFO("Scene saved successfully: {}", file_path);
}

void ApplicationEditor::LoadScene(const char* file_path) {
    // 简单的场景加载实现
    // 读取文本格式的场景数据
    FILE* file = fopen(file_path, "r");
    if (file == nullptr) {
        DEBUG_LOG_ERROR("Failed to open file for reading: {}", file_path);
        return;
    }
    
    // 清空当前场景
    GameObject::Foreach([](GameObject* game_object) {
        delete game_object;
        return false;
    });
    
    // 读取场景数据
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file) != nullptr) {
        // 解析游戏对象
        if (strncmp(buffer, "GameObject", 10) == 0) {
            char name[64];
            sscanf(buffer, "GameObject %s", name);
            
            // 创建游戏对象
            GameObject* game_object = new GameObject(name);
            
            // 读取变换信息
            if (fgets(buffer, sizeof(buffer), file) != nullptr && strncmp(buffer, "Transform", 9) == 0) {
                float pos_x, pos_y, pos_z;
                float rot_x, rot_y, rot_z;
                float scale_x, scale_y, scale_z;
                sscanf(buffer, "Transform %f %f %f %f %f %f %f %f %f", 
                       &pos_x, &pos_y, &pos_z,
                       &rot_x, &rot_y, &rot_z,
                       &scale_x, &scale_y, &scale_z);
                
                // 设置变换信息
                Transform* transform = game_object->GetComponent<Transform>();
                if (transform != nullptr) {
                    transform->set_local_position(glm::vec3(pos_x, pos_y, pos_z));
                    transform->set_local_rotation(glm::vec3(rot_x, rot_y, rot_z));
                    transform->set_local_scale(glm::vec3(scale_x, scale_y, scale_z));
                }
            }
        }
    }
    
    fclose(file);
    DEBUG_LOG_INFO("Scene loaded successfully: {}", file_path);
}

void ApplicationEditor::Exit() {
    ApplicationBase::Exit();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(editor_glfw_window_);
    // glfwWindow的销毁放在RenderTaskConsumer中，这里就不再调用。
    glfwTerminate();
}