/**
 * @file asset_diagnostics.h
 * @brief 各内容格式共享的资产诊断 DTO 与版本策略（引擎侧，无 JSON 依赖）。
 *
 * DSEngine 的六种版本化内容格式（.dbp / .dasm / .dsequence / .dcutscene /
 * .dshadergraph / .dscriptmeta）在各自的序列化层曾各自声明形状完全一致的
 * 「诊断结构」（ok/source_version/migrated/errors/warnings）并各自手写前向兼容
 * 告警。本头把该 DTO 收敛为单一 dse::assets::AssetDiagnostics，并提供统一的
 * 版本策略助手 NoteForwardCompat()，让所有格式复用同一告警措辞与语义。
 *
 * 各格式的 XxxDiagnostics 现为本类型的别名（类型完全一致，调用点零改动），
 * 而 kXxxSchemaVersion（当前版本号）仍由各格式自行持有（各格式独立演进）。
 */

#pragma once

#include <string>
#include <vector>

#include "engine/core/dse_export.h"

namespace dse::assets {

/// 缺失 version 字段的旧文件统一视为该版本（legacy）。
constexpr int kSchemaVersionLegacy = 0;

/// 读/写版本化内容资产的结构化诊断结果。所有内容格式共用此形状：
/// 不以静默回退掩盖问题，未知字段/前向版本记录为 warning 而非丢弃或失败。
struct AssetDiagnostics {
    bool ok = false;                     ///< 整体是否成功
    int source_version = kSchemaVersionLegacy;  ///< 文件中声明（或推断）的版本
    bool migrated = false;               ///< 是否发生了版本迁移
    std::vector<std::string> errors;     ///< 致命错误（导致失败）
    std::vector<std::string> warnings;   ///< 非致命（未知字段/前向版本/迁移说明）
};

/// 统一的前向兼容版本策略：当 source_version 高于当前支持的 current_version 时，
/// 追加一条规范措辞的 warning（宽松加载，不失败）。format_name 用于消息前缀
/// （如 ".dbp"）。source_version <= current_version 时不产生 warning。
DSE_EXPORT void NoteForwardCompat(int source_version, int current_version,
                                  const char* format_name, AssetDiagnostics& diag);

}  // namespace dse::assets
