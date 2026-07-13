/**
 * @file asset_version_envelope.h
 * @brief 六种版本化内容格式共享的 {version, ...} 信封读写助手（rapidjson）。
 *
 * 此前每个格式（.dbp / .dasm / .dsequence / .dcutscene / .dshadergraph /
 * .dscriptmeta）在各自的序列化层重复同一段字段级逻辑：
 *   diag.source_version = (root.HasMember("version") && root["version"].IsInt())
 *                             ? root["version"].GetInt() : kSchemaVersionLegacy;
 *   NoteForwardCompat(diag.source_version, kXxxSchemaVersion, "...", diag);
 * 本头把该「版本信封」的读/写收敛到单一助手，让六格式复用同一字段名、缺省语义与
 * 前向兼容策略（asset_diagnostics 只共享了 DTO + 告警文案，字段级读写此前仍分散）。
 */

#pragma once

#include <rapidjson/document.h>

#include "engine/core/asset_diagnostics.h"

namespace dse::assets {

/// 从 JSON 根对象读取 "version" 整数字段：缺失/非整数时按 kSchemaVersionLegacy(0)
/// 处理；把结果写入 diag.source_version，并应用统一的前向兼容策略
/// （NoteForwardCompat）。返回解析出的版本号，便于调用方按版本分支迁移。
inline int ReadVersionEnvelope(const rapidjson::Value& root, int current_version,
                               const char* format_name, AssetDiagnostics& diag) {
    const int version = (root.HasMember("version") && root["version"].IsInt())
                            ? root["version"].GetInt()
                            : kSchemaVersionLegacy;
    diag.source_version = version;
    NoteForwardCompat(version, current_version, format_name, diag);
    return version;
}

/// 写入当前 schema 版本到 JSON 根对象的 "version" 字段（与 ReadVersionEnvelope
/// 读取的字段名对称）。
inline void WriteVersionEnvelope(rapidjson::Value& root, int current_version,
                                 rapidjson::Document::AllocatorType& alloc) {
    root.AddMember("version", current_version, alloc);
}

}  // namespace dse::assets
