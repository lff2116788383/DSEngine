# DSE Editor UI Test Supplement Plan

> Generated: 2026-07-03
> Branch: `feature/engine-lib`
> Current: 214 tests (28 files), 214/214 passing

## 1. Current Coverage Summary

### Test Infrastructure
- **Engine**: Dear ImGui Test Engine (C++, headless)
- **Run**: `bin\dsengine-editor-uitest.exe --run-ui-tests`
- **Output**: JUnit XML + text summary
- **Files**: 28 `ui_tests_*.cpp` files

### Test Distribution (214 tests)

| File | Tests | Category |
|:-----|:------|:---------|
| ui_tests_panels.cpp | 37 | Panel existence (kPanels[] loop) |
| ui_tests_blueprint.cpp | 28 | Blueprint system |
| ui_tests_2d_tools.cpp | 19 | 2D tools |
| ui_tests_editor_features.cpp | 15 | Editor features |
| ui_tests_components.cpp | 11 | Component add/remove |
| ui_tests_misc.cpp | 8 | About/Build/Autosave |
| ui_tests_undo.cpp | 8 | Undo/Redo |
| ui_tests_hierarchy.cpp | 7 | Hierarchy operations |
| ui_tests_inspector.cpp | 7 | Inspector fields |
| ui_tests_animation.cpp | 7 | Animation timeline/state machine |
| ui_tests_gizmo.cpp | 6 | Gizmo interaction |
| ui_tests_prefab.cpp | 6 | Prefab Apply/Override/Revert |
| ui_tests_negative.cpp | 6 | Negative/boundary input |
| ui_tests_scene.cpp | 5 | Scene operations |
| ui_tests_shortcuts.cpp | 5 | Keyboard shortcuts |
| ui_tests_tabs.cpp | 5 | Multi-tab scene isolation |
| ui_tests_multiselect.cpp | 4 | Multi-select/copy/paste |
| ui_tests_dragdrop.cpp | 4 | Drag and drop |
| ui_tests_assetmgmt.cpp | 4 | Asset management |
| ui_tests_project.cpp | 4 | Project new/open/save |
| ui_tests_graph.cpp | 3 | Shader Graph |
| ui_tests_terrain.cpp | 3 | Terrain |
| ui_tests_layout.cpp | 3 | Layout |
| ui_tests_play.cpp | 3 | Play/Pause/Stop |
| ui_tests_console.cpp | 2 | Console (Clear + Auto-scroll) |
| ui_tests_menubar.cpp | 2 | Menu bar |
| ui_tests_assets.cpp | 2 | Asset browser |

### Panel Existence Tests (kPanels[], 37 panels)

Tested panels: Hierarchy, Inspector, Project, Console, Material, Scene, Game, Toolbar,
Profiler, Animation, Tile Palette, Terrain Brush, Lua Console, Localization Preview,
Undo History, Asset Browser, Animation Timeline, NavMesh, Shader Graph, Version Control,
Multi-Viewport, Anim State Machine, Lua Debugger, Streaming Debug, Curve Editor,
Anim Retarget, Preferences, Plugins, AI Agent,
Animation Clip Editor, Sequencer, Terrain Sculpt Preview, World Partition Editor,
Plugin Hot Reload, Version Control.

## 2. Coverage Gaps

### Tier 0: No test at all (not even existence)

| Panel | Lines | Type | Window/Section Name |
|:------|:------|:-----|:--------------------|
| Audio Panel | 239 | Inspector Section | `"Audio Source"` / `"Audio Listener"` CollapsingHeader |
| C# Script Panel | 217 | Inspector Section | `"C# Script"` CollapsingHeader |
| AI Config | 311 | Standalone Window | `"AI Configuration"` |
| Physics Debug | 173 | Inspector Section | (section in Inspector) |
| Particle Panel | 458 | Inspector Section | `"Particle Curves"` CollapsingHeader |
| Vegetation Panel | 313 | Standalone Window | `"Vegetation Brush"` |

### Tier 1: Existence test only (in kPanels[]), no interaction test

| Panel | Lines | Window Name |
|:------|:------|:------------|
| NavMesh | 316 | `"NavMesh"` |
| Version Control | 559 | `"Version Control"` |
| Lua Debugger | 303 | `"Lua Debugger"` |
| World Partition Editor | 403 | `"World Partition Editor"` |
| Multi-Viewport | 173 | `"Multi-Viewport"` |
| Streaming Debug | 149 | `"Streaming Debug"` |
| Profiler | 297 | `"Profiler"` |
| Agent Panel | 1033 | `"AI Agent"` (existence only) |

### Tier 2: Has tests but coverage insufficient

