#pragma once

#include "editor_context.h"
#include <string>
#include <vector>

namespace dse::editor {

/// Draw the Cinematic Sequencer panel (multi-track timeline editor)
void DrawSequencerPanel(EditorContext& ctx);

// ─── Test accessors ─────────────────────────────────────────────────────
struct SeqTestTrack {
    std::string name;
};

struct SequencerTestState {
    std::vector<SeqTestTrack> tracks;
    float sequence_duration = 10.0f;
    float playhead_time = 0.0f;
    bool playing = false;
};

SequencerTestState& GetSequencerState();
void SequencerPlay();
void SequencerPause();
void SequencerStop();

} // namespace dse::editor
