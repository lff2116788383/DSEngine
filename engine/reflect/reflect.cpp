#include "engine/reflect/reflect.h"

#include <deque>
#include <unordered_map>

namespace dse::reflect {

namespace {

// TypeInfo 存放在 deque 中以保证指针稳定（map 仅存指针）。
struct Registry {
    std::deque<TypeInfo> storage;
    std::unordered_map<std::string, TypeInfo*> by_name;
    std::unordered_map<std::type_index, TypeInfo*> by_type;
};

Registry& registry() {
    static Registry r;
    return r;
}

}  // namespace

TypeBuilder Reflection::Add(const char* name, std::type_index type, std::size_t size) {
    Registry& r = registry();
    auto it = r.by_name.find(name);
    TypeInfo* ti = nullptr;
    if (it != r.by_name.end()) {
        ti = it->second;
        ti->fields.clear();  // 重复注册：重建字段
    } else {
        r.storage.emplace_back();
        ti = &r.storage.back();
        ti->name = name;
        r.by_name.emplace(ti->name, ti);
    }
    ti->type = type;
    ti->size = size;
    r.by_type[type] = ti;
    return TypeBuilder(ti);
}

const TypeInfo* Reflection::Find(const std::string& name) {
    Registry& r = registry();
    auto it = r.by_name.find(name);
    return it != r.by_name.end() ? it->second : nullptr;
}

const TypeInfo* Reflection::Find(std::type_index type) {
    Registry& r = registry();
    auto it = r.by_type.find(type);
    return it != r.by_type.end() ? it->second : nullptr;
}

std::vector<const TypeInfo*> Reflection::All() {
    Registry& r = registry();
    std::vector<const TypeInfo*> out;
    out.reserve(r.storage.size());
    for (const TypeInfo& ti : r.storage) out.push_back(&ti);
    return out;
}

}  // namespace dse::reflect
