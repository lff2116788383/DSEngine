#pragma once

// 骨骼动画重定向（Animation Retargeting）纯核心 —— 无 ImGui 依赖，可无头测试。
//
// 把一段源骨架上的动画（按骨骼名引用通道）映射到目标骨架：
//   1) 自动按名匹配：精确名 → 归一化名（去 "mixamorig:" 等前缀/分隔符/大小写）→ 人形同义词
//   2) 用户可在 UI 中逐条覆盖映射
//   3) 依据映射"烘焙"出一段以目标骨架命名/索引的新动画（RawAnimation）
//
// 这里只放纯数据逻辑；资源导入（GltfImporter）与 ImGui 面板在 editor_anim_retarget.cpp。

#include <string>
#include <unordered_map>
#include <vector>

#include "engine/assets/compiler/raw_scene_data.h"

namespace dse::editor::retarget {

/// 单条骨骼的匹配来源，用于 UI 着色与诊断。
enum class MatchType { None, Exact, Normalized, Humanoid, Manual };

struct BoneMatch {
    int source_index = -1;       // 源骨架骨骼下标
    int target_index = -1;       // 目标骨架骨骼下标（-1 = 未映射）
    MatchType type = MatchType::None;
};

/// 源 → 目标的整套骨骼映射。matches 与 source 骨骼一一对应。
struct BoneMap {
    std::vector<BoneMatch> matches;
};

/// 归一化骨骼名：小写、去常见前缀（mixamorig:/armature 等）、去分隔符与空白。
std::string NormalizeBoneName(const std::string& name);

/// 人形规范名（如 lefthand/upperarm.l → "hand.l"），无法识别返回空串。
std::string HumanoidCanonical(const std::string& name);

/// 自动建立映射：对每个源骨骼按 精确→归一化→人形同义词 顺序找目标骨骼。
BoneMap AutoMapBones(const std::vector<std::string>& source_bones,
                     const std::vector<std::string>& target_bones);

/// 覆盖某源骨骼的目标（target_index<0 表示清除映射），标记为 Manual。
void SetManualMapping(BoneMap& map, int source_index, int target_index);

/// 已成功映射的骨骼数。
int MappedCount(const BoneMap& map);

/// 依据映射把源动画重定向为以目标骨架命名/索引的新动画。
/// - 通道按映射改写 target_node_name/target_node_index；未映射的通道被丢弃。
/// - 关键帧数据原样保留（仅重定向骨骼对应关系，不做骨架空间换算）。
asset::compiler::RawAnimation RetargetAnimation(
    const asset::compiler::RawAnimation& source_anim,
    const std::vector<std::string>& source_bones,
    const std::vector<std::string>& target_bones,
    const BoneMap& map);

}  // namespace dse::editor::retarget