| Panel | Current Tests | Missing |
|:------|:-------------|:--------|
| Console | 2 (Clear + Auto-scroll) | Export button, Category filter, Level filter toggles |
| Agent Panel | 0 interaction | Send/Clear/Reconnect/combo (1033 lines, complex UI) |

## 3. Test Supplement Plan

### Phase 1: Console New Features (4 tests)

**File**: `ui_tests_console.cpp` (extend existing)

| Test ID | Description | Key Actions |
|:--------|:------------|:------------|
| `dse-console/export_button_click` | Click Export button, verify no crash | `WindowFocus("//Console")` → `ItemClick("Export")` |
| `dse-console/category_filter_combo` | Open category dropdown, select a category | `ItemClick("##cat_filter")` → select item |
| `dse-console/level_filter_toggle_info` | Toggle Info filter button | `ItemClick("###info_toggle")` |
| `dse-console/level_filter_toggle_warn_error` | Toggle Warn and Error filter buttons | `ItemClick("###warn_toggle")` → `ItemClick("###error_toggle")` |

### Phase 2: Inspector Section Tests (10 tests)

**File**: NEW `ui_tests_inspector_sections.cpp`

**Prerequisite**: Each test must create a test entity with the required component via the test harness, select it, then test the Inspector section.

#### Audio (3 tests)

| Test ID | Description | Setup | Key Actions |
|:--------|:------------|:------|:------------|
| `dse-inspector-sections/audio_source_play_pause` | Play/Pause buttons in Audio Source section | Create entity with AudioSourceComponent | Open "Audio Source" header → Click Play → Click Pause |
| `dse-inspector-sections/audio_source_volume_slider` | Modify volume slider | Create entity with AudioSourceComponent | Find volume slider → InputValue |
| `dse-inspector-sections/audio_listener_section` | Audio Listener section exists | Create entity with AudioListenerComponent | Verify "Audio Listener" CollapsingHeader |

#### C# Script (2 tests)

| Test ID | Description | Setup | Key Actions |
|:--------|:------------|:------|:------------|
| `dse-inspector-sections/csharp_class_name_input` | Edit class name | Create entity with CSharpScriptComponent | InputText "##csharp_class" |
| `dse-inspector-sections/csharp_build_button` | Click Build button | Same | ItemClick "Build" |

#### Particle (2 tests)

| Test ID | Description | Setup | Key Actions |
|:--------|:------------|:------|:------------|
| `dse-inspector-sections/particle_curves_section` | Open Particle Curves section | Create entity with ParticleEmitterComponent | Verify CollapsingHeader |
| `dse-inspector-sections/particle_emission_rate` | Modify emission rate | Same | Find emission rate field → InputValue |

#### Physics Debug (1 test)

| Test ID | Description |
|:--------|:------------|
| `dse-inspector-sections/physics_debug_overlay` | Verify physics debug visualization toggles |

#### AI Config (2 tests)

| Test ID | Description | Key Actions |
|:--------|:------------|:------------|
| `dse-inspector-sections/ai_config_window_open` | Open AI Configuration window | Menu/Window → AI Configuration → verify window exists |
| `dse-inspector-sections/ai_config_model_select` | Select AI model from combo | Open combo → select item |

### Phase 3: Tool Panel Interaction Tests (12 tests)

**File**: NEW `ui_tests_tool_panels.cpp`

#### NavMesh (3 tests)

| Test ID | Description | Key Actions |
|:--------|:------------|:------------|
| `dse-tool-panels/navmesh_bake_settings` | Modify Cell Size and Agent Height | `WindowFocus("//NavMesh")` → find sliders → InputValue |
| `dse-tool-panels/navmesh_bake_button` | Click Bake NavMesh button | ItemClick "Bake NavMesh" |
| `dse-tool-panels/navmesh_overlay_toggle` | Toggle Show Overlay checkbox | ItemClick "Show Overlay" |

#### Version Control (3 tests)

| Test ID | Description | Key Actions |
|:--------|:------------|:------------|
| `dse-tool-panels/vcs_panel_open` | Open panel and verify UI elements | `WindowFocus("//Version Control")` → verify buttons exist |
| `dse-tool-panels/vcs_refresh_button` | Click Refresh button | ItemClick "Refresh" |
| `dse-tool-panels/vcs_stage_unstage` | Stage/Unstage file operation | Find file entry → click Stage |

#### Agent Panel (3 tests)

| Test ID | Description | Key Actions |
|:--------|:------------|:------------|
| `dse-tool-panels/agent_send_message` | Type text and click Send | `WindowFocus("//AI Agent")` → InputText "##agent_input" → ItemClick "Send" |
| `dse-tool-panels/agent_clear_button` | Click Clear button | ItemClick "Clear" |
| `dse-tool-panels/agent_combo_select` | Open agent selection combo | ItemClick "##agent_combo" |

#### Vegetation (2 tests)

