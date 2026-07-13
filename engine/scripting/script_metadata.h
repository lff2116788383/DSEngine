/**
 * @file script_metadata.h
 * @brief C# 脚本元数据（.dscriptmeta）的版本化共享契约
 *
 * 描述一个 C# DseScript 子类对编辑器/运行时可见的元信息：类名、命名空间、
 * 基类、所属程序集、以及导出到 Inspector 的序列化字段（名称/类型/默认值）。
 * 该契约与 CoreCLR 宿主无关，只承载数据，故位于 engine/scripting 下（不随
 * DSE_ENABLE_CSHARP 开关排除），编辑器与运行时/构建期共用同一份磁盘格式。
 *
 * 序列化采用与 .dcutscene/.dshadergraph 一致的版本包裹：
 *   { "version": N, "script": {...} }
 * 旧版裸对象（version 0）按迁移读取，保持前向/后向兼容。
 */

#pragma once

#include <string>
#include <vector>

#include "engine/core/dse_export.h"

namespace dse {
namespace scripting {

constexpr int kScriptMetaSchemaVersion = 1;

enum class ScriptFieldType {
    Bool,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Color,
    EntityRef,
    AssetRef,
};

struct ScriptFieldMeta {
    std::string name;
    ScriptFieldType type = ScriptFieldType::Float;
    std::string tooltip;
    // 数值/向量/颜色默认值（按分量填充；标量只用 [0]）。
    float number_default[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    // 字符串/资产路径/实体引用默认值。
    std::string string_default;
};

struct ScriptMetadata {
    std::string class_name;   ///< 简单类名（如 "PlayerController"）
    std::string full_name;    ///< 命名空间限定名（如 "Game.PlayerController"）
    std::string base_type;    ///< 基类（约定为 "DseScript"）
    std::string assembly;     ///< 所属程序集（如 "DSEngine.Game"）
    std::string source_path;  ///< .cs 源路径（编辑器提示，运行时忽略）
    std::vector<ScriptFieldMeta> fields;
};

struct ScriptMetaDiagnostics {
    bool ok = false;
    int source_version = 0;
    bool migrated = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

DSE_EXPORT const char* ScriptFieldTypeName(ScriptFieldType type);
DSE_EXPORT ScriptFieldType ScriptFieldTypeFromName(const char* name);

/// 结构完整性校验：类名非空、字段名非空且互不重复。
DSE_EXPORT bool ValidateScriptMetadata(const ScriptMetadata& meta, std::string& error);

DSE_EXPORT std::string SerializeScriptMetadata(const ScriptMetadata& meta);

DSE_EXPORT bool DeserializeScriptMetadata(const std::string& json,
                                          ScriptMetadata& out,
                                          ScriptMetaDiagnostics& diag);

DSE_EXPORT bool SaveScriptMetadataToFile(const ScriptMetadata& meta,
                                         const std::string& path,
                                         ScriptMetaDiagnostics& diag);

DSE_EXPORT bool LoadScriptMetadataFromFile(const std::string& path,
                                           ScriptMetadata& out,
                                           ScriptMetaDiagnostics& diag);

}  // namespace scripting
}  // namespace dse
