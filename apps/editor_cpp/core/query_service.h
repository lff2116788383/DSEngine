#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// EditorCore — QueryService（读路径门面）
//
// 把现有只读工具（scene_get_state / entity_get_components ...）包装成类型化 ViewModel。
// 这是"逻辑与 UI 隔离"的读侧契约：UI 只消费 VM 快照，不直接遍历 registry。
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>

#include "apps/editor_cpp/core/tool_sink.h"
#include "apps/editor_cpp/core/view_models.h"

namespace dse::runtime { class EngineInstance; }

namespace dse::editor::core {

class QueryService {
public:
    /// sink 通常绑定到 ControlServer::DispatchTool。
    explicit QueryService(ToolSink sink) : sink_(std::move(sink)) {}

    /// 场景概览（实体数、编辑器状态）。
    SceneSummaryVM sceneSummary(dse::runtime::EngineInstance& engine);

    /// 单实体组件清单。
    EntityComponentsVM entityComponents(std::uint32_t entity_id,
                                        dse::runtime::EngineInstance& engine);

    bool has_sink() const { return static_cast<bool>(sink_); }

private:
    ToolSink sink_;
};

} // namespace dse::editor::core
