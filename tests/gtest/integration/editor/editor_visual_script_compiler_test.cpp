/**
 * @file editor_visual_script_compiler_test.cpp
 * @brief 可视化脚本控制流 → Lua 编译器（editor_visual_script_compiler）的无头测试。
 *
 * 直接构建纯数据图（与编辑器 ImGui 节点图等价），验证真正的控制流生成：
 * - 事件入口生成函数体（替换 {body}）
 * - Branch 生成 if/else，分支体内联各自 Flow 语句
 * - For Loop 生成数值 for，Body 体续接、Done 续接
 * - 数据流（Add 等纯数据节点）按需内联为表达式
 * - 空事件体回退、无事件回退
 */

#include <gtest/gtest.h>

#include <string>

#include "editor_visual_script_compiler.h"

using namespace dse::editor::vs;

namespace {

bool Contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// 按出现顺序检查多个子串。
bool ContainsInOrder(const std::string& hay, std::initializer_list<std::string> needles) {
    size_t pos = 0;
    for (const auto& n : needles) {
        size_t f = hay.find(n, pos);
        if (f == std::string::npos) return false;
        pos = f + n.size();
    }
    return true;
}

// ── 简易构图器：自动分配 pin id ───────────────────────────────────────────
struct Builder {
    Graph g;
    int next_pin = 100;

    int Pin(NodeData& n, const char* name, PinType t, PinKind k) {
        PinData p;
        p.id = next_pin++;
        p.name = name;
        p.type = t;
        p.kind = k;
        if (k == PinKind::Input) n.inputs.push_back(p);
        else n.outputs.push_back(p);
        return p.id;
    }

    NodeData& Add(int id, const char* name, const char* cat, const char* tmpl) {
        NodeData n;
        n.id = id;
        n.name = name;
        n.category = cat;
        n.code_template = tmpl;
        g.nodes.push_back(std::move(n));
        return g.nodes.back();
    }

    void Link(int from_out, int to_in) {
        g.links.push_back(LinkData{static_cast<int>(g.links.size()) + 1, from_out, to_in});
    }
};

}  // namespace

// ── 事件 + Branch + 两个 Print ─────────────────────────────────────────────

TEST(VisualScriptCompiler, EventBranchGeneratesIfElseWithBodies) {
    Builder b;

    auto& ev = b.Add(1, "On Update", "Event", "function on_update(dt)\n{body}\nend");
    int ev_exec = b.Pin(ev, "Exec", PinType::Flow, PinKind::Output);
    b.Pin(ev, "dt", PinType::Float, PinKind::Output);

    auto& br = b.Add(2, "Branch", "Flow", "");
    int br_exec = b.Pin(br, "Exec", PinType::Flow, PinKind::Input);
    auto& cond_pin = br.inputs.emplace_back();
    cond_pin.id = b.next_pin++; cond_pin.name = "Condition";
    cond_pin.type = PinType::Bool; cond_pin.kind = PinKind::Input; cond_pin.default_bool = true;
    int br_true = b.Pin(br, "True", PinType::Flow, PinKind::Output);
    int br_false = b.Pin(br, "False", PinType::Flow, PinKind::Output);

    auto& p_true = b.Add(3, "Print", "Utility", "print({input1})");
    int pt_exec = b.Pin(p_true, "Exec", PinType::Flow, PinKind::Input);
    auto& msg_t = p_true.inputs.emplace_back();
    msg_t.id = b.next_pin++; msg_t.name = "Message"; msg_t.type = PinType::String;
    msg_t.kind = PinKind::Input; msg_t.default_string = "yes";
    b.Pin(p_true, "Exec", PinType::Flow, PinKind::Output);

    auto& p_false = b.Add(4, "Print", "Utility", "print({input1})");
    int pf_exec = b.Pin(p_false, "Exec", PinType::Flow, PinKind::Input);
    auto& msg_f = p_false.inputs.emplace_back();
    msg_f.id = b.next_pin++; msg_f.name = "Message"; msg_f.type = PinType::String;
    msg_f.kind = PinKind::Input; msg_f.default_string = "no";
    b.Pin(p_false, "Exec", PinType::Flow, PinKind::Output);

    b.Link(ev_exec, br_exec);
    b.Link(br_true, pt_exec);
    b.Link(br_false, pf_exec);

    const std::string lua = CompileVisualScript(b.g);

    EXPECT_TRUE(Contains(lua, "function on_update(dt)"));
    EXPECT_TRUE(ContainsInOrder(lua, {"if true then", "print(\"yes\")", "else", "print(\"no\")", "end"}));
    // {body} 占位必须被真正替换，不再残留 TODO 注释。
    EXPECT_FALSE(Contains(lua, "{body}"));
    EXPECT_FALSE(Contains(lua, "TODO"));
}

