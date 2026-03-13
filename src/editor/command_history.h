#ifndef DSE_COMMAND_HISTORY_H
#define DSE_COMMAND_HISTORY_H

#include <stack>
#include <memory>
#include "command.h"

class CommandHistory {
public:
    static void Push(std::shared_ptr<ICommand> command) {
        command->Execute();
        undo_stack_.push(command);
        // Clear redo stack on new action
        while (!redo_stack_.empty()) {
            redo_stack_.pop();
        }
    }

    static void Undo() {
        if (undo_stack_.empty()) return;
        auto command = undo_stack_.top();
        undo_stack_.pop();
        command->Undo();
        redo_stack_.push(command);
    }

    static void Redo() {
        if (redo_stack_.empty()) return;
        auto command = redo_stack_.top();
        redo_stack_.pop();
        command->Execute();
        undo_stack_.push(command);
    }

private:
    static std::stack<std::shared_ptr<ICommand>> undo_stack_;
    static std::stack<std::shared_ptr<ICommand>> redo_stack_;
};

#endif // DSE_COMMAND_HISTORY_H
