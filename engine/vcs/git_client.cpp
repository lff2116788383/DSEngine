#include "engine/vcs/git_client.h"

#include "engine/platform/process.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace dse::vcs {

namespace {

std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (unsigned char)s[b] <= ' ') ++b;
    while (e > b && (unsigned char)s[e - 1] <= ' ') --e;
    return s.substr(b, e - b);
}

FileChange MapCode(char c) {
    switch (c) {
        case 'M': return FileChange::Modified;
        case 'A': return FileChange::Added;
        case 'D': return FileChange::Deleted;
        case 'R': return FileChange::Renamed;
        case 'C': return FileChange::Copied;
        case 'T': return FileChange::TypeChanged;
        case 'U': return FileChange::Conflict;
        case '?': return FileChange::Untracked;
        case '!': return FileChange::Ignored;
        default:  return FileChange::None;
    }
}

// Split on a single-char delimiter, keeping empty fields.
std::vector<std::string> Split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

} // namespace

std::filesystem::path GitClient::WorkingDir() const {
    if (!root_.empty()) return std::filesystem::path(root_);
    return repo_dir_;
}

GitResult GitClient::Run(const std::vector<std::string>& args, std::chrono::milliseconds timeout,
                         const std::atomic<bool>* cancel, const GitLineFn& on_line) {
    platform::ProcessOptions opts;
    opts.executable = "git";
    opts.args = args;
    opts.working_dir = WorkingDir();
    // Deterministic, machine-parseable output.
    opts.env.emplace_back("LC_ALL", "C");
    opts.env.emplace_back("GIT_TERMINAL_PROMPT", "0"); // never block on credential prompts

    GitResult r;
    std::string combined;
    platform::ProcessResult pr = platform::RunProcess(
        opts,
        [&](std::string_view line, bool is_stderr) {
            combined.append(line.data(), line.size());
            combined.push_back('\n');
            if (on_line) on_line(std::string(line), is_stderr);
        },
        timeout, cancel);

    r.launched = pr.launched;
    r.timed_out = pr.timed_out;
    r.canceled = pr.canceled;
    r.exit_code = pr.exit_code;
    r.output = combined;
    r.ok = pr.Succeeded();
    if (!pr.launched) r.error = pr.error.empty() ? "failed to launch git" : pr.error;
    else if (pr.timed_out) r.error = "git operation timed out";
    else if (pr.canceled) r.error = "git operation canceled";
    else if (pr.exit_code != 0) r.error = Trim(combined);
    return r;
}

bool GitClient::GitAvailable() const {
    platform::ProcessOptions opts;
    opts.executable = "git";
    opts.args = {"--version"};
    std::string out;
    return platform::RunProcessCapture(opts, out, std::chrono::seconds(10)).Succeeded();
}

bool GitClient::DiscoverRepo() {
    is_repo_ = false;
    root_.clear();
    platform::ProcessOptions opts;
    opts.executable = "git";
    opts.args = {"rev-parse", "--show-toplevel"};
    opts.working_dir = repo_dir_;
    opts.env.emplace_back("LC_ALL", "C");
    std::string out;
    auto pr = platform::RunProcessCapture(opts, out, std::chrono::seconds(15));
    if (pr.Succeeded()) {
        root_ = Trim(out);
        is_repo_ = !root_.empty();
    }
    return is_repo_;
}

