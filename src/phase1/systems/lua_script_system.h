#ifndef DSE_PHASE1_LUA_SCRIPT_SYSTEM_H
#define DSE_PHASE1_LUA_SCRIPT_SYSTEM_H

#include "phase1/ecs/world.h"
#include <string>

class LuaScriptSystem {
public:
    void Init(Phase1World& world);
    void Update(Phase1World& world, float delta_time);
    void Shutdown(Phase1World& world);

    // Reload a specific script file and update all entities using it
    void HotReloadScript(Phase1World& world, const std::string& script_path);
};

#endif // DSE_PHASE1_LUA_SCRIPT_SYSTEM_H
