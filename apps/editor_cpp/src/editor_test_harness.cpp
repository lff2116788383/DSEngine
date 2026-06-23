/**
 * @file editor_test_harness.cpp
 * @brief 编辑器自动化测试壳实现
 */

#include "editor_test_harness.h"

#include <cstring>
#include <cstdlib>
#include <string>

namespace dse::editor::test {

namespace {

bool StartsWith(const char* str, const char* prefix) {
    return std::strncmp(str, prefix, std::strlen(prefix)) == 0;
}

const char* ExtractValue(const char* arg, const char* prefix) {
    return arg + std::strlen(prefix);
}

} // namespace

EditorTestConfig ParseEditorTestArgs(int argc, char* argv[]) {
    EditorTestConfig config;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "--headless") == 0 ||
            std::strcmp(arg, "--automation-mode") == 0) {
            config.headless = true;
        } else if (std::strcmp(arg, "--automation-api") == 0) {
            // 常驻 RPC 驱动模式：ControlServer 始终启动，且不按 max_frames 自动退出，
            // 仅由 dsengine_editor_quit 显式结束（供 API 会话 / soak 长稳使用）。
            config.automation_api = true;
        } else if (std::strcmp(arg, "--api-port") == 0) {
            // 空格分隔形式：--api-port <N>
            if (i + 1 < argc) {
                int p = std::atoi(argv[++i]);
                if (p > 0) config.api_port = p;
            }
        } else if (StartsWith(arg, "--api-port=")) {
            int p = std::atoi(ExtractValue(arg, "--api-port="));
            if (p > 0) config.api_port = p;
        } else if (StartsWith(arg, "--replay=")) {
            config.replay_path = ExtractValue(arg, "--replay=");
        } else if (StartsWith(arg, "--verify=")) {
            config.verify_path = ExtractValue(arg, "--verify=");
        } else if (StartsWith(arg, "--scene=")) {
            config.scene_path = ExtractValue(arg, "--scene=");
        } else if (StartsWith(arg, "--screenshot=")) {
            config.screenshot_path = ExtractValue(arg, "--screenshot=");
        } else if (StartsWith(arg, "--screenshot-frame=")) {
            config.screenshot_frame = std::atoi(ExtractValue(arg, "--screenshot-frame="));
        } else if (StartsWith(arg, "--max-frames=")) {
            config.max_frames = std::atoi(ExtractValue(arg, "--max-frames="));
            if (config.max_frames <= 0) config.max_frames = 300;
        }
    }

    return config;
}

bool HasTestArgs(const EditorTestConfig& config) {
    return config.headless || !config.replay_path.empty() || !config.verify_path.empty();
}

} // namespace dse::editor::test
