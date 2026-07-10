#pragma once

/**
 * @file editor_blueprint_vm.h
 * @brief Blueprint Virtual Machine - lightweight register-based bytecode interpreter
 *
 * Design: ~20 opcodes, register-based (not stack), supports:
 *   - Arithmetic, comparison, logic
 *   - Flow control (jump, branch, call, return)
 *   - ECS operations (get/set component fields)
 *   - Variable load/store (blueprint instance state)
 *   - External C++ function calls via bridge
 */

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include "engine/scripting/blueprint/blueprint_vm.h"

namespace dse::editor::bp {

using OpCode = dse::bp::OpCode;
using Instruction = dse::bp::Instruction;
using BpValue = dse::bp::BpValue;
using CompiledFunction = dse::bp::CompiledFunction;
using CompiledBlueprint = dse::bp::CompiledBlueprint;
using BlueprintInstance = dse::bp::BlueprintInstance;

// ─── VM Execution ──────────────────────────────────────────────────────────

// External function bridge: C++ functions callable from blueprint
using ExternFn = std::function<BpValue(const std::vector<BpValue>& args)>;

struct VmContext {
    BlueprintInstance* instance = nullptr;
    uint32_t entity_id = 0;
    float delta_time = 0.0f;
};

class BlueprintVM {
public:
    static BlueprintVM& Get();

    /// Register an external C++ function callable from blueprints
    void RegisterExtern(const std::string& name, ExternFn fn);
    int GetExternIndex(const std::string& name) const;

    /// Execute a compiled function on a blueprint instance
    BpValue Execute(const CompiledFunction& func, VmContext& ctx,
                    const std::vector<BpValue>& args = {});

    /// Execute on_init for an instance
    void RunInit(BlueprintInstance& instance, uint32_t entity_id);

    /// Execute on_update for an instance
    void RunUpdate(BlueprintInstance& instance, uint32_t entity_id, float dt);

    // Debug
    int GetInstructionCount() const { return instruction_count_; }
    bool HasError() const { return !last_error_.empty(); }
    const std::string& GetLastError() const { return last_error_; }

private:
    std::vector<std::pair<std::string, ExternFn>> extern_functions_;
    std::unordered_map<std::string, int> extern_index_;
    int instruction_count_ = 0;
    std::string last_error_;
};

}  // namespace dse::editor::bp
