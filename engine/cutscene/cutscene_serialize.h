/**
 * @file cutscene_serialize.h
 * @brief 运行时过场序列（CutsceneSequence）的版本化 .dcutscene 序列化契约
 *
 * 与编辑器 Sequencer 共用同一份磁盘格式：编辑器烘焙出的 .dcutscene 即为
 * 打包运行时消费的资产。回调（apply/fire/play）为运行期绑定，不入盘。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <rapidjson/document.h>

#include "engine/cutscene/cutscene_player.h"
#include "engine/core/dse_export.h"

namespace dse {
namespace cutscene {

constexpr int kCutsceneSchemaVersion = 1;

struct CutsceneDiagnostics {
    bool ok = false;
    int source_version = 0;
    bool migrated = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

DSE_EXPORT const char* InterpModeName(InterpMode mode);
DSE_EXPORT InterpMode InterpModeFromName(const char* name);
DSE_EXPORT const char* TrackTypeName(TrackType type);

/// 写出序列内容到已存在的 JSON 对象（不含版本包裹）。
DSE_EXPORT void WriteSequenceJson(const CutsceneSequence& seq,
                                  rapidjson::Value& out,
                                  rapidjson::Document::AllocatorType& alloc);

/// 从 JSON 对象读出为新建序列；失败返回 nullptr 并填充 diag。
DSE_EXPORT std::shared_ptr<CutsceneSequence> ReadSequenceJson(const rapidjson::Value& in,
                                                              CutsceneDiagnostics& diag);

DSE_EXPORT std::string SerializeSequence(const CutsceneSequence& seq);

DSE_EXPORT std::shared_ptr<CutsceneSequence> DeserializeSequence(const std::string& json,
                                                                CutsceneDiagnostics& diag);

DSE_EXPORT bool SaveSequenceToFile(const CutsceneSequence& seq, const std::string& path,
                                   CutsceneDiagnostics& diag);

DSE_EXPORT std::shared_ptr<CutsceneSequence> LoadSequenceFromFile(const std::string& path,
                                                                 CutsceneDiagnostics& diag);

}  // namespace cutscene
}  // namespace dse