RepoStatus GitClient::GetStatus() {
    RepoStatus st;
    if (!is_repo_ && !DiscoverRepo()) {
        st.error = "not a git repository";
        return st;
    }
    st.is_repo = true;
    st.root = root_;

    GitResult r = Run({"status", "--porcelain=v1", "-z", "--branch"});
    if (!r.ok) { st.error = r.error.empty() ? "git status failed" : r.error; return st; }

    // Records are NUL-separated (git status -z). RunProcess delivers the blob as
    // one flushed chunk; the '\n' our sink appends is only a line-join artifact.
    std::vector<std::string> recs;
    {
        std::string cur;
        for (char c : r.output) {
            if (c == '\0') { recs.push_back(cur); cur.clear(); }
            else if (c == '\n') { if (!cur.empty()) { recs.push_back(cur); cur.clear(); } }
            else cur += c;
        }
        if (!cur.empty()) recs.push_back(cur);
    }

    for (size_t i = 0; i < recs.size(); ++i) {
        const std::string& rec = recs[i];
        if (rec.rfind("## ", 0) == 0) {
            std::string b = rec.substr(3);
            if (b.rfind("HEAD (no branch)", 0) == 0) { st.detached = true; continue; }
            // Format: branch...upstream [ahead N, behind M]
            size_t dots = b.find("...");
            std::string branch_part = (dots == std::string::npos) ? b : b.substr(0, dots);
            // strip trailing " [ahead...]" from branch_part if no upstream
            size_t br = branch_part.find(" [");
            if (br != std::string::npos) branch_part = branch_part.substr(0, br);
            st.branch = Trim(branch_part);
            if (dots != std::string::npos) {
                std::string rest = b.substr(dots + 3);
                size_t sp = rest.find(" [");
                st.upstream = Trim(sp == std::string::npos ? rest : rest.substr(0, sp));
                size_t ap = rest.find("ahead ");
                if (ap != std::string::npos) st.ahead = std::atoi(rest.c_str() + ap + 6);
                size_t bp = rest.find("behind ");
                if (bp != std::string::npos) st.behind = std::atoi(rest.c_str() + bp + 7);
            }
            continue;
        }
        if (rec.size() < 3) continue;
        char x = rec[0], y = rec[1];
        std::string path = rec.substr(3);
        StatusEntry e;
        e.index = MapCode(x);
        e.worktree = MapCode(y);
        e.untracked = (x == '?');
        e.conflict = (x == 'U' || y == 'U' || (x == 'A' && y == 'A') || (x == 'D' && y == 'D'));
        e.staged = (x != ' ' && x != '?' && x != '!' && !e.conflict);
        e.unstaged = (y != ' ' && !e.conflict) || e.untracked;
        // Rename/copy: original path is in the next NUL record.
        if ((x == 'R' || x == 'C') && i + 1 < recs.size()) {
            e.orig_path = recs[++i];
        }
        e.path = path;
        st.entries.push_back(std::move(e));
    }

    // In-progress operation detection via the resolved git dir.
    GitResult gd = Run({"rev-parse", "--git-dir"});
    if (gd.ok) {
        std::filesystem::path git_dir = Trim(gd.output);
        if (git_dir.is_relative()) git_dir = std::filesystem::path(root_) / git_dir;
        std::error_code ec;
        st.merging = std::filesystem::exists(git_dir / "MERGE_HEAD", ec);
        st.cherry_picking = std::filesystem::exists(git_dir / "CHERRY_PICK_HEAD", ec);
        st.rebasing = std::filesystem::exists(git_dir / "rebase-merge", ec) ||
                      std::filesystem::exists(git_dir / "rebase-apply", ec);
    }
    return st;
}

std::string GitClient::GetDiff(const std::string& path, bool staged) {
    std::vector<std::string> args = {"diff"};
    if (staged) args.push_back("--cached");
    args.push_back("--");
    args.push_back(path);
    GitResult r = Run(args);
    return r.output;
}

GitResult GitClient::Stage(const std::string& path)    { return Run({"add", "--", path}); }
GitResult GitClient::StageAll()                        { return Run({"add", "-A"}); }
GitResult GitClient::Unstage(const std::string& path)  { return Run({"reset", "-q", "HEAD", "--", path}); }
GitResult GitClient::DiscardWorktree(const std::string& path) { return Run({"checkout", "--", path}); }

GitResult GitClient::Commit(const std::string& message, bool amend) {
    std::vector<std::string> args = {"commit", "-m", message};
    if (amend) args.push_back("--amend");
    return Run(args);
}

