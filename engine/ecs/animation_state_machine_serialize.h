#pragma once

/**
 * @file animation_state_machine_serialize.h
 * @brief AnimationStateMachine (.dasm) 共享序列化契约层（引擎侧，无 ImGui 依赖）。
 *
 * 提供编辑器与运行时/场景共用的单一状态机读写路径：
 *   - WriteStateMachineJson / ReadStateMachineJson : 与已打开的 rapidjson 文档互转
 *     （scene.cpp 把状态机嵌入 Animator3DComponent，实现场景重载后仍保留作者数据）。
 *   - SerializeStateMachine / DeserializeStateMachine : 独立 .dasm 文本（带版本根对象）。
 *   - SaveStateMachineToFile / LoadStateMachineFromFile : 磁盘 .dasm 资产。
 * 具备显式诊断（errors/warnings）、版本号与前向兼容（未知字段记为 warning，不静默丢弃）。
 */

#include <string>
#include <vector>

#include <rapidjson/document.h>

#include "engine/ecs/animation_state_machine.h"

namespace dse {
namespace gameplay3d {

/// 当前 .dasm schema 版本。缺失 version 字段的旧文件视为版本 0（legacy）。
constexpr int kAnimStateMachineSchemaVersion = 1;

/// 读/写 .dasm 的结构化诊断结果。不以静默回退掩盖问题。
struct AsmDiagnostics {
    bool ok = false;                    ///< 整体是否成功
    int source_version = 0;             ///< 文件中声明（或推断）的版本
    bool migrated = false;              ///< 是否发生了版本迁移
    std::vector<std::string> errors;    ///< 致命错误（导致失败）
    std::vector<std::string> warnings;  ///< 非致命（未知字段/前向版本）
};

/// 枚举 <-> 名称（读写共用，保证对称）。
const char* AnimParamTypeName(AnimParamType type);
AnimParamType AnimParamTypeFromName(const char* name);
const char* AnimConditionModeName(AnimConditionMode mode);
AnimConditionMode AnimConditionModeFromName(const char* name);

/// 写入到已存在的 rapidjson 对象（不含版本包裹，供 scene 内嵌使用）。
void WriteStateMachineJson(const AnimationStateMachine& sm,
                           rapidjson::Value& out,
                           rapidjson::Document::AllocatorType& alloc);

/// 从 rapidjson 对象读取（供 scene 内嵌使用）。未知字段记为 warning。
bool ReadStateMachineJson(const rapidjson::Value& in,
                          AnimationStateMachine& sm,
                          AsmDiagnostics& diag);

/// 序列化为带版本根对象的格式化 .dasm 文本。
std::string SerializeStateMachine(const AnimationStateMachine& sm);

/// 从 .dasm 文本解析（带诊断/前向兼容）。
bool DeserializeStateMachine(AnimationStateMachine& sm, const std::string& json,
                             AsmDiagnostics& diag);

/// 保存到磁盘。失败时 diag.errors 说明原因。
bool SaveStateMachineToFile(const AnimationStateMachine& sm, const std::string& path,
                            AsmDiagnostics& diag);

/// 从磁盘读取（带诊断/前向兼容）。
bool LoadStateMachineFromFile(AnimationStateMachine& sm, const std::string& path,
                              AsmDiagnostics& diag);

}  // namespace gameplay3d
}  // namespace dse
