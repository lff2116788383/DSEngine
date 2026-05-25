#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "imgui.h"

namespace dse::editor {

/// Interpolation mode for curve segments
enum class CurveInterp { Linear, Cubic };

/// Single keyframe on a curve
struct CurveKey {
    float time = 0.0f;
    float value = 0.0f;
    float in_tangent = 0.0f;   // for cubic
    float out_tangent = 0.0f;  // for cubic
};

/// A single named animation/parameter curve
struct EditorCurve {
    std::string name;
    std::vector<CurveKey> keys;
    CurveInterp interp = CurveInterp::Cubic;
    ImU32 color = 0xFFFFFFFF;

    float Evaluate(float t) const;
    void SortKeys();
};

/// State for the curve editor widget
struct CurveEditorState {
    std::vector<EditorCurve> curves;
    int selected_curve = 0;
    int selected_key = -1;
    bool dragging_key = false;
    float view_time_min = 0.0f;
    float view_time_max = 1.0f;
    float view_value_min = 0.0f;
    float view_value_max = 1.0f;
    bool snap_to_grid = false;
    float grid_snap_time = 0.1f;
    float grid_snap_value = 0.1f;
};

/// Draw a self-contained curve editor widget.
/// Returns true if any value was modified.
bool DrawCurveEditor(const char* label, CurveEditorState& state, const ImVec2& size = ImVec2(0, 200));

/// Utility: create a default linear ramp curve
EditorCurve MakeDefaultCurve(const char* name, float start_val = 0.0f, float end_val = 1.0f);

} // namespace dse::editor
