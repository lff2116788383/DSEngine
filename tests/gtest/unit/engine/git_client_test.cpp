// Unit tests for the real Git client (P1-2). Operates on a throwaway repo
// created in the OS temp directory; skipped entirely if git is unavailable.
#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

#include "engine/vcs/git_client.h"

using namespace dse::vcs;
namespace fs = std::filesystem;

namespace {

fs::path MakeTempRepoDir() {
    static std::atomic<int> counter{0};
    auto base = fs::temp_directory_path() /
                ("dse_git_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                 "_" + std::to_string(counter.fetch_add(1)));
    fs::remove_all(base);
    fs::create_directories(base);
    return base;
}

void WriteFile(const fs::path& p, const std::string& content) {
    std::ofstream f(p, std::ios::binary);
    f << content;
}

// Fixture that initializes a git repo with an identity so commits succeed.
class GitRepoTest : public ::testing::Test {
protected:
    void SetUp() override {
        GitClient probe;
        if (!probe.GitAvailable()) { GTEST_SKIP() << "git not available on PATH"; }
        dir_ = MakeTempRepoDir();
        client_.SetRepoDir(dir_);
        ASSERT_TRUE(client_.Run({"init"}).ok);
        ASSERT_TRUE(client_.DiscoverRepo());
        ASSERT_TRUE(client_.Run({"config", "user.email", "test@dse.local"}).ok);
        ASSERT_TRUE(client_.Run({"config", "user.name", "DSE Test"}).ok);
        ASSERT_TRUE(client_.Run({"checkout", "-b", "main"}).ok);
    }
    void TearDown() override {
        std::error_code ec;
        if (!dir_.empty()) fs::remove_all(dir_, ec);
    }
    fs::path dir_;
    GitClient client_;
};

} // namespace

TEST_F(GitRepoTest, DetectsUntrackedThenStageAndCommit) {
    WriteFile(dir_ / "hello.txt", "one\n");

    RepoStatus s = client_.GetStatus();
    ASSERT_TRUE(s.is_repo) << s.error;
    ASSERT_EQ(s.entries.size(), 1u);
    EXPECT_EQ(s.entries[0].path, "hello.txt");
    EXPECT_TRUE(s.entries[0].untracked);

    ASSERT_TRUE(client_.Stage("hello.txt").ok);
    s = client_.GetStatus();
    ASSERT_EQ(s.entries.size(), 1u);
    EXPECT_TRUE(s.entries[0].staged);
    EXPECT_FALSE(s.entries[0].untracked);

    ASSERT_TRUE(client_.Commit("initial commit", false).ok);
    s = client_.GetStatus();
    EXPECT_TRUE(s.entries.empty());

    auto log = client_.GetLog(10);
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0].subject, "initial commit");
    EXPECT_FALSE(log[0].short_hash.empty());
    EXPECT_EQ(log[0].author, "DSE Test");
}

TEST_F(GitRepoTest, ModifiedFileShowsUnstaged) {
    WriteFile(dir_ / "a.txt", "v1\n");
    ASSERT_TRUE(client_.StageAll().ok);
    ASSERT_TRUE(client_.Commit("c1", false).ok);

    WriteFile(dir_ / "a.txt", "v2\n");
    RepoStatus s = client_.GetStatus();
    ASSERT_EQ(s.entries.size(), 1u);
    EXPECT_EQ(s.entries[0].worktree, FileChange::Modified);
    EXPECT_TRUE(s.entries[0].unstaged);

    std::string diff = client_.GetDiff("a.txt", false);
    EXPECT_NE(diff.find("+v2"), std::string::npos);
    EXPECT_NE(diff.find("-v1"), std::string::npos);
}

TEST_F(GitRepoTest, BranchCreateCheckoutAndList) {
    WriteFile(dir_ / "f.txt", "x\n");
    ASSERT_TRUE(client_.StageAll().ok);
    ASSERT_TRUE(client_.Commit("base", false).ok);

    ASSERT_TRUE(client_.CreateBranch("feature", true).ok);
    auto branches = client_.GetBranches();
    bool found_feature = false, feature_current = false;
    for (auto& b : branches) {
        if (b.name == "feature") { found_feature = true; feature_current = b.current; }
    }
    EXPECT_TRUE(found_feature);
    EXPECT_TRUE(feature_current);

    ASSERT_TRUE(client_.Checkout("main").ok);
    ASSERT_TRUE(client_.DeleteBranch("feature", true).ok);
    branches = client_.GetBranches();
    for (auto& b : branches) EXPECT_NE(b.name, "feature");
}

TEST_F(GitRepoTest, UnstageMovesBackToWorktree) {
    WriteFile(dir_ / "s.txt", "content\n");
    ASSERT_TRUE(client_.Stage("s.txt").ok);
    RepoStatus s = client_.GetStatus();
    ASSERT_EQ(s.entries.size(), 1u);
    EXPECT_TRUE(s.entries[0].staged);

    ASSERT_TRUE(client_.Unstage("s.txt").ok);
    s = client_.GetStatus();
    ASSERT_EQ(s.entries.size(), 1u);
    EXPECT_TRUE(s.entries[0].untracked);
    EXPECT_FALSE(s.entries[0].staged);
}
