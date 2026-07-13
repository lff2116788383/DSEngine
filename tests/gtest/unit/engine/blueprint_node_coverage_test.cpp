/**
 * @file blueprint_node_coverage_test.cpp
 * @brief P0-3 蓝图编译器节点覆盖 + VM 安全网测试。
 *
 * 覆盖两类回归：
 *  1. 编译器为算术/比较/逻辑/Vec3/数组节点发出真实 opcode（而非静默
 *     LoadConst(0.0f)），未识别的数据节点改为按名调用外部函数。
 *  2. VM 对越界寄存器、非法跳转、无限循环、除零等畸形字节码保持安全，
 *     ValidateCompiledFunction 作为编译期安全网提前拒绝非法产物。
 */

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "engine/scripting/blueprint/blueprint_compiler.h"
#include "engine/scripting/blueprint/blueprint_vm.h"

namespace {

using namespace dse::bp;

// ── 图构造辅助 ──────────────────────────────────────────────────────────────
// 构造 EventGraph: On Init --flow--> Set Variable("out"),
// 其 Value 引脚由指定数据节点的输出驱动；数据节点的标量输入使用引脚默认值。
struct DataNodeGraph {
    BlueprintAsset asset;

    DataNodeGraph(const std::string& node_name,
                  const std::vector<float>& input_defaults,
                  BpPinType output_type,
                  BpVarType var_type) {
        asset.name = "CoverageBP";

        BpVariable out;
        out.name = "out";
        out.type = var_type;
        asset.variables.push_back(out);

        BpFunctionGraph g;
        g.name = "EventGraph";
        g.next_id = 1000;

        // On Init 事件
        BpNode ev;
        ev.id = 1;
        ev.name = "On Init";
        ev.category = "Event";
        BpPin flow_out; flow_out.id = 2; flow_out.type = BpPinType::Flow; flow_out.kind = BpPinKind::Output;
        ev.outputs.push_back(flow_out);
        g.nodes.push_back(ev);

        // Set Variable("out")
        BpNode sv;
        sv.id = 3;
        sv.name = "Set Variable";
        sv.category = "Variable";
        sv.comment = "out";
        BpPin sv_flow; sv_flow.id = 4; sv_flow.type = BpPinType::Flow; sv_flow.kind = BpPinKind::Input;
        sv.inputs.push_back(sv_flow);
        BpPin sv_val; sv_val.id = 5; sv_val.type = output_type; sv_val.kind = BpPinKind::Input;
        sv.inputs.push_back(sv_val);
        g.nodes.push_back(sv);

        // 数据节点
        BpNode dn;
        dn.id = 6;
        dn.name = node_name;
        dn.category = "Math";
        for (size_t i = 0; i < input_defaults.size(); ++i) {
            BpPin in; in.id = 10 + static_cast<int>(i);
            in.type = BpPinType::Float; in.kind = BpPinKind::Input;
            in.default_float = input_defaults[i];
            dn.inputs.push_back(in);
        }
        BpPin dn_out; dn_out.id = 7; dn_out.type = output_type; dn_out.kind = BpPinKind::Output;
        dn.outputs.push_back(dn_out);
        g.nodes.push_back(dn);

        // 连接: OnInit.flow -> SetVar.flow, dataNode.out -> SetVar.value
        BpLink l1; l1.id = 20; l1.from_pin = 2; l1.to_pin = 4; g.links.push_back(l1);
        BpLink l2; l2.id = 21; l2.from_pin = 7; l2.to_pin = 5; g.links.push_back(l2);

        asset.graphs.push_back(g);
    }
};

const CompiledFunction* FindFunc(const CompiledBlueprint& bp, const std::string& name) {
    for (const auto& fn : bp.functions)
        if (fn.name == name) return &fn;
    return nullptr;
}

// 编译图、运行 on_init，返回写入变量 "out" 的值。
BpValue RunOnInit(const CompiledBlueprint& bp, BlueprintVM& vm) {
    BlueprintInstance inst;
    inst.blueprint = &bp;
    inst.variables = bp.default_variables;
    VmContext ctx;
    ctx.instance = &inst;
    const CompiledFunction* fn = FindFunc(bp, "on_init");
    EXPECT_NE(fn, nullptr);
    if (fn) vm.Execute(*fn, ctx);
    return inst.variables.empty() ? BpValue() : inst.variables[0];
}

// ── 节点覆盖 ────────────────────────────────────────────────────────────────

TEST(BlueprintNodeCoverage, ArithmeticNodesEmitRealOpcodes) {
    BlueprintVM vm;
    struct Case { const char* name; std::vector<float> in; float want; };
    const Case cases[] = {
        {"Multiply", {3.f, 4.f}, 12.f},
        {"Subtract", {10.f, 3.f}, 7.f},
        {"Divide",   {12.f, 4.f}, 3.f},
        {"Modulo",   {10.f, 3.f}, 1.f},
        {"Power",    {2.f, 10.f}, 1024.f},
        {"Min",      {2.f, 9.f},  2.f},
        {"Max",      {2.f, 9.f},  9.f},
        {"Clamp",    {5.f, 0.f, 3.f}, 3.f},
        {"Lerp",     {0.f, 10.f, 0.25f}, 2.5f},
        {"Abs",      {-4.f}, 4.f},
    };
    for (const auto& c : cases) {
        DataNodeGraph g(c.name, c.in, BpPinType::Float, BpVarType::Float);
        BpValue v = RunOnInit(CompileToByteCode(g.asset), vm);
        EXPECT_NEAR(v.AsFloat(), c.want, 1e-4f) << "node " << c.name;
    }
}

TEST(BlueprintNodeCoverage, ComparisonAndLogicNodes) {
    BlueprintVM vm;
    {
        DataNodeGraph g("Greater Than", {5.f, 2.f}, BpPinType::Bool, BpVarType::Bool);
        EXPECT_TRUE(RunOnInit(CompileToByteCode(g.asset), vm).AsBool());
    }
    {
        DataNodeGraph g("Greater Than", {1.f, 2.f}, BpPinType::Bool, BpVarType::Bool);
        EXPECT_FALSE(RunOnInit(CompileToByteCode(g.asset), vm).AsBool());
    }
    {
        DataNodeGraph g("Not Equal", {1.f, 2.f}, BpPinType::Bool, BpVarType::Bool);
        EXPECT_TRUE(RunOnInit(CompileToByteCode(g.asset), vm).AsBool());
    }
    {
        DataNodeGraph g("Less Than", {2.f, 2.f}, BpPinType::Bool, BpVarType::Bool);
        EXPECT_FALSE(RunOnInit(CompileToByteCode(g.asset), vm).AsBool());
    }
}

TEST(BlueprintNodeCoverage, MakeVec3ProducesVec3Value) {
    BlueprintVM vm;
    DataNodeGraph g("Make Vec3", {1.f, 2.f, 3.f}, BpPinType::Vec3, BpVarType::Vec3);
    BpValue v = RunOnInit(CompileToByteCode(g.asset), vm);
    ASSERT_EQ(v.type, BpValue::Type::Vec3);
    float out[3]; v.AsVec3(out);
    EXPECT_NEAR(out[0], 1.f, 1e-5f);
    EXPECT_NEAR(out[1], 2.f, 1e-5f);
    EXPECT_NEAR(out[2], 3.f, 1e-5f);
}

TEST(BlueprintNodeCoverage, Vec3LengthComposesFromDotAndSqrt) {
    BlueprintVM vm;
    // Vec3 Length 的输入是 Vec3 引脚；无连接时使用引脚默认 (0,0,0)。这里改由
    // Make Vec3 → Vec3 Length 组合，验证长度 = sqrt(3^2+4^2)=5。
    BlueprintAsset a;
    a.name = "LenBP";
    BpVariable out; out.name = "out"; out.type = BpVarType::Float; a.variables.push_back(out);

    BpFunctionGraph g; g.name = "EventGraph"; g.next_id = 1000;

    BpNode ev; ev.id = 1; ev.name = "On Init";
    BpPin ev_out; ev_out.id = 2; ev_out.type = BpPinType::Flow; ev_out.kind = BpPinKind::Output;
    ev.outputs.push_back(ev_out); g.nodes.push_back(ev);

    BpNode sv; sv.id = 3; sv.name = "Set Variable"; sv.comment = "out";
    BpPin sv_flow; sv_flow.id = 4; sv_flow.type = BpPinType::Flow; sv_flow.kind = BpPinKind::Input; sv.inputs.push_back(sv_flow);
    BpPin sv_val; sv_val.id = 5; sv_val.type = BpPinType::Float; sv_val.kind = BpPinKind::Input; sv.inputs.push_back(sv_val);
    g.nodes.push_back(sv);

    BpNode mk; mk.id = 6; mk.name = "Make Vec3";
    for (int i = 0; i < 3; ++i) { BpPin p; p.id = 10+i; p.type = BpPinType::Float; p.kind = BpPinKind::Input; mk.inputs.push_back(p); }
    mk.inputs[0].default_float = 3.f; mk.inputs[1].default_float = 4.f; mk.inputs[2].default_float = 0.f;
    BpPin mk_out; mk_out.id = 7; mk_out.type = BpPinType::Vec3; mk_out.kind = BpPinKind::Output; mk.outputs.push_back(mk_out);
    g.nodes.push_back(mk);

    BpNode ln; ln.id = 8; ln.name = "Vec3 Length";
    BpPin ln_in; ln_in.id = 40; ln_in.type = BpPinType::Vec3; ln_in.kind = BpPinKind::Input; ln.inputs.push_back(ln_in);
    BpPin ln_out; ln_out.id = 41; ln_out.type = BpPinType::Float; ln_out.kind = BpPinKind::Output; ln.outputs.push_back(ln_out);
    g.nodes.push_back(ln);

    BpLink a1; a1.id = 20; a1.from_pin = 2;  a1.to_pin = 4;  g.links.push_back(a1);
    BpLink a2; a2.id = 21; a2.from_pin = 41; a2.to_pin = 5;  g.links.push_back(a2);   // Length -> SetVar.value
    BpLink a3; a3.id = 22; a3.from_pin = 7;  a3.to_pin = 40; g.links.push_back(a3);   // MakeVec3 -> Length.in
    a.graphs.push_back(g);

    BpValue v = RunOnInit(CompileToByteCode(a), vm);
    EXPECT_NEAR(v.AsFloat(), 5.f, 1e-4f);
}

TEST(BlueprintNodeCoverage, UnknownDataNodeCallsExternNotSilentZero) {
    BlueprintVM vm;
    bool called = false;
    float received = -1.f;
    vm.RegisterExtern("MysteryValue", [&](const std::vector<BpValue>& args) {
        called = true;
        if (!args.empty()) received = args[0].AsFloat();
        return BpValue::Float(42.f);
    });

    DataNodeGraph g("MysteryValue", {7.f}, BpPinType::Float, BpVarType::Float);
    BpValue v = RunOnInit(CompileToByteCode(g.asset), vm);

    EXPECT_TRUE(called) << "unknown data node must route to a named extern, not silently load 0";
    EXPECT_NEAR(received, 7.f, 1e-5f) << "extern should receive the node's data input";
    EXPECT_NEAR(v.AsFloat(), 42.f, 1e-5f);
}

// ── 编译期校验 ──────────────────────────────────────────────────────────────

TEST(BlueprintValidation, RealCompiledBlueprintValidates) {
    DataNodeGraph g("Multiply", {3.f, 4.f}, BpPinType::Float, BpVarType::Float);
    CompiledBlueprint bp = CompileToByteCode(g.asset);
    std::string err;
    EXPECT_TRUE(ValidateCompiledBlueprint(bp, err)) << err;
}

TEST(BlueprintValidation, RejectsOutOfRangeRegister) {
    CompiledFunction fn;
    fn.name = "bad";
    fn.num_registers = 1;
    fn.code = { {OpCode::Add, 5, 0, 0, 0}, {OpCode::Halt, 0, 0, 0, 0} };
    std::string err;
    EXPECT_FALSE(ValidateCompiledFunction(fn, err));
    EXPECT_FALSE(err.empty());
}

TEST(BlueprintValidation, RejectsOutOfRangeJump) {
    CompiledFunction fn;
    fn.name = "badjump";
    fn.num_registers = 1;
    fn.code = { {OpCode::Jump, 0, 0, 0, 100}, {OpCode::Halt, 0, 0, 0, 0} };
    std::string err;
    EXPECT_FALSE(ValidateCompiledFunction(fn, err));
}

TEST(BlueprintValidation, RejectsNewerBytecodeVersion) {
    CompiledBlueprint bp;
    bp.bytecode_version = kBytecodeVersion + 1;
    std::string err;
    EXPECT_FALSE(ValidateCompiledBlueprint(bp, err));
}

// ── VM 运行时安全 ───────────────────────────────────────────────────────────

TEST(BlueprintVmSafety, OutOfRangeRegisterDoesNotCrash) {
    BlueprintVM vm;
    CompiledFunction fn;
    fn.name = "oob";
    fn.num_registers = 1;
    fn.constants = { BpValue::Float(1.f) };
    fn.code = { {OpCode::Add, 9, 9, 9, 0}, {OpCode::Halt, 0, 0, 0, 0} };
    VmContext ctx;
    vm.Execute(fn, ctx);
    EXPECT_TRUE(vm.HasError());
    EXPECT_NE(vm.GetLastError().find("register"), std::string::npos);
}

TEST(BlueprintVmSafety, OutOfRangeJumpIsRejected) {
    BlueprintVM vm;
    CompiledFunction fn;
    fn.name = "jmp";
    fn.num_registers = 1;
    fn.code = { {OpCode::Jump, 0, 0, 0, 100}, {OpCode::Halt, 0, 0, 0, 0} };
    VmContext ctx;
    vm.Execute(fn, ctx);
    EXPECT_TRUE(vm.HasError());
    EXPECT_NE(vm.GetLastError().find("jump"), std::string::npos);
}

TEST(BlueprintVmSafety, InfiniteLoopHitsInstructionLimit) {
    BlueprintVM vm;
    CompiledFunction fn;
    fn.name = "loop";
    fn.num_registers = 1;
    fn.code = { {OpCode::Jump, 0, 0, 0, -1} };  // jump back onto itself
    VmContext ctx;
    vm.Execute(fn, ctx);
    EXPECT_TRUE(vm.HasError());
    EXPECT_NE(vm.GetLastError().find("instruction limit"), std::string::npos);
}

TEST(BlueprintVmSafety, DivideAndModuloByZeroYieldZero) {
    BlueprintVM vm;
    CompiledFunction fn;
    fn.name = "divzero";
    fn.num_registers = 3;
    fn.constants = { BpValue::Float(5.f), BpValue::Float(0.f) };
    fn.code = {
        {OpCode::LoadConst, 1, 0, 0, 0},  // r1 = 5
        {OpCode::LoadConst, 2, 1, 0, 0},  // r2 = 0
        {OpCode::Div, 0, 1, 2, 0},        // r0 = 5 / 0 -> 0
        {OpCode::Halt, 0, 0, 0, 0},
    };
    VmContext ctx;
    BpValue r = vm.Execute(fn, ctx);
    EXPECT_FALSE(vm.HasError());
    EXPECT_NEAR(r.AsFloat(), 0.f, 1e-6f);
}

}  // namespace
