/**
 * @file asset_dto.h
 * @brief 资产格式「DTO + 字段元数据表 + 统一映射」层（ADR-3）。
 *
 * 背景：本仓库 6 种资产格式都统一到「版本信封」（ReadVersionEnvelope +
 * AssetDiagnostics），但 body 仍是各写各的手工逐字段读写  加一个字段要改 N 处
 * serializer，且写路径容易出现「字段漏写 / 字符串没转义」这类不一致。
 *
 * 本层把 body 的读写收敛成：
 *   1. 每个格式定义一个 POD `XxxDto`（字段即 JSON 键的宿主）；
 *   2. 一张 `FieldDesc[]` 字段表描述 {JSON 键名, 类型, DTO 内偏移}；
 *   3. 读路径调 `ReadFields`、写路径调 `WriteFields`，新增字段只改 DTO + 表。
 *
 * 约定：
 *   - 读路径是**宽容**的：文档里缺失或类型不符的字段保持 DTO 原值（即声明的默认值），
 *     这正是既有 legacy 迁移（缺字段用默认）所需的语义；
 *   - 写路径由字段表驱动，因此字符串一定被正确转义（旧的手写 `f << "..."` 不会）。
 *
 * 说明：`FieldDesc::offset` 用 `offsetof` 取得。DTO 里允许有 std::string / glm 成员
 * （MSVC 支持对这类类型取 offsetof）；DTO 需保持「可平凡构造 + 公开成员」。
 */

#ifndef DSE_CORE_ASSET_DTO_H
#define DSE_CORE_ASSET_DTO_H

#include <cstddef>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <rapidjson/document.h>

