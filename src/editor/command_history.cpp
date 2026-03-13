#include "command_history.h"

std::stack<std::shared_ptr<ICommand>> CommandHistory::undo_stack_;
std::stack<std::shared_ptr<ICommand>> CommandHistory::redo_stack_;
