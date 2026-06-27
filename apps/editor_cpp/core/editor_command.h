#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// EditorCore — 类型化编辑器命令（写路径）
//
// 把"用户能做的每件事"建模为类型化命令，与现有 dsengine_* 工具一一对应。
// 所有 UI 前端（当前前端 / 未来的 Qt / 自研）通过 CommandBus 下发这些命令来修改状态，
// 从而统一写路径、统一撤销栈，并让前端与具体执行逻辑解耦。
//
// Phase 0 仅落一组有代表性的起步命令（create / delete / rename / reparent），
// 验证门面模式可编译、可测试、确实复用现有工具逻辑。后续 Phase 1 再补齐其余命令。
//
// ⚠ core/ 禁止依赖任何 UI 框架。实体以 uint32_t id 表达，避免在公共命令结构里
//   引入 entt 等执行层细节。
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace dse::editor::core {

/// 实体句柄的稳定整型表示（对应 entt::entity 的底层 uint32，含版本位）。
using EntityId = std::uint32_t;

/// 创建实体 → dsengine_entity_create
struct CreateEntityCmd {
    std::string name;                  ///< 可空；空则由工具取默认名
};

/// 删除实体 → dsengine_entity_delete
struct DeleteEntityCmd {
    EntityId entity_id = 0;
};

/// 重命名实体 → dsengine_entity_modify { name }
struct RenameEntityCmd {
    EntityId entity_id = 0;
    std::string name;
};

/// 重设父级 → dsengine_entity_reparent
/// parent 为空表示挂到根（detach）。
struct ReparentEntityCmd {
    EntityId entity_id = 0;
    std::optional<EntityId> parent;
};

/// 所有受支持命令的合集。新增命令时在此追加，并在 command_bus.cpp 的 visitor 里补一支。
using EditorCommand = std::variant<
    CreateEntityCmd,
    DeleteEntityCmd,
    RenameEntityCmd,
    ReparentEntityCmd>;

} // namespace dse::editor::core
