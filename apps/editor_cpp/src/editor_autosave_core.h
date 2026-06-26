#pragma once

// 自动保存纯核心 —— 无 ImGui / 文件系统 / 单例依赖，可无头测试。
//
// 这里只放可纯函数化的判定逻辑：
//   - 场景名 → 自动保存文件名（非法字符净化 + 空名兜底）
//   - 恢复文件识别谓词
//   - 自动保存间隔下限钳制
//   - 每帧「是否该自动保存」的状态机判定
//
// 实际文件系统读写、单例状态、ImGui 对话框留在 editor_autosave.cpp。

#include <string>

namespace dse::editor {

/// 把文件系统非法字符（/ \ : * ? " < > |）替换为 '_'；空名返回 "Untitled"。
std::string SanitizeSceneName(std::string scene_name);

/// 由场景名生成自动保存文件名："<净化名>.autosave.dscene"。
std::string MakeAutoSaveFileName(const std::string& scene_name);

/// 恢复文件谓词：扩展名为 .dscene 且文件名（stem）含 ".autosave"。
bool IsAutoSaveRecoveryFile(const std::string& filename);

/// 自动保存间隔下限钳制（最小 10 秒，防过于频繁写盘）。
double ClampAutoSaveInterval(double interval_sec);

/// 每帧自动保存判定结果。
enum class AutoSaveDecision {
    Skip,       ///< 本帧不动作（播放中 / 未启用 / 未脏 / 未到间隔）
    InitTimer,  ///< 首次计时：仅把 last_save_time 置为 now
    Save,       ///< 应执行一次自动保存并刷新 last_save_time
};

/// 根据当前状态判定本帧是否该自动保存。纯函数，不触碰时钟/磁盘。
AutoSaveDecision DecideAutoSave(bool in_play_mode,
                               bool enabled,
                               bool dirty,
                               double last_save_time,
                               double now,
                               double interval_sec);

}  // namespace dse::editor
