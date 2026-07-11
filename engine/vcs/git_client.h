/**
 * @file git_client.h
 * @brief Real Git integration backed by the unified process runner (P1-2).
 *
 * Thin, UI-agnostic wrapper around the `git` CLI. Every operation shells out
 * through dse::platform::RunProcess (argument arrays, no unsafe shell strings)
 * and parses porcelain output. No demo/simulated data lives here — callers get
 * the actual repository state or an explicit error.
 */
#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "engine/core/dse_export.h"

namespace dse::vcs {

enum class FileChange {
    None, Modified, Added, Deleted, Renamed, Copied, TypeChanged, Untracked, Conflict, Ignored
};

/// One entry from `git status`.
struct StatusEntry {
    std::string path;
    std::string orig_path;   ///< For renames/copies: the source path.
    FileChange index = FileChange::None;    ///< Staged change (index vs HEAD).
    FileChange worktree = FileChange::None; ///< Unstaged change (worktree vs index).
    bool staged = false;     ///< Has a staged component.
    bool unstaged = false;   ///< Has an unstaged/worktree component.
    bool conflict = false;   ///< Unmerged (conflict) entry.
    bool untracked = false;
};

/// Aggregate repository status.
struct RepoStatus {
    bool is_repo = false;
    std::string root;
    std::string branch;      ///< Current branch, or "" if detached.
    std::string upstream;    ///< Tracking branch, or "".
    bool detached = false;
    int ahead = 0;
    int behind = 0;
    bool merging = false;
    bool rebasing = false;
    bool cherry_picking = false;
    std::vector<StatusEntry> entries;
    std::string error;       ///< Non-empty on failure.
};

struct CommitInfo {
    std::string hash;
    std::string short_hash;
    std::string author;
    std::string email;
    std::string date;        ///< Relative (e.g. "2 hours ago").
    std::string subject;
    std::vector<std::string> parents;
    std::string refs;        ///< Decoration (branch/tag names), raw.
};

struct BranchInfo {
    std::string name;
    bool current = false;
    bool remote = false;
    std::string upstream;
    int ahead = 0;
    int behind = 0;
};

/// Result of an operation. `output` is combined stdout+stderr for display.
struct GitResult {
    bool ok = false;
    int exit_code = -1;
    bool launched = false;
    bool timed_out = false;
    bool canceled = false;
    std::string output;
    std::string error;
};

using GitLineFn = std::function<void(const std::string& line, bool is_stderr)>;

class DSE_EXPORT GitClient {
public:
    GitClient() = default;
    explicit GitClient(std::filesystem::path repo_dir) : repo_dir_(std::move(repo_dir)) {}

    /// Set any directory inside (or at) the repository.
    void SetRepoDir(std::filesystem::path dir) { repo_dir_ = std::move(dir); root_.clear(); is_repo_ = false; }

    /// Locate the working-tree root; populates Root()/IsRepo().
    bool DiscoverRepo();
    const std::string& Root() const { return root_; }
    bool IsRepo() const { return is_repo_; }
    bool GitAvailable() const;

    RepoStatus GetStatus();
    std::string GetDiff(const std::string& path, bool staged);

    GitResult Stage(const std::string& path);
    GitResult StageAll();
    GitResult Unstage(const std::string& path);
    GitResult DiscardWorktree(const std::string& path); ///< Destructive; caller confirms.
    GitResult Commit(const std::string& message, bool amend = false);

    std::vector<CommitInfo> GetLog(int max_count = 100);
    std::vector<BranchInfo> GetBranches();
    GitResult CreateBranch(const std::string& name, bool checkout);
    GitResult Checkout(const std::string& name);
    GitResult DeleteBranch(const std::string& name, bool force);
    GitResult Merge(const std::string& name);

    // Network operations — potentially slow; run these through the editor's
    // background task service. cancel/on_line stream progress.
    GitResult Fetch(const std::atomic<bool>* cancel = nullptr, const GitLineFn& on_line = {});
    GitResult Pull(const std::atomic<bool>* cancel = nullptr, const GitLineFn& on_line = {});
    GitResult Push(bool set_upstream, const std::atomic<bool>* cancel = nullptr, const GitLineFn& on_line = {});

    // Conflict resolution.
    GitResult AcceptOurs(const std::string& path);
    GitResult AcceptTheirs(const std::string& path);
    GitResult MergeAbort();
    GitResult RebaseAbort();
    GitResult RebaseContinue();

    /// Run an arbitrary git subcommand from the repo root.
    GitResult Run(const std::vector<std::string>& args,
                  std::chrono::milliseconds timeout = std::chrono::seconds(30),
                  const std::atomic<bool>* cancel = nullptr,
                  const GitLineFn& on_line = {});

private:
    std::filesystem::path WorkingDir() const;
    std::filesystem::path repo_dir_;
    std::string root_;
    bool is_repo_ = false;
};

} // namespace dse::vcs
