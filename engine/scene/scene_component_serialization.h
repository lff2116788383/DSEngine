#ifndef DSE_SCENE_COMPONENT_SERIALIZATION_H
#define DSE_SCENE_COMPONENT_SERIALIZATION_H

#include <entt/entt.hpp>
#include <rapidjson/document.h>

using Entity = entt::entity;

namespace scene::component_io {

void SerializeExtendedComponents(entt::registry& registry, Entity entity,
                               rapidjson::Value& components,
                               rapidjson::Document::AllocatorType& allocator);

void DeserializeExtendedComponents(entt::registry& registry, Entity entity,
                                   const rapidjson::Value& components);

} // namespace scene::component_io

#endif // DSE_SCENE_COMPONENT_SERIALIZATION_H
