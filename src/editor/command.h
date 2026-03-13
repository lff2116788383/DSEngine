#ifndef DSE_COMMAND_H
#define DSE_COMMAND_H

#include <string>

class ICommand {
public:
    virtual ~ICommand() {}
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual std::string GetDescription() const { return "Command"; }
};

#endif // DSE_COMMAND_H
