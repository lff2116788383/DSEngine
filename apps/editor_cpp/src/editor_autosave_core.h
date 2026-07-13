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

#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

/// 原子写用的临时兄弟路径："<final_path>.tmp"。
std::string MakeAtomicTempPath(const std::string& final_path);

/// 原子地产出 final_path：先由 write_to(tmp) 写到同目录临时文件，成功后
/// rename 覆盖 final_path。任一步失败都会删除临时文件并保持 final_path 不变，
/// 因此写到一半崩溃不会损坏已有的自动保存文件。
/// write_to 成功须返回 true（抛异常视为失败）。仅当 rename 成功时返回 true。
bool AtomicWriteFile(const std::string& final_path,
                     const std::function<bool(const std::string& tmp_path)>& write_to,
                     std::error_code& ec);

/// 多文档自动保存的一次写入项：目标路径 + 写临时文件的回调。
struct AutoSaveItem {
    std::string final_path;
    std::function<bool(const std::string& tmp_path)> write_to;
};

/// 原子地产出一批 final_path（多场景/多资产事务）：分两阶段提交——先把每一项
/// 都写到各自的同目录临时文件（<path>.tmp），只有在「全部」写入都成功后，才逐个
/// rename 覆盖到最终路径。任一项写入失败则删除所有临时文件、不改动任何最终文件，
/// 因此绝不会提交半截/损坏的内容（全有或全无的写入阶段）。
/// 说明：跨文件的 rename 阶段本身无法做到完全原子回滚，但由于所有内容在任何
/// rename 之前都已完整落盘校验，提交阶段不会写出被截断的文档。
bool AtomicWriteAll(const std::vector<AutoSaveItem>& items, std::error_code& ec);

/// 枚举目录下「所有」可恢复的自动保存文件（不是只取第一个），按路径排序返回。
/// 目录不存在/迭代出错时降级为空列表，不抛异常。
std::vector<std::string> CollectRecoveryFiles(const std::string& dir);

/// 恢复候选的逐文档校验结果。
struct RecoveryValidation {
    bool loadable = false;              ///< JSON 解析成功且含 entities 数组（LoadScene 的前置条件）
    int material_schema_version = -1;   ///< 读到的 material_schema_version（无/非整数则 -1）
    int entity_count = 0;               ///< entities 数组长度（仅 loadable 时有意义）
    std::string message;                ///< 面向用户的诊断（可直接展示）
};

/// 在真正 LoadScene 之前，对恢复候选做纯结构校验：解析 JSON、要求根为对象且含
/// entities 数组（与 scene 反序列化的硬前置条件一致），并顺带读出
/// material_schema_version。用于在恢复对话框里提前拦截半截/损坏的自动保存文件，
/// 而不是让加载路径抛异常。纯函数，不触碰文件系统，可无头测试。
RecoveryValidation ValidateRecoveryScene(const std::string& json_text);

}  // namespace dse::editor
