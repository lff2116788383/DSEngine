#include "engine/reflect/component_serializer.h"

#include "engine/reflect/component_reflection.h"
#include "engine/reflect/reflect_json.h"
#include "engine/scene/scene_json_codec.gen.h"

namespace dse::reflect {

ComponentSerializer& ComponentSerializer::Get() {
    static ComponentSerializer instance;
    return instance;
}

void ComponentSerializer::Register(ComponentIO io) {
    index_[io.name] = entries_.size();
    entries_.push_back(std::move(io));
}

void ComponentSerializer::EnsureAutoRegistered() const {
    static bool done = false;
    if (done) return;
    done = true;

    // 一次性惰性初始化：const 入口只允许修改"逻辑上初始化一次"的内部注册表。
    ComponentSerializer& self = const_cast<ComponentSerializer&>(*this);

    // 数据源：反射注册表先就绪（幂等），再用 scene_codec 的按名分发表填充序列化器。
    EnsureCoreReflectionRegistered();

    for (const auto& [name, codec] : dse::scene_codec::GetCodecTable()) {
        // 显式手动注册的条目优先，自动注册不覆盖。
        if (self.index_.count(name) != 0) continue;
        ComponentIO io;
        io.name = name;
        io.type_info = Reflection::Find(name);
        io.serialize = [codec](entt::registry& r, entt::entity e,
                               rapidjson::Value& out,
                               rapidjson::Document::AllocatorType& alloc) {
            codec.serialize(r, e, out, alloc);
        };
        // scene_codec 的 Deserialize_xxx 期望传入"该组件的字段对象"；
        // ComponentIO 语义为传入整个 components 容器，故先按名取出再转发。
        io.deserialize = [name, codec](entt::registry& r, entt::entity e,
                                       const rapidjson::Value& components) {
            if (!components.IsObject() || !components.HasMember(name.c_str())) return;
            codec.deserialize(r, e, components[name.c_str()]);
        };
        self.Register(std::move(io));
    }
}

void ComponentSerializer::SerializeAll(entt::registry& registry, entt::entity entity,
                                       rapidjson::Value& out,
                                       rapidjson::Document::AllocatorType& allocator) const {
    EnsureAutoRegistered();
    for (auto& entry : entries_) {
        if (entry.serialize) {
            entry.serialize(registry, entity, out, allocator);
        }
    }
}

void ComponentSerializer::DeserializeAll(entt::registry& registry, entt::entity entity,
                                         const rapidjson::Value& components) const {
    EnsureAutoRegistered();
    for (auto& entry : entries_) {
        if (entry.deserialize) {
            entry.deserialize(registry, entity, components);
        }
    }
}

const std::vector<ComponentIO>& ComponentSerializer::GetAll() const {
    EnsureAutoRegistered();
    return entries_;
}

const ComponentIO* ComponentSerializer::Find(const std::string& name) const {
    EnsureAutoRegistered();
    auto it = index_.find(name);
    return (it != index_.end()) ? &entries_[it->second] : nullptr;
}

}  // namespace dse::reflect