// ── For Loop：Body 续接 + Done 续接 ────────────────────────────────────────

TEST(VisualScriptCompiler, ForLoopGeneratesNumericForAndContinuation) {
    Builder b;

    auto& ev = b.Add(1, "On Init", "Event", "function on_init()\n{body}\nend");
    int ev_exec = b.Pin(ev, "Exec", PinType::Flow, PinKind::Output);

    auto& loop = b.Add(2, "For Loop", "Flow", "");
    int loop_exec = b.Pin(loop, "Exec", PinType::Flow, PinKind::Input);
    auto& start = loop.inputs.emplace_back();
    start.id = b.next_pin++; start.name = "Start"; start.type = PinType::Float;
    start.kind = PinKind::Input; start.default_float = 1.0f;
    auto& endp = loop.inputs.emplace_back();
    endp.id = b.next_pin++; endp.name = "End"; endp.type = PinType::Float;
    endp.kind = PinKind::Input; endp.default_float = 10.0f;
    int loop_body = b.Pin(loop, "Body", PinType::Flow, PinKind::Output);
    int loop_index = b.Pin(loop, "Index", PinType::Float, PinKind::Output);
    int loop_done = b.Pin(loop, "Done", PinType::Flow, PinKind::Output);

    // Body: Print(index)
    auto& p_body = b.Add(3, "Print", "Utility", "print({input1})");
    int pb_exec = b.Pin(p_body, "Exec", PinType::Flow, PinKind::Input);
    auto& msg_b = p_body.inputs.emplace_back();
    msg_b.id = b.next_pin++; msg_b.name = "Message"; msg_b.type = PinType::Any;
    msg_b.kind = PinKind::Input;
    b.Pin(p_body, "Exec", PinType::Flow, PinKind::Output);

    // Done: Print("done")
    auto& p_done = b.Add(4, "Print", "Utility", "print({input1})");
    int pd_exec = b.Pin(p_done, "Exec", PinType::Flow, PinKind::Input);
    auto& msg_d = p_done.inputs.emplace_back();
    msg_d.id = b.next_pin++; msg_d.name = "Message"; msg_d.type = PinType::String;
    msg_d.kind = PinKind::Input; msg_d.default_string = "done";
    b.Pin(p_done, "Exec", PinType::Flow, PinKind::Output);

    b.Link(ev_exec, loop_exec);
    b.Link(loop_body, pb_exec);
    b.Link(loop_index, msg_b.id);  // index 作为 Print 的消息（数据连线）
    b.Link(loop_done, pd_exec);

    const std::string lua = CompileVisualScript(b.g);

    EXPECT_TRUE(Contains(lua, "function on_init()"));
    // for 头 + 索引变量被打印 + 循环外 done。
    EXPECT_TRUE(ContainsInOrder(lua, {"for i", " = 1, 10 do", "print(i", "end", "print(\"done\")"}));
}

// ── 数据流内联：Print(Message = Add(2,3)) ──────────────────────────────────

TEST(VisualScriptCompiler, DataFlowInlinesExpression) {
    Builder b;

    auto& ev = b.Add(1, "On Init", "Event", "function on_init()\n{body}\nend");
    int ev_exec = b.Pin(ev, "Exec", PinType::Flow, PinKind::Output);

    auto& add = b.Add(2, "Add", "Math", "{output0} = {input0} + {input1}");
    auto& a = add.inputs.emplace_back();
    a.id = b.next_pin++; a.name = "A"; a.type = PinType::Float; a.kind = PinKind::Input; a.default_float = 2.0f;
    auto& bb = add.inputs.emplace_back();
    bb.id = b.next_pin++; bb.name = "B"; bb.type = PinType::Float; bb.kind = PinKind::Input; bb.default_float = 3.0f;
    int add_result = b.Pin(add, "Result", PinType::Float, PinKind::Output);

    auto& pr = b.Add(3, "Print", "Utility", "print({input1})");
    int pr_exec = b.Pin(pr, "Exec", PinType::Flow, PinKind::Input);
    auto& msg = pr.inputs.emplace_back();
    msg.id = b.next_pin++; msg.name = "Message"; msg.type = PinType::Any; msg.kind = PinKind::Input;
    b.Pin(pr, "Exec", PinType::Flow, PinKind::Output);

    b.Link(ev_exec, pr_exec);
    b.Link(add_result, msg.id);

    const std::string lua = CompileVisualScript(b.g);
    EXPECT_TRUE(Contains(lua, "print((2 + 3))"));
}

