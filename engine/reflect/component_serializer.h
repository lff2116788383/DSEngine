#ifndef DSE_REFLECT_COMPONENT_SERIALIZER_H
#define DSE_REFLECT_COMPONENT_SERIALIZER_H

#include <entt/entt.hpp>
#include <rapidjson/document.h>
#include "engine/reflect/reflect.h"
#include "engine/reflect/reflect_json.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace dse::reflect {

/// Per-component serialize/deserialize function pair.
struct ComponentIO {
    using SerializeFn   = std::function<void(entt::registry&, entt::entity,
                                            rapidjson::Value&, rapidjson::Document::AllocatorType&)>;
    using DeserializeFn = std::function<void(entt::registry&, entt::entity, const rapidjson::Value&)>;

    std::string     name;
    const TypeInfo* type_info = nullptr;
    SerializeFn     serialize;
    DeserializeFn   deserialize;
};

/// Central registry that unifies component reflection and serialization.
/// Components register once; both Inspector reflection and scene I/O are handled.
class ComponentSerializer {
public:
    static ComponentSerializer& Get();

    /// Register a component with reflection-based serialize/deserialize.
    void Register(ComponentIO io);

    /// Serialize all registered components on an entity.
    void SerializeAll(entt::registry& registry, entt::entity entity,
                      rapidjson::Value& out, rapidjson::Document::AllocatorType& allocator) const;

    /// Deserialize all recognized components onto an entity.
    void DeserializeAll(entt::registry& registry, entt::entity entity,
                        const rapidjson::Value& components) const;

    /// Query registered entries.
    const std::vector<ComponentIO>& GetAll() const { return entries_; }
    const ComponentIO* Find(const std::string& name) const;

private:
    ComponentSerializer() = default;
    std::vector<ComponentIO> entries_;
    std::unordered_map<std::string, size_t> index_;
};

/// Helper macro: register a component whose TypeInfo is already in Reflection.
/// Generates serialize/deserialize lambdas that use SerializeReflected / DeserializeReflected.
#define DSE_REGISTER_SERIALIZABLE_COMPONENT(Component, json_key)                              \
    do {                                                                                       \
        const auto* ti = dse::reflect::Reflection::Find<Component>();                          \
        if (!ti) break;                                                                        \
        dse::reflect::ComponentIO io;                                                          \
        io.name = (json_key);                                                                  \
        io.type_info = ti;                                                                     \
        io.serialize = [ti](entt::registry& r, entt::entity e,                                 \
                           rapidjson::Value& out, rapidjson::Document::AllocatorType& alloc) {  \
            if (!r.all_of<Component>(e)) return;                                               \
            const auto& comp = r.get<Component>(e);                                            \
            rapidjson::Value v(rapidjson::kObjectType);                                        \
            dse::reflect::SerializeReflected(*ti, &comp, v, alloc);                            \
            out.AddMember(rapidjson::Value((json_key), alloc), v.Move(), alloc);               \
        };                                                                                     \
        io.deserialize = [ti](entt::registry& r, entt::entity e,                               \
                              const rapidjson::Value& in) {                                    \
            if (!in.HasMember((json_key))) return;                                             \
            auto& comp = r.get_or_emplace<Component>(e);                                      \
            dse::reflect::DeserializeReflected(*ti, &comp, in[(json_key)]);                    \
        };                                                                                     \
        dse::reflect::ComponentSerializer::Get().Register(std::move(io));                       \
    } while (0)

}  // namespace dse::reflect

#endif  // DSE_REFLECT_COMPONENT_SERIALIZER_H
