#pragma once

#include <entt/entt.hpp>

namespace dse::editor {

struct EditorInspectorPanelContext {
    entt::registry& registry;
    entt::entity selected_entity;
    bool is_2d;
    bool& inspector_active;
    bool& inspector_static;
};

void DrawInspectorPanel(EditorInspectorPanelContext& context,
                        void (*draw_ui_layout_inspector)(entt::registry&, entt::entity));

} // namespace dse::editor