// ── Create Entity 的数据输出赋局部变量并可被后续引用 ───────────────────────

TEST(VisualScriptCompiler, FlowDataOutputBoundToLocal) {
    Builder b;

    auto& ev = b.Add(1, "On Init", "Event", "function on_init()\n{body}\nend");
    int ev_exec = b.Pin(ev, "Exec", PinType::Flow, PinKind::Output);

    auto& create = b.Add(2, "Create Entity", "ECS", "{output1} = ecs.create_entity({input1})");
    int ce_exec_in = b.Pin(create, "Exec", PinType::Flow, PinKind::Input);
    auto& name = create.inputs.emplace_back();
    name.id = b.next_pin++; name.name = "Name"; name.type = PinType::String;
    name.kind = PinKind::Input; name.default_string = "enemy";
    int ce_exec_out = b.Pin(create, "Exec", PinType::Flow, PinKind::Output);
    int ce_entity = b.Pin(create, "Entity", PinType::Entity, PinKind::Output);

    auto& setpos = b.Add(3, "Set Position", "ECS", "ecs.set_position({input1}, {input2})");
    int sp_exec = b.Pin(setpos, "Exec", PinType::Flow, PinKind::Input);
    int sp_entity = b.Pin(setpos, "Entity", PinType::Entity, PinKind::Input);
    b.Pin(setpos, "Position", PinType::Vec3, PinKind::Input);
    b.Pin(setpos, "Exec", PinType::Flow, PinKind::Output);

    b.Link(ev_exec, ce_exec_in);
    b.Link(ce_exec_out, sp_exec);
    b.Link(ce_entity, sp_entity);

    const std::string lua = CompileVisualScript(b.g);
    // create entity 赋给局部变量，set_position 用同一变量。
    EXPECT_TRUE(Contains(lua, "local v0 = ecs.create_entity(\"enemy\")"));
    EXPECT_TRUE(Contains(lua, "ecs.set_position(v0,"));
}

// ── 空事件体回退 ───────────────────────────────────────────────────────────

TEST(VisualScriptCompiler, EmptyEventBodyFallback) {
    Builder b;
    auto& ev = b.Add(1, "On Update", "Event", "function on_update(dt)\n{body}\nend");
    b.Pin(ev, "Exec", PinType::Flow, PinKind::Output);
    b.Pin(ev, "dt", PinType::Float, PinKind::Output);

    const std::string lua = CompileVisualScript(b.g);
    EXPECT_TRUE(Contains(lua, "function on_update(dt)"));
    EXPECT_TRUE(Contains(lua, "-- (empty)"));
    EXPECT_FALSE(Contains(lua, "{body}"));
}

// ── 无事件回退：纯数据节点输出局部变量 ─────────────────────────────────────

TEST(VisualScriptCompiler, NoEventFallbackEmitsDataLocals) {
    Builder b;
    auto& add = b.Add(1, "Add", "Math", "{output0} = {input0} + {input1}");
    auto& a = add.inputs.emplace_back();
    a.id = b.next_pin++; a.name = "A"; a.type = PinType::Float; a.kind = PinKind::Input; a.default_float = 4.0f;
    auto& bb = add.inputs.emplace_back();
    bb.id = b.next_pin++; bb.name = "B"; bb.type = PinType::Float; bb.kind = PinKind::Input; bb.default_float = 5.0f;
    b.Pin(add, "Result", PinType::Float, PinKind::Output);

    const std::string lua = CompileVisualScript(b.g);
    EXPECT_TRUE(Contains(lua, "local v0 = (4 + 5)"));
}
