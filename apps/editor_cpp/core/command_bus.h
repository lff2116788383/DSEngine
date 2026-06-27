#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// EditorCore — CommandBus（写路径门面）
//
// 把类型化 EditorCommand 翻译为 (method, params) 并委派给 ToolSink，
// 再把 JsonRpcResponse 归一成 CommandResult。
//
// 这是"逻辑与 UI 隔离"的写侧契约：UI 只构造命令并 dispatch，永远不直接碰 registry。
// 因为底层走的就是现有 dsengine_* 工具，UI 与自动化天然共用同一条路径、同一撤销栈。
// ─────────────────────────────────────────────────────────────────────────────

#include <string>

#include <rapidjson/document.h>

#include "apps/editor_cpp/core/editor_command.h"
#include "apps/editor_cpp/core/tool_sink.h"

namespace dse::runtime { class EngineInstance; }

namespace dse::editor::core {

/// 命令执行结果。成功时 data 承载工具返回的 result（如新建实体的 entity_id）。
struct CommandResult {
    bool ok = false;
    int error_code = 0;
    std::string error_message;
    rapidjson::Document data;  ///< 仅成功时有意义；失败时为空对象

    CommandResult() : data(rapidjson::kObjectType) {}
};

class CommandBus {
public:
    /// sink 通常绑定到 ControlServer::DispatchTool。
    explicit CommandBus(ToolSink sink) : sink_(std::move(sink)) {}

    /// 下发一条命令。engine 为目标引擎实例（与 ToolSink 约定一致）。
    CommandResult dispatch(const EditorCommand& cmd,
                           dse::runtime::EngineInstance& engine);

    bool has_sink() const { return static_cast<bool>(sink_); }

private:
    ToolSink sink_;
};

} // namespace dse::editor::core
