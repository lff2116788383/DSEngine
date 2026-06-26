/**
 * @file editor_autosave_test.cpp
 * @brief 自动保存纯核心（editor_autosave_core）的无头测试。
 *
 * 覆盖：SanitizeSceneName（非法字符净化 + 空名兜底）、MakeAutoSaveFileName、
 * IsAutoSaveRecoveryFile（恢复文件谓词）、ClampAutoSaveInterval（间隔下限）、
 * DecideAutoSave（播放中 / 未启用 / 未脏 / 首帧计时 / 未到间隔 / 应保存 状态机）。
 * 文件系统读写 / 单例 / ImGui 对话框不在此覆盖。
 */

#include <gtest/gtest.h>

#include <string>

#include "editor_autosave_core.h"

using namespace dse::editor;

// ── SanitizeSceneName ────────────────────────────────────────────────────────

TEST(AutoSaveCore, SanitizeEmptyIsUntitled) {
    EXPECT_EQ(SanitizeSceneName(""), "Untitled");
}

TEST(AutoSaveCore, SanitizeKeepsCleanName) {
    EXPECT_EQ(SanitizeSceneName("Level_01"), "Level_01");
}

TEST(AutoSaveCore, SanitizeReplacesIllegalChars) {
    EXPECT_EQ(SanitizeSceneName("a/b\\c:d*e?f\"g<h>i|j"), "a_b_c_d_e_f_g_h_i_j");
}

// ── MakeAutoSaveFileName ─────────────────────────────────────────────────────

TEST(AutoSaveCore, MakeFileNameAppendsSuffix) {
    EXPECT_EQ(MakeAutoSaveFileName("Level_01"), "Level_01.autosave.dscene");
}

TEST(AutoSaveCore, MakeFileNameEmptyUsesUntitled) {
    EXPECT_EQ(MakeAutoSaveFileName(""), "Untitled.autosave.dscene");
}

TEST(AutoSaveCore, MakeFileNameSanitizesPath) {
    EXPECT_EQ(MakeAutoSaveFileName("a/b"), "a_b.autosave.dscene");
}

// ── IsAutoSaveRecoveryFile ───────────────────────────────────────────────────

TEST(AutoSaveCore, RecoveryFileMatchesGeneratedName) {
    EXPECT_TRUE(IsAutoSaveRecoveryFile(MakeAutoSaveFileName("MyScene")));
    EXPECT_TRUE(IsAutoSaveRecoveryFile("Foo.autosave.dscene"));
    EXPECT_TRUE(IsAutoSaveRecoveryFile("/tmp/dir/Foo.autosave.dscene"));
}

TEST(AutoSaveCore, RecoveryFileRejectsNonMatches) {
    EXPECT_FALSE(IsAutoSaveRecoveryFile("Foo.dscene"));       // 无 .autosave
    EXPECT_FALSE(IsAutoSaveRecoveryFile("Foo.autosave.bak")); // 扩展名不符
    EXPECT_FALSE(IsAutoSaveRecoveryFile("readme.txt"));
}

// ── ClampAutoSaveInterval ────────────────────────────────────────────────────

TEST(AutoSaveCore, ClampIntervalEnforcesMinimum) {
    EXPECT_DOUBLE_EQ(ClampAutoSaveInterval(0.0), 10.0);
    EXPECT_DOUBLE_EQ(ClampAutoSaveInterval(5.0), 10.0);
    EXPECT_DOUBLE_EQ(ClampAutoSaveInterval(10.0), 10.0);
    EXPECT_DOUBLE_EQ(ClampAutoSaveInterval(60.0), 60.0);
}

// ── DecideAutoSave ───────────────────────────────────────────────────────────

TEST(AutoSaveCore, DecideSkipsInPlayMode) {
    EXPECT_EQ(DecideAutoSave(/*play*/true, /*enabled*/true, /*dirty*/true,
                             /*last*/100.0, /*now*/200.0, /*interval*/30.0),
              AutoSaveDecision::Skip);
}

TEST(AutoSaveCore, DecideSkipsWhenDisabled) {
    EXPECT_EQ(DecideAutoSave(false, /*enabled*/false, true, 100.0, 200.0, 30.0),
              AutoSaveDecision::Skip);
}

TEST(AutoSaveCore, DecideSkipsWhenNotDirty) {
    EXPECT_EQ(DecideAutoSave(false, true, /*dirty*/false, 100.0, 200.0, 30.0),
              AutoSaveDecision::Skip);
}

TEST(AutoSaveCore, DecideInitsTimerOnFirstTick) {
    // last_save_time == 0 → 仅初始化计时
    EXPECT_EQ(DecideAutoSave(false, true, true, /*last*/0.0, /*now*/123.0, 30.0),
              AutoSaveDecision::InitTimer);
}

TEST(AutoSaveCore, DecideSkipsBeforeInterval) {
    // 距上次 20s < 间隔 30s
    EXPECT_EQ(DecideAutoSave(false, true, true, /*last*/100.0, /*now*/120.0, 30.0),
              AutoSaveDecision::Skip);
}

TEST(AutoSaveCore, DecideSavesAfterInterval) {
    // 距上次 40s >= 间隔 30s
    EXPECT_EQ(DecideAutoSave(false, true, true, /*last*/100.0, /*now*/140.0, 30.0),
              AutoSaveDecision::Save);
}

TEST(AutoSaveCore, DecideAppliesIntervalClamp) {
    // 配置 1s 被钳到 10s：距上次 5s 仍不够
    EXPECT_EQ(DecideAutoSave(false, true, true, 100.0, 105.0, /*interval*/1.0),
              AutoSaveDecision::Skip);
    // 距上次 11s >= 钳后的 10s → 保存
    EXPECT_EQ(DecideAutoSave(false, true, true, 100.0, 111.0, /*interval*/1.0),
              AutoSaveDecision::Save);
}
