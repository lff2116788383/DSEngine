#include "engine/reflect/reflect_json.h"

#include <algorithm>
#include <functional>

#include <glm/glm.hpp>

namespace dse::reflect {

namespace {

rapidjson::Value MakeFloatArray(const float* v, int n,
                                rapidjson::Document::AllocatorType& a) {
    rapidjson::Value arr(rapidjson::kArrayType);
    for (int i = 0; i < n; ++i) arr.PushBack(v[i], a);
    return arr;
}

bool ReadFloatArray(const rapidjson::Value& v, float* out, int n) {
    if (!v.IsArray() || static_cast<int>(v.Size()) < n) return false;
    for (int i = 0; i < n; ++i) {
        if (!v[i].IsNumber()) return false;
        out[i] = v[i].GetFloat();
    }
    return true;
}

double ClampToRange(double x, const FieldAttributes& attr) {
    if (!attr.has_range) return x;
    return std::min(attr.max_value, std::max(attr.min_value, x));
}

// 写出一个"叶子值"（标量/向量/字符串/枚举/嵌套 struct）。enum_value 仅当 t==Enum 时使用，
// 由调用方按实例或元素读出。p 为值指针（struct 递归用）。
rapidjson::Value WriteLeaf(FieldType t, const void* p, const TypeInfo* struct_info,
                           const EnumInfo* enum_info, long long enum_value,
                           rapidjson::Document::AllocatorType& a) {
    rapidjson::Value v;
    switch (t) {
        case FieldType::Bool:   v.SetBool(*static_cast<const bool*>(p)); break;
        case FieldType::Int:    v.SetInt(*static_cast<const int*>(p)); break;
        case FieldType::UInt:   v.SetUint(*static_cast<const unsigned int*>(p)); break;
        case FieldType::Float:  v.SetFloat(*static_cast<const float*>(p)); break;
        case FieldType::Double: v.SetDouble(*static_cast<const double*>(p)); break;
        case FieldType::Vec2:   v = MakeFloatArray(&static_cast<const glm::vec2*>(p)->x, 2, a); break;
        case FieldType::Vec3:   v = MakeFloatArray(&static_cast<const glm::vec3*>(p)->x, 3, a); break;
        case FieldType::Vec4:   v = MakeFloatArray(&static_cast<const glm::vec4*>(p)->x, 4, a); break;
        case FieldType::String: {
            const std::string& s = *static_cast<const std::string*>(p);
            v.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), a);
            break;
        }
        case FieldType::Enum: {
            const char* name = nullptr;
            if (enum_info) {
                for (const EnumEntry& e : enum_info->entries) {
                    if (e.value == enum_value) { name = e.name.c_str(); break; }
                }
            }
            if (name) v.SetString(name, a);
            else      v.SetInt64(enum_value);
            break;
        }
        case FieldType::Struct:
            v.SetObject();
            if (struct_info) SerializeReflected(*struct_info, p, v, a);
            break;
        case FieldType::Array:
            v.SetNull();  // 容器在外层处理，不应到这里
            break;
    }
    return v;
}

