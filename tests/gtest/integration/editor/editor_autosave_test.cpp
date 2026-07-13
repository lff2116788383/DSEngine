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

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "editor_autosave_core.h"

using namespace dse::editor;

namespace {

std::string ReadAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// 每个用例独立的临时工作目录，析构时清理。
struct TempDir {
    std::filesystem::path dir;
    TempDir() {
        dir = std::filesystem::temp_directory_path() /
              ("dse_autosave_test_" +
               std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    std::string file(const std::string& name) const {
        return (dir / name).string();
    }
};

}  // namespace

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

// ── AtomicWriteFile ──────────────────────────────────────────────────────────

TEST(AutoSaveCore, AtomicTempPathIsSibling) {
    EXPECT_EQ(MakeAtomicTempPath("/a/b/scene.dscene"), "/a/b/scene.dscene.tmp");
}

TEST(AutoSaveCore, AtomicWriteCreatesFileAndLeavesNoTemp) {
    TempDir tmp;
    std::string target = tmp.file("Level.autosave.dscene");

    std::error_code ec;
    bool ok = AtomicWriteFile(
        target,
        [](const std::string& p) {
            std::ofstream(p, std::ios::binary) << "hello-scene";
            return true;
        },
        ec);

    EXPECT_TRUE(ok);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(std::filesystem::exists(target));
    EXPECT_EQ(ReadAll(target), "hello-scene");
    // rename 成功后临时兄弟文件不应残留。
    EXPECT_FALSE(std::filesystem::exists(MakeAtomicTempPath(target)));
}

TEST(AutoSaveCore, AtomicWriteReplacesExistingContent) {
    TempDir tmp;
    std::string target = tmp.file("Level.autosave.dscene");
    std::ofstream(target, std::ios::binary) << "OLD-GOOD-CONTENT";

    std::error_code ec;
    bool ok = AtomicWriteFile(
        target,
        [](const std::string& p) {
            std::ofstream(p, std::ios::binary) << "NEW";
            return true;
        },
        ec);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ReadAll(target), "NEW");
}

TEST(AutoSaveCore, AtomicWriteFailurePreservesExistingFile) {
    TempDir tmp;
    std::string target = tmp.file("Level.autosave.dscene");
    std::ofstream(target, std::ios::binary) << "OLD-GOOD-CONTENT";

    std::error_code ec;
    bool ok = AtomicWriteFile(
        target,
        [](const std::string& p) {
            // 模拟写到一半失败：产出临时文件后报告失败。
            std::ofstream(p, std::ios::binary) << "HALF-WRITTEN-GARBAGE";
            return false;
        },
        ec);

    EXPECT_FALSE(ok);
    EXPECT_TRUE(ec);
    // 已有文件必须原样保留，不能被截断/覆盖。
    EXPECT_EQ(ReadAll(target), "OLD-GOOD-CONTENT");
    // 失败路径必须清掉临时文件。
    EXPECT_FALSE(std::filesystem::exists(MakeAtomicTempPath(target)));
}

TEST(AutoSaveCore, AtomicWriteThrowIsTreatedAsFailure) {
    TempDir tmp;
    std::string target = tmp.file("Level.autosave.dscene");
    std::ofstream(target, std::ios::binary) << "OLD-GOOD-CONTENT";

    std::error_code ec;
    bool ok = AtomicWriteFile(
        target,
        [](const std::string& p) -> bool {
            std::ofstream(p, std::ios::binary) << "PARTIAL";
            throw std::runtime_error("save blew up");
        },
        ec);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ReadAll(target), "OLD-GOOD-CONTENT");
    EXPECT_FALSE(std::filesystem::exists(MakeAtomicTempPath(target)));
}

TEST(AutoSaveCore, AtomicWriteClearsStaleTempFromPriorAbort) {
    TempDir tmp;
    std::string target = tmp.file("Level.autosave.dscene");
    // 上次中断遗留的陈旧临时文件。
    std::ofstream(MakeAtomicTempPath(target), std::ios::binary) << "STALE";

    std::error_code ec;
    bool ok = AtomicWriteFile(
        target,
        [](const std::string& p) {
            std::ofstream(p, std::ios::binary) << "FRESH";
            return true;
        },
        ec);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ReadAll(target), "FRESH");
    EXPECT_FALSE(std::filesystem::exists(MakeAtomicTempPath(target)));
}

// ── AtomicWriteAll（多文档事务）────────────────────────────────────────────

