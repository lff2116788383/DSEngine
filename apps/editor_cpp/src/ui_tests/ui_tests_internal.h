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
struct ImGuiTestContext;
struct ImGuiWindow;
struct ImVec2;

namespace dse::editor::uitest {

/// 取当前测试服务句柄（Init 后有效）。
const UiTestServices& Services();

/// 由 harness 在 Init 时写入服务句柄。
void SetServices(const UiTestServices& services);

/// 注册全部 UI 用例到测试引擎（在 ui_tests_common.cpp 汇聚分发到各用例文件）。
void RegisterAllUiTests(ImGuiTestEngine* engine);

// ─── 跨用例共享的工具（无捕获 lambda 用例体经此自由函数调用） ──────────────────

/// 当前被测世界中 valid 的实体数（断言基准）；引擎不可用时返回 -1。
int CountValidEntities();

/// 把全部面板可见性开关置真，让被隐藏的面板下一帧起被绘制（覆盖全部面板的前提）。
void EnsureAllPanelsVisible();

/// 关掉全部“可选/开关”面板（与 EnsureAllPanelsVisible 互逆）：dse-panels 用例会把它们全开，
/// 这些面板首帧以浮动窗形式出现并压在 Hierarchy 等常驻面板上，干扰后续依赖屏幕坐标的拖拽用例。
void HideOptionalPanels();

/// 在 g.Windows 中查找“上一帧仍在绘制”的窗口：先精确匹配窗口名，再回退到包含子串。
/// 用于按面板窗口名（含图标前缀/本地化标题时用子串）断言面板已打开。
ImGuiWindow* FindActiveWindow(const char* name_or_substr);

/// 在 Hierarchy 窗口体空白处右键打开上下文菜单，并把 ref 指向弹窗（"//$FOCUSED"）。
void OpenHierarchyContextMenu(ImGuiTestContext* ctx);

/// 手动分步投递一次鼠标拖拽（不走 ItemDragAndDrop）：源激活→跨帧拖动→落点悬停→释放。
/// ItemDragAndDrop 会调 _MakeAimingSpaceOverPos 试图挪开挡住落点的窗口，而 ImGuizmo 每帧建的
/// 全屏 "gizmo" 覆盖窗（NoTitleBar 不可拖动）挪不开会致落点漂移；手动逐帧 Yield 更可靠。
void ManualMouseDrag(ImGuiTestContext* ctx, const ImVec2& src, const ImVec2& dst);

// ─── 各用例文件的注册入口（在各自 .cpp 中实现） ───────────────────────────────
void RegisterHarnessSanityTests(ImGuiTestEngine* engine);
void RegisterHierarchyTests(ImGuiTestEngine* engine);
void RegisterPanelRenderTests(ImGuiTestEngine* engine);
void RegisterInspectorTests(ImGuiTestEngine* engine);
void RegisterConsoleTests(ImGuiTestEngine* engine);
void RegisterMenuBarTests(ImGuiTestEngine* engine);
void RegisterAssetBrowserTests(ImGuiTestEngine* engine);
void RegisterSceneTests(ImGuiTestEngine* engine);
void RegisterUndoTests(ImGuiTestEngine* engine);
void RegisterPlayTests(ImGuiTestEngine* engine);
void RegisterShortcutTests(ImGuiTestEngine* engine);
void RegisterDragDropTests(ImGuiTestEngine* engine);
void RegisterProjectTests(ImGuiTestEngine* engine);
void RegisterNegativeTests(ImGuiTestEngine* engine);
void RegisterSceneTabTests(ImGuiTestEngine* engine);

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
