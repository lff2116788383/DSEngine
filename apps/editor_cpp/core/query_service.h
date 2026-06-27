#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// EditorCore — QueryService（读路径门面）
//
// 把现有只读工具（scene_get_state / entity_get_components ...）包装成类型化 ViewModel。
// 这是"逻辑与 UI 隔离"的读侧契约：UI 只消费 VM 快照，不直接遍历 registry。
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <string>

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

    /// Inspector 单实体明细（含 Transform）← entity_get_state
    InspectorVM inspector(std::uint32_t entity_id,
                          dse::runtime::EngineInstance& engine);

    /// 编辑器全局状态 ← editor_get_state
    EditorStateVM editorState(dse::runtime::EngineInstance& engine);

    /// 当前选择集 ← selection_get
    SelectionVM selection(dse::runtime::EngineInstance& engine);

    /// 撤销/重做栈 ← undo_history
    UndoHistoryVM undoHistory(dse::runtime::EngineInstance& engine);

    /// 按名查找 ← entity_find_by_name（partial=true 为子串匹配）
    FindResultVM findByName(const std::string& name, bool partial,
                            dse::runtime::EngineInstance& engine);

    /// 运行指标 ← editor_get_metrics
    MetricsVM metrics(dse::runtime::EngineInstance& engine);

    /// 完整层级树（由 scene_get_state 的 parent_id/sibling_index 重建）。
    /// 把 hierarchy 面板原先的"遍历 registry 构树"逻辑搬上读路径。
    HierarchyVM hierarchy(dse::runtime::EngineInstance& engine);

    bool has_sink() const { return static_cast<bool>(sink_); }

private:
    ToolSink sink_;
};

} // namespace dse::editor::core
