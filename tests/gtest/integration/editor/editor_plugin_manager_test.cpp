/**
 * @file editor_plugin_manager_test.cpp
 * @brief PluginManager 无头逻辑测试
 *
 * 覆盖：ScanPlugins / ParsePluginJson / StartPlugin error path
 * 不启动真实子进程，仅测试纯逻辑层。
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

#include "apps/editor_cpp/src/editor_plugin_manager.h"

namespace fs = std::filesystem;

namespace {

fs::path TempPluginDir(const std::string& name) {
    return fs::temp_directory_path() / ("dse_pmtest_" + name);
}

void WriteFile(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream ofs(path);
    ofs << content;
}

void RemoveDir(const fs::path& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
}

}

// ============================================================
// Test 1: ScanPlugins - 空目录 -> 0 插件
// ============================================================

TEST(PluginManagerTest, ScanPlugins_EmptyDir_ReturnsZero) {
    auto dir = TempPluginDir("empty");
    RemoveDir(dir);
    fs::create_directories(dir);

    dse::editor::PluginManager mgr;
    mgr.ScanPlugins(dir);

    EXPECT_EQ(mgr.GetPluginCount(), 0u);
    RemoveDir(dir);
}

// ============================================================
// Test 2: ScanPlugins - 目录不存在 -> 自动创建 + 0 插件
// ============================================================

TEST(PluginManagerTest, ScanPlugins_NonExistentDir_CreatesAndReturnsZero) {
    auto dir = TempPluginDir("nonexistent_xyz");
    RemoveDir(dir);

    dse::editor::PluginManager mgr;
    mgr.ScanPlugins(dir);

    EXPECT_EQ(mgr.GetPluginCount(), 0u);
    EXPECT_TRUE(fs::exists(dir));
    RemoveDir(dir);
}

// ============================================================
// Test 3: ScanPlugins - 有效 plugin.json -> 元数据正确解析
// ============================================================

TEST(PluginManagerTest, ScanPlugins_ValidPlugin_MetadataParsed) {
    auto dir = TempPluginDir("valid");
    RemoveDir(dir);

    WriteFile(dir / "hello_plugin" / "plugin.json", R"({
        "name": "HelloPlugin",
        "version": "1.0.0",
        "author": "TestAuthor",
        "description": "A test plugin",
        "runtime": "python",
        "entry": "main.py",
        "requires_ui": false,
        "ui_port": 0
    })");

    dse::editor::PluginManager mgr;
    mgr.ScanPlugins(dir);

    ASSERT_EQ(mgr.GetPluginCount(), 1u);
    const auto& meta = mgr.GetPlugins()[0].metadata;
    EXPECT_EQ(meta.name, "HelloPlugin");
    EXPECT_EQ(meta.version, "1.0.0");
    EXPECT_EQ(meta.author, "TestAuthor");
    EXPECT_EQ(meta.runtime, "python");
    EXPECT_EQ(meta.entry, "main.py");
    EXPECT_FALSE(meta.requires_ui);
    EXPECT_EQ(mgr.GetPlugins()[0].state, dse::editor::PluginState::Stopped);

    RemoveDir(dir);
}

// ============================================================
// Test 4: ScanPlugins - 缺少 name 字段 -> 回退到目录名
// ============================================================

TEST(PluginManagerTest, ScanPlugins_MissingName_FallsBackToDirName) {
    auto dir = TempPluginDir("noname");
    RemoveDir(dir);

    WriteFile(dir / "my_plugin_dir" / "plugin.json", R"({
        "version": "0.1",
        "runtime": "node",
        "entry": "index.js"
    })");

    dse::editor::PluginManager mgr;
    mgr.ScanPlugins(dir);

    ASSERT_EQ(mgr.GetPluginCount(), 1u);
    EXPECT_EQ(mgr.GetPlugins()[0].metadata.name, "my_plugin_dir");
    RemoveDir(dir);
}

// ============================================================
// Test 5: ScanPlugins - 非法 JSON -> 跳过，不崩溃
// ============================================================

TEST(PluginManagerTest, ScanPlugins_InvalidJson_SkipsBadPlugin) {
    auto dir = TempPluginDir("badjson");
    RemoveDir(dir);

    WriteFile(dir / "bad_plugin" / "plugin.json", "{ this is not valid json !!!");
    WriteFile(dir / "good_plugin" / "plugin.json", R"({
        "name": "GoodPlugin",
        "runtime": "python",
        "entry": "main.py"
    })");

    dse::editor::PluginManager mgr;
    mgr.ScanPlugins(dir);

    EXPECT_EQ(mgr.GetPluginCount(), 1u);
    EXPECT_EQ(mgr.GetPlugins()[0].metadata.name, "GoodPlugin");
    RemoveDir(dir);
}

// ============================================================
// Test 6: StartPlugin - entry 文件不存在 -> 返回 false + state = Error
// ============================================================

TEST(PluginManagerTest, StartPlugin_EntryNotFound_ReturnsErrorState) {
    auto dir = TempPluginDir("noentry");
    RemoveDir(dir);

    WriteFile(dir / "missing_entry_plugin" / "plugin.json", R"({
        "name": "NoEntry",
        "runtime": "python",
        "entry": "nonexistent_script.py"
    })");

    dse::editor::PluginManager mgr;
    mgr.ScanPlugins(dir);

    ASSERT_EQ(mgr.GetPluginCount(), 1u);
    bool started = mgr.StartPlugin(0);

    EXPECT_FALSE(started);
    EXPECT_EQ(mgr.GetPlugins()[0].state, dse::editor::PluginState::Error);
    EXPECT_FALSE(mgr.GetPlugins()[0].last_error.empty());

    RemoveDir(dir);
}
