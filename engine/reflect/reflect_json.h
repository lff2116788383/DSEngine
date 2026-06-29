#ifndef DSE_REFLECT_REFLECT_JSON_H
#define DSE_REFLECT_REFLECT_JSON_H

// 基于反射的通用 JSON 序列化驱动。
// 取代手写的逐字段 json.AddMember / json.HasMember 代码：消费方只需提供 TypeInfo
// 与实例指针，驱动遍历字段读写。后端切到 codegen 时本驱动完全不变。

#include <rapidjson/document.h>

#include "engine/reflect/reflect.h"

namespace dse::reflect {

/// 将 instance 的所有（非 hidden）反射字段写入 out（应为 kObjectType）。
void SerializeReflected(const TypeInfo& type, const void* instance,
                        rapidjson::Value& out,
                        rapidjson::Document::AllocatorType& allocator);

/// 从 in 读取并写回 instance 的反射字段；缺失或类型不符的字段保持原值。
void DeserializeReflected(const TypeInfo& type, void* instance,
                          const rapidjson::Value& in);

}  // namespace dse::reflect

#endif  // DSE_REFLECT_REFLECT_JSON_H
