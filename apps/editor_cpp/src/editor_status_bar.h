#pragma once

#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/render_profiler.h"
#include <entt/entt.hpp>

namespace dse::editor {

struct EditorStatusBarContext {
    dse::profiler::CPUProfiler& cpu_profiler;
    dse::profiler::RenderProfiler& render_profiler;
    entt::registry& registry;
    int current_gizmo_operation;
    int current_gizmo_mode;
};

void DrawStatusBar(EditorStatusBarContext& context);

} // namespace dse::editor
