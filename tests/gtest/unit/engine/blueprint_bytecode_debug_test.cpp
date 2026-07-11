/**
 * @file blueprint_bytecode_debug_test.cpp
 * @brief P0-3 蓝图调试器真实执行基础：验证编译器发出的 pc→节点映射
 *        (CompiledFunction::source_nodes) 与字节码一一对应，供编辑器单步
 *        调试器高亮当前节点与按节点命中断点使用。
 */

#include <gtest/gtest.h>

#include <algorithm>

#include "engine/scripting/blueprint/blueprint_compiler.h"
#include "engine/scripting/blueprint/blueprint_vm.h"

namespace {

using namespace dse::bp;

// EventGraph: On Update --flow--> Set Variable("hp" = 7).
BlueprintAsset MakeSetVarAsset() {
    BlueprintAsset a;
    a.name = "DebugBP";

    BpVariable hp;
    hp.name = "hp";
    hp.type = BpVarType::Float;
    hp.default_float = 5.0f;
    a.variables.push_back(hp);

    BpFunctionGraph g;
    g.name = "EventGraph";
    g.next_id = 100;

    BpNode ev;
    ev.id = 10;
    ev.name = "On Update";
    ev.category = "Event";
    {
        BpPin then;
        then.id = 11;
        then.name = "Then";
        then.type = BpPinType::Flow;
        then.kind = BpPinKind::Output;
        ev.outputs.push_back(then);
    }
    g.nodes.push_back(ev);

    BpNode set;
    set.id = 30;
    set.name = "Set Variable";
    set.category = "Variable";
    set.comment = "hp";
    {
        BpPin flow_in;
        flow_in.id = 31;
        flow_in.name = "";
        flow_in.type = BpPinType::Flow;
        flow_in.kind = BpPinKind::Input;
        set.inputs.push_back(flow_in);

        BpPin value_in;
        value_in.id = 32;
        value_in.name = "Value";
        value_in.type = BpPinType::Float;
        value_in.kind = BpPinKind::Input;
        value_in.default_float = 7.0f;
        set.inputs.push_back(value_in);
    }
    g.nodes.push_back(set);

    BpLink link;
    link.id = 40;
    link.from_pin = 11;  // On Update.Then
    link.to_pin = 31;    // Set Variable.flow-in
    g.links.push_back(link);

    a.graphs.push_back(g);
    return a;
}

TEST(BlueprintBytecodeDebug, SourceNodesParallelToCode) {
    CompiledBlueprint bp = CompileToByteCode(MakeSetVarAsset());
    ASSERT_FALSE(bp.functions.empty());
    for (const auto& fn : bp.functions) {
        // Every instruction has exactly one source-node attribution.
        EXPECT_EQ(fn.code.size(), fn.source_nodes.size()) << "func " << fn.name;
    }
}

TEST(BlueprintBytecodeDebug, MapsInstructionsToOwningNode) {
    CompiledBlueprint bp = CompileToByteCode(MakeSetVarAsset());

    const CompiledFunction* on_update = nullptr;
    for (const auto& fn : bp.functions)
        if (fn.name == "on_update") on_update = &fn;
    ASSERT_NE(on_update, nullptr);

    // The Set Variable node (id 30) must own at least one emitted instruction
    // (its value load + StoreVar), proving real pc→node mapping rather than a
    // synthesized/placeholder trace.
    bool owns_node_30 = std::find(on_update->source_nodes.begin(),
                                  on_update->source_nodes.end(), 30) !=
                        on_update->source_nodes.end();
    EXPECT_TRUE(owns_node_30);

    // Compiler-synthesized terminator (Halt) is attributed to no node (-1).
    ASSERT_FALSE(on_update->code.empty());
    ASSERT_FALSE(on_update->source_nodes.empty());
    EXPECT_EQ(on_update->code.back().op, OpCode::Halt);
    EXPECT_EQ(on_update->source_nodes.back(), -1);

    // The StoreVar that writes "hp" (variable index 0) is attributed to node 30.
    for (size_t i = 0; i < on_update->code.size(); ++i) {
        if (on_update->code[i].op == OpCode::StoreVar) {
            EXPECT_EQ(on_update->source_nodes[i], 30);
        }
    }
}

}  // namespace
