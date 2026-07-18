#include "engine/reflect/component_serializer.h"

namespace dse::reflect {

ComponentSerializer& ComponentSerializer::Get() {
    static ComponentSerializer instance;
    return instance;
}

void ComponentSerializer::Register(ComponentIO io) {
    index_[io.name] = entries_.size();
    entries_.push_back(std::move(io));
}

void ComponentSerializer::SerializeAll(entt::registry& registry, entt::entity entity,
                                       rapidjson::Value& out,
                                       rapidjson::Document::AllocatorType& allocator) const {
    for (auto& entry : entries_) {
        if (entry.serialize) {
            entry.serialize(registry, entity, out, allocator);
        }
    }
}

void ComponentSerializer::DeserializeAll(entt::registry& registry, entt::entity entity,
                                         const rapidjson::Value& components) const {
    for (auto& entry : entries_) {
        if (entry.deserialize) {
            entry.deserialize(registry, entity, components);
        }
    }
}

const ComponentIO* ComponentSerializer::Find(const std::string& name) const {
    auto it = index_.find(name);
    return (it != index_.end()) ? &entries_[it->second] : nullptr;
}

}  // namespace dse::reflect
