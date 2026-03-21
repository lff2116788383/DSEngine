#include "phase1/systems/lua_script_system.h"
#include "phase1/ecs/components_2d.h"
#include "lua_binding/lua_binding.h"
#include "utils/debug.h"

void LuaScriptSystem::Init(Phase1World& world) {
    auto view = world.registry().view<LuaScriptComponent>();
    for (auto entity : view) {
        auto& script = view.get<LuaScriptComponent>(entity);
        if (!script.is_initialized && !script.script_path.empty()) {
            try {
                // Load script
                sol::state& lua = LuaBinding::sol_state();
                sol::load_result loaded_script = lua.load_file(script.script_path);
                
                if (!loaded_script.valid()) {
                    sol::error err = loaded_script;
                    DEBUG_LOG_ERROR("Failed to load lua script {}: {}", script.script_path, err.what());
                    continue;
                }

                // Execute script which should return a table representing the class/behavior
                sol::table script_table = loaded_script();
                
                // Create an instance of the script for this entity
                if (script_table.valid() && script_table["New"].valid()) {
                    sol::table instance = script_table["New"](script_table);
                    instance["entity"] = (uint32_t)entity; // Inject entity ID
                    
                    // Store instance on heap and keep pointer
                    sol::table* instance_ptr = new sol::table(instance);
                    script.script_instance = instance_ptr;
                    script.is_initialized = true;

                    // Call Awake/Init if exists
                    if (instance["Awake"].valid()) {
                        instance["Awake"](instance);
                    }
                }
            } catch (const sol::error& e) {
                DEBUG_LOG_ERROR("Lua Error during Init {}: {}", script.script_path, e.what());
            }
        }
    }
}

void LuaScriptSystem::Update(Phase1World& world, float delta_time) {
    auto view = world.registry().view<LuaScriptComponent>();
    for (auto entity : view) {
        auto& script = view.get<LuaScriptComponent>(entity);
        if (script.is_initialized && script.script_instance) {
            try {
                sol::table* instance = static_cast<sol::table*>(script.script_instance);
                if (instance->valid() && (*instance)["Update"].valid()) {
                    (*instance)["Update"](*instance, delta_time);
                }
            } catch (const sol::error& e) {
                DEBUG_LOG_ERROR("Lua Error during Update {}: {}", script.script_path, e.what());
            }
        }
    }
}

void LuaScriptSystem::Shutdown(Phase1World& world) {
    auto view = world.registry().view<LuaScriptComponent>();
    for (auto entity : view) {
        auto& script = view.get<LuaScriptComponent>(entity);
        if (script.script_instance) {
            sol::table* instance = static_cast<sol::table*>(script.script_instance);
            
            if (instance->valid() && (*instance)["OnDestroy"].valid()) {
                try {
                    (*instance)["OnDestroy"](*instance);
                } catch (const sol::error& e) {
                    DEBUG_LOG_ERROR("Lua Error during OnDestroy {}: {}", script.script_path, e.what());
                }
            }
            
            delete instance;
            script.script_instance = nullptr;
        }
        script.is_initialized = false;
    }
}

void LuaScriptSystem::HotReloadScript(Phase1World& world, const std::string& script_path) {
    auto view = world.registry().view<LuaScriptComponent>();
    sol::state& lua = LuaBinding::sol_state();
    
    // First, compile the new script file to see if there are syntax errors
    sol::load_result loaded_script = lua.load_file(script_path);
    if (!loaded_script.valid()) {
        sol::error err = loaded_script;
        DEBUG_LOG_ERROR("Hot Reload failed. Syntax error in {}: {}", script_path, err.what());
        return;
    }
    
    sol::table new_script_class;
    try {
        new_script_class = loaded_script();
    } catch (const sol::error& e) {
        DEBUG_LOG_ERROR("Hot Reload failed. Execution error in {}: {}", script_path, e.what());
        return;
    }
    
    // Now apply to all entities using this script
    int reloaded_count = 0;
    for (auto entity : view) {
        auto& script = view.get<LuaScriptComponent>(entity);
        
        if (script.script_path == script_path) {
            // Keep old state if possible, or re-initialize
            // A robust hot-reload would serialize state from old instance and deserialize to new instance.
            // Here we do a simple replace and call a hypothetical "OnReload" or just replace the metatable/functions.
            
            if (script.script_instance) {
                sol::table* old_instance = static_cast<sol::table*>(script.script_instance);
                
                // Swap functions/metatable rather than destroying state
                if (new_script_class.valid() && old_instance->valid()) {
                    // Very simplistic hot swap: copy new class functions to old instance's metatable
                    // or just call an OnReload hook.
                    if ((*old_instance)["OnReload"].valid()) {
                        (*old_instance)["OnReload"](*old_instance, new_script_class);
                    }
                }
            } else {
                // Was never initialized, initialize it now
                script.is_initialized = false; // Force re-init next frame or do it here
            }
            reloaded_count++;
        }
    }
    
    DEBUG_LOG_INFO("Hot reloaded script: {}, affected {} entities.", script_path, reloaded_count);
}
