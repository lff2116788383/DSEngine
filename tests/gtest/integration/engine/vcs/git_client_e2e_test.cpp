/**
 * @file git_client_e2e_test.cpp
 * @brief P1-2 E2E integration tests for Git fetch/pull/push/merge/conflict.
 *
 * Creates two local repos in the OS temp directory — one acts as "remote"
 * (bare), the other as the working "local" clone. Uses file:// protocol,
 * so no network or credentials are needed.
 *
 * All tests are skipped if `git` is not on PATH.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

#include "engine/vcs/git_client.h"

using namespace dse::vcs;
namespace fs = std::filesystem;

namespace {

fs::path MakeTempDir(const std::string& tag) {
    static std::atomic<int> counter{0};
    auto base = fs::temp_directory_path() /
                ("dse_git_e2e_" + tag + "_" +
                 std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                 "_" + std::to_string(counter.fetch_add(1)));
    fs::remove_all(base);
    fs::create_directories(base);
    return base;
}

void WriteFile(const fs::path& p, const std::string& content) {
    std::ofstream f(p, std::ios::binary);
    f << content;
}

std::string ReadFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::string raw((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
    // Normalize CRLF → LF for cross-platform comparisons.
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (c != '\r') out += c;
    }
    return out;
}

/// Fixture: sets up a bare "remote" repo and a "local" clone.
class GitE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        GitClient probe;
        if (!probe.GitAvailable()) {
            GTEST_SKIP() << "git not available on PATH";
        }

        remote_dir_ = MakeTempDir("remote");
        local_dir_  = MakeTempDir("local");

        // Create bare remote repo with default branch "main"
        GitClient remote;
        remote.SetRepoDir(remote_dir_);
        ASSERT_TRUE(remote.Run({"init", "--bare", "--initial-branch=main"}).ok)
            << "failed to init bare remote";
        // For older git versions that don't support --initial-branch:
        remote.Run({"symbolic-ref", "HEAD", "refs/heads/main"});

        // Clone the bare remote into local_dir_
        GitClient init;
        init.SetRepoDir(local_dir_);
        // Clone into our local dir (which already exists but is empty)
        ASSERT_TRUE(init.Run({"clone", remote_dir_.string(), "."}).ok)
            << "git clone failed";

        local_.SetRepoDir(local_dir_);
        ASSERT_TRUE(local_.DiscoverRepo()) << "discover local repo failed";
        ASSERT_TRUE(local_.Run({"config", "user.email", "test@dse.local"}).ok);
        ASSERT_TRUE(local_.Run({"config", "user.name", "DSE Test"}).ok);
        // Disable line-ending conversion for deterministic file comparisons
        ASSERT_TRUE(local_.Run({"config", "core.autocrlf", "false"}).ok);
        // Ensure consistent branch name "main" regardless of git version default.
        // After cloning an empty repo, HEAD is an unborn branch; rename works
        // whether the default is "master" or "main".
        local_.Run({"branch", "-m", "main"});  // ok if already "main"

        // The remote also needs identity for pushes that trigger server-side hooks.
        // For bare repos, git uses the pusher's identity, so this is fine.
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(remote_dir_, ec);
        fs::remove_all(local_dir_, ec);
    }

    fs::path remote_dir_;
    fs::path local_dir_;
    GitClient local_;
};

} // namespace

// ============================================================
// Fetch
// ============================================================

TEST_F(GitE2ETest, FetchRetrievesNewBranch) {
    // Create initial commit + push to remote
    WriteFile(local_dir_ / "README.md", "hello\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("initial", false).ok);
    ASSERT_TRUE(local_.Push(true).ok);

    // Create a new branch on the remote directly (via a second clone)
    fs::path other_dir = MakeTempDir("other");
    GitClient other;
    other.SetRepoDir(other_dir);
    ASSERT_TRUE(other.Run({"clone", remote_dir_.string(), "."}).ok);
    ASSERT_TRUE(other.Run({"config", "user.email", "test2@dse.local"}).ok);
    ASSERT_TRUE(other.Run({"config", "user.name", "DSE Test 2"}).ok);
    ASSERT_TRUE(other.Run({"checkout", "-b", "feature-branch"}).ok);
    WriteFile(other_dir / "feature.txt", "new\n");
    ASSERT_TRUE(other.StageAll().ok);
    ASSERT_TRUE(other.Commit("feature work", false).ok);
    ASSERT_TRUE(other.Push(true).ok);

    // Now fetch from the local clone — should see the new branch
    GitResult r = local_.Fetch();
    EXPECT_TRUE(r.ok) << r.error;

    auto branches = local_.GetBranches();
    bool found_remote_feature = false;
    for (const auto& b : branches) {
        if (b.name.find("feature-branch") != std::string::npos && b.remote) {
            found_remote_feature = true;
        }
    }
    EXPECT_TRUE(found_remote_feature);

    std::error_code ec;
    fs::remove_all(other_dir, ec);
}

TEST_F(GitE2ETest, FetchShowsAheadAfterRemoteCommit) {
    // Push initial commit
    WriteFile(local_dir_ / "base.txt", "v1\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("base", false).ok);
    ASSERT_TRUE(local_.Push(true).ok);

    // Another clone pushes a new commit
    fs::path other_dir = MakeTempDir("other2");
    GitClient other;
    other.SetRepoDir(other_dir);
    ASSERT_TRUE(other.Run({"clone", remote_dir_.string(), "."}).ok);
    ASSERT_TRUE(other.Run({"config", "user.email", "t@t.local"}).ok);
    ASSERT_TRUE(other.Run({"config", "user.name", "T"}).ok);
    WriteFile(other_dir / "base.txt", "v2\n");
    ASSERT_TRUE(other.StageAll().ok);
    ASSERT_TRUE(other.Commit("update", false).ok);
    ASSERT_TRUE(other.Run({"push", "origin", "main"}).ok);

    // Fetch on local
    ASSERT_TRUE(local_.Fetch().ok);

    // Local should now be behind the remote
    RepoStatus s = local_.GetStatus();
    EXPECT_GT(s.behind, 0);

    std::error_code ec;
    fs::remove_all(other_dir, ec);
}

// ============================================================
// Pull (fast-forward)
// ============================================================

TEST_F(GitE2ETest, PullFastForward) {
    // Push initial commit
    WriteFile(local_dir_ / "file.txt", "content\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("initial", false).ok);
    ASSERT_TRUE(local_.Push(true).ok);

    // Another clone pushes a new commit
    fs::path other_dir = MakeTempDir("other_ff");
    GitClient other;
    other.SetRepoDir(other_dir);
    ASSERT_TRUE(other.Run({"clone", remote_dir_.string(), "."}).ok);
    ASSERT_TRUE(other.Run({"config", "user.email", "t@t.local"}).ok);
    ASSERT_TRUE(other.Run({"config", "user.name", "T"}).ok);
    WriteFile(other_dir / "new.txt", "new content\n");
    ASSERT_TRUE(other.StageAll().ok);
    ASSERT_TRUE(other.Commit("add new file", false).ok);
    ASSERT_TRUE(other.Run({"push", "origin", "main"}).ok);

    // Pull on local — should fast-forward
    GitResult r = local_.Pull();
    EXPECT_TRUE(r.ok) << r.error;

    // The new file should now exist locally
    EXPECT_TRUE(fs::exists(local_dir_ / "new.txt"));

    std::error_code ec;
    fs::remove_all(other_dir, ec);
}

// ============================================================
// Push
// ============================================================

TEST_F(GitE2ETest, PushNewCommitToRemote) {
    WriteFile(local_dir_ / "push.txt", "data\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("push test", false).ok);

    GitResult r = local_.Push(true);
    EXPECT_TRUE(r.ok) << r.error;

    // Verify by cloning the remote fresh
    fs::path verify_dir = MakeTempDir("verify");
    GitClient verify;
    verify.SetRepoDir(verify_dir);
    ASSERT_TRUE(verify.Run({"clone", remote_dir_.string(), "."}).ok);
    EXPECT_TRUE(fs::exists(verify_dir / "push.txt"));
    EXPECT_EQ(ReadFile(verify_dir / "push.txt"), "data\n");

    std::error_code ec;
    fs::remove_all(verify_dir, ec);
}

TEST_F(GitE2ETest, PushWithoutUpstreamFailsCleanly) {
    // Commit without setting upstream
    WriteFile(local_dir_ / "x.txt", "x\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("x", false).ok);

    // Push without -u and without upstream set → should fail with clear error.
    // Set push.default to "nothing" to ensure no implicit remote branch creation.
    ASSERT_TRUE(local_.Run({"config", "push.default", "nothing"}).ok);
    GitResult r = local_.Push(false);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
}

// ============================================================
// Conflict detection and resolution
// ============================================================

TEST_F(GitE2ETest, MergeConflictDetection) {
    // Push initial commit
    WriteFile(local_dir_ / "conflict.txt", "base\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("base", false).ok);
    ASSERT_TRUE(local_.Push(true).ok);

    // Another clone makes a conflicting change
    fs::path other_dir = MakeTempDir("other_conflict");
    GitClient other;
    other.SetRepoDir(other_dir);
    ASSERT_TRUE(other.Run({"clone", remote_dir_.string(), "."}).ok);
    ASSERT_TRUE(other.Run({"config", "user.email", "t@t.local"}).ok);
    ASSERT_TRUE(other.Run({"config", "user.name", "T"}).ok);
    WriteFile(other_dir / "conflict.txt", "theirs\n");
    ASSERT_TRUE(other.StageAll().ok);
    ASSERT_TRUE(other.Commit("theirs change", false).ok);
    ASSERT_TRUE(other.Run({"push", "origin", "main"}).ok);

    // Local makes a conflicting change
    WriteFile(local_dir_ / "conflict.txt", "ours\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("ours change", false).ok);

    // Fetch + merge → conflict
    ASSERT_TRUE(local_.Fetch().ok);
    GitResult merge = local_.Merge("origin/main");
    // Merge returns non-zero on conflict
    EXPECT_FALSE(merge.ok);

    // Status should show conflict
    RepoStatus s = local_.GetStatus();
    EXPECT_TRUE(s.merging);
    bool found_conflict = false;
    for (const auto& e : s.entries) {
        if (e.path == "conflict.txt" && e.conflict) {
            found_conflict = true;
        }
    }
    EXPECT_TRUE(found_conflict);

    std::error_code ec;
    fs::remove_all(other_dir, ec);
}

TEST_F(GitE2ETest, ConflictResolveOurs) {
    // Push initial commit
    WriteFile(local_dir_ / "c.txt", "base\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("base", false).ok);
    ASSERT_TRUE(local_.Push(true).ok);

    // Other clone: conflicting change
    fs::path other_dir = MakeTempDir("other_ours");
    GitClient other;
    other.SetRepoDir(other_dir);
    ASSERT_TRUE(other.Run({"clone", remote_dir_.string(), "."}).ok);
    ASSERT_TRUE(other.Run({"config", "user.email", "t@t.local"}).ok);
    ASSERT_TRUE(other.Run({"config", "user.name", "T"}).ok);
    WriteFile(other_dir / "c.txt", "theirs\n");
    ASSERT_TRUE(other.StageAll().ok);
    ASSERT_TRUE(other.Commit("theirs", false).ok);
    ASSERT_TRUE(other.Run({"push", "origin", "main"}).ok);

    // Local: conflicting change
    WriteFile(local_dir_ / "c.txt", "ours\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("ours", false).ok);

    // Fetch + merge → conflict
    ASSERT_TRUE(local_.Fetch().ok);
    ASSERT_FALSE(local_.Merge("origin/main").ok);

    // Resolve with "ours"
    GitResult r = local_.AcceptOurs("c.txt");
    EXPECT_TRUE(r.ok) << r.error;

    // Commit the merge
    ASSERT_TRUE(local_.Commit("merge: keep ours", false).ok);

    // File should contain "ours"
    EXPECT_EQ(ReadFile(local_dir_ / "c.txt"), "ours\n");

    std::error_code ec;
    fs::remove_all(other_dir, ec);
}

TEST_F(GitE2ETest, ConflictResolveTheirs) {
    // Push initial commit
    WriteFile(local_dir_ / "c.txt", "base\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("base", false).ok);
    ASSERT_TRUE(local_.Push(true).ok);

    // Other clone: conflicting change
    fs::path other_dir = MakeTempDir("other_theirs");
    GitClient other;
    other.SetRepoDir(other_dir);
    ASSERT_TRUE(other.Run({"clone", remote_dir_.string(), "."}).ok);
    ASSERT_TRUE(other.Run({"config", "user.email", "t@t.local"}).ok);
    ASSERT_TRUE(other.Run({"config", "user.name", "T"}).ok);
    WriteFile(other_dir / "c.txt", "theirs\n");
    ASSERT_TRUE(other.StageAll().ok);
    ASSERT_TRUE(other.Commit("theirs", false).ok);
    ASSERT_TRUE(other.Run({"push", "origin", "main"}).ok);

    // Local: conflicting change
    WriteFile(local_dir_ / "c.txt", "ours\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("ours", false).ok);

    // Fetch + merge → conflict
    ASSERT_TRUE(local_.Fetch().ok);
    ASSERT_FALSE(local_.Merge("origin/main").ok);

    // Resolve with "theirs"
    GitResult r = local_.AcceptTheirs("c.txt");
    EXPECT_TRUE(r.ok) << r.error;

    // Commit the merge
    ASSERT_TRUE(local_.Commit("merge: take theirs", false).ok);

    // File should contain "theirs"
    EXPECT_EQ(ReadFile(local_dir_ / "c.txt"), "theirs\n");

    std::error_code ec;
    fs::remove_all(other_dir, ec);
}

TEST_F(GitE2ETest, MergeAbortCleansUp) {
    // Push initial commit
    WriteFile(local_dir_ / "c.txt", "base\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("base", false).ok);
    ASSERT_TRUE(local_.Push(true).ok);

    // Other clone: conflicting change
    fs::path other_dir = MakeTempDir("other_abort");
    GitClient other;
    other.SetRepoDir(other_dir);
    ASSERT_TRUE(other.Run({"clone", remote_dir_.string(), "."}).ok);
    ASSERT_TRUE(other.Run({"config", "user.email", "t@t.local"}).ok);
    ASSERT_TRUE(other.Run({"config", "user.name", "T"}).ok);
    WriteFile(other_dir / "c.txt", "theirs\n");
    ASSERT_TRUE(other.StageAll().ok);
    ASSERT_TRUE(other.Commit("theirs", false).ok);
    ASSERT_TRUE(other.Run({"push", "origin", "main"}).ok);

    // Local: conflicting change
    WriteFile(local_dir_ / "c.txt", "ours\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("ours", false).ok);

    // Fetch + merge → conflict
    ASSERT_TRUE(local_.Fetch().ok);
    ASSERT_FALSE(local_.Merge("origin/main").ok);

    // Abort
    GitResult r = local_.MergeAbort();
    EXPECT_TRUE(r.ok) << r.error;

    // Should no longer be in merge state
    RepoStatus s = local_.GetStatus();
    EXPECT_FALSE(s.merging);

    // File should be back to "ours" (pre-merge state)
    EXPECT_EQ(ReadFile(local_dir_ / "c.txt"), "ours\n");

    std::error_code ec;
    fs::remove_all(other_dir, ec);
}

// ============================================================
// Credential diagnostic (no crash without credentials)
// ============================================================

TEST_F(GitE2ETest, PushToNonexistentRemoteFailsCleanly) {
    WriteFile(local_dir_ / "x.txt", "x\n");
    ASSERT_TRUE(local_.StageAll().ok);
    ASSERT_TRUE(local_.Commit("x", false).ok);

    // Add a nonexistent remote and try to push to it
    ASSERT_TRUE(local_.Run({"remote", "add", "bogus",
                            "file:///nonexistent/path/repo.git"}).ok);
    GitResult r = local_.Run({"push", "bogus", "main"});
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
    // Should not crash — error should be a clear diagnostic
    // git push to nonexistent remote returns non-zero exit code
    EXPECT_NE(r.exit_code, 0);
}
