// editor_undo.cpp - Undo/Redo manager implementation
#include "editor_undo.h"

namespace dse::editor {

UndoRedoManager& GetUndoRedoManager() {
    static UndoRedoManager instance(200);
    return instance;
}

} // namespace dse::editor
