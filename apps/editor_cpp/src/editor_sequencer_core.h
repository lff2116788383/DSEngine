#pragma once

// Sequencer pure core — no ImGui / GLFW / UI dependency, can be tested headlessly.
//
// Data model + versioned serialization for .dsequence project files, plus
// mapping to runtime CutsceneSequence via the shared cutscene serializer.
//
// This is the "logic core" extracted from editor_sequencer.cpp following the
// same pattern as editor_autosave_core.{h,cpp}:
//   - Pure data structs (SequencerState / Track / Clip / Keyframe)
//   - Pure functions (serialize / deserialize / bake / un-bake)
//   - No ImGui, no file system, no singleton state
//
// The .dsequence file format is the editor's own versioned JSON (schema v1).
// It stores editor-only metadata (muted, locked, target_entity, property_path,
// frame_rate, clips with color/volume) that the runtime .dcutscene format does
// not carry.  The shared engine serializer (cutscene_serialize) is used for the
// .dcutscene bake path and for mapping between editor and runtime models.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/cutscene/cutscene_player.h"
#include "engine/cutscene/cutscene_track.h"

namespace dse::editor {

// ─── Data model ─────────────────────────────────────────────────────────

enum class SeqTrackType {
    Camera, Property, Event, Audio, Video, Fade, Group
};

struct SequencerKeyframe {
    float time = 0.0f;
    float value = 0.0f;
    float in_tangent = 0.0f;
    float out_tangent = 0.0f;
};

struct SequencerClip {
    std::string name;
    float start_time = 0.0f;
    float end_time = 1.0f;
    uint32_t color = 0;
    bool selected = false;
    // For audio/video clips
    std::string asset_path;
    float volume = 1.0f;
};

struct SequencerTrack {
    std::string name;
    SeqTrackType type = SeqTrackType::Property;
    bool expanded = true;
    bool locked = false;
    bool muted = false;
    bool visible = true;
    float height = 28.0f;
    std::vector<SequencerClip> clips;
    std::vector<SequencerKeyframe> keyframes;
    uint32_t track_color = 0;
    int group_index = -1;      // parent group track index (-1 = top level)
    // Property binding
    std::string target_entity;
    std::string property_path;
};

struct SequencerState {
    std::vector<SequencerTrack> tracks;
    float duration = 10.0f;
    float current_time = 0.0f;
    bool playing = false;
    float playback_speed = 1.0f;
    bool loop = false;
    // View
    float view_start = 0.0f;
    float view_end = 10.0f;
    float track_header_width = 200.0f;
    int selected_track = -1;
    int selected_clip = -1;
    // Snapping
    bool snap_enabled = true;
    float snap_interval = 0.5f;
    float frame_rate = 30.0f;
    bool initialized = false;

    // ── UI interaction state (not serialized; runtime only) ──
    bool dragging_playhead = false;
    bool dragging_clip = false;
    int drag_clip_track = -1;
    int drag_clip_index = -1;
    float drag_offset = 0.0f;
};

// ─── Color helper (equivalent to IM_COL32 without imgui dependency) ──────

constexpr uint32_t MakeSeqColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(r);
}

// ─── Type name conversion ───────────────────────────────────────────────

const char* SeqTrackTypeName(SeqTrackType type);
SeqTrackType SeqTrackTypeFromName(const std::string& name);

// ─── Serialization (.dsequence project format) ──────────────────────────

constexpr int kSequencerSchemaVersion = 1;

/// Serialize a SequencerState to a JSON string (.dsequence format).
std::string SerializeSequencerProject(const SequencerState& state);

/// Deserialize a JSON string to a SequencerState.
/// Returns true on success; false with an error message on failure.
/// Does NOT silently fall back — corrupt or missing data produces a
/// diagnostic in `err`.
bool DeserializeSequencerProject(const std::string& json,
                                 SequencerState& state,
                                 std::string& err);

// ─── Runtime mapping (via shared cutscene serializer) ────────────────────

/// Map editor SequencerState to runtime CutsceneSequence.
/// Property keyframes → PropertyTrack, Event clips → EventTrack,
/// Audio clips → AudioTrack, Video clips → VideoTrack,
/// Camera clips → CameraTrack (empty, no camera-transform editing in this editor).
/// Fade/Group tracks have no runtime equivalent and are skipped.
std::shared_ptr<cutscene::CutsceneSequence>
BakeToRuntimeSequence(const SequencerState& state, const std::string& seq_name);

/// Map a runtime CutsceneSequence back to editor SequencerState.
/// Runtime keyframes/events/cues become editor clips/keyframes.
/// Editor-only fields (muted, locked, target_entity, etc.) get defaults.
/// This is the inverse of BakeToRuntimeSequence for the data that round-trips.
SequencerState SequenceFromRuntime(const cutscene::CutsceneSequence& seq);

// ─── Empty state ─────────────────────────────────────────────────────────

/// Return a minimal empty SequencerState (no tracks, default duration).
/// Used on "new project" instead of demo data.
SequencerState MakeEmptySequencerState();

}  // namespace dse::editor
