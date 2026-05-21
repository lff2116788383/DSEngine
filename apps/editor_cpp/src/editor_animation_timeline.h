#pragma once

namespace dse::editor {

struct EditorContext;

/// Draw the Animation Timeline panel with keyframe editing, play/stop controls,
/// and curve preview.
void DrawAnimationTimelinePanel(EditorContext& ctx);

} // namespace dse::editor
