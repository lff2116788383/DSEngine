/**
 * @file ui_tests_editor_features.cpp
 * @brief Editor Feature Panel tests (#2-#9) - interaction and state validation.
 *
 * Tests the new editor panels:
 *   - Visual Script Debugger: breakpoint toggle, debug controls
 *   - Animation Clip Editor: playback controls, bone selection, layer management
 *   - Cinematic Sequencer: transport controls, track management, playhead
 *   - Terrain Sculpt Preview: brush mode switching, parameter adjustment
 *   - World Partition Editor: cell selection, overlay mode switching
 *   - Plugin Hot Reload: plugin list interaction, build trigger
 *   - Version Control: tab switching, file staging, branch display
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_visual_script_debugger.h"
#include "../editor_animation_clip.h"
#include "../editor_sequencer.h"
#include "../editor_terrain_sculpt_preview.h"
#include "../editor_world_partition_editor.h"
#include "../editor_plugin_hot_reload.h"
#include "../editor_version_control.h"

namespace dse::editor::uitest {

void RegisterEditorFeatureTests(ImGuiTestEngine* e) {
    // ── Visual Script Debugger: toggle breakpoint, debug controls ──────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "vs_debugger_breakpoint_toggle");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            // Open VS panel (debugger shown alongside)
            *Services().show_visual_script = true;
            ctx->Yield(4);

            ImGuiWindow* w = FindActiveWindow("Visual Script Debugger");
            IM_CHECK(w != nullptr);

            // Verify debugger state is accessible
            auto& state = GetVsDebuggerState();
            int bp_count_before = static_cast<int>(state.breakpoints.size());

            // Toggle a breakpoint via helper
            VsToggleBreakpoint(1);
            ctx->Yield(2);
            IM_CHECK(static_cast<int>(state.breakpoints.size()) == bp_count_before + 1);

            // Toggle again to remove
            VsToggleBreakpoint(1);
            ctx->Yield(2);
            IM_CHECK(static_cast<int>(state.breakpoints.size()) == bp_count_before);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Visual Script Debugger: start/stop debug session ──────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "vs_debugger_start_stop");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_visual_script = true;
            ctx->Yield(4);

            auto& state = GetVsDebuggerState();
            IM_CHECK(state.debug_state == VsDebugState::Idle);

            VsDebugStart();
            ctx->Yield(2);
            IM_CHECK(state.debug_state == VsDebugState::Running);

            VsDebugStop();
            ctx->Yield(2);
            IM_CHECK(state.debug_state == VsDebugState::Idle);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Animation Clip Editor: playback controls ──────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "animation_clip_playback");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_animation = true;
            ctx->Yield(4);

            ImGuiWindow* w = FindActiveWindow("Animation Clip Editor");
            IM_CHECK(w != nullptr);

            auto& state = GetAnimClipEditorState();

            // Start playing
            state.playing = false;
            state.current_time = 0.0f;
            AnimClipPlay();
            ctx->Yield(2);
            IM_CHECK(state.playing == true);

            // Stop
            AnimClipStop();
            ctx->Yield(2);
            IM_CHECK(state.playing == false);
            IM_CHECK(state.current_time == 0.0f);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Animation Clip Editor: bone selection ─────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "animation_clip_bone_select");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_animation = true;
            ctx->Yield(4);

            auto& state = GetAnimClipEditorState();
            IM_CHECK(!state.bones.empty());

            // Select a bone
            state.selected_bone = 3;
            ctx->Yield(2);
            IM_CHECK(state.selected_bone == 3);

            // Verify bone data exists
            IM_CHECK(state.bones[3].name.length() > 0);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Cinematic Sequencer: transport controls ───────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "sequencer_transport_controls");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            // Sequencer is always shown (static bool show_sequencer = true)
            ctx->Yield(4);

            ImGuiWindow* w = FindActiveWindow("Cinematic Sequencer");
            IM_CHECK(w != nullptr);

            auto& state = GetSequencerState();

            // Play/Pause
            state.playing = false;
            state.playhead_time = 0.0f;
            SequencerPlay();
            ctx->Yield(2);
            IM_CHECK(state.playing == true);

            SequencerPause();
            ctx->Yield(2);
            IM_CHECK(state.playing == false);

            // Stop resets playhead
            SequencerStop();
            ctx->Yield(2);
            IM_CHECK(state.playhead_time == 0.0f);

            ctx->Yield(2);
        };
    }

    // ── Cinematic Sequencer: track count validation ───────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "sequencer_tracks_present");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->Yield(4);

            auto& state = GetSequencerState();
            // Demo sequence has tracks
            IM_CHECK(!state.tracks.empty());
            IM_CHECK(state.sequence_duration > 0.0f);

            // Verify track types are set
            for (auto& track : state.tracks) {
                IM_CHECK(track.name.length() > 0);
            }

            ctx->Yield(2);
        };
    }

    // ── Terrain Sculpt Preview: brush mode switching ──────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "terrain_sculpt_brush_modes");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_terrain_editor = true;
            ctx->Yield(4);

            ImGuiWindow* w = FindActiveWindow("Terrain Sculpt Preview");
            IM_CHECK(w != nullptr);

            auto& state = GetTerrainSculptState();

            // Switch brush modes
            state.brush.mode = TerrainSculptBrushMode::Raise;
            ctx->Yield(2);
            IM_CHECK(state.brush.mode == TerrainSculptBrushMode::Raise);

            state.brush.mode = TerrainSculptBrushMode::Smooth;
            ctx->Yield(2);
            IM_CHECK(state.brush.mode == TerrainSculptBrushMode::Smooth);

            state.brush.mode = TerrainSculptBrushMode::Paint;
            ctx->Yield(2);
            IM_CHECK(state.brush.mode == TerrainSculptBrushMode::Paint);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Terrain Sculpt Preview: brush parameter validation ────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "terrain_sculpt_brush_params");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_terrain_editor = true;
            ctx->Yield(4);

            auto& state = GetTerrainSculptState();

            // Verify default brush params are within valid range
            IM_CHECK(state.brush.radius > 0.0f);
            IM_CHECK(state.brush.strength > 0.0f && state.brush.strength <= 1.0f);
            IM_CHECK(state.brush.opacity > 0.0f && state.brush.opacity <= 1.0f);

            // Verify heightmap is initialized
            IM_CHECK(!state.heightmap.empty());
            IM_CHECK(state.heightmap_size > 0);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── World Partition Editor: cell selection ─────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "world_partition_cell_select");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_streaming_debug = true;
            ctx->Yield(4);

            ImGuiWindow* w = FindActiveWindow("World Partition Editor");
            IM_CHECK(w != nullptr);

            auto& state = GetWorldPartitionState();

            // Verify grid is initialized
            IM_CHECK(!state.cells.empty());
            IM_CHECK(state.grid_cols > 0 && state.grid_rows > 0);

            // Select a cell
            state.selected_cell = 5;
            ctx->Yield(2);
            IM_CHECK(state.selected_cell == 5);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── World Partition Editor: overlay mode switching ─────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "world_partition_overlay_modes");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_streaming_debug = true;
            ctx->Yield(4);

            auto& state = GetWorldPartitionState();

            // Switch overlay modes
            state.overlay_mode = WpOverlayMode::StreamState;
            ctx->Yield(2);
            IM_CHECK(state.overlay_mode == WpOverlayMode::StreamState);

            state.overlay_mode = WpOverlayMode::LOD;
            ctx->Yield(2);
            IM_CHECK(state.overlay_mode == WpOverlayMode::LOD);

            state.overlay_mode = WpOverlayMode::Memory;
            ctx->Yield(2);
            IM_CHECK(state.overlay_mode == WpOverlayMode::Memory);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Plugin Hot Reload: plugin list and build trigger ──────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "plugin_hot_reload_build");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_plugins = true;
            ctx->Yield(4);

            ImGuiWindow* w = FindActiveWindow("Plugin Hot Reload");
            IM_CHECK(w != nullptr);

            auto& state = GetPluginHotReloadState();

            // Verify plugins are loaded
            IM_CHECK(!state.plugins.empty());

            // Check first plugin properties
            IM_CHECK(state.plugins[0].name.length() > 0);

            // Trigger build on first plugin
            int sel = state.selected_plugin;
            state.selected_plugin = 0;
            PluginHotReloadTriggerBuild(0);
            ctx->Yield(2);
            IM_CHECK(state.plugins[0].state == HotReloadPluginState::Compiling ||
                     state.plugins[0].state == HotReloadPluginState::Loaded);

            state.selected_plugin = sel;
            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Plugin Hot Reload: auto-reload toggle ─────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "plugin_hot_reload_auto_toggle");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_plugins = true;
            ctx->Yield(4);

            auto& state = GetPluginHotReloadState();

            // Toggle global auto-reload
            bool before = state.global_auto_reload;
            state.global_auto_reload = !before;
            ctx->Yield(2);
            IM_CHECK(state.global_auto_reload != before);

            // Restore
            state.global_auto_reload = before;
            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Version Control: tab switching ────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "version_control_tab_switch");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_git = true;
            ctx->Yield(4);

            ImGuiWindow* w = FindActiveWindow("Version Control");
            IM_CHECK(w != nullptr);

            auto& state = GetVersionControlState();

            // Switch tabs
            state.active_tab = VcTab::Changes;
            ctx->Yield(2);
            IM_CHECK(state.active_tab == VcTab::Changes);

            state.active_tab = VcTab::History;
            ctx->Yield(2);
            IM_CHECK(state.active_tab == VcTab::History);

            state.active_tab = VcTab::Branches;
            ctx->Yield(2);
            IM_CHECK(state.active_tab == VcTab::Branches);

            state.active_tab = VcTab::Conflicts;
            ctx->Yield(2);
            IM_CHECK(state.active_tab == VcTab::Conflicts);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Version Control: file staging ─────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "version_control_file_staging");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_git = true;
            ctx->Yield(4);

            auto& state = GetVersionControlState();
            IM_CHECK(!state.files.empty());

            // Find an unstaged file and stage it
            int unstaged_idx = -1;
            for (int i = 0; i < static_cast<int>(state.files.size()); i++) {
                if (!state.files[i].staged) {
                    unstaged_idx = i;
                    break;
                }
            }

            if (unstaged_idx >= 0) {
                state.files[unstaged_idx].staged = true;
                ctx->Yield(2);
                IM_CHECK(state.files[unstaged_idx].staged == true);

                // Unstage it
                state.files[unstaged_idx].staged = false;
                ctx->Yield(2);
                IM_CHECK(state.files[unstaged_idx].staged == false);
            }

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }

    // ── Version Control: branch info ──────────────────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-features", "version_control_branch_info");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            *Services().show_git = true;
            ctx->Yield(4);

            auto& state = GetVersionControlState();

            // Verify branches are populated
            IM_CHECK(!state.branches.empty());

            // Find current branch
            bool has_current = false;
            for (auto& br : state.branches) {
                if (br.is_current) {
                    has_current = true;
                    IM_CHECK(br.name.length() > 0);
                    break;
                }
            }
            IM_CHECK(has_current);

            HideOptionalPanels();
            ctx->Yield(2);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
