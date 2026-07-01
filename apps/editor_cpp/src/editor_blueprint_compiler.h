#pragma once

/**
 * @file editor_blueprint_compiler.h
 * @brief Blueprint compiler - dual target: Lua source export + ByteCode for VM
 */

#include "editor_blueprint.h"
#include "editor_blueprint_vm.h"

#include <string>

namespace dse::editor::bp {

/// Compile a blueprint asset to bytecode for the Blueprint VM
CompiledBlueprint CompileToByteCode(const BlueprintAsset& asset);

/// Compile a blueprint asset to Lua source (export/debug)
std::string CompileToLua(const BlueprintAsset& asset);

/// Compile a single function graph to bytecode
CompiledFunction CompileFunctionGraph(const BpFunctionGraph& graph,
                                      const std::vector<BpVariable>& variables);

/// Validate graph connectivity and type compatibility
struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

ValidationResult ValidateGraph(const BpFunctionGraph& graph);

}  // namespace dse::editor::bp
