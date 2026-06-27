/**
 * @file ui_test_harness.h
 * @brief 编辑器内嵌 Dear ImGui Test Engine 的最小驱动壳（仅 DSE_EDITOR_UI_TESTS 构建编入）。
 *
 * 设计取自 Dear ImGui 官方测试套件的结构：测试引擎与被测 ImGui 共用同一个
 * ImGuiContext，由编辑器主循环每帧驱动；无头 `--run-ui-tests` 模式下从命令行
 * 入队全部用例，跑完按结果摘要返回进程退出码（0=全过 / 1=有失败）。
 *
 * 生命周期与编辑器主循环的对应关系：
 *   Init()        —— EditorApp::Init 末尾调用：建测试引擎、配 IO、注册用例、Start、入队。
 *   PostFrame()   —— 每帧 glfwSwapBuffers 之后调用（= ImGuiTestEngine_PostSwap）。
 *   IsFinished()  —— 队列为空即测试跑完（= ImGuiTestEngine_IsTestQueueEmpty）。
 *   ResultExitCode() —— 打印 tested/passed/failed 摘要并返回 0/1。
 *   Stop()        —— 必须在 ImGui::DestroyContext() 之前调用。
 *   Destroy()     —— 必须在 ImGui::DestroyContext() 之后调用（落 ini 等）。
 */
#pragma once

#ifdef DSE_EDITOR_UI_TESTS

#include <string>

struct ImGuiContext;

namespace dse::runtime { class EngineInstance; }
namespace dse::editor::core { class CommandBus; }

namespace dse::editor::uitest {

/// 测试用例可访问的编辑器服务（用例体是无捕获 lambda，经全局只读句柄取用）。
struct UiTestServices {
    dse::runtime::EngineInstance* engine = nullptr;  ///< 引擎实例（取 world / registry）
    dse::editor::core::CommandBus* bus = nullptr;     ///< 写路径门面（保留，结构写经现有工具）
};

/// 建测试引擎并绑定到 ui_ctx；配置无头快跑 IO；注册全部用例；Start 并按 filter 入队。
void Init(ImGuiContext* ui_ctx, const UiTestServices& services, const std::string& filter);

/// 每帧 framebuffer swap 之后调用，处理测试引擎的帧后逻辑。
void PostFrame();

/// 测试队列是否已清空（全部用例跑完）。
bool IsFinished();

/// 打印结果摘要（tested/passed/failed）并返回进程退出码：全过 0，有失败 1。
int ResultExitCode();

/// 停止协程并导出（若有）。必须在 ImGui::DestroyContext() 之前调用。
void Stop();

/// 销毁测试引擎。必须在 ImGui::DestroyContext() 之后调用。
void Destroy();

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
