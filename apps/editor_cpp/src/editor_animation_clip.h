#pragma once

#include "editor_context.h"
#include <string>
#include <vector>

namespace dse::editor {

/// Draw the Animation Clip Editor panel (bone pose preview, curve fine-tuning, additive layers)
void DrawAnimationClipEditor(EditorContext& ctx);

// ─── Test accessors ─────────────────────────────────────────────────────
struct AnimClipTestBone {
    std::string name;
    int parent_index = -1;
};

struct AnimClipTestState {
    std::vector<AnimClipTestBone> bones;
    int selected_bone = -1;
    float current_time = 0.0f;
    float clip_duration = 2.0f;
    bool playing = false;
};

AnimClipTestState& GetAnimClipEditorState();
void AnimClipPlay();
void AnimClipStop();

} // namespace dse::editor
