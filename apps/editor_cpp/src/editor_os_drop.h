#pragma once

#include <string>
#include <vector>

namespace dse::editor {

/// Stores pending OS file drop events for consumption by panels each frame.
struct OsDropEvent {
    std::vector<std::string> paths;
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
};

/// Push a new OS drop event (called from GLFW callback)
void PushOsDropEvent(const std::vector<std::string>& paths, float mouse_x, float mouse_y);

/// Consume and clear the pending OS drop event. Returns true if an event was pending.
bool ConsumeOsDropEvent(OsDropEvent& out);

/// Check if there is a pending OS drop event without consuming it.
bool HasPendingOsDrop();

} // namespace dse::editor
