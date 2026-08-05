/**
 * @file editor_version_control.cpp
 * @brief Version Control (Git) panel — real repository integration (P1-2).
 *
 * All data comes from the actual repository via dse::vcs::GitClient (which
 * shells out through the unified process runner). There is no demo/simulated
 * data: buttons run real git commands against the working tree. Network
 * operations (fetch/pull/push) run on the editor background task service so the
 * UI never blocks; destructive operations require confirmation.
 */

#include "editor_version_control.h"
#include "editor_icons.h"
#include "editor_task_service.h"
#include "engine/vcs/git_client.h"
#include "imgui.h"

#include <filesystem>
#include <string>
#include <vector>

#include "editor_panel_registry.h"

namespace dse::editor {

namespace {

using dse::vcs::BranchInfo;
using dse::vcs::CommitInfo;
using dse::vcs::FileChange;
using dse::vcs::GitClient;
using dse::vcs::RepoStatus;
using dse::vcs::StatusEntry;

struct GitPanelState {
    GitClient client;
    RepoStatus status;
    std::vector<CommitInfo> log;
    std::vector<BranchInfo> branches;

    int active_tab = 0; // 0 Changes, 1 History, 2 Branches, 3 Conflicts
    int selected_file = -1;
    std::string diff_path;
    bool diff_staged = false;
    std::string diff_text;

    char commit_message[1024] = "";
    bool amend = false;
    char new_branch[256] = "";

    std::string banner;      // transient status/error line
    bool banner_error = false;

    // Confirmation modal.
    bool confirm_open = false;
    std::string confirm_text;
    std::function<void()> confirm_action;

    bool initialized = false;
    bool git_missing = false;
};

GitPanelState g;

void SetBanner(const std::string& msg, bool error) { g.banner = msg; g.banner_error = error; }

void Refresh() {
    g.status = g.client.GetStatus();
    g.log = g.client.GetLog(100);
    g.branches = g.client.GetBranches();
    if (!g.status.error.empty()) SetBanner(g.status.error, true);
}

void EnsureInit() {
    if (g.initialized) return;
    g.initialized = true;
    g.client.SetRepoDir(std::filesystem::current_path());
    if (!g.client.GitAvailable()) {
        g.git_missing = true;
        SetBanner("git executable not found on PATH", true);
        return;
    }
    if (!g.client.DiscoverRepo()) {
        SetBanner("current directory is not a git repository", true);
        return;
    }
    Refresh();
}

void RequestConfirm(std::string text, std::function<void()> action) {
    g.confirm_open = true;
    g.confirm_text = std::move(text);
    g.confirm_action = std::move(action);
}

void ApplyResult(const dse::vcs::GitResult& r, const char* ok_msg) {
    if (r.ok) { SetBanner(ok_msg, false); Refresh(); }
    else SetBanner(r.error.empty() ? "git command failed" : r.error, true);
}

// Launch a slow network op on the background task service; refresh on completion.
void RunNetworkOp(const std::string& title, std::function<dse::vcs::GitResult(const std::atomic<bool>&,
                  const dse::vcs::GitLineFn&)> op) {
    auto shared_out = std::make_shared<std::string>();
    auto shared_ok = std::make_shared<bool>(false);
    BackgroundTaskService::Get().Submit(
        title,
        [op, shared_out, shared_ok](TaskContext& ctx) -> bool {
            ctx.SetProgress(-1.0f, "running " );
            auto r = op(ctx.CancelFlag(), [&ctx](const std::string& line, bool err) { ctx.Log(line, err); });
            *shared_out = r.error;
            *shared_ok = r.ok;
            if (!r.ok && !ctx.IsCancelRequested()) ctx.Fail(r.error);
            return r.ok;
        },
        [title, shared_out, shared_ok](bool success) {
            // On UI thread.
            SetBanner(title + (success ? ": done" : (": failed " + *shared_out)), !success);
            Refresh();
        });
    SetBanner(title + " started (see Background Tasks)", false);
}

const char* ChangeGlyph(const StatusEntry& e) {
    if (e.conflict) return "!";
    FileChange c = e.staged ? e.index : e.worktree;
    switch (c) {
        case FileChange::Modified:    return "M";
        case FileChange::Added:       return "A";
        case FileChange::Deleted:     return "D";
        case FileChange::Renamed:     return "R";
        case FileChange::Copied:      return "C";
        case FileChange::TypeChanged: return "T";
        case FileChange::Untracked:   return "?";
        default:                      return " ";
    }
}

ImVec4 ChangeColor(const StatusEntry& e) {
    if (e.conflict) return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    FileChange c = e.staged ? e.index : e.worktree;
    switch (c) {
        case FileChange::Modified:    return ImVec4(0.86f, 0.70f, 0.20f, 1.0f);
        case FileChange::Added:       return ImVec4(0.31f, 0.78f, 0.31f, 1.0f);
        case FileChange::Deleted:     return ImVec4(0.86f, 0.31f, 0.31f, 1.0f);
        case FileChange::Renamed:     return ImVec4(0.39f, 0.59f, 0.86f, 1.0f);
        case FileChange::Untracked:   return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        default:                      return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    }
}

void SelectDiff(const StatusEntry& e, bool staged) {
    g.diff_path = e.path;
    g.diff_staged = staged;
    g.diff_text = g.client.GetDiff(e.path, staged);
}

void DrawDiffView() {
    if (g.diff_path.empty()) return;
    ImGui::Text("Diff: %s%s", g.diff_path.c_str(), g.diff_staged ? " (staged)" : "");
    ImGui::BeginChild("DiffView", ImVec2(0, 180), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    size_t start = 0;
    const std::string& t = g.diff_text;
    for (size_t i = 0; i <= t.size(); ++i) {
        if (i == t.size() || t[i] == '\n') {
            std::string line = t.substr(start, i - start);
            start = i + 1;
            if (line.empty()) { ImGui::TextUnformatted(""); continue; }
            char c = line[0];
            if (c == '+' && line.rfind("+++", 0) != 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 200, 80, 255));
                ImGui::TextUnformatted(line.c_str()); ImGui::PopStyleColor();
            } else if (c == '-' && line.rfind("---", 0) != 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(210, 80, 80, 255));
                ImGui::TextUnformatted(line.c_str()); ImGui::PopStyleColor();
            } else if (line.rfind("@@", 0) == 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 160, 220, 255));
                ImGui::TextUnformatted(line.c_str()); ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("%s", line.c_str());
            }
        }
    }
    ImGui::EndChild();
}

