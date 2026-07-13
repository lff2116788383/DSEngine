#pragma once

/**
 * @file blueprint_serialize.h
 * @brief Blueprint (.dbp) 共享序列化契约层（引擎侧，无 ImGui 依赖）。
 *
 * 提供编辑器与运行时共用的单一 .dbp 读/写路径：
 *   - SerializeBlueprintAsset / SaveBlueprintAsset : dse::bp::BlueprintAsset -> JSON
 *   - DeserializeBlueprintAsset / LoadBlueprintAssetChecked : JSON -> BlueprintAsset
 * 具备显式诊断（errors/warnings）、版本号、版本迁移与前向兼容（未知字段记录为
 * 警告而非静默丢弃或失败）。blueprint_compiler.cpp 的 LoadBlueprintAsset() 现在
 * 委托到本层，保证只有一条解析路径。
 */

#include <string>
#include <vector>

#include "engine/core/asset_diagnostics.h"
#include "engine/scripting/blueprint/blueprint_compiler.h"

namespace dse::bp {

/// 当前 .dbp schema 版本。缺失 version 字段的旧文件视为版本 0（legacy）。
constexpr int kBlueprintSchemaVersion = 1;

/// 读/写 .dbp 的结构化诊断结果（收敛到共享 DTO）。不再以静默回退掩盖问题。
using BlueprintDiagnostics = dse::assets::AssetDiagnostics;

/// 枚举 <-> 名称（与解析侧共用，保证读写对称）。
const char* BpVarTypeName(BpVarType type);
const char* BpPinTypeName(BpPinType type);
BpPinType   BpPinTypeFromName(const char* name);

/// 序列化为格式化 JSON 文本（写入当前 kBlueprintSchemaVersion）。
std::string SerializeBlueprintAsset(const BlueprintAsset& asset);

/// 从 JSON 文本解析。填充 diag；未知字段记录为 warning，不静默丢弃。
bool DeserializeBlueprintAsset(BlueprintAsset& asset, const std::string& json,
                               BlueprintDiagnostics& diag);

/// 保存到磁盘。失败时 diag.errors 说明原因。
bool SaveBlueprintAsset(const BlueprintAsset& asset, const std::string& path,
                        BlueprintDiagnostics& diag);

/// 从磁盘读取（带诊断/迁移）。
bool LoadBlueprintAssetChecked(BlueprintAsset& asset, const std::string& path,
                               BlueprintDiagnostics& diag);

}  // namespace dse::bp