std::vector<CommitInfo> GitClient::GetLog(int max_count) {
    std::vector<CommitInfo> out;
    if (!is_repo_ && !DiscoverRepo()) return out;
    // \x1f field sep, \x1e record sep.
    std::string fmt = "%H\x1f%h\x1f%an\x1f%ae\x1f%ar\x1f%s\x1f%P\x1f%D\x1e";
    GitResult r = Run({"log", "-n", std::to_string(max_count), "--pretty=format:" + fmt});
    if (!r.ok) return out;
    // Reconstruct raw (RunProcess joined lines with '\n'; record sep \x1e survives).
    std::string raw = r.output;
    std::vector<std::string> records = Split(raw, '\x1e');
    for (auto& rec : records) {
        std::string clean;
        for (char c : rec) if (c != '\n') clean += c; // strip join newlines around records
        if (Trim(clean).empty()) continue;
        std::vector<std::string> f = Split(clean, '\x1f');
        if (f.size() < 8) continue;
        CommitInfo ci;
        ci.hash = Trim(f[0]);
        ci.short_hash = Trim(f[1]);
        ci.author = f[2];
        ci.email = f[3];
        ci.date = f[4];
        ci.subject = f[5];
        for (auto& p : Split(Trim(f[6]), ' ')) if (!p.empty()) ci.parents.push_back(p);
        ci.refs = Trim(f[7]);
        out.push_back(std::move(ci));
    }
    return out;
}

std::vector<BranchInfo> GitClient::GetBranches() {
    std::vector<BranchInfo> out;
    if (!is_repo_ && !DiscoverRepo()) return out;
    std::string fmt = "%(refname:short)\x1f%(HEAD)\x1f%(upstream:short)\x1f%(upstream:track)";
    GitResult r = Run({"branch", "-a", "--format=" + fmt});
    if (!r.ok) return out;
    for (auto& line : Split(r.output, '\n')) {
        if (Trim(line).empty()) continue;
        std::vector<std::string> f = Split(line, '\x1f');
        if (f.empty()) continue;
        BranchInfo b;
        b.name = Trim(f[0]);
        if (b.name.empty()) continue;
        b.current = (f.size() > 1 && Trim(f[1]) == "*");
        b.upstream = f.size() > 2 ? Trim(f[2]) : "";
        b.remote = (b.name.rfind("remotes/", 0) == 0) || (b.name.rfind("origin/", 0) == 0);
        if (f.size() > 3) {
            const std::string& track = f[3];
            size_t ap = track.find("ahead ");
            if (ap != std::string::npos) b.ahead = std::atoi(track.c_str() + ap + 6);
            size_t bp = track.find("behind ");
            if (bp != std::string::npos) b.behind = std::atoi(track.c_str() + bp + 7);
        }
        out.push_back(std::move(b));
    }
    return out;
}

GitResult GitClient::CreateBranch(const std::string& name, bool checkout) {
    if (checkout) return Run({"checkout", "-b", name});
    return Run({"branch", name});
}
GitResult GitClient::Checkout(const std::string& name)  { return Run({"checkout", name}); }
GitResult GitClient::DeleteBranch(const std::string& name, bool force) {
    return Run({"branch", force ? "-D" : "-d", name});
}
GitResult GitClient::Merge(const std::string& name)     { return Run({"merge", name}); }

GitResult GitClient::Fetch(const std::atomic<bool>* cancel, const GitLineFn& on_line) {
    return Run({"fetch", "--all", "--prune"}, std::chrono::seconds(180), cancel, on_line);
}
GitResult GitClient::Pull(const std::atomic<bool>* cancel, const GitLineFn& on_line) {
    return Run({"pull", "--ff-only"}, std::chrono::seconds(180), cancel, on_line);
}
GitResult GitClient::Push(bool set_upstream, const std::atomic<bool>* cancel, const GitLineFn& on_line) {
    std::vector<std::string> args = {"push"};
    if (set_upstream) { args.push_back("-u"); args.push_back("origin"); args.push_back("HEAD"); }
    return Run(args, std::chrono::seconds(180), cancel, on_line);
}

GitResult GitClient::AcceptOurs(const std::string& path) {
    GitResult r = Run({"checkout", "--ours", "--", path});
    if (!r.ok) return r;
    return Run({"add", "--", path});
}
GitResult GitClient::AcceptTheirs(const std::string& path) {
    GitResult r = Run({"checkout", "--theirs", "--", path});
    if (!r.ok) return r;
    return Run({"add", "--", path});
}
GitResult GitClient::MergeAbort()      { return Run({"merge", "--abort"}); }
GitResult GitClient::RebaseAbort()     { return Run({"rebase", "--abort"}); }
GitResult GitClient::RebaseContinue()  { return Run({"rebase", "--continue"}); }

} // namespace dse::vcs