void DrawChangesTab() {
    g.active_tab = 0;

    int staged = 0, unstaged = 0, conflicts = 0;
    for (auto& e : g.status.entries) {
        if (e.conflict) ++conflicts;
        else { if (e.staged) ++staged; if (e.unstaged) ++unstaged; }
    }

    if (ImGui::CollapsingHeader(("Staged (" + std::to_string(staged) + ")").c_str(),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < (int)g.status.entries.size(); ++i) {
            auto& e = g.status.entries[i];
            if (e.conflict || !e.staged) continue;
            ImGui::PushID(i);
            ImGui::TextColored(ChangeColor(e), " %s", ChangeGlyph(e));
            ImGui::SameLine();
            if (ImGui::Selectable(e.path.c_str(), g.selected_file == i)) { g.selected_file = i; SelectDiff(e, true); }
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
            if (ImGui::SmallButton("-")) ApplyResult(g.client.Unstage(e.path), "unstaged");
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader(("Changes (" + std::to_string(unstaged) + ")").c_str(),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < (int)g.status.entries.size(); ++i) {
            auto& e = g.status.entries[i];
            if (e.conflict || !e.unstaged) continue;
            ImGui::PushID(i + 100000);
            ImGui::TextColored(ChangeColor(e), " %s", ChangeGlyph(e));
            ImGui::SameLine();
            if (ImGui::Selectable(e.path.c_str(), g.selected_file == i)) { g.selected_file = i; SelectDiff(e, false); }
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
            if (ImGui::SmallButton("+")) ApplyResult(g.client.Stage(e.path), "staged");
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) {
                std::string p = e.path;
                RequestConfirm("Discard changes to '" + p + "'? This cannot be undone.",
                               [p]() { ApplyResult(g.client.DiscardWorktree(p), "discarded"); });
            }
            ImGui::PopID();
        }
    }

    ImGui::Separator();
    DrawDiffView();
    ImGui::Separator();

    ImGui::Text(MDI_ICON_CHECK " Commit message");
    ImGui::InputTextMultiline("##commit_msg", g.commit_message, sizeof(g.commit_message), ImVec2(-1, 60));
    ImGui::Checkbox("Amend last commit", &g.amend);
    ImGui::BeginDisabled(staged == 0 && !g.amend);
    if (ImGui::Button("Commit")) {
        if (g.commit_message[0]) {
            ApplyResult(g.client.Commit(g.commit_message, g.amend), "committed");
            g.commit_message[0] = '\0';
            g.amend = false;
        } else SetBanner("commit message is empty", true);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Stage All")) ApplyResult(g.client.StageAll(), "staged all");
    ImGui::SameLine();
    if (ImGui::Button(MDI_ICON_REFRESH " Refresh")) Refresh();
}

void DrawHistoryTab() {
    g.active_tab = 1;
    ImGui::BeginChild("LogView", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (g.log.empty()) ImGui::TextDisabled("No commits.");
    for (auto& c : g.log) {
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "%s", c.short_hash.c_str());
        ImGui::SameLine();
        ImGui::TextUnformatted(c.subject.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 220);
        ImGui::TextDisabled("%s  %s", c.author.c_str(), c.date.c_str());
        if (!c.refs.empty()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 200, 80, 255));
            ImGui::Text("(%s)", c.refs.c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();
}

void DrawBranchesTab() {
    g.active_tab = 2;

    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##newbranch", "new-branch-name", g.new_branch, sizeof(g.new_branch));
    ImGui::SameLine();
    if (ImGui::Button("Create & Checkout")) {
        if (g.new_branch[0]) { ApplyResult(g.client.CreateBranch(g.new_branch, true), "branch created"); g.new_branch[0] = '\0'; }
    }
    ImGui::Separator();

    ImGui::TextDisabled("Local");
    for (auto& b : g.branches) {
        if (b.remote) continue;
        ImGui::PushID(b.name.c_str());
        ImVec4 col = b.current ? ImVec4(0.31f, 0.86f, 0.31f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        ImGui::TextColored(col, "%s %s", b.current ? MDI_ICON_CHECK : MDI_ICON_SOURCE_BRANCH, b.name.c_str());
        if (!b.upstream.empty()) {
            ImGui::SameLine(); ImGui::TextDisabled("-> %s", b.upstream.c_str());
            if (b.ahead)  { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.4f,0.8f,0.4f,1), "+%d", b.ahead); }
            if (b.behind) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.8f,0.4f,0.4f,1), "-%d", b.behind); }
        }
        if (!b.current) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 160);
            if (ImGui::SmallButton("Checkout")) ApplyResult(g.client.Checkout(b.name), "checked out");
            ImGui::SameLine();
            if (ImGui::SmallButton("Merge")) ApplyResult(g.client.Merge(b.name), "merged");
            ImGui::SameLine();
            if (ImGui::SmallButton(MDI_ICON_DELETE)) {
                std::string n = b.name;
                RequestConfirm("Delete branch '" + n + "'?",
                               [n]() { ApplyResult(g.client.DeleteBranch(n, false), "branch deleted"); });
            }
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Remote");
    for (auto& b : g.branches) {
        if (!b.remote) continue;
        ImGui::TextDisabled("  %s %s", MDI_ICON_CLOUD, b.name.c_str());
    }
}

