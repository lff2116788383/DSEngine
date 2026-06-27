#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// EditorCore — ToolSink
//
// EditorCore（命令/查询门面层）与具体执行后端之间的唯一耦合点。
// 门面层只依赖一个函数对象：给定 (method, params, engine) 返回 JsonRpcResponse。
// app 层在装配时把它绑定到 ControlServer::DispatchTool（已存在、零 UI 框架依赖），
// 从而让 EditorCore 完整复用现有 37 个 dsengine_* 工具的实现，而**不重复任何逻辑**。
//
// ⚠ 本文件及 core/ 下所有文件**禁止**包含/命名任何 UI 框架（即时模式 GUI、
//   gizmo 库等）。该约束由 scripts/check_core_ui_free + EditorCoreIsolation gtest 强制。
// ─────────────────────────────────────────────────────────────────────────────

#include <functional>
#include <string>

#include "apps/editor_cpp/src/editor_control_server.h"  // JsonRpcResponse（无 UI 框架依赖）

namespace dse::runtime { class EngineInstance; }

namespace dse::editor::core {

/// 执行后端：把一次工具调用委派给同进程的 tool handler。
/// 典型绑定：[&server](m, p, e) { return server.DispatchTool(m, p, e); }
using ToolSink = std::function<dse::editor::JsonRpcResponse(
    const std::string& method,
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine)>;

} // namespace dse::editor::core
