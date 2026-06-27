/**
 * @file ui_test_harness.cpp
 * @brief ui_test_harness.h 的实现（仅 DSE_EDITOR_UI_TESTS 构建编入）。
 */
#include "ui_test_harness.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "ui_tests_internal.h"

#include <cstdio>
#include <fstream>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "imgui_te_exporters.h"

namespace dse::editor::uitest {

namespace {

// 测试引擎与服务句柄全进程唯一（无头跑单实例），用例的无捕获 lambda 经 Services() 取用。
ImGuiTestEngine* g_engine = nullptr;
UiTestServices   g_services;
std::string      g_filter;
bool             g_started = false;

// 编辑器是 WIN32（GUI 子系统）进程，stdout/stderr 不进管道；结果改落文件供 CI 读取。
// JUnit XML 给机器读，summary.txt 给人读，路径相对 CWD（无头跑统一在仓根启动）。
std::string      g_results_xml     = "bin/ui_test_results.xml";
const char*      kResultsSummary   = "bin/ui_test_summary.txt";

} // namespace

const UiTestServices& Services() { return g_services; }

void SetServices(const UiTestServices& services) { g_services = services; }

void Init(ImGuiContext* ui_ctx, const UiTestServices& services, const std::string& filter) {
    SetServices(services);
    g_filter = filter;

    g_engine = ImGuiTestEngine_CreateContext();

    ImGuiTestEngineIO& io = ImGuiTestEngine_GetIO(g_engine);
    io.ConfigRunSpeed       = ImGuiTestRunSpeed_Fast;  // 无头快跑：瞬移鼠标、跳过延时
    io.ConfigSavedSettings  = false;                   // 不读写 .ini，避免污染编辑器布局
    io.ConfigCaptureEnabled = false;                   // 关截图（与 imconfig 一致）
    io.ConfigLogToTTY       = true;                     // 日志直出终端（仅控制台子系统可见）
    io.ConfigVerboseLevel        = ImGuiTestVerboseLevel_Info;
    io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    // 注册导出文件：跑完/崩溃都会写出 JUnit XML（含失败用例日志）。
    io.ExportResultsFilename = g_results_xml.c_str();
    io.ExportResultsFormat   = ImGuiTestEngineExportFormat_JUnitXml;

    RegisterAllUiTests(g_engine);

    ImGuiTestEngine_Start(g_engine, ui_ctx);
    g_started = true;

    // 从命令行入队：空 filter 跑全部；filter 形如 "dse-hierarchy/" 可只跑子集。
    const char* filter_cstr = g_filter.empty() ? nullptr : g_filter.c_str();
    ImGuiTestEngine_QueueTests(g_engine, ImGuiTestGroup_Tests, filter_cstr,
                               ImGuiTestRunFlags_RunFromCommandLine);
}

void PostFrame() {
    if (g_engine) {
        ImGuiTestEngine_PostSwap(g_engine);
    }
}

bool IsFinished() {
    return g_engine ? ImGuiTestEngine_IsTestQueueEmpty(g_engine) : true;
}

int ResultExitCode() {
    if (!g_engine) return 1;

    // 写出 JUnit XML（使用 Init 注册的 filename/format）。
    ImGuiTestEngine_Export(g_engine);

    ImGuiTestEngineResultSummary summary;
    ImGuiTestEngine_GetResultSummary(g_engine, &summary);
    const int failed = summary.CountTested - summary.CountSuccess;
    // 一个都没跑到也视为失败：避免“0 通过”被误判为成功。
    const bool ok = (summary.CountTested > 0) && (failed == 0) && (summary.CountInQueue == 0);

    std::printf("[ui-tests] tested=%d passed=%d failed=%d (in_queue=%d)\n",
                summary.CountTested, summary.CountSuccess, failed, summary.CountInQueue);
    // 同步落人读摘要（WIN32 子系统下 stdout 不可见，CI 读此文件）。
    if (std::ofstream ofs{kResultsSummary, std::ios::trunc}) {
        ofs << "result=" << (ok ? "PASS" : "FAIL") << "\n"
            << "tested=" << summary.CountTested << "\n"
            << "passed=" << summary.CountSuccess << "\n"
            << "failed=" << failed << "\n"
            << "in_queue=" << summary.CountInQueue << "\n";
    }
    return ok ? 0 : 1;
}

void Stop() {
    if (g_engine && g_started) {
        ImGuiTestEngine_Stop(g_engine);
        g_started = false;
    }
}

void Destroy() {
    if (g_engine) {
        ImGuiTestEngine_DestroyContext(g_engine);
        g_engine = nullptr;
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
