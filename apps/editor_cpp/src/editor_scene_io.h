#pragma once

#include <string>

#include <entt/entt.hpp>

#include "editor_shared_components.h"

namespace dse::editor {

void CopyRegistry(entt::registry& dst, entt::registry& src);
void SaveScene(entt::registry& registry, const std::string& filepath);
void LoadScene(entt::registry& registry, const std::string& filepath);
/// 追加式加载：不清空 registry，新实体作为 parent 的子节点（parent==entt::null 时退化为普通加载）
void LoadSceneAdditive(entt::registry& registry, const std::string& filepath, entt::entity parent);

} // namespace dse::editor
