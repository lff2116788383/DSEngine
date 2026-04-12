#pragma once

#include <entt/entt.hpp>

namespace dse::editor {

struct EditorInspectorPanelContext {
    entt::registry& registry;
    entt::entity selected_entity;
    bool is_2d;
    bool& inspector_active;
    bool& inspector_static;
    bool read_only = false;
};

void DrawInspectorPanel(EditorInspectorPanelContext& context,
                        void (*draw_ui_layout_inspector)(entt::registry&, entt::entity));

} // namespace dse::editor