namespace dse {
namespace assets {

/// JSON 字段的标量/向量类型（覆盖当前 6 种格式用到的全部形态）
enum class FieldType : uint8_t {
    Bool,
    Int,
    UInt,
    UInt64,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    FloatArray4,
};

/// 一条字段的元数据：JSON 键名、类型、在 DTO 内的字节偏移
struct FieldDesc {
    const char* name;
    FieldType   type;
    std::size_t offset;
};

namespace dto_detail {
inline void*       At(void* base, std::size_t off)       { return static_cast<unsigned char*>(base) + off; }
inline const void* At(const void* base, std::size_t off) { return static_cast<const unsigned char*>(base) + off; }
}  // namespace dto_detail

/// 按字段表把文档里的字段读入 POD DTO；返回实际赋值的字段数。
/// 缺失 / 类型不符的字段**不动**目标成员（保留其默认值）。
inline int ReadFields(const rapidjson::Value& obj, const FieldDesc* fields,
                      std::size_t count, void* dto) {
    if (!obj.IsObject()) return 0;
    int applied = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const FieldDesc& fd = fields[i];
        if (!obj.HasMember(fd.name)) continue;
        const rapidjson::Value& v = obj[fd.name];
        void* dst = dto_detail::At(dto, fd.offset);
        switch (fd.type) {
            case FieldType::Bool:
                if (v.IsBool()) { *static_cast<bool*>(dst) = v.GetBool(); ++applied; }
                break;
            case FieldType::Int:
                if (v.IsInt()) { *static_cast<int*>(dst) = v.GetInt(); ++applied; }
                break;
            case FieldType::UInt:
                if (v.IsUint()) { *static_cast<unsigned int*>(dst) = v.GetUint(); ++applied; }
                break;
            case FieldType::UInt64:
                if (v.IsUint64()) { *static_cast<uint64_t*>(dst) = v.GetUint64(); ++applied; }
                break;
            case FieldType::Float:
                if (v.IsNumber()) { *static_cast<float*>(dst) = v.GetFloat(); ++applied; }
                break;
            case FieldType::String:
                if (v.IsString()) {
                    *static_cast<std::string*>(dst) = std::string(v.GetString(), v.GetStringLength());
                    ++applied;
                }
                break;
            case FieldType::Vec2:
                if (v.IsArray() && v.Size() >= 2 && v[0].IsNumber() && v[1].IsNumber()) {
                    *static_cast<glm::vec2*>(dst) = glm::vec2(v[0].GetFloat(), v[1].GetFloat());
                    ++applied;
                }
                break;
            case FieldType::Vec3:
                if (v.IsArray() && v.Size() >= 3 && v[0].IsNumber() && v[1].IsNumber() &&
                    v[2].IsNumber()) {
                    *static_cast<glm::vec3*>(dst) = glm::vec3(v[0].GetFloat(), v[1].GetFloat(),
                                                              v[2].GetFloat());
                    ++applied;
                }
                break;
            case FieldType::Vec4:
                if (v.IsArray() && v.Size() >= 4 && v[0].IsNumber() && v[1].IsNumber() &&
                    v[2].IsNumber() && v[3].IsNumber()) {
                    *static_cast<glm::vec4*>(dst) = glm::vec4(v[0].GetFloat(), v[1].GetFloat(),
                                                              v[2].GetFloat(), v[3].GetFloat());
                    ++applied;
                }
                break;
            case FieldType::FloatArray4:
                if (v.IsArray() && v.Size() >= 4 && v[0].IsNumber() && v[1].IsNumber() &&
                    v[2].IsNumber() && v[3].IsNumber()) {
                    float* out = static_cast<float*>(dst);
                    out[0] = v[0].GetFloat();
                    out[1] = v[1].GetFloat();
                    out[2] = v[2].GetFloat();
                    out[3] = v[3].GetFloat();
                    ++applied;
                }
                break;
        }
    }
    return applied;
}

/// 按字段表把 POD DTO 写进文档（由字段表驱动，字符串自动转义）。
inline void WriteFields(rapidjson::Value& obj, rapidjson::Document::AllocatorType& alloc,
                        const FieldDesc* fields, std::size_t count, const void* dto) {
    for (std::size_t i = 0; i < count; ++i) {
        const FieldDesc& fd = fields[i];
        const void* src = dto_detail::At(dto, fd.offset);
        rapidjson::Value key(fd.name, alloc);
        switch (fd.type) {
            case FieldType::Bool:
                obj.AddMember(key, *static_cast<const bool*>(src), alloc);
                break;
            case FieldType::Int:
                obj.AddMember(key, *static_cast<const int*>(src), alloc);
                break;
            case FieldType::UInt:
                obj.AddMember(key, *static_cast<const unsigned int*>(src), alloc);
                break;
            case FieldType::UInt64:
                obj.AddMember(key, *static_cast<const uint64_t*>(src), alloc);
                break;
            case FieldType::Float:
                obj.AddMember(key, *static_cast<const float*>(src), alloc);
                break;
            case FieldType::String: {
                const std::string& s = *static_cast<const std::string*>(src);
                rapidjson::Value v(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
                obj.AddMember(key, v, alloc);
                break;
            }
            case FieldType::Vec2: {
                const glm::vec2& v = *static_cast<const glm::vec2*>(src);
                rapidjson::Value arr(rapidjson::kArrayType);
                arr.PushBack(v.x, alloc).PushBack(v.y, alloc);
                obj.AddMember(key, arr, alloc);
                break;
            }
            case FieldType::Vec3: {
                const glm::vec3& v = *static_cast<const glm::vec3*>(src);
                rapidjson::Value arr(rapidjson::kArrayType);
                arr.PushBack(v.x, alloc).PushBack(v.y, alloc).PushBack(v.z, alloc);
                obj.AddMember(key, arr, alloc);
                break;
            }
            case FieldType::Vec4: {
                const glm::vec4& v = *static_cast<const glm::vec4*>(src);
                rapidjson::Value arr(rapidjson::kArrayType);
                arr.PushBack(v.x, alloc).PushBack(v.y, alloc)
                   .PushBack(v.z, alloc).PushBack(v.w, alloc);
                obj.AddMember(key, arr, alloc);
                break;
            }
            case FieldType::FloatArray4: {
                const float* v = static_cast<const float*>(src);
                rapidjson::Value arr(rapidjson::kArrayType);
                for (int i = 0; i < 4; ++i) arr.PushBack(v[i], alloc);
                obj.AddMember(key, arr, alloc);
                break;
            }
        }
    }
}

}  // namespace assets
}  // namespace dse

#endif  // DSE_CORE_ASSET_DTO_H