void DrawConflictsTab() {
    g.active_tab = 3;
    bool any = false;
    for (int i = 0; i < (int)g.status.entries.size(); ++i) {
        auto& e = g.status.entries[i];
        if (!e.conflict) continue;
        any = true;
        ImGui::PushID(i);
        ImGui::Text(MDI_ICON_FILE_ALERT " %s", e.path.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 260);
        if (ImGui::SmallButton("Accept Ours"))   ApplyResult(g.client.AcceptOurs(e.path), "took ours");
        ImGui::SameLine();
        if (ImGui::SmallButton("Accept Theirs")) ApplyResult(g.client.AcceptTheirs(e.path), "took theirs");
        ImGui::SameLine();
        if (ImGui::SmallButton("View")) SelectDiff(e, false);
        ImGui::PopID();
    }
    if (!any) ImGui::TextDisabled("No merge conflicts.");

    DrawDiffView();

    if (g.status.merging || g.status.rebasing || g.status.cherry_picking) {
        ImGui::Separator();
        const char* what = g.status.rebasing ? "rebase" : (g.status.cherry_picking ? "cherry-pick" : "merge");
        ImGui::Text("In progress: %s", what);
        if (g.status.rebasing) {
            if (ImGui::Button("Continue")) ApplyResult(g.client.RebaseContinue(), "rebase continued");
            ImGui::SameLine();
            if (ImGui::Button("Abort")) ApplyResult(g.client.RebaseAbort(), "rebase aborted");
        } else {
            if (ImGui::Button("Abort")) ApplyResult(g.client.MergeAbort(), "merge aborted");
        }
    }
}

} // anonymous namespace

