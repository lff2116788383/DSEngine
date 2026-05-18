#pragma once

#include "editor_context.h"

namespace dse::editor {

void DrawInspectorPanel(EditorContext& ctx,
                        void (*draw_ui_layout_inspector)(entt::registry&, entt::entity));

} // namespace dse::editor
