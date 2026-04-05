#pragma once

#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/memory_profiler.h"
#include "engine/profiler/render_profiler.h"

namespace dse::editor {

struct EditorProfilerContext {
    dse::profiler::CPUProfiler& cpu_profiler;
    dse::profiler::MemoryProfiler& memory_profiler;
    dse::profiler::RenderProfiler& render_profiler;
};

void DrawProfilerPanel(EditorProfilerContext& context);

} // namespace dse::editor
