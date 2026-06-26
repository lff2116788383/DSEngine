/**
 * @file editor_curve_editor_core.cpp
 * @brief 曲线编辑器纯逻辑（无 ImGui）：关键帧排序、线性/三次 Hermite 求值、默认曲线工厂。
 *        从 editor_curve_editor.cpp 抽出，供无头 gtest 直接链接测试。
 */

#include "editor_curve_editor.h"

#include <algorithm>

namespace dse::editor {

void EditorCurve::SortKeys() {
    std::sort(keys.begin(), keys.end(),
              [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
}

float EditorCurve::Evaluate(float t) const {
    if (keys.empty()) return 0.0f;
    if (keys.size() == 1 || t <= keys.front().time) return keys.front().value;
    if (t >= keys.back().time) return keys.back().value;

    // Find segment
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (t >= keys[i].time && t <= keys[i + 1].time) {
            float dt = keys[i + 1].time - keys[i].time;
            if (dt < 1e-6f) return keys[i].value;
            float s = (t - keys[i].time) / dt;

            if (interp == CurveInterp::Linear) {
                return keys[i].value + (keys[i + 1].value - keys[i].value) * s;
            }

            // Cubic Hermite
            float s2 = s * s;
            float s3 = s2 * s;
            float h00 = 2 * s3 - 3 * s2 + 1;
            float h10 = s3 - 2 * s2 + s;
            float h01 = -2 * s3 + 3 * s2;
            float h11 = s3 - s2;
            float m0 = keys[i].out_tangent * dt;
            float m1 = keys[i + 1].in_tangent * dt;
            return h00 * keys[i].value + h10 * m0 + h01 * keys[i + 1].value + h11 * m1;
        }
    }
    return keys.back().value;
}

EditorCurve MakeDefaultCurve(const char* name, float start_val, float end_val) {
    EditorCurve c;
    c.name = name;
    c.keys.push_back({0.0f, start_val, 0.0f, 0.0f});
    c.keys.push_back({1.0f, end_val, 0.0f, 0.0f});
    return c;
}

} // namespace dse::editor
