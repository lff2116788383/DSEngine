#pragma once

#include <entt/entt.hpp>

class World;

namespace dse {
struct Camera3DComponent;
struct DirectionalLight3DComponent;
struct MeshRendererComponent;
struct ParticleSystem3DComponent;
}

#include "editor_shared_components.h"

struct ParentComponent;
struct TransformComponent;
struct UIRendererComponent;

namespace dse::editor {

struct EditorHierarchyPanelContext {
    World& world;
    entt::registry& registry;
    entt::entity& selected_entity;
    bool read_only = false;
};

void DrawHierarchyPanel(EditorHierarchyPanelContext& context);

} // namespace dse::editor
