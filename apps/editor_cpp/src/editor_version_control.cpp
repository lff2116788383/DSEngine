/**
 * @file editor_version_control.cpp
 * @brief Version Control Integration — Git diff, commit, conflict visualization
 *
 * Features:
 *  - File status view (modified/staged/untracked/conflict)
 *  - Inline diff viewer with side-by-side comparison
 *  - Commit panel with message editor
 *  - Branch management
 *  - Conflict resolution UI (ours/theirs/manual merge)
 *  - Git log/history visualization
 */

#include "editor_version_control.h"
#include "editor_icons.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace dse::editor {

namespace {

// ─── Data model ─────────────────────────────────────────────────────────

enum class FileStatus { Modified, Added, Deleted, Renamed, Untracked, Conflict, Staged };

struct GitFile {
    std::string path;
    FileStatus status = FileStatus::Modified;
    bool staged = false;
    int additions = 0;
    int deletions = 0;
};

struct DiffHunk {
    int old_start = 0;
    int new_start = 0;
    int old_count = 0;
    int new_count = 0;
    std::vector<std::string> lines;  // prefixed with +, -, or space
};

struct DiffFile {
    std::string path;
    std::vector<DiffHunk> hunks;
};

struct GitCommit {
    std::string hash;       // short hash
    std::string message;
    std::string author;
    std::string date;
    std::string branch;
    bool is_head = false;
    std::vector<std::string> parents; // for merge visualization
};

struct ConflictRegion {
    std::string ours;
    std::string theirs;
    std::string base;
    enum class Resolution { Unresolved, Ours, Theirs, Manual } resolution = Resolution::Unresolved;
    std::string manual_text;
};

struct ConflictFile {
    std::string path;
    std::vector<ConflictRegion> regions;
};

struct GitBranch {
    std::string name;
    bool is_current = false;
    bool is_remote = false;
    std::string tracking;
    int ahead = 0;
    int behind = 0;
};

struct VersionControlState {
    // Status
    std::vector<GitFile> files;
    std::vector<DiffFile> diffs;
    std::vector<GitCommit> log;
    std::vector<GitBranch> branches;
    std::vector<ConflictFile> conflicts;
    // UI state
    int selected_file = -1;
    int selected_commit = -1;
    int selected_branch = -1;
    int active_tab = 0;  // 0=Changes, 1=History, 2=Branches, 3=Conflicts
    // Commit
    char commit_message[512] = "";
    bool amend = false;
    // View options
    bool show_staged_only = false;
    bool show_untracked = true;
    bool unified_diff = true;  // false = side-by-side
    int context_lines = 3;
    // Current branch
    std::string current_branch = "feature/engine-lib";
    std::string remote_url = "origin";
    bool has_upstream = true;
    int ahead_count = 2;
    int behind_count = 0;