| Test ID | Description | Key Actions |
|:--------|:------------|:------------|
| `dse-tool-panels/vegetation_brush_open` | Open Vegetation Brush window | EnsureAllPanelsVisible → WindowFocus |
| `dse-tool-panels/vegetation_brush_settings` | Modify brush size/density | Find sliders → InputValue |

#### 2D Tools (1 test)

| Test ID | Description | Key Actions |
|:--------|:------------|:------------|
| `dse-tool-panels/sprite_slicer_open` | Open Sprite Slicer window | WindowFocus → verify exists |

### Phase 4: Existing Panel Deep Tests (8 tests)

**File**: NEW `ui_tests_panel_deep.cpp`

#### Lua Debugger (2 tests)

| Test ID | Description |
|:--------|:------------|
| `dse-panel-deep/lua_debugger_open` | Focus Lua Debugger and verify UI regions |
| `dse-panel-deep/lua_debugger_breakpoint_list` | Verify breakpoint list area exists |

#### World Partition (2 tests)

| Test ID | Description |
|:--------|:------------|
| `dse-panel-deep/world_partition_grid_view` | Verify grid view renders |
| `dse-panel-deep/world_partition_cell_select` | Click a partition cell |

#### Profiler (2 tests)

| Test ID | Description |
|:--------|:------------|
| `dse-panel-deep/profiler_frame_graph` | Verify frame graph area exists |
| `dse-panel-deep/profiler_clear` | Clear profiler data |

### Phase 5: kPanels[] Supplement (2 entries + existence tests)

**File**: `ui_tests_panels.cpp` (extend kPanels[])

Add missing standalone-window panels to `kPanels[]`:

```cpp
{"ai_config",         "AI Configuration"},
{"vegetation_brush",  "Vegetation Brush"},
```

Note: Audio, C#, Particle, Physics Debug are Inspector Sections (no `ImGui::Begin`),
so they cannot be tested via kPanels[] window existence. Their tests go in Phase 2.

## 4. Implementation Notes

### Test Entity Creation Pattern

For Inspector Section tests (Phase 2), use the existing test harness pattern:

```cpp
// Example: create entity with AudioSourceComponent
auto& reg = Reg();
auto ent = reg.create();
reg.emplace<dse::NameComponent>(ent, "TestAudioEntity");
reg.emplace<dse::AudioSourceComponent>(ent);
Services().engine->pipeline()->world().select_entity(ent);
ctx->Yield(4); // wait for Inspector to update
```

### Widget ID Convention

- Reflected fields: `##<TypeName>.<field_name>` (e.g., `##AudioSourceComponent.volume`)
- Manual fields: `##<custom_id>` (e.g., `##csharp_class`)
- Buttons: Label text (e.g., `"Play"`, `"Bake NavMesh"`)
- Toggles: `###<id>` for stable IDs (e.g., `###info_toggle`)

### Registration Pattern

Each new test file needs a `Register*Tests(ImGuiTestEngine*)` function declared in
`ui_tests_internal.h` and called from the test registration entry point.

### Build

```bash
cmake -DDSE_EDITOR_UI_TESTS=ON ..
cmake --build build --config Release --target dse_editor_cpp
bin\dsengine-editor-uitest.exe --run-ui-tests
```

## 5. Effort Estimate

| Phase | New Tests | Effort | Priority |
|:------|:----------|:-------|:---------|
| Phase 1: Console new features | 4 | ~30 min | P0 (just shipped) |
| Phase 2: Inspector sections | 10 | ~2-3 hours | P1 |
| Phase 3: Tool panel interaction | 12 | ~2-3 hours | P1 |
| Phase 4: Panel deep tests | 8 | ~1-2 hours | P2 |
| Phase 5: kPanels supplement | 2 | ~15 min | P2 |
| **Total** | **~36 new tests** | **~6-9 hours** | |

Post-implementation: 214 + 36 = **~250 tests**, panel coverage ~47/50.

## 6. New Files to Create

| File | Phase | Content |
|:-----|:------|:--------|
| `ui_tests_inspector_sections.cpp` | Phase 2 | Inspector section interaction tests |
| `ui_tests_tool_panels.cpp` | Phase 3 | Tool panel interaction tests |
| `ui_tests_panel_deep.cpp` | Phase 4 | Deep tests for existing panels |

## 7. Files to Modify

| File | Phase | Change |
|:-----|:------|:-------|
| `ui_tests_console.cpp` | Phase 1 | Add 4 new console tests |
| `ui_tests_panels.cpp` | Phase 5 | Add 2 entries to kPanels[] |
| `ui_tests_internal.h` | Phase 2-4 | Declare new Register*Tests functions |
| `ui_test_harness.cpp` (or registration entry) | Phase 2-4 | Call new Register*Tests functions |
