#pragma once

#include <string>

// 编辑器侧崩溃捕获薄封装。
//
// 引擎已在 EngineInstance::Init() 内安装进程级崩溃处理器（SetUnhandledExceptionFilter），
// 但它使用通用 app_name "DSEngine"、且只在引擎初始化之后才生效。本封装在编辑器侧补足：
//  1) 在 main()/Init 的最早期就安装一次，覆盖引擎初始化之前的窗口/ImGui/字体加载阶段崩溃；
//  2) 用编辑器专属 app_name 与独立 dump 目录，区分编辑器进程与玩家/打包进程的崩溃；
//  3) 提供面包屑/元数据转发，记录崩溃前的编辑器上下文（场景、命令、面板、Play 状态）；
//  4) 暴露“上次会话是否崩溃”的查询，供启动期与已有 AutoSave 恢复联动提示。
//
// 注意：engine 的 CrashReporter::Install 是“后者覆盖”语义（会重置 config 与面包屑）。
// 因此 InstallEditorCrashHandler() 需要在引擎 Init 之后再调用一次以重新夺回编辑器身份，
// 该函数本身幂等、可安全多次调用。

namespace dse::editor {

/// 安装/重申编辑器崩溃处理器（幂等）。应在 main() 最早期调用一次，并在引擎 Init 之后再调用一次。
/// 设置环境变量 DSE_CRASH_HANDLER=0 可整体关闭；DSE_CRASH_DIR 可覆盖输出目录。
void InstallEditorCrashHandler();

/// 是否已成功安装。
bool IsEditorCrashHandlerInstalled();

/// 追加一条编辑器面包屑（未安装时为空操作，线程安全转发）。
void AddEditorBreadcrumb(const std::string& entry);

/// 设置/更新一条编辑器元数据（未安装时为空操作）。
void SetEditorCrashMetadata(const std::string& key, const std::string& value);

/// 编辑器崩溃输出目录（DSE_CRASH_DIR 或默认 "crashes/editor"）。
std::string GetEditorCrashDir();

/// 扫描指定目录，返回最新的一份编辑器崩溃报告 .txt 路径（无则空）。纯函数，便于测试。
std::string FindLatestCrashReportInDir(const std::string& dir);

/// 扫描编辑器崩溃目录，返回最新一份编辑器崩溃报告路径（无则空）。
std::string FindLatestEditorCrashReport();

/// 返回“本次会话开始之前”就已存在的最新编辑器崩溃报告路径（即上次会话遗留的崩溃，若有）。
/// 在首次 InstallEditorCrashHandler() 时快照，不受本次会话新写报告影响。
std::string GetPreviousSessionCrashReport();

/// 仅供测试：重置内部安装/快照状态并卸载处理器，使后续 Install 重新快照。
void ResetEditorCrashHandlerForTesting();

}  // namespace dse::editor
