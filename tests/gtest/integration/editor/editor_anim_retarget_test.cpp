/**
 * @file editor_anim_retarget_test.cpp
 * @brief 骨骼动画重定向纯核心（editor_anim_retarget_core）的无头测试。
 *
 * 覆盖：骨骼名归一化、人形同义词规范化、自动映射（精确/归一化/人形）、手动覆盖、
 * 以及依据映射把源动画通道重定向到目标骨架。资源导入（Gltf/Fbx）与 ImGui 面板不在此覆盖。
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "editor_anim_retarget_core.h"

using namespace dse::editor::retarget;
using dse::asset::compiler::RawAnimation;
using dse::asset::compiler::RawAnimationChannel;

// ── 归一化 ───────────────────────────────────────────────────────────────────

// 测试 动画重定向：归一化剥离前缀且分隔符
TEST(AnimRetarget, NormalizeStripsPrefixAndSeparators) {
    EXPECT_EQ(NormalizeBoneName("mixamorig:LeftArm"), "leftarm");
    EXPECT_EQ(NormalizeBoneName("Left_Arm"), "leftarm");
    EXPECT_EQ(NormalizeBoneName("Hips"), "hips");
}

// ── 人形规范化（含侧别） ────────────────────────────────────────────────────

// 测试 动画重定向：Humanoid Canonical Maps Synonyms且侧
TEST(AnimRetarget, HumanoidCanonicalMapsSynonymsAndSide) {
    EXPECT_EQ(HumanoidCanonical("LeftForeArm"), "lowerarm.l");   // forearm → lowerarm
    EXPECT_EQ(HumanoidCanonical("RightThigh"), "upperleg.r");    // thigh → upperleg
    EXPECT_EQ(HumanoidCanonical("Pelvis"), "hips");              // pelvis → hips, 无侧别
    EXPECT_EQ(HumanoidCanonical("Hand_L"), "hand.l");
    EXPECT_EQ(HumanoidCanonical("totally_unknown_bone"), "");    // 无法识别
}

// ── 自动映射：精确名优先 ────────────────────────────────────────────────────

// 测试 动画重定向：自动映射Exact名称Wins
TEST(AnimRetarget, AutoMapExactNameWins) {
    std::vector<std::string> src = {"Hips", "Spine", "Head"};
    std::vector<std::string> tgt = {"Hips", "Spine", "Head"};
    BoneMap m = AutoMapBones(src, tgt);
    ASSERT_EQ(m.matches.size(), 3u);
    for (const auto& bm : m.matches) EXPECT_EQ(bm.type, MatchType::Exact);
    EXPECT_EQ(m.matches[0].target_index, 0);
    EXPECT_EQ(m.matches[2].target_index, 2);
    EXPECT_EQ(MappedCount(m), 3);
}

// ── 自动映射：跨命名约定走人形同义词 ────────────────────────────────────────

// 测试 动画重定向：自动映射Humanoid Across Conventions
TEST(AnimRetarget, AutoMapHumanoidAcrossConventions) {
    std::vector<std::string> src = {"mixamorig:LeftForeArm", "mixamorig:RightUpLeg"};
    std::vector<std::string> tgt = {"lowerarm_l", "thigh_r"};
    BoneMap m = AutoMapBones(src, tgt);
    ASSERT_EQ(m.matches.size(), 2u);
    // lowerarm.l <-> lowerarm_l
    EXPECT_EQ(m.matches[0].target_index, 0);
    EXPECT_EQ(m.matches[0].type, MatchType::Humanoid);
    // upperleg.r (UpLeg→upperleg) <-> thigh_r (thigh→upperleg)
    EXPECT_EQ(m.matches[1].target_index, 1);
    EXPECT_EQ(m.matches[1].type, MatchType::Humanoid);
}

// ── 归一化匹配：大小写/分隔符差异 ───────────────────────────────────────────

// 测试 动画重定向：自动映射归一化匹配
TEST(AnimRetarget, AutoMapNormalizedMatch) {
    std::vector<std::string> src = {"Left_Hand"};
    std::vector<std::string> tgt = {"lefthand"};
    BoneMap m = AutoMapBones(src, tgt);
    ASSERT_EQ(m.matches.size(), 1u);
    EXPECT_EQ(m.matches[0].target_index, 0);
    EXPECT_EQ(m.matches[0].type, MatchType::Normalized);
}

// ── 无匹配 ───────────────────────────────────────────────────────────────────

// 测试 动画重定向：Unmapped当无目标
TEST(AnimRetarget, UnmappedWhenNoTarget) {
    std::vector<std::string> src = {"WeirdBoneXYZ"};
    std::vector<std::string> tgt = {"Hips", "Spine"};
    BoneMap m = AutoMapBones(src, tgt);
    ASSERT_EQ(m.matches.size(), 1u);
    EXPECT_LT(m.matches[0].target_index, 0);
    EXPECT_EQ(m.matches[0].type, MatchType::None);
    EXPECT_EQ(MappedCount(m), 0);
}

// ── 手动覆盖 ─────────────────────────────────────────────────────────────────

// 测试 动画重定向：手动覆盖设置且清空
TEST(AnimRetarget, ManualOverrideSetsAndClears) {
    std::vector<std::string> src = {"BoneA"};
    std::vector<std::string> tgt = {"X", "Y"};
    BoneMap m = AutoMapBones(src, tgt);
    SetManualMapping(m, 0, 1);
    EXPECT_EQ(m.matches[0].target_index, 1);
    EXPECT_EQ(m.matches[0].type, MatchType::Manual);
    SetManualMapping(m, 0, -1);
    EXPECT_LT(m.matches[0].target_index, 0);
    EXPECT_EQ(m.matches[0].type, MatchType::None);
}

// ── 动画重定向：通道改写到目标索引/名，未映射的丢弃 ─────────────────────────

// 测试 动画重定向：重定向动画重映射通道
TEST(AnimRetarget, RetargetAnimationRemapsChannels) {
    std::vector<std::string> src = {"Hips", "Spine", "OrphanBone"};
    std::vector<std::string> tgt = {"pelvis", "chest"};  // Hips→pelvis(human), Spine→? chest is spine1

    // 手工构造确定的映射，避免依赖同义词细节：Hips->0, Spine->1, Orphan->none
    BoneMap m;
    m.matches.resize(3);
    m.matches[0] = {0, 0, MatchType::Humanoid};
    m.matches[1] = {1, 1, MatchType::Manual};
    m.matches[2] = {2, -1, MatchType::None};

    RawAnimation anim;
    anim.name = "Walk";
    anim.duration = 1.5f;
    RawAnimationChannel c0; c0.target_node_index = 0; c0.target_node_name = "Hips";
    c0.time_keys = {0.0f, 1.0f}; c0.position_keys = {glm::vec3(0.0f), glm::vec3(1.0f)};
    RawAnimationChannel c1; c1.target_node_index = 1; c1.target_node_name = "Spine";
    RawAnimationChannel c2; c2.target_node_index = 2; c2.target_node_name = "OrphanBone";
    anim.channels = {c0, c1, c2};

    RawAnimation out = RetargetAnimation(anim, src, tgt, m);

    EXPECT_EQ(out.name, "Walk_retargeted");
    EXPECT_FLOAT_EQ(out.duration, 1.5f);
    ASSERT_EQ(out.channels.size(), 2u);  // orphan dropped
    EXPECT_EQ(out.channels[0].target_node_index, 0);
    EXPECT_EQ(out.channels[0].target_node_name, "pelvis");
    EXPECT_EQ(out.channels[1].target_node_index, 1);
    EXPECT_EQ(out.channels[1].target_node_name, "chest");
    // 关键帧原样保留
    EXPECT_EQ(out.channels[0].time_keys.size(), 2u);
    EXPECT_EQ(out.channels[0].position_keys.size(), 2u);
}

// ── 动画重定向：通道仅带骨骼名（index=-1）也能解析 ──────────────────────────

// 测试 动画重定向：重定向解析通道按名称
TEST(AnimRetarget, RetargetResolvesChannelByName) {
    std::vector<std::string> src = {"Hips"};
    std::vector<std::string> tgt = {"Hips"};
    BoneMap m = AutoMapBones(src, tgt);

    RawAnimation anim;
    RawAnimationChannel c; c.target_node_index = -1; c.target_node_name = "Hips";
    anim.channels = {c};

    RawAnimation out = RetargetAnimation(anim, src, tgt, m);
    ASSERT_EQ(out.channels.size(), 1u);
    EXPECT_EQ(out.channels[0].target_node_index, 0);
    EXPECT_EQ(out.channels[0].target_node_name, "Hips");
}
