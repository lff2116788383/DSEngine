/**
 * @file sample_plugin.cpp
 * @brief DSEngine 编辑器示例插件 DLL
 *
 * 编译方式 (MSVC):
 *   cl /LD /EHsc /std:c++20 /I ../../apps/editor_cpp/src sample_plugin.cpp /Fe:sample_plugin.dll
 *
 * 放置到编辑器 plugins/ 目录下即可自动加载。
 */

// 仅需包含插件 API 头文件
#include "editor_plugin_api.h"
#include "imgui.h"

#include <cmath>
#include <ctime>

namespace {

// 插件自定义状态
struct SampleState {
    float counter = 0.0f;
    char note_buf[256] = "Hello from Sample Plugin!";
    bool show_time = true;
};

SampleState& GetSampleState() {
    static SampleState s;
    return s;
}

// 自定义面板绘制
void DrawSamplePanel(dse::editor::EditorContext& /*ctx*/) {
    auto& s = GetSampleState();

    ImGui::Text("Sample Plugin Panel");
    ImGui::Separator();

    ImGui::Text("Frame Counter: %.1f", s.counter);
    ImGui::ProgressBar(std::fmod(s.counter / 100.0f, 1.0f));

    ImGui::Spacing();
    ImGui::Checkbox("Show Time", &s.show_time);
    if (s.show_time) {
        std::time_t t = std::time(nullptr);
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", std::localtime(&t));
        ImGui::Text("Current Time: %s", time_buf);
    }

    ImGui::Spacing();
    ImGui::InputTextMultiline("##notes", s.note_buf, sizeof(s.note_buf), ImVec2(-1, 80));

    if (ImGui::Button("Reset Counter")) {
        s.counter = 0;
    }
}

} // namespace

// DLL 入口点：编辑器调用此函数注册插件
DSE_PLUGIN_EXPORT void dse_plugin_register(dse::editor::EditorPluginManager* mgr) {
    auto plugin = std::make_shared<dse::editor::EditorPlugin>();
    plugin->name = "Sample Plugin";
    plugin->version = "1.0.0";
    plugin->author = "DSEngine Team";
    plugin->description = "A demonstration plugin showing the editor plugin API capabilities.";

    // 注册自定义面板
    dse::editor::PluginPanel panel;
    panel.name = "Sample Plugin Panel";
    panel.draw = DrawSamplePanel;
    panel.visible = true;
    plugin->panels.push_back(panel);

    // 注册菜单项
    dse::editor::PluginMenuItem item;
    item.label = "Reset Sample Counter";
    item.action = []() { GetSampleState().counter = 0; };
    plugin->menu_items.push_back(item);

    // 每帧更新回调
    plugin->on_update = [](dse::editor::EditorContext&, float dt) {
        GetSampleState().counter += dt;
    };

    plugin->on_init = [](dse::editor::EditorContext&) {
        // 初始化时可做资源加载等
    };

    plugin->on_shutdown = []() {
        // 清理
    };

    mgr->RegisterPlugin(plugin);
}
