/**
 * @file editor_test_harness.h
 * @brief 编辑器自动化测试壳：CLI 参数解析 + headless 运行支持
 */

#pragma once

#include <string>
#include <vector>

namespace dse::editor::test {

/**
 * @struct EditorTestConfig
 * @brief 存储编辑器自动化测试的 CLI 解析结果
 */
struct EditorTestConfig {
    bool headless = false;              ///< --headless / --automation-mode: 不显示窗口
    std::string replay_path;            ///< --replay=<path.json>: 输入回放文件
    std::string verify_path;            ///< --verify=<path.json>: 期望快照文件
    std::string scene_path;             ///< --scene=<path>: 指定启动场景
    std::string screenshot_path;         ///< --screenshot=<path>: 退出前截图保存路径
    int screenshot_frame = -1;           ///< --screenshot-frame=<N>: 第 N 帧截图（-1 表示最后一帧）
    int max_frames = 300;               ///< --max-frames=<N>: 自动退出帧数
    int api_port = 9527;                ///< --api-port=<N> / --api-port <N>: ControlServer 监听端口
};

/**
 * @brief 从命令行参数解析 EditorTestConfig
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 解析后的配置
 */
EditorTestConfig ParseEditorTestArgs(int argc, char* argv[]);

/**
 * @brief 检查是否有任何测试/自动化参数被传入
 * @param config 解析后的配置
 * @return 如果有 headless/replay/verify 参数则返回 true
 */
bool HasTestArgs(const EditorTestConfig& config);

} // namespace dse::editor::test
