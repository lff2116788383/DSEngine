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
#include <type_traits>
#include <typeindex>
#include <vector>

#include <glm/glm.hpp>

namespace dse::reflect {

/// 字段标量/向量类型标签。
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
    Enum,    ///< 枚举：底层按整数存储，配 EnumInfo 提供名值对（序列化用名字、Inspector 用下拉）。
    Struct,  ///< 嵌套已注册类型：配 TypeInfo* 递归处理。
};

/// 枚举的一个名值对。value 统一以 long long 存放，覆盖任意底层整型。
struct EnumEntry {
    std::string name;
    long long   value;
};

/// 枚举类型的反射信息（名字 + 名值对列表）。
struct EnumInfo {
    std::string name;
    std::type_index type = std::type_index(typeid(void));
    std::vector<EnumEntry> entries;
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

struct TypeInfo;  // 前置声明：FieldInfo 的 struct_info 指向嵌套类型。

/// 单个字段的反射信息。get/cget 返回指向实例内该字段的指针。
struct FieldInfo {
    std::string name;
    FieldType   type;
    FieldAttributes attr;
    std::function<void*(void*)> get;              ///< 可写字段指针
    std::function<const void*(const void*)> cget; ///< 只读字段指针

    // type == Enum 时有效：名值对来源 + 以 long long 读写底层整数（屏蔽底层类型差异）。
    const EnumInfo* enum_info = nullptr;
    std::function<long long(const void*)> get_enum;
    std::function<void(void*, long long)> set_enum;

    // type == Struct 时有效：嵌套类型信息（递归遍历），get/cget 返回嵌套实例指针。
    const TypeInfo* struct_info = nullptr;
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

/// 检测某类型是否为内建标量/向量（即上面有 FieldTypeOf 特化）。
template <class T, class = void> struct HasFieldType : std::false_type {};
template <class T> struct HasFieldType<T, std::void_t<decltype(FieldTypeOf<T>::value)>> : std::true_type {};

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

class Reflection;  // 前置声明：field() 的离线定义依赖完整 Reflection。

/// 类型注册构建器（DSE_REFLECT_TYPE 返回）。
class TypeBuilder {
public:
    explicit TypeBuilder(TypeInfo* ti) : ti_(ti) {}

    /// 声明一个成员字段。M 为标量/向量 → 直接定型；为枚举 → Enum + EnumInfo；
    /// 否则视为嵌套的已注册类型 → Struct + TypeInfo。定义见本文件末尾（需完整 Reflection）。
    template <class C, class M>
    FieldRef field(const char* name, M C::* mp);

private:
    TypeInfo* ti_;
};

/// 枚举注册构建器（DSE_REFLECT_ENUM 返回）。
class EnumBuilder {
public:
    explicit EnumBuilder(EnumInfo* e) : e_(e) {}
    /// 追加一个名值对。接受任意整型/枚举，统一转 long long 存放。
    template <class V>
    EnumBuilder& value(const char* name, V v) {
        e_->entries.push_back(EnumEntry{name, static_cast<long long>(v)});
        return *this;
    }
private:
    EnumInfo* e_;
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

    /// 注册（或取得）一个枚举类型并返回其构建器。重复注册会清空旧名值对重建。
    static EnumBuilder AddEnum(const char* name, std::type_index type);
    static const EnumInfo* FindEnum(std::type_index type);
    template <class E> static const EnumInfo* FindEnum() { return FindEnum(std::type_index(typeid(E))); }
};

// ─── TypeBuilder::field 的离线定义（此处 Reflection 已完整可见） ────────────────
template <class C, class M>
inline FieldRef TypeBuilder::field(const char* name, M C::* mp) {
    FieldInfo fi;
    fi.name = name;
    if constexpr (std::is_enum_v<M>) {
        fi.type = FieldType::Enum;
        fi.enum_info = Reflection::FindEnum(std::type_index(typeid(M)));
        fi.get_enum = [mp](const void* inst) -> long long {
            return static_cast<long long>(static_cast<const C*>(inst)->*mp);
        };
        fi.set_enum = [mp](void* inst, long long v) {
            static_cast<C*>(inst)->*mp = static_cast<M>(v);
        };
    } else if constexpr (HasFieldType<M>::value) {
        fi.type = FieldTypeOf<M>::value;
    } else {
        // 既非标量也非枚举 → 视为嵌套的已注册类型（须先于本类型注册）。
        fi.type = FieldType::Struct;
        fi.struct_info = Reflection::Find(std::type_index(typeid(M)));
    }
    fi.get  = [mp](void* inst) -> void* {
        return static_cast<void*>(&(static_cast<C*>(inst)->*mp));
    };
    fi.cget = [mp](const void* inst) -> const void* {
        return static_cast<const void*>(&(static_cast<const C*>(inst)->*mp));
    };
    ti_->fields.push_back(std::move(fi));
    return FieldRef(&ti_->fields.back());
}

}  // namespace dse::reflect

/// 在注册函数体内开始声明某类型的反射。返回 TypeBuilder。
#define DSE_REFLECT_TYPE(T) \
    ::dse::reflect::Reflection::Add(#T, std::type_index(typeid(T)), sizeof(T))

/// 在注册函数体内开始声明某枚举的反射。返回 EnumBuilder。
#define DSE_REFLECT_ENUM(E) \
    ::dse::reflect::Reflection::AddEnum(#E, std::type_index(typeid(E)))

#endif  // DSE_REFLECT_REFLECT_H
