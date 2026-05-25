/**
 * @file editor_plugin_api_test.cpp
 * @brief EditorPluginManager (新 DLL 插件 API) 单元测试
 *
 * 自包含测试：复制 EditorPluginManager 核心逻辑用于无 ImGui/Editor 环境验证。
 *
 * 覆盖场景：
 * - 注册/反注册插件
 * - 重复注册拒绝
 * - 面板可见性切换
 * - DLL 路径不存在时加载失败
 * - 空目录扫描返回 0
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

// ─── 最小化 Plugin 模型（与 editor_plugin_api.h 对齐）───────────────────────────

struct PluginPanel {
    std::string name;
    std::function<void()> draw;
    bool visible = false;
};

struct EditorPlugin {
    std::string name;
    std::string version;
    std::string author;
    std::vector<PluginPanel> panels;
};

class PluginManager {
public:
    void RegisterPlugin(std::shared_ptr<EditorPlugin> plugin) {
        if (!plugin) return;
        for (const auto& p : plugins_) {
            if (p->name == plugin->name) return;
        }
        plugins_.push_back(std::move(plugin));
    }

    void UnregisterPlugin(const std::string& name) {
        plugins_.erase(
            std::remove_if(plugins_.begin(), plugins_.end(),
                [&](const std::shared_ptr<EditorPlugin>& p) { return p->name == name; }),
            plugins_.end());
    }

    void TogglePanelVisibility(const std::string& plugin_name, const std::string& panel_name) {
        for (auto& p : plugins_) {
            if (p->name != plugin_name) continue;
            for (auto& panel : p->panels) {
                if (panel.name == panel_name) { panel.visible = !panel.visible; return; }
            }
        }
    }

    bool LoadPluginFromDll(const std::string& dll_path) {
#ifdef _WIN32
        HMODULE hmod = LoadLibraryA(dll_path.c_str());
        if (!hmod) return false;
        FreeLibrary(hmod);
        return true; // Simplified: real impl checks entry point
#else
        (void)dll_path;
        return false;
#endif
    }

    int LoadPluginsFromDirectory(const std::string& dir_path) {
        namespace fs = std::filesystem;
        std::error_code ec;
        if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec)) return 0;
        int count = 0;
        for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
#ifdef _WIN32
            if (ext != ".dll") continue;
#else
            if (ext != ".so") continue;
#endif
            if (LoadPluginFromDll(entry.path().string())) ++count;
        }
        return count;
    }

    const std::vector<std::shared_ptr<EditorPlugin>>& GetPlugins() const { return plugins_; }

private:
    std::vector<std::shared_ptr<EditorPlugin>> plugins_;
};

std::shared_ptr<EditorPlugin> MakeTestPlugin(const std::string& name) {
    auto p = std::make_shared<EditorPlugin>();
    p->name = name;
    p->version = "1.0";
    p->author = "Test";
    return p;
}

} // namespace

// ============================================================
// 注册 / 反注册
// ============================================================

TEST(EditorPluginApiTest, Register_添加插件) {
    PluginManager mgr;
    auto p = MakeTestPlugin("TestPlugin_Reg");
    mgr.RegisterPlugin(p);

    ASSERT_EQ(mgr.GetPlugins().size(), 1u);
    EXPECT_EQ(mgr.GetPlugins()[0]->name, "TestPlugin_Reg");

    mgr.UnregisterPlugin("TestPlugin_Reg");
    EXPECT_EQ(mgr.GetPlugins().size(), 0u);
}

TEST(EditorPluginApiTest, Register_空指针不崩溃) {
    PluginManager mgr;
    mgr.RegisterPlugin(nullptr);
    EXPECT_EQ(mgr.GetPlugins().size(), 0u);
}

TEST(EditorPluginApiTest, Register_重复名称拒绝) {
    PluginManager mgr;
    mgr.RegisterPlugin(MakeTestPlugin("Dup"));
    mgr.RegisterPlugin(MakeTestPlugin("Dup"));
    EXPECT_EQ(mgr.GetPlugins().size(), 1u);
}

// ============================================================
// 面板可见性
// ============================================================

TEST(EditorPluginApiTest, TogglePanelVisibility) {
    PluginManager mgr;
    auto p = MakeTestPlugin("PanelPlugin");
    PluginPanel panel;
    panel.name = "MyPanel";
    panel.visible = false;
    p->panels.push_back(panel);
    mgr.RegisterPlugin(p);

    mgr.TogglePanelVisibility("PanelPlugin", "MyPanel");
    EXPECT_TRUE(mgr.GetPlugins()[0]->panels[0].visible);

    mgr.TogglePanelVisibility("PanelPlugin", "MyPanel");
    EXPECT_FALSE(mgr.GetPlugins()[0]->panels[0].visible);
}

TEST(EditorPluginApiTest, TogglePanel_不存在的插件不崩溃) {
    PluginManager mgr;
    mgr.TogglePanelVisibility("NoSuch", "Panel"); // no crash
}

// ============================================================
// DLL 加载
// ============================================================

TEST(EditorPluginApiTest, LoadDll_不存在的路径返回false) {
    PluginManager mgr;
    bool result = mgr.LoadPluginFromDll("C:\\nonexistent_path_xyz\\fake_plugin.dll");
    EXPECT_FALSE(result);
}

TEST(EditorPluginApiTest, ScanDirectory_空目录返回0) {
    namespace fs = std::filesystem;
    auto dir = fs::temp_directory_path() / "dse_plugin_api_test_empty";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);

    PluginManager mgr;
    int count = mgr.LoadPluginsFromDirectory(dir.string());
    EXPECT_EQ(count, 0);

    fs::remove_all(dir, ec);
}

TEST(EditorPluginApiTest, ScanDirectory_不存在的目录返回0) {
    PluginManager mgr;
    int count = mgr.LoadPluginsFromDirectory("Z:\\absolutely_nonexistent_dir_12345");
    EXPECT_EQ(count, 0);
}
