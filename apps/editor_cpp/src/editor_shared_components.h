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
