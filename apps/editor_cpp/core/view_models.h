#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// EditorCore — ViewModel（读路径）
//
// 不可变 POD 快照，只承载 UI 渲染所需的数据，不含任何 UI 框架类型、不含 entt 句柄语义
// 之外的执行细节。前端拿到 VM 直接画，永远不遍历 registry。
//
// Phase 0 仅落 SceneSummaryVM / EntityComponentsVM 两个起步快照，验证读路径门面。
// 后续 Phase 2 再补 HierarchyVM / InspectorVM 等完整视图模型。
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <string>
#include <vector>

namespace dse::editor::core {

/// 场景概览快照 ← dsengine_scene_get_state
struct SceneSummaryVM {
    bool valid = false;              ///< 查询是否成功
    std::string editor_state;        ///< "edit" / "play" / ...
    int entity_count = 0;            ///< 存活实体数（已修正：仅计 valid()）
};

/// 单实体组件清单快照 ← dsengine_entity_get_components
struct EntityComponentsVM {
    bool valid = false;
    std::uint32_t entity_id = 0;
    std::vector<std::string> components;  ///< 组件类型名列表
};

} // namespace dse::editor::core