void DrawVersionControlPanel(EditorContext& /*ctx*/) {
    EnsureInit();

    ImGui::Begin(MDI_ICON_SOURCE_BRANCH "  Version Control", PanelRegistry::Get().GetCurrentPanelOpen());
    PanelRegistry::Get().DrawMaximizeRestoreButton();

    if (g.git_missing || !g.status.is_repo) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", g.banner.empty() ? "No repository" : g.banner.c_str());
        if (ImGui::Button(MDI_ICON_REFRESH " Retry")) { g.initialized = false; EnsureInit(); }
        ImGui::End();
        return;
    }

    // Header: branch + ahead/behind + network ops.
    ImGui::Text(MDI_ICON_SOURCE_BRANCH " %s", g.status.detached ? "(detached HEAD)" : g.status.branch.c_str());
    if (g.status.ahead > 0)  { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.4f,0.8f,0.4f,1), " %d" MDI_ICON_ARROW_UP, g.status.ahead); }
    if (g.status.behind > 0) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.8f,0.4f,0.4f,1), " %d" MDI_ICON_ARROW_DOWN, g.status.behind); }
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 250);
    if (ImGui::Button(MDI_ICON_CLOUD_DOWNLOAD " Fetch"))
        RunNetworkOp("git fetch", [this_c = &g.client](const std::atomic<bool>& cx, const dse::vcs::GitLineFn& fn){ return this_c->Fetch(&cx, fn); });
    ImGui::SameLine();
    if (ImGui::Button(MDI_ICON_CLOUD_DOWNLOAD " Pull"))
        RunNetworkOp("git pull", [this_c = &g.client](const std::atomic<bool>& cx, const dse::vcs::GitLineFn& fn){ return this_c->Pull(&cx, fn); });
    ImGui::SameLine();
    if (ImGui::Button(MDI_ICON_CLOUD_UPLOAD " Push")) {
        bool need_upstream = g.status.upstream.empty();
        RunNetworkOp("git push", [this_c = &g.client, need_upstream](const std::atomic<bool>& cx, const dse::vcs::GitLineFn& fn){ return this_c->Push(need_upstream, &cx, fn); });
    }

    if (!g.banner.empty()) {
        ImGui::TextColored(g.banner_error ? ImVec4(1,0.5f,0.4f,1) : ImVec4(0.5f,0.85f,0.5f,1), "%s", g.banner.c_str());
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("VCTabs")) {
        if (ImGui::BeginTabItem(MDI_ICON_FILE_DOCUMENT_EDIT " Changes")) { DrawChangesTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(MDI_ICON_HISTORY " History"))           { DrawHistoryTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(MDI_ICON_SOURCE_BRANCH " Branches"))    { DrawBranchesTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(MDI_ICON_ALERT " Conflicts"))          { DrawConflictsTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    // Destructive-operation confirmation modal.
    if (g.confirm_open) { ImGui::OpenPopup("Confirm##vc"); g.confirm_open = false; }
    if (ImGui::BeginPopupModal("Confirm##vc", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", g.confirm_text.c_str());
        ImGui::Separator();
        if (ImGui::Button("Confirm")) { if (g.confirm_action) g.confirm_action(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ─── Test accessors ─────────────────────────────────────────────────────
static VersionControlTestState s_test_state;

VersionControlTestState& GetVersionControlState() {
    EnsureInit();
    Refresh();
    s_test_state.files.clear();
    for (auto& e : g.status.entries) {
        VcTestFile tf;
        tf.path = e.path;
        tf.staged = e.staged;
        s_test_state.files.push_back(tf);
    }
    s_test_state.branches.clear();
    for (auto& b : g.branches) {
        if (b.remote) continue;
        VcTestBranch tb;
        tb.name = b.name;
        tb.is_current = b.current;
        s_test_state.branches.push_back(tb);
    }
    s_test_state.active_tab = static_cast<VcTab>(g.active_tab);
    return s_test_state;
}

// P0-6 self-registration: data-driven; editor_app binds visibility by id.
DSE_EDITOR_PANEL([](dse::editor::PanelRegistry& reg) {
    dse::editor::PanelEntry e;
    e.id = "git";
    e.display_name = "Git";
    e.category = "Tool";
    e.menu_icon = MDI_ICON_SOURCE_BRANCH;
    e.order = 250;
    e.draw = [](dse::editor::EditorContext& ctx) { DrawVersionControlPanel(ctx); };
    reg.Register(std::move(e));
});

} // namespace dse::editor
