#pragma once

#include <entt/entt.hpp>

class World;

namespace dse::runtime { class EngineInstance; }
namespace dse::profiler {
    class CPUProfiler;
    class MemoryProfiler;
    class RenderProfiler;
}

namespace dse::editor {

/// 编辑器统一上下文：所有面板 / 快捷键 / 工具栏共享的核心状态。
/// 由 EditorApp 拥有并在每帧开始时更新，各子系统通过 const ref 或 ref 访问。
struct EditorContext {
    dse::runtime::EngineInstance& engine;
    World& world;
    entt::registry& registry;
    entt::entity& selected_entity;

    bool read_only = false;   // Play 模式下为 true
    bool is_2d = false;

    // Profiler（非所有面板都需要，但放在统一 context 避免额外构造）
    dse::profiler::CPUProfiler& cpu_profiler;
    dse::profiler::MemoryProfiler& memory_profiler;
    dse::profiler::RenderProfiler& render_profiler;

    // Gizmo
    int& current_gizmo_operation;
    int& current_gizmo_mode;
};

} // namespace dse::editor
