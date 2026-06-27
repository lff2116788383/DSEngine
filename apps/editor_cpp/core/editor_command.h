#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// EditorCore — 类型化编辑器命令（写路径）
//
// 把"用户能做的每件事"建模为类型化命令，与现有 dsengine_* 工具一一对应。
// 所有 UI 前端（当前前端 / 未来的 Qt / 自研）通过 CommandBus 下发这些命令来修改状态，
// 从而统一写路径、统一撤销栈，并让前端与具体执行逻辑解耦。
//
// 覆盖范围：全部"写 / 动作"类工具。只读工具（get_state / get_components /
// selection_get / undo_history / find_by_name / get_metrics 等）属于读路径，
// 由 QueryService + ViewModel 承载，不在此处。纯自动化/宿主生命周期工具
// （ping / list_tools / editor_idle / editor_quit / editor_screenshot）不进 UI
// 命令词汇表。
//
// ⚠ core/ 禁止依赖/命名任何 UI 框架。实体以 uint32_t id 表达，避免在公共命令结构里
//   引入 entt 等执行层细节。
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dse::editor::core {

/// 实体句柄的稳定整型表示（对应 entt::entity 的底层 uint32，含版本位）。
using EntityId = std::uint32_t;

/// 三分量数组（position / rotation(欧拉角,度) / scale 等）。
using Vec3 = std::array<float, 3>;

// ── 实体 ──────────────────────────────────────────────────────────────────

/// 创建实体 → dsengine_entity_create
struct CreateEntityCmd {
    std::string name;                      ///< 可空；空则由工具取默认名
    std::vector<std::string> components;   ///< 额外组件类型（如 "UIRenderer"），由工具 AddComponentByType 解析
};

/// 删除实体 → dsengine_entity_delete
struct DeleteEntityCmd {
    EntityId entity_id = 0;
};

/// 批量删除 → dsengine_entity_batch_delete
struct BatchDeleteEntitiesCmd {
    std::vector<EntityId> entity_ids;
};

/// 重命名实体 → dsengine_entity_modify { name }
struct RenameEntityCmd {
    EntityId entity_id = 0;
    std::string name;
};

/// 修改 Transform → dsengine_entity_modify { position/rotation/scale }
/// 任意分量可空：只下发提供的字段。
struct TransformEntityCmd {
    EntityId entity_id = 0;
    std::optional<Vec3> position;
    std::optional<Vec3> rotation;      ///< 欧拉角，单位：度
    std::optional<Vec3> scale;
};

/// 重设父级 → dsengine_entity_reparent
/// parent 为空表示挂到根（detach）；sibling_index 可指定插入序。
struct ReparentEntityCmd {
    EntityId entity_id = 0;
    std::optional<EntityId> parent;
    std::optional<int> sibling_index;
};

/// 复制实体 → dsengine_entity_duplicate
struct DuplicateEntityCmd {
    EntityId entity_id = 0;
};

/// 添加组件 → dsengine_entity_add_component
struct AddComponentCmd {
    EntityId entity_id = 0;
    std::string type;
};

/// 移除组件 → dsengine_entity_remove_component
struct RemoveComponentCmd {
    EntityId entity_id = 0;
    std::string type;
};

// ── 选择 ──────────────────────────────────────────────────────────────────

/// 设置选择集 → dsengine_selection_set
struct SetSelectionCmd {
    std::vector<EntityId> entity_ids;
};

/// 清空选择集 → dsengine_selection_clear
struct ClearSelectionCmd {};

// ── 场景 ──────────────────────────────────────────────────────────────────

/// 新建空场景 → dsengine_scene_new
struct NewSceneCmd {};

/// 保存场景 → dsengine_scene_save（path 为空则落回当前场景文件）
struct SaveSceneCmd {
    std::optional<std::string> path;
};

/// 加载场景 → dsengine_scene_load
struct LoadSceneCmd {
    std::string path;
};

// ── 资产 / 材质 / 脚本 ──────────────────────────────────────────────────────

/// 导入资产 → dsengine_asset_import（type 可空，工具按扩展名自动判别）
struct ImportAssetCmd {
    std::string path;
    std::optional<std::string> type;
};

/// 新建材质 → dsengine_material_create
struct CreateMaterialCmd {
    std::string name;
    std::optional<std::string> shader_variant;
    std::optional<std::string> save_path;
};

/// 新建脚本文件 → dsengine_script_create
struct CreateScriptCmd {
    std::string path;
    std::string content;
};

// ── 预制体 ──────────────────────────────────────────────────────────────────

/// 保存预制体 → dsengine_prefab_save
struct SavePrefabCmd {
    EntityId entity_id = 0;
    std::string path;
};

/// 实例化预制体 → dsengine_prefab_instantiate
struct InstantiatePrefabCmd {
    std::string path;
};

// ── 编辑器生命周期 / 动作 ────────────────────────────────────────────────────

/// 进入 Play 模式 → dsengine_editor_play
struct PlayCmd {};

/// 退出 Play 模式 → dsengine_editor_stop
struct StopCmd {};

/// 撤销 → dsengine_editor_undo
struct UndoCmd {};

/// 重做 → dsengine_editor_redo
struct RedoCmd {};

/// 打开工程 → dsengine_project_open
struct OpenProjectCmd {
    std::string path;
};

/// 执行 Lua → dsengine_lua_execute（Play 模式下才会真正求值）
struct ExecuteLuaCmd {
    std::string code;
};

/// 所有受支持命令的合集。新增命令时在此追加，并在 command_bus.cpp 的 visitor 里补一支。
using EditorCommand = std::variant<
    CreateEntityCmd,
    DeleteEntityCmd,
    BatchDeleteEntitiesCmd,
    RenameEntityCmd,
    TransformEntityCmd,
    ReparentEntityCmd,
    DuplicateEntityCmd,
    AddComponentCmd,
    RemoveComponentCmd,
    SetSelectionCmd,
    ClearSelectionCmd,
    NewSceneCmd,
    SaveSceneCmd,
    LoadSceneCmd,
    ImportAssetCmd,
    CreateMaterialCmd,
    CreateScriptCmd,
    SavePrefabCmd,
    InstantiatePrefabCmd,
    PlayCmd,
    StopCmd,
    UndoCmd,
    RedoCmd,
    OpenProjectCmd,
    ExecuteLuaCmd>;

} // namespace dse::editor::core
