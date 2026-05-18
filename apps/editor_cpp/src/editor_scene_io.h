#pragma once

#include <string>

#include <entt/entt.hpp>

#include "editor_shared_components.h"

namespace dse::editor {

void CopyRegistry(entt::registry& dst, entt::registry& src);
void SaveScene(entt::registry& registry, const std::string& filepath);
void LoadScene(entt::registry& registry, const std::string& filepath);

} // namespace dse::editor
