#include "engine/reflect/reflect_json.h"

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

}  // namespace

void SerializeReflected(const TypeInfo& type, const void* instance,
                        rapidjson::Value& out,
                        rapidjson::Document::AllocatorType& allocator) {
    for (const FieldInfo& f : type.fields) {
        if (f.attr.hidden) continue;
        const void* p = f.cget(instance);
        rapidjson::Value v;
        switch (f.type) {
            case FieldType::Bool:   v.SetBool(*static_cast<const bool*>(p)); break;
            case FieldType::Int:    v.SetInt(*static_cast<const int*>(p)); break;
            case FieldType::UInt:   v.SetUint(*static_cast<const unsigned int*>(p)); break;
            case FieldType::Float:  v.SetFloat(*static_cast<const float*>(p)); break;
            case FieldType::Double: v.SetDouble(*static_cast<const double*>(p)); break;
            case FieldType::Vec2: {
                const glm::vec2& vv = *static_cast<const glm::vec2*>(p);
                v = MakeFloatArray(&vv.x, 2, allocator);
                break;
            }
            case FieldType::Vec3: {
                const glm::vec3& vv = *static_cast<const glm::vec3*>(p);
                v = MakeFloatArray(&vv.x, 3, allocator);
                break;
            }
            case FieldType::Vec4: {
                const glm::vec4& vv = *static_cast<const glm::vec4*>(p);
                v = MakeFloatArray(&vv.x, 4, allocator);
                break;
            }
            case FieldType::String: {
                const std::string& s = *static_cast<const std::string*>(p);
                v.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), allocator);
                break;
            }
            case FieldType::Enum: {
                // 优先写出枚举名（对重排序/数值变更稳健）；未知值回退为整数。
                const long long iv = f.get_enum(instance);
                const char* name = nullptr;
                if (f.enum_info) {
                    for (const EnumEntry& e : f.enum_info->entries) {
                        if (e.value == iv) { name = e.name.c_str(); break; }
                    }
                }
                if (name) v.SetString(name, allocator);
                else      v.SetInt64(iv);
                break;
            }
            case FieldType::Struct: {
                v.SetObject();
                if (f.struct_info) SerializeReflected(*f.struct_info, p, v, allocator);
                break;
            }
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
        switch (f.type) {
            case FieldType::Bool:   if (v.IsBool())   *static_cast<bool*>(p) = v.GetBool(); break;
            case FieldType::Int:    if (v.IsInt())    *static_cast<int*>(p) = v.GetInt(); break;
            case FieldType::UInt:   if (v.IsUint())   *static_cast<unsigned int*>(p) = v.GetUint();
                                    else if (v.IsInt() && v.GetInt() >= 0) *static_cast<unsigned int*>(p) = static_cast<unsigned int>(v.GetInt());
                                    break;
            case FieldType::Float:  if (v.IsNumber()) *static_cast<float*>(p) = v.GetFloat(); break;
            case FieldType::Double: if (v.IsNumber()) *static_cast<double*>(p) = v.GetDouble(); break;
            case FieldType::Vec2:   ReadFloatArray(v, &static_cast<glm::vec2*>(p)->x, 2); break;
            case FieldType::Vec3:   ReadFloatArray(v, &static_cast<glm::vec3*>(p)->x, 3); break;
            case FieldType::Vec4:   ReadFloatArray(v, &static_cast<glm::vec4*>(p)->x, 4); break;
            case FieldType::String: if (v.IsString()) *static_cast<std::string*>(p) = v.GetString(); break;
            case FieldType::Enum: {
                if (v.IsString()) {
                    if (f.enum_info) {
                        for (const EnumEntry& e : f.enum_info->entries) {
                            if (e.name == v.GetString()) { f.set_enum(instance, e.value); break; }
                        }
                    }
                } else if (v.IsInt64() || v.IsInt()) {
                    f.set_enum(instance, v.GetInt64());
                }
                break;
            }
            case FieldType::Struct:
                if (v.IsObject() && f.struct_info) DeserializeReflected(*f.struct_info, p, v);
                break;
        }
    }
}

}  // namespace dse::reflect
