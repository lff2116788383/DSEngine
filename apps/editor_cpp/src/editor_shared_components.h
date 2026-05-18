#pragma once

#include <string>

namespace dse::editor {

struct EditorNameComponent {
    std::string name;
};

struct SiblingIndexComponent {
    int index = 0;
};

} // namespace dse::editor

// Keep short aliases for backward compatibility in files that don't use the namespace
using dse::editor::EditorNameComponent;
using dse::editor::SiblingIndexComponent;
