#ifndef DSE_LUA_DEBUGGER_H
#define DSE_LUA_DEBUGGER_H

#include <string>

class LuaDebugger {
public:
    static void StartServer(int port);
    static void StopServer();

    static void OnBreakPoint(const std::string& file, int line);
    static void OnException(const std::string& message);

    // Protocol handling for VSCode / MobDebug
    static void HandleMessage(const std::string& msg);
};

#endif // DSE_LUA_DEBUGGER_H