    bool initialized = false;
};

static VersionControlState s_state;

void InitVersionControl() {
    if (s_state.initialized) return;
    s_state.initialized = true;

    // Demo files
    s_state.files.push_back({"engine/render/pbr_shader.cpp", FileStatus::Modified, true, 15, 3});
    s_state.files.push_back({"engine/render/gbuffer.h", FileStatus::Modified, true, 8, 2});
    s_state.files.push_back({"engine/ecs/transform_system.cpp", FileStatus::Modified, false, 42, 18});
    s_state.files.push_back({"engine/physics/ragdoll.cpp", FileStatus::Added, true, 120, 0});
    s_state.files.push_back({"engine/audio/old_mixer.cpp", FileStatus::Deleted, false, 0, 85});
    s_state.files.push_back({"assets/shaders/new_post.glsl", FileStatus::Untracked, false, 45, 0});
    s_state.files.push_back({"engine/ai/behavior_tree.cpp", FileStatus::Conflict, false, 30, 10});
    s_state.files.push_back({"engine/render/shadow_map.cpp", FileStatus::Renamed, true, 5, 5});

    // Demo diff
    DiffFile df;
    df.path = "engine/render/pbr_shader.cpp";
    DiffHunk hunk;
    hunk.old_start = 42; hunk.new_start = 42;
    hunk.old_count = 7; hunk.new_count = 10;
    hunk.lines = {
        " #include \"pbr_shader.h\"",
        " #include \"brdf.h\"",
        "-#include \"old_lighting.h\"",
        "+#include \"new_lighting.h\"",
        "+#include \"area_lights.h\"",
        " ",
        " void PBRShader::Bind() {",
        "-    SetFloat(\"roughness\", 0.5f);",
        "+    SetFloat(\"roughness\", material_.roughness);",
        "+    SetFloat(\"metallic\", material_.metallic);",
        "+    SetVec3(\"f0\", CalculateF0(material_));",
        "     SetTexture(\"albedo_map\", albedo_);",
        " }"
    };
    df.hunks.push_back(hunk);
    s_state.diffs.push_back(df);

    // Demo log
    s_state.log.push_back({"a3f1b2c", "feat: implement PBR area lights", "dev", "2 hours ago", "feature/engine-lib", true, {}});
    s_state.log.push_back({"7e4d9a1", "fix: shadow acne in CSM", "dev", "5 hours ago", "", false, {}});
    s_state.log.push_back({"b82c1f0", "refactor: extract BRDF into separate module", "dev", "1 day ago", "", false, {}});
    s_state.log.push_back({"5f0a3e2", "feat: GPU-driven culling pipeline", "dev", "2 days ago", "", false, {}});
    s_state.log.push_back({"c91d4b7", "Merge branch 'feature/ecs-parallel'", "dev", "3 days ago", "", false, {"5f0a3e2", "x9y8z7w"}});
    s_state.log.push_back({"2a6b8c0", "feat: ParallelEach for ECS systems", "dev", "4 days ago", "", false, {}});

    // Demo branches
    s_state.branches.push_back({"feature/engine-lib", true, false, "origin/feature/engine-lib", 2, 0});
    s_state.branches.push_back({"main", false, false, "origin/main", 0, 5});
    s_state.branches.push_back({"develop", false, false, "origin/develop", 0, 2});
    s_state.branches.push_back({"origin/main", false, true, "", 0, 0});
    s_state.branches.push_back({"origin/develop", false, true, "", 0, 0});
    s_state.branches.push_back({"origin/feature/engine-lib", false, true, "", 0, 0});

    // Demo conflict
    ConflictFile cf;
    cf.path = "engine/ai/behavior_tree.cpp";
    ConflictRegion cr;
    cr.ours = "void BehaviorTree::Tick(float dt) {\n    root_->Execute(context_, dt);\n    UpdateBlackboard();\n}";
    cr.theirs = "void BehaviorTree::Tick(float delta_time) {\n    if (enabled_) {\n        root_->Execute(context_, delta_time);\n    }\n}";
    cr.base = "void BehaviorTree::Tick(float dt) {\n    root_->Execute(context_, dt);\n}";
    s_state.conflicts.push_back(cf);
    s_state.conflicts[0].regions.push_back(cr);
}

ImU32 FileStatusColor(FileStatus status) {
    switch (status) {
        case FileStatus::Modified: return IM_COL32(220, 180, 50, 255);
        case FileStatus::Added: return IM_COL32(80, 200, 80, 255);
        case FileStatus::Deleted: return IM_COL32(220, 80, 80, 255);
        case FileStatus::Renamed: return IM_COL32(100, 150, 220, 255);
        case FileStatus::Untracked: return IM_COL32(150, 150, 150, 255);
        case FileStatus::Conflict: return IM_COL32(255, 80, 80, 255);
        case FileStatus::Staged: return IM_COL32(80, 200, 80, 255);
    }
    return IM_COL32(180, 180, 180, 255);
}

const char* FileStatusIcon(FileStatus status) {
    switch (status) {
        case FileStatus::Modified: return "M";
        case FileStatus::Added: return "A";
        case FileStatus::Deleted: return "D";
        case FileStatus::Renamed: return "R";
        case FileStatus::Untracked: return "?";
        case FileStatus::Conflict: return "!";
        case FileStatus::Staged: return "S";
    }
    return "?";
}

} // anonymous namespace

