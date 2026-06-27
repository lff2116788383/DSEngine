/**
 * @file ui_tests_internal.h
 * @brief UI 测试内部共享声明：服务句柄存取 + 用例注册入口（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * harness 在 Init 时经 SetServices() 写入引擎/命令总线句柄；各用例文件里的无捕获
 * lambda 经 Services() 读取（无捕获 lambda 无法捕获 this，只能走该全局只读句柄）。
 */
#pragma once

#ifdef DSE_EDITOR_UI_TESTS

#include "ui_test_harness.h"

struct ImGuiTestEngine;

namespace dse::editor::uitest {

/// 取当前测试服务句柄（Init 后有效）。
const UiTestServices& Services();

/// 由 harness 在 Init 时写入服务句柄。
void SetServices(const UiTestServices& services);

/// 注册全部 UI 用例到测试引擎（在各用例 .cpp 中实现并汇聚）。
void RegisterAllUiTests(ImGuiTestEngine* engine);

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
