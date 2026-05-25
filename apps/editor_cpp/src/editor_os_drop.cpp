#include "editor_os_drop.h"

namespace dse::editor {

namespace {
    bool s_has_event = false;
    OsDropEvent s_event{};
}

void PushOsDropEvent(const std::vector<std::string>& paths, float mouse_x, float mouse_y) {
    s_event.paths = paths;
    s_event.mouse_x = mouse_x;
    s_event.mouse_y = mouse_y;
    s_has_event = true;
}

bool ConsumeOsDropEvent(OsDropEvent& out) {
    if (!s_has_event) return false;
    out = std::move(s_event);
    s_event = {};
    s_has_event = false;
    return true;
}

bool HasPendingOsDrop() {
    return s_has_event;
}

} // namespace dse::editor