void DrawVersionControlPanel(EditorContext& /*ctx*/) {
    InitVersionControl();
    auto& state = s_state;

    ImGui::Begin(MDI_ICON_SOURCE_BRANCH "  Version Control");

    // ─── Header: branch info ─────────────────────────────────────────────
    {
        ImGui::Text(MDI_ICON_SOURCE_BRANCH " %s", state.current_branch.c_str());
        ImGui::SameLine();
        if (state.ahead_count > 0) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), " %d" MDI_ICON_ARROW_UP, state.ahead_count);
        }
        ImGui::SameLine();
        if (state.behind_count > 0) {
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), " %d" MDI_ICON_ARROW_DOWN, state.behind_count);
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 180);
        if (ImGui::Button(MDI_ICON_CLOUD_DOWNLOAD " Pull")) { /* simulate pull */ }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_CLOUD_UPLOAD " Push")) { /* simulate push */ }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_REFRESH " Refresh")) { /* refresh status */ }
    }

    ImGui::Separator();

    // ─── Tabs ────────────────────────────────────────────────────────────
    if (ImGui::BeginTabBar("VCTabs")) {
        // ═══ Changes Tab ═════════════════════════════════════════════════
        if (ImGui::BeginTabItem(MDI_ICON_FILE_DOCUMENT_EDIT " Changes")) {
            state.active_tab = 0;

            // Staged files
            int staged_count = 0;
            for (auto& f : state.files) if (f.staged) staged_count++;

            if (ImGui::CollapsingHeader(("Staged (" + std::to_string(staged_count) + ")").c_str(),
                                         ImGuiTreeNodeFlags_DefaultOpen)) {
                for (int i = 0; i < static_cast<int>(state.files.size()); i++) {
                    auto& f = state.files[i];
                    if (!f.staged) continue;
                    ImGui::PushID(i);
                    ImU32 col = FileStatusColor(f.status);
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), " %s", FileStatusIcon(f.status));
                    ImGui::SameLine();
                    bool sel = (state.selected_file == i);
                    if (ImGui::Selectable(f.path.c_str(), sel)) state.selected_file = i;
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
                    ImGui::TextDisabled("+%d -%d", f.additions, f.deletions);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("-")) f.staged = false; // unstage
                    ImGui::PopID();
                }
            }

            // Unstaged files
            int unstaged_count = 0;
            for (auto& f : state.files) if (!f.staged) unstaged_count++;

            if (ImGui::CollapsingHeader(("Changes (" + std::to_string(unstaged_count) + ")").c_str(),
                                         ImGuiTreeNodeFlags_DefaultOpen)) {
                for (int i = 0; i < static_cast<int>(state.files.size()); i++) {
                    auto& f = state.files[i];
                    if (f.staged) continue;
                    if (!state.show_untracked && f.status == FileStatus::Untracked) continue;
                    ImGui::PushID(i + 100);
                    ImU32 col = FileStatusColor(f.status);
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), " %s", FileStatusIcon(f.status));
                    ImGui::SameLine();
                    bool sel = (state.selected_file == i);
                    if (ImGui::Selectable(f.path.c_str(), sel)) state.selected_file = i;
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
                    ImGui::TextDisabled("+%d -%d", f.additions, f.deletions);
                    ImGui::SameLine();
                    if (f.status != FileStatus::Conflict) {
                        if (ImGui::SmallButton("+")) f.staged = true; // stage
                    }
                    ImGui::PopID();
                }
            }

            ImGui::Separator();

            // ─── Diff viewer ─────────────────────────────────────────────
            if (state.selected_file >= 0 && !state.diffs.empty()) {
                ImGui::Text("Diff: %s", state.files[state.selected_file].path.c_str());
                ImGui::BeginChild("DiffView", ImVec2(0, 150), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_HorizontalScrollbar);
                for (auto& diff : state.diffs) {
                    for (auto& hunk : diff.hunks) {
                        ImGui::TextDisabled("@@ -%d,%d +%d,%d @@",
                                            hunk.old_start, hunk.old_count, hunk.new_start, hunk.new_count);
                        for (auto& line : hunk.lines) {
                            if (line.empty()) { ImGui::Text(" "); continue; }
                            if (line[0] == '+') {
                                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 220, 80, 255));
                                ImGui::TextUnformatted(line.c_str());
                                ImGui::PopStyleColor();
                            } else if (line[0] == '-') {
                                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 80, 80, 255));
                                ImGui::TextUnformatted(line.c_str());
                                ImGui::PopStyleColor();
                            } else {
                                ImGui::TextDisabled("%s", line.c_str());
                            }
                        }
                    }
                }
                ImGui::EndChild();
            }

            ImGui::Separator();

            // ─── Commit panel ────────────────────────────────────────────
            ImGui::Text(MDI_ICON_CHECK " Commit");
            ImGui::InputTextMultiline("##commit_msg", state.commit_message, sizeof(state.commit_message),
                                      ImVec2(-1, 60));
            if (ImGui::Button("Commit Staged")) {
                if (state.commit_message[0]) {
                    // Simulate commit
                    GitCommit c;
                    c.hash = "new1234";
                    c.message = state.commit_message;
                    c.author = "dev";
                    c.date = "just now";
                    c.is_head = true;
                    if (!state.log.empty()) state.log[0].is_head = false;
                    state.log.insert(state.log.begin(), c);
                    // Remove staged files
                    state.files.erase(std::remove_if(state.files.begin(), state.files.end(),
                        [](const GitFile& f) { return f.staged; }), state.files.end());
                    state.commit_message[0] = '\0';
                    state.ahead_count++;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Stage All")) {
                for (auto& f : state.files) {
                    if (f.status != FileStatus::Conflict) f.staged = true;
                }
            }

            ImGui::EndTabItem();
        }

        // ═══ History Tab ═════════════════════════════════════════════════
        if (ImGui::BeginTabItem(MDI_ICON_HISTORY " History")) {
            state.active_tab = 1;

            ImGui::BeginChild("LogView", ImVec2(0, 0), ImGuiChildFlags_Borders);
            for (int i = 0; i < static_cast<int>(state.log.size()); i++) {
                auto& commit = state.log[i];
                ImGui::PushID(i);

                // Graph line (simple)
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                float cx = pos.x + 10;
                float cy = pos.y + 10;
                if (i > 0) dl->AddLine(ImVec2(cx, pos.y), ImVec2(cx, cy - 4), IM_COL32(100, 100, 150, 200));
                if (i < static_cast<int>(state.log.size()) - 1) dl->AddLine(ImVec2(cx, cy + 4), ImVec2(cx, pos.y + 24), IM_COL32(100, 100, 150, 200));

                // Commit dot
                ImU32 dot_col = commit.is_head ? IM_COL32(255, 200, 50, 255) : IM_COL32(100, 150, 220, 255);
                dl->AddCircleFilled(ImVec2(cx, cy), commit.is_head ? 5 : 4, dot_col);

                // Merge indicator
                if (commit.parents.size() > 1) {
                    dl->AddCircle(ImVec2(cx, cy), 7, IM_COL32(200, 100, 200, 200), 0, 1.5f);
                }

                ImGui::SetCursorPosX(24);
                bool sel = (state.selected_commit == i);
                if (ImGui::Selectable(("##commit" + std::to_string(i)).c_str(), sel, 0, ImVec2(0, 20))) {
                    state.selected_commit = i;
                }
                ImGui::SameLine(28);
                ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "%s", commit.hash.c_str());
                ImGui::SameLine(90);
                ImGui::Text("%s", commit.message.c_str());
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
                ImGui::TextDisabled("%s", commit.date.c_str());

                // Branch tag
                if (!commit.branch.empty()) {
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 180);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 200, 80, 255));
                    ImGui::Text("[%s]", commit.branch.c_str());
                    ImGui::PopStyleColor();
                }

                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // ═══ Branches Tab ════════════════════════════════════════════════
        if (ImGui::BeginTabItem(MDI_ICON_SOURCE_BRANCH " Branches")) {
            state.active_tab = 2;

            ImGui::Text("Local Branches:");
            ImGui::Separator();
            for (int i = 0; i < static_cast<int>(state.branches.size()); i++) {
                auto& b = state.branches[i];
                if (b.is_remote) continue;
                ImGui::PushID(i);
                ImU32 col = b.is_current ? IM_COL32(80, 220, 80, 255) : IM_COL32(200, 200, 200, 255);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s %s",
                                   b.is_current ? MDI_ICON_CHECK : MDI_ICON_SOURCE_BRANCH, b.name.c_str());
                if (!b.tracking.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("-> %s", b.tracking.c_str());
                    if (b.ahead > 0) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "+%d", b.ahead); }
                    if (b.behind > 0) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1), "-%d", b.behind); }
                }
                if (!b.is_current) {
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
                    if (ImGui::SmallButton("Checkout")) {
                        for (auto& br : state.branches) br.is_current = false;
                        b.is_current = true;
                        state.current_branch = b.name;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Merge")) { /* simulate merge */ }
                }
                ImGui::PopID();
            }

            ImGui::Spacing();
            ImGui::Text("Remote Branches:");
            ImGui::Separator();
            for (auto& b : state.branches) {
                if (!b.is_remote) continue;
                ImGui::TextDisabled("  %s %s", MDI_ICON_CLOUD, b.name.c_str());
            }
            ImGui::EndTabItem();
        }

        // ═══ Conflicts Tab ═══════════════════════════════════════════════
        if (ImGui::BeginTabItem(MDI_ICON_ALERT " Conflicts")) {
            state.active_tab = 3;

            if (state.conflicts.empty()) {
                ImGui::TextDisabled("No merge conflicts.");
            } else {
                for (int fi = 0; fi < static_cast<int>(state.conflicts.size()); fi++) {
                    auto& cf = state.conflicts[fi];
                    ImGui::PushID(fi);
                    ImGui::Text(MDI_ICON_FILE_ALERT " %s", cf.path.c_str());

                    for (int ri = 0; ri < static_cast<int>(cf.regions.size()); ri++) {
                        auto& region = cf.regions[ri];
                        ImGui::PushID(ri);
                        ImGui::Separator();
                        ImGui::Text("Conflict Region %d:", ri + 1);

                        // Side by side
                        ImGui::BeginChild("ConflictSplit", ImVec2(0, 120), ImGuiChildFlags_Borders);
                        ImGui::Columns(2, "conflict_cols");

                        // Ours
                        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "<<< OURS (current)");
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 220, 150, 255));
                        ImGui::TextWrapped("%s", region.ours.c_str());
                        ImGui::PopStyleColor();
                        ImGui::NextColumn();

                        // Theirs
                        ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), ">>> THEIRS (incoming)");
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 150, 150, 255));
                        ImGui::TextWrapped("%s", region.theirs.c_str());
                        ImGui::PopStyleColor();

                        ImGui::Columns(1);
                        ImGui::EndChild();

                        // Resolution buttons
                        const char* res_labels[] = {"Unresolved", "Use Ours", "Use Theirs", "Manual"};
                        int res_i = static_cast<int>(region.resolution);
                        ImGui::Text("Resolution: %s", res_labels[res_i]);
                        if (ImGui::Button("Accept Ours")) region.resolution = ConflictRegion::Resolution::Ours;
                        ImGui::SameLine();
                        if (ImGui::Button("Accept Theirs")) region.resolution = ConflictRegion::Resolution::Theirs;
                        ImGui::SameLine();
                        if (ImGui::Button("Manual Edit")) region.resolution = ConflictRegion::Resolution::Manual;

                        if (region.resolution == ConflictRegion::Resolution::Ours) {
                            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Resolved: using ours");
                        } else if (region.resolution == ConflictRegion::Resolution::Theirs) {
                            ImGui::TextColored(ImVec4(0.3f, 0.3f, 0.8f, 1.0f), "Resolved: using theirs");
                        }

                        ImGui::PopID();
                    }

                    ImGui::Separator();
                    bool all_resolved = true;
                    for (auto& r : cf.regions) {
                        if (r.resolution == ConflictRegion::Resolution::Unresolved) { all_resolved = false; break; }
                    }
                    ImGui::BeginDisabled(!all_resolved);
                    if (ImGui::Button("Mark Resolved")) {
                        state.conflicts.erase(state.conflicts.begin() + fi);
                        fi--;
                    }
                    ImGui::EndDisabled();
                    if (!all_resolved) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("Resolve all regions first");
                    }

                    ImGui::PopID();
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ─── Test accessors ─────────────────────────────────────────────────────
static VersionControlTestState s_test_state;

VersionControlTestState& GetVersionControlState() {
    InitVersionControl();
    s_test_state.files.clear();
    for (auto& f : s_state.files) {
        VcTestFile tf;
        tf.path = f.path;
        tf.staged = f.staged;
        s_test_state.files.push_back(tf);
    }
    s_test_state.branches.clear();
    for (auto& b : s_state.branches) {
        VcTestBranch tb;
        tb.name = b.name;
        tb.is_current = b.is_current;
        s_test_state.branches.push_back(tb);
    }
    s_test_state.active_tab = static_cast<VcTab>(s_state.active_tab);
    return s_test_state;
}

} // namespace dse::editor
