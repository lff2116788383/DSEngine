#ifndef DSE_REFLECT_REFLECT_H
#define DSE_REFLECT_REFLECT_H

// ─────────────────────────────────────────────────────────────────────────────
// DSE 反射核心（后端无关的单一事实来源）
//
// 设计目标：消费方（序列化 / Inspector / 未来的网络复制）只依赖这里暴露的
// TypeInfo / FieldInfo introspection API；"谁来填充这些 TypeInfo" 是可替换的后端：
//   - A 阶段（当前）：手写运行时注册（DSE_REFLECT_TYPE + builder，见 component_reflection.cpp）。
//   - C 阶段（未来）：由 codegen 生成等价的注册代码，消费方一行不改。
//
// 因此从 A 平滑迁移到 C 只是"换后端"，而非重写。字段访问基于成员指针生成的
// resolver（标准、无 UB），codegen 后端同样可以发出等价 resolver 或 offset。
// ─────────────────────────────────────────────────────────────────────────────

#include <cstddef>
#include <functional>
#include <string>
#include <typeindex>
#include <vector>

#include <glm/glm.hpp>

namespace dse::reflect {

/// 字段标量/向量类型标签。PoC 覆盖组件里实际出现的类型；枚举后续以 Int 之上扩展。
enum class FieldType {
    Bool,
    Int,
    UInt,
    Float,
    Double,
    Vec2,
    Vec3,
    Vec4,
    String,
};

/// 字段特性（编辑器/校验用元数据）。空 = 无约束。
struct FieldAttributes {
    bool   has_range = false;
    double min_value = 0.0;
    double max_value = 0.0;
    double step = 0.0;          ///< 0 = 由消费方决定默认步进
    const char* tooltip = nullptr;
    const char* label = nullptr; ///< Inspector 显示名（nullptr = 用字段名）
    bool   hidden = false;      ///< 序列化与 Inspector 均跳过
    bool   read_only = false;   ///< Inspector 只读（序列化仍写出）
    bool   is_color = false;    ///< vec3/vec4 作为颜色编辑（Inspector 用 ColorEdit）
};

/// 单个字段的反射信息。get/cget 返回指向实例内该字段的指针。
struct FieldInfo {
    std::string name;
    FieldType   type;
    FieldAttributes attr;
    std::function<void*(void*)> get;              ///< 可写字段指针
    std::function<const void*(const void*)> cget; ///< 只读字段指针
};

/// 一个反射类型（组件）的信息：名字 + 字段列表。
struct TypeInfo {
    std::string name;
    std::type_index type = std::type_index(typeid(void));
    std::size_t size = 0;
    std::vector<FieldInfo> fields;
};

// ─── 类型推导：成员类型 → FieldType ────────────────────────────────────────────
template <class T> struct FieldTypeOf;  // 未特化的类型会编译报错（即不支持）

template <> struct FieldTypeOf<bool>          { static constexpr FieldType value = FieldType::Bool; };
template <> struct FieldTypeOf<int>           { static constexpr FieldType value = FieldType::Int; };
template <> struct FieldTypeOf<unsigned int>  { static constexpr FieldType value = FieldType::UInt; };
template <> struct FieldTypeOf<float>         { static constexpr FieldType value = FieldType::Float; };
template <> struct FieldTypeOf<double>        { static constexpr FieldType value = FieldType::Double; };
template <> struct FieldTypeOf<glm::vec2>     { static constexpr FieldType value = FieldType::Vec2; };
template <> struct FieldTypeOf<glm::vec3>     { static constexpr FieldType value = FieldType::Vec3; };
template <> struct FieldTypeOf<glm::vec4>     { static constexpr FieldType value = FieldType::Vec4; };
template <> struct FieldTypeOf<std::string>   { static constexpr FieldType value = FieldType::String; };

/// 链式特性设置器（field() 返回，用于就地附加 range/tooltip 等）。
class FieldRef {
public:
    explicit FieldRef(FieldInfo* f) : f_(f) {}
    FieldRef& range(double mn, double mx) { f_->attr.has_range = true; f_->attr.min_value = mn; f_->attr.max_value = mx; return *this; }
    FieldRef& step(double s)              { f_->attr.step = s; return *this; }
    FieldRef& tooltip(const char* t)      { f_->attr.tooltip = t; return *this; }
    FieldRef& label(const char* l)        { f_->attr.label = l; return *this; }
    FieldRef& hidden()                    { f_->attr.hidden = true; return *this; }
    FieldRef& read_only()                 { f_->attr.read_only = true; return *this; }
    FieldRef& color()                     { f_->attr.is_color = true; return *this; }
private:
    FieldInfo* f_;
};

/// 类型注册构建器（DSE_REFLECT_TYPE 返回）。
class TypeBuilder {
public:
    explicit TypeBuilder(TypeInfo* ti) : ti_(ti) {}

    template <class C, class M>
    FieldRef field(const char* name, M C::* mp) {
        FieldInfo fi;
        fi.name = name;
        fi.type = FieldTypeOf<M>::value;
        fi.get  = [mp](void* inst) -> void* {
            return static_cast<void*>(&(static_cast<C*>(inst)->*mp));
        };
        fi.cget = [mp](const void* inst) -> const void* {
            return static_cast<const void*>(&(static_cast<const C*>(inst)->*mp));
        };
        ti_->fields.push_back(std::move(fi));
        return FieldRef(&ti_->fields.back());
    }

private:
    TypeInfo* ti_;
};

/// 全局反射注册表（后端无关）。
class Reflection {
public:
    /// 注册（或取得）一个类型并返回其构建器。重复注册同名类型会清空旧字段重建。
    static TypeBuilder Add(const char* name, std::type_index type, std::size_t size);

    static const TypeInfo* Find(const std::string& name);
    static const TypeInfo* Find(std::type_index type);
    template <class T> static const TypeInfo* Find() { return Find(std::type_index(typeid(T))); }

    static std::vector<const TypeInfo*> All();
};

}  // namespace dse::reflect

/// 在注册函数体内开始声明某类型的反射。返回 TypeBuilder。
#define DSE_REFLECT_TYPE(T) \
    ::dse::reflect::Reflection::Add(#T, std::type_index(typeid(T)), sizeof(T))

#endif  // DSE_REFLECT_REFLECT_H