TEST(AutoSaveCore, AtomicWriteAllCommitsEveryDocument) {
    TempDir tmp;
    std::string a = tmp.file("A.autosave.dscene");
    std::string b = tmp.file("B.autosave.dscene");
    std::string c = tmp.file("C.autosave.dscene");

    std::vector<AutoSaveItem> items = {
        {a, [](const std::string& p) { std::ofstream(p, std::ios::binary) << "AAA"; return true; }},
        {b, [](const std::string& p) { std::ofstream(p, std::ios::binary) << "BBB"; return true; }},
        {c, [](const std::string& p) { std::ofstream(p, std::ios::binary) << "CCC"; return true; }},
    };

    std::error_code ec;
    EXPECT_TRUE(AtomicWriteAll(items, ec));
    EXPECT_EQ(ReadAll(a), "AAA");
    EXPECT_EQ(ReadAll(b), "BBB");
    EXPECT_EQ(ReadAll(c), "CCC");
    EXPECT_FALSE(std::filesystem::exists(MakeAtomicTempPath(a)));
    EXPECT_FALSE(std::filesystem::exists(MakeAtomicTempPath(b)));
    EXPECT_FALSE(std::filesystem::exists(MakeAtomicTempPath(c)));
}

TEST(AutoSaveCore, AtomicWriteAllRollsBackOnAnyFailure) {
    TempDir tmp;
    std::string a = tmp.file("A.autosave.dscene");
    std::string b = tmp.file("B.autosave.dscene");
    // b 已有完整旧内容，事务失败后必须原样保留、不被提交覆盖。
    std::ofstream(b, std::ios::binary) << "OLD-B";

    std::vector<AutoSaveItem> items = {
        {a, [](const std::string& p) { std::ofstream(p, std::ios::binary) << "AAA"; return true; }},
        {b, [](const std::string&) { return false; }},  // 第二项写入失败
    };

    std::error_code ec;
    EXPECT_FALSE(AtomicWriteAll(items, ec));
    // 全有或全无：没有任何最终文件被改动，也不残留临时文件。
    EXPECT_FALSE(std::filesystem::exists(a));
    EXPECT_EQ(ReadAll(b), "OLD-B");
    EXPECT_FALSE(std::filesystem::exists(MakeAtomicTempPath(a)));
    EXPECT_FALSE(std::filesystem::exists(MakeAtomicTempPath(b)));
}

// ── CollectRecoveryFiles（枚举全部可恢复文档）──────────────────────────────

TEST(AutoSaveCore, CollectRecoveryFilesFindsAllSorted) {
    TempDir tmp;
    std::ofstream(tmp.file("A.autosave.dscene"), std::ios::binary) << "a";
    std::ofstream(tmp.file("B.autosave.dscene"), std::ios::binary) << "b";
    std::ofstream(tmp.file("regular.dscene"), std::ios::binary) << "x";  // 非恢复文件
    std::ofstream(tmp.file("notes.txt"), std::ios::binary) << "y";

    auto files = CollectRecoveryFiles(tmp.dir.string());
    ASSERT_EQ(files.size(), 2u);
    EXPECT_NE(files[0].find("A.autosave.dscene"), std::string::npos);
    EXPECT_NE(files[1].find("B.autosave.dscene"), std::string::npos);
}

TEST(AutoSaveCore, CollectRecoveryFilesMissingDirIsEmpty) {
    auto files = CollectRecoveryFiles(
        (std::filesystem::temp_directory_path() / "dse_no_such_dir_zzz").string());
    EXPECT_TRUE(files.empty());
}

// ── ValidateRecoveryScene（加载前逐文档结构校验）───────────────────────────

TEST(AutoSaveCore, ValidateRecoveryAcceptsWellFormedScene) {
    auto v = ValidateRecoveryScene(
        R"({"material_schema_version":2,"entities":[{"id":1},{"id":2}]})");
    EXPECT_TRUE(v.loadable);
    EXPECT_EQ(v.entity_count, 2);
    EXPECT_EQ(v.material_schema_version, 2);
}

TEST(AutoSaveCore, ValidateRecoveryAcceptsEmptyEntitiesArray) {
    auto v = ValidateRecoveryScene(R"({"entities":[]})");
    EXPECT_TRUE(v.loadable);
    EXPECT_EQ(v.entity_count, 0);
    EXPECT_EQ(v.material_schema_version, -1);  // 无版本字段 → -1
}

TEST(AutoSaveCore, ValidateRecoveryRejectsTruncatedJson) {
    // 上次崩溃写到一半的典型形态：JSON 被截断。
    auto v = ValidateRecoveryScene(R"({"entities":[{"id":1)");
    EXPECT_FALSE(v.loadable);
    EXPECT_FALSE(v.message.empty());
}

TEST(AutoSaveCore, ValidateRecoveryRejectsMissingEntities) {
    auto v = ValidateRecoveryScene(R"({"material_schema_version":2})");
    EXPECT_FALSE(v.loadable);
}

TEST(AutoSaveCore, ValidateRecoveryRejectsNonObjectRoot) {
    EXPECT_FALSE(ValidateRecoveryScene("[]").loadable);
    EXPECT_FALSE(ValidateRecoveryScene("").loadable);
}
