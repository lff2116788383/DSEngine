#pragma once

#include <string>

#include <entt/entt.hpp>

#include "editor_shared_components.h"

namespace dse::editor {

void CopyRegistry(entt::registry& dst, entt::registry& src);

/// 把 src 实体的已注册组件拷贝到 dst 实体（组件拷贝的单一来源，见 editor_scene_io.cpp）。
/// additive=true 时跳过层级组件（ParentComponent）等仅在整表复制时携带的条目。
void CopyRegisteredComponents(entt::registry& dst, entt::entity dst_entity,
                              entt::registry& src, entt::entity src_entity,
                              bool additive);

void SaveScene(entt::registry& registry, const std::string& filepath);
void LoadScene(entt::registry& registry, const std::string& filepath);
/// 追加式加载：不清空 registry，新实体作为 parent 的子节点（parent==entt::null 时退化为普通加载）
void LoadSceneAdditive(entt::registry& registry, const std::string& filepath, entt::entity parent);

} // namespace dse::editor
