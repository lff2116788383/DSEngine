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

// Resumable single-instruction execution state, used by the editor debugger to
// step through the same bytecode the VM runs (no separate "simulation" path).
struct StepState {
    const CompiledFunction* func = nullptr;
    std::vector<BpValue> regs;
    int pc = 0;
    bool finished = false;
    BpValue result;
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

    /// Prepare a resumable execution paused before the first instruction.
    void BeginStep(StepState& state, const CompiledFunction& func, VmContext& ctx,
                   const std::vector<BpValue>& args = {});
    /// Execute exactly one bytecode instruction (Call is stepped over).
    /// Returns the source node id of the next instruction, or -1 when finished.
    int StepOnce(StepState& state, VmContext& ctx);
    /// Source graph node id mapped to the instruction at the current pc (-1 if none).
    int CurrentNode(const StepState& state) const;

    /// Execute on_init for an instance
    void RunInit(BlueprintInstance& instance, uint32_t entity_id);

    /// Execute on_update for an instance
    void RunUpdate(BlueprintInstance& instance, uint32_t entity_id, float dt);

    // Debug
    int GetInstructionCount() const { return instruction_count_; }
    bool HasError() const { return !last_error_.empty(); }
    const std::string& GetLastError() const { return last_error_; }

private:
    enum class StepResult { Continue, Returned };
    // Execute the single instruction at `pc` (advancing it); shared by Execute
    // and StepOnce so both run identical bytecode semantics.
    StepResult RunOne(const CompiledFunction& func, VmContext& ctx,
                      std::vector<BpValue>& regs, int& pc, BpValue& out_return);

    std::vector<std::pair<std::string, ExternFn>> extern_functions_;
    std::unordered_map<std::string, int> extern_index_;
    int instruction_count_ = 0;
    std::string last_error_;
};

}  // namespace dse::editor::bp