// 读取一个"叶子值"写回 p。set_enum 在 t==Enum 时使用（按实例或元素绑定）。
// attr 用于数值钳制（按 range）。
void ReadLeaf(FieldType t, void* p, const TypeInfo* struct_info, const EnumInfo* enum_info,
              const std::function<void(long long)>& set_enum, const FieldAttributes& attr,
              const rapidjson::Value& v) {
    switch (t) {
        case FieldType::Bool:   if (v.IsBool())   *static_cast<bool*>(p) = v.GetBool(); break;
        case FieldType::Int:    if (v.IsInt())    *static_cast<int*>(p) = static_cast<int>(ClampToRange(v.GetInt(), attr)); break;
        case FieldType::UInt:
            if (v.IsUint())              *static_cast<unsigned int*>(p) = static_cast<unsigned int>(ClampToRange(v.GetUint(), attr));
            else if (v.IsInt() && v.GetInt() >= 0) *static_cast<unsigned int*>(p) = static_cast<unsigned int>(ClampToRange(v.GetInt(), attr));
            break;
        case FieldType::Float:  if (v.IsNumber()) *static_cast<float*>(p) = static_cast<float>(ClampToRange(v.GetDouble(), attr)); break;
        case FieldType::Double: if (v.IsNumber()) *static_cast<double*>(p) = ClampToRange(v.GetDouble(), attr); break;
        case FieldType::Vec2:   ReadFloatArray(v, &static_cast<glm::vec2*>(p)->x, 2); break;
        case FieldType::Vec3:   ReadFloatArray(v, &static_cast<glm::vec3*>(p)->x, 3); break;
        case FieldType::Vec4:   ReadFloatArray(v, &static_cast<glm::vec4*>(p)->x, 4); break;
        case FieldType::String: if (v.IsString()) *static_cast<std::string*>(p) = v.GetString(); break;
        case FieldType::Enum:
            if (v.IsString()) {
                if (enum_info) {
                    for (const EnumEntry& e : enum_info->entries) {
                        if (e.name == v.GetString()) { if (set_enum) set_enum(e.value); break; }
                    }
                }
            } else if (v.IsInt64() || v.IsInt()) {
                if (set_enum) set_enum(v.GetInt64());
            }
            break;
        case FieldType::Struct:
            if (v.IsObject() && struct_info) DeserializeReflected(*struct_info, p, v);
            break;
        case FieldType::Array:
            break;  // 容器在外层处理
    }
}

}  // namespace

void SerializeReflected(const TypeInfo& type, const void* instance,
                        rapidjson::Value& out,
                        rapidjson::Document::AllocatorType& allocator) {
    for (const FieldInfo& f : type.fields) {
        if (f.attr.hidden) continue;
        const void* p = f.cget(instance);
        rapidjson::Value v;
        if (f.type == FieldType::Array) {
            v.SetArray();
            const std::size_t n = f.container_size ? f.container_size(p) : 0;
            for (std::size_t i = 0; i < n; ++i) {
                const void* ep = f.container_celem(p, i);
                long long ev = (f.element_type == FieldType::Enum && f.elem_get_enum)
                                   ? f.elem_get_enum(ep) : 0;
                rapidjson::Value elem =
                    WriteLeaf(f.element_type, ep, f.element_struct_info, f.element_enum_info, ev, allocator);
                v.PushBack(elem, allocator);
            }
        } else {
            long long ev = (f.type == FieldType::Enum) ? f.get_enum(instance) : 0;
            v = WriteLeaf(f.type, p, f.struct_info, f.enum_info, ev, allocator);
        }
        out.AddMember(rapidjson::Value(f.name.c_str(), allocator), v, allocator);
    }
}

void DeserializeReflected(const TypeInfo& type, void* instance,
                          const rapidjson::Value& in) {
    if (!in.IsObject()) return;
    for (const FieldInfo& f : type.fields) {
        if (f.attr.hidden) continue;
        if (!in.HasMember(f.name.c_str())) continue;
        const rapidjson::Value& v = in[f.name.c_str()];
        void* p = f.get(instance);

        if (f.type == FieldType::Array) {
            if (!v.IsArray()) continue;
            std::size_t json_n = v.Size();
            if (!f.container_fixed && f.container_resize) f.container_resize(p, json_n);
            std::size_t cap = f.container_size ? f.container_size(p) : 0;
            std::size_t n = std::min<std::size_t>(json_n, cap);
            for (std::size_t i = 0; i < n; ++i) {
                void* ep = f.container_elem(p, i);
                std::function<void(long long)> set_enum;
                if (f.element_type == FieldType::Enum && f.elem_set_enum)
                    set_enum = [&f, ep](long long x) { f.elem_set_enum(ep, x); };
                ReadLeaf(f.element_type, ep, f.element_struct_info, f.element_enum_info,
                         set_enum, f.attr, v[static_cast<rapidjson::SizeType>(i)]);
            }
            continue;
        }

        std::function<void(long long)> set_enum;
        if (f.type == FieldType::Enum) set_enum = [&f, instance](long long x) { f.set_enum(instance, x); };
        ReadLeaf(f.type, p, f.struct_info, f.enum_info, set_enum, f.attr, v);
    }
}

}  // namespace dse::reflect
