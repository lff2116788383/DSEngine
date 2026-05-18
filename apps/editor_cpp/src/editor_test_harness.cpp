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

        if (std::strcmp(arg, "--headless") == 0) {
            config.headless = true;
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
