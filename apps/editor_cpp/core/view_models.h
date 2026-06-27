#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// EditorCore — ViewModel（读路径）
//
// 不可变 POD 快照，只承载 UI 渲染所需的数据，不含任何 UI 框架类型、不含 entt 句柄
// 语义之外的执行细节。前端拿到 VM 直接画，永远不遍历 registry。
//
// 覆盖：全部只读工具（scene/editor/entity/selection/undo/find/metrics）+ 由
// scene_get_state 重建的层级树。写路径见 editor_command.h / command_bus.h。
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dse::editor::core {

/// 场景概览快照 ← dsengine_scene_get_state
struct SceneSummaryVM {
    bool valid = false;              ///< 查询是否成功
    std::string editor_state;        ///< "edit" / "play" / "pause"
    int entity_count = 0;            ///< 存活实体数（仅计 valid()）
};

/// 单实体组件类型名清单 ← dsengine_entity_get_components
struct EntityComponentsVM {
    bool valid = false;
    std::uint32_t entity_id = 0;
    std::vector<std::string> components;  ///< 组件类型名列表
};

/// Transform 快照（rotation 为四元数 xyzw）。
struct TransformVM {
    bool present = false;
    std::array<float, 3> position{{0.f, 0.f, 0.f}};
    std::array<float, 4> rotation{{0.f, 0.f, 0.f, 1.f}};
    std::array<float, 3> scale{{1.f, 1.f, 1.f}};
};

/// Inspector 单实体明细 ← dsengine_entity_get_state
struct InspectorVM {
    bool valid = false;
    std::uint32_t entity_id = 0;
    std::string name;
    TransformVM transform;
    std::vector<std::string> components;  ///< 组件类型名列表
};

/// 编辑器全局状态 ← dsengine_editor_get_state
struct EditorStateVM {
    bool valid = false;
    std::string editor_state;
    int entity_count = 0;
    std::string data_root;           ///< 可空
};

/// 选择集 ← dsengine_selection_get
struct SelectionVM {
    bool valid = false;
    std::vector<std::uint32_t> entity_ids;
    int count = 0;
    bool has_primary = false;
    std::uint32_t primary_id = 0;
};

/// 撤销/重做栈 ← dsengine_undo_history
struct UndoHistoryVM {
    bool valid = false;
    bool can_undo = false;
    bool can_redo = false;
    int undo_count = 0;
    int redo_count = 0;
    std::string undo_description;
    std::string redo_description;
    std::vector<std::string> undo_history;
    std::vector<std::string> redo_history;
};

/// 按名查找命中项。
struct EntityMatchVM {
    std::uint32_t entity_id = 0;
    std::string name;
};

/// 按名查找结果 ← dsengine_entity_find_by_name
struct FindResultVM {
    bool valid = false;
    int count = 0;
    bool has_first = false;
    std::uint32_t first_id = 0;
    std::vector<EntityMatchVM> matches;
};

/// 运行指标 ← dsengine_editor_get_metrics
struct MetricsVM {
    bool valid = false;
    float fps = 0.f;
    float delta_ms = 0.f;
    int draw_calls = 0;
    int entity_count = 0;
    float time_since_startup = 0.f;
    std::string editor_state;
};

/// 层级树节点。children/roots 用索引引用 HierarchyVM::nodes，避免在 POD 里持有指针。
struct HierarchyNodeVM {
    std::uint32_t entity_id = 0;
    std::string name;
    std::vector<std::string> components;     ///< 组件类型名列表
    std::vector<std::size_t> children;       ///< 指向 HierarchyVM::nodes 的下标，按 sibling_index 排序
    int sibling_index = 0;
    bool has_parent = false;
    std::uint32_t parent_id = 0;
};

/// 层级树快照 ← dsengine_scene_get_state（含 parent_id / sibling_index）
/// 把 hierarchy 面板原先"遍历 registry 构树"的逻辑搬到读路径。
struct HierarchyVM {
    bool valid = false;
    std::vector<HierarchyNodeVM> nodes;      ///< 扁平存储，全部实体
    std::vector<std::size_t> roots;          ///< 根节点下标，按 sibling_index 排序
};

} // namespace dse::editor::core
