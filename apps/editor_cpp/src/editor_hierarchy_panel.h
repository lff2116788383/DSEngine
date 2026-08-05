#pragma once

#include "editor_context.h"
#include "editor_shared_components.h"

namespace dse {
struct Camera3DComponent;
struct DirectionalLight3DComponent;
struct MeshRendererComponent;
struct ParticleSystem3DComponent;
}

struct ParentComponent;
struct TransformComponent;
struct UIRendererComponent;

namespace dse::editor {

void DrawHierarchyPanel(EditorContext& ctx);

/// 触发 Hierarchy 内联重命名（由 F2 快捷键调用）
void BeginHierarchyRename(entt::entity entity, const std::string& current_name);

// ── 写路径收敛（快捷键与层级面板共用）────────────────────────────────────────
// 经 CommandBus 复用 dsengine_entity_* 工具 + 统一撤销栈；command_bus 缺失时
// 退回与工具等价的全组件快照直写（含 LambdaCommand undo/redo）。

/// 创建实体（name + 额外组件类型）。返回新实体；失败返回 entt::null。
entt::entity CreateEntityViaBus(EditorContext& context, const std::string& name,
                                const std::vector<std::string>& components);

/// 删除实体（全组件快照，撤销可完整还原）。
void DeleteEntityViaBus(EditorContext& context, entt::entity target);

/// 复制实体（全组件快照复制，挂根 + 改名 + 偏移）。返回副本；失败返回 entt::null。
entt::entity DuplicateEntityViaBus(EditorContext& context, entt::entity source);

} // namespace dse::editor
