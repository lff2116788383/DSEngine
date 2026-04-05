#pragma once

#include <entt/entt.hpp>

namespace dse::runtime {
class EngineInstance;
}

void DrawEditorToolbar(dse::runtime::EngineInstance& engine,
                       entt::registry& registry,
                       entt::entity& selected_entity);
