/**
 * @file blueprint_lifecycle_e2e_test.cpp
 * @brief P0-3 headless end-to-end acceptance for the single .dbp pipeline:
 *        author -> save(.dbp on disk) -> reopen -> compile -> validate ->
 *        VM execute, asserting the authored logic actually produces the
 *        expected runtime value after a full disk round-trip.
 *
 * This exercises the same shared serialize/compile/VM path the editor and
 * runtime use (no ImGui / no editor session required), closing the
 * "no end-to-end editor->save->reopen->compile->runtime->execute" gap for the
 * parts that can be validated headlessly.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "engine/scripting/blueprint/blueprint_compiler.h"
#include "engine/scripting/blueprint/blueprint_serialize.h"
#include "engine/scripting/blueprint/blueprint_vm.h"

namespace {

using namespace dse::bp;

// Author an EventGraph: On Init --flow--> Set Variable("out"), whose value is
// driven by Multiply(lhs, rhs) using pin default values.
BlueprintAsset AuthorMultiplyBlueprint(float lhs, float rhs) {
    BlueprintAsset asset;
    asset.name = "LifecycleBP";

    BpVariable out;
    out.name = "out";
    out.type = BpVarType::Float;
    asset.variables.push_back(out);

    BpFunctionGraph g;
    g.name = "EventGraph";
    g.next_id = 1000;

    BpNode ev;
    ev.id = 1;
    ev.name = "On Init";
    ev.category = "Event";
    BpPin flow_out; flow_out.id = 2; flow_out.type = BpPinType::Flow; flow_out.kind = BpPinKind::Output;
    ev.outputs.push_back(flow_out);
    g.nodes.push_back(ev);

    BpNode sv;
    sv.id = 3;
    sv.name = "Set Variable";
    sv.category = "Variable";
    sv.comment = "out";
    BpPin sv_flow; sv_flow.id = 4; sv_flow.type = BpPinType::Flow; sv_flow.kind = BpPinKind::Input;
    sv.inputs.push_back(sv_flow);
    BpPin sv_val; sv_val.id = 5; sv_val.type = BpPinType::Float; sv_val.kind = BpPinKind::Input;
    sv.inputs.push_back(sv_val);
    g.nodes.push_back(sv);

    BpNode mul;
    mul.id = 6;
    mul.name = "Multiply";
    mul.category = "Math";
    BpPin a; a.id = 10; a.type = BpPinType::Float; a.kind = BpPinKind::Input; a.default_float = lhs;
    BpPin b; b.id = 11; b.type = BpPinType::Float; b.kind = BpPinKind::Input; b.default_float = rhs;
    mul.inputs.push_back(a);
    mul.inputs.push_back(b);
    BpPin mul_out; mul_out.id = 7; mul_out.type = BpPinType::Float; mul_out.kind = BpPinKind::Output;
    mul.outputs.push_back(mul_out);
    g.nodes.push_back(mul);

    BpLink l1; l1.id = 20; l1.from_pin = 2; l1.to_pin = 4; g.links.push_back(l1);
    BpLink l2; l2.id = 21; l2.from_pin = 7; l2.to_pin = 5; g.links.push_back(l2);

    asset.graphs.push_back(g);
    return asset;
}

const CompiledFunction* FindFunc(const CompiledBlueprint& bp, const std::string& name) {
    for (const auto& fn : bp.functions)
        if (fn.name == name) return &fn;
    return nullptr;
}

BpValue RunOnInit(const CompiledBlueprint& bp) {
    BlueprintInstance inst;
    inst.blueprint = &bp;
    inst.variables = bp.default_variables;
    VmContext ctx;
    ctx.instance = &inst;
    const CompiledFunction* fn = FindFunc(bp, "on_init");
    EXPECT_NE(fn, nullptr);
    BlueprintVM vm;
    if (fn) vm.Execute(*fn, ctx);
    return inst.variables.empty() ? BpValue() : inst.variables[0];
}

}  // namespace

TEST(BlueprintLifecycleE2E, AuthorSaveReopenCompileValidateExecute) {
    const BlueprintAsset authored = AuthorMultiplyBlueprint(3.f, 4.f);

    // 1. Save to a real .dbp file on disk.
    auto path = (std::filesystem::temp_directory_path() /
                 "dse_bp_lifecycle_e2e.dbp").string();
    BlueprintDiagnostics save_diag;
    ASSERT_TRUE(SaveBlueprintAsset(authored, path, save_diag))
        << (save_diag.errors.empty() ? "" : save_diag.errors.front());
    ASSERT_TRUE(std::filesystem::exists(path));

    // 2. Reopen from disk through the shared checked-load path.
    BlueprintAsset reloaded;
    BlueprintDiagnostics load_diag;
    ASSERT_TRUE(LoadBlueprintAssetChecked(reloaded, path, load_diag))
        << (load_diag.errors.empty() ? "" : load_diag.errors.front());
    EXPECT_TRUE(load_diag.errors.empty());

    // Structural round-trip fidelity.
    ASSERT_EQ(reloaded.graphs.size(), 1u);
    EXPECT_EQ(reloaded.graphs[0].nodes.size(), authored.graphs[0].nodes.size());
    EXPECT_EQ(reloaded.graphs[0].links.size(), authored.graphs[0].links.size());
    ASSERT_EQ(reloaded.variables.size(), 1u);
    EXPECT_EQ(reloaded.variables[0].name, "out");

    // 3. Compile the reloaded asset and 4. validate the produced bytecode.
    CompiledBlueprint bp = CompileToByteCode(reloaded);
    std::string verr;
    ASSERT_TRUE(ValidateCompiledBlueprint(bp, verr)) << verr;

    // 5. Execute on_init in the VM and assert the authored logic ran (3*4=12).
    BpValue v = RunOnInit(bp);
    EXPECT_NEAR(v.AsFloat(), 12.f, 1e-4f);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST(BlueprintLifecycleE2E, ReloadedAssetReexecutesConsistently) {
    // Author with different inputs; the reopened+recompiled asset must reflect
    // exactly what was persisted (not a cached/authored in-memory value).
    const BlueprintAsset authored = AuthorMultiplyBlueprint(6.f, 7.f);

    std::string json = SerializeBlueprintAsset(authored);
    ASSERT_NE(json.find("\"version\""), std::string::npos)
        << "serialized .dbp must carry a schema version envelope";

    BlueprintAsset reloaded;
    BlueprintDiagnostics diag;
    ASSERT_TRUE(DeserializeBlueprintAsset(reloaded, json, diag));

    CompiledBlueprint bp = CompileToByteCode(reloaded);
    std::string verr;
    ASSERT_TRUE(ValidateCompiledBlueprint(bp, verr)) << verr;

    EXPECT_NEAR(RunOnInit(bp).AsFloat(), 42.f, 1e-4f);
}
