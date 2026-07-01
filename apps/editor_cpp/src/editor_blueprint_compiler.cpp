/**
 * @file editor_blueprint_compiler.cpp
 * @brief Blueprint compiler - dual target: ByteCode (VM) + Lua source (export)
 */

#include "editor_blueprint_compiler.h"

#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdio>

namespace dse::editor::bp {

namespace {

// ─── Register allocator (simple linear scan) ───────────────────────────────

class RegAlloc {
public:
    int Alloc() { return next_++; }
    int AllocN(int n) { int base = next_; next_ += n; return base; }
    int Count() const { return next_; }
private:
    int next_ = 0;
};

// ─── Helper: find pin connections ──────────────────────────────────────────

int FindLinkedOutput(const BpFunctionGraph& graph, int input_pin_id) {
    for (const auto& l : graph.links) {
        if (l.to_pin == input_pin_id) return l.from_pin;
    }
    return -1;
}

int FindLinkedInput(const BpFunctionGraph& graph, int output_pin_id) {
    for (const auto& l : graph.links) {
        if (l.from_pin == output_pin_id) return l.to_pin;
    }
    return -1;
}

const BpNode* FindPinOwner(const BpFunctionGraph& graph, int pin_id) {
    for (const auto& n : graph.nodes) {
        for (const auto& p : n.inputs) if (p.id == pin_id) return &n;
        for (const auto& p : n.outputs) if (p.id == pin_id) return &n;
    }
    return nullptr;
}

const BpPin* FindPin(const BpFunctionGraph& graph, int pin_id) {
    for (const auto& n : graph.nodes) {
        for (const auto& p : n.inputs) if (p.id == pin_id) return &p;
        for (const auto& p : n.outputs) if (p.id == pin_id) return &p;
    }
    return nullptr;
}

// ─── ByteCode compiler ─────────────────────────────────────────────────────

class ByteCodeCompiler {
public:
    ByteCodeCompiler(const BpFunctionGraph& graph, const std::vector<BpVariable>& vars)
        : graph_(graph), vars_(vars) {}

    CompiledFunction Compile() {
        CompiledFunction func;
        func.name = graph_.name;
        func.num_params = static_cast<int>(graph_.input_params.size());

        // Reserve registers for parameters
        for (int i = 0; i < func.num_params; ++i) {
            regs_.Alloc();
        }

        // Find event nodes and compile flow from them
        for (const auto& node : graph_.nodes) {
            if (node.category == "Event") {
                CompileFlowFrom(node);
            }
        }

        // If no event nodes, compile pure data output
        if (code_.empty()) {
            for (const auto& node : graph_.nodes) {
                if (!node.outputs.empty() && node.outputs[0].type != BpPinType::Flow) {
                    int reg = CompileDataNode(node);
                    (void)reg;
                }
            }
        }

        // Add halt
        Emit(OpCode::Halt);

        func.code = std::move(code_);
        func.constants = std::move(constants_);
        func.num_registers = regs_.Count();
        return func;
    }

private:
    const BpFunctionGraph& graph_;
    const std::vector<BpVariable>& vars_;
    RegAlloc regs_;
    std::vector<Instruction> code_;
    std::vector<BpValue> constants_;
    std::unordered_map<int, int> pin_to_reg_; // pin_id → register holding its value

    void Emit(OpCode op, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, int16_t extra = 0) {
        code_.push_back({op, a, b, c, extra});
    }

    int AddConstant(const BpValue& v) {
        constants_.push_back(v);
        return static_cast<int>(constants_.size() - 1);
    }

    int GetVarIndex(const std::string& name) const {
        for (size_t i = 0; i < vars_.size(); ++i) {
            if (vars_[i].name == name) return static_cast<int>(i);
        }
        return -1;
    }

    void CompileFlowFrom(const BpNode& event_node) {
        // Find the Flow output pin
        for (const auto& pin : event_node.outputs) {
            if (pin.type == BpPinType::Flow) {
                int next_pin = FindLinkedInput(graph_, pin.id);
                if (next_pin >= 0) {
                    const BpNode* next_node = FindPinOwner(graph_, next_pin);
                    if (next_node) CompileFlowNode(*next_node);
                }
            }
        }
    }

    void CompileFlowNode(const BpNode& node) {
        // Compile data inputs first
        for (const auto& pin : node.inputs) {
            if (pin.type != BpPinType::Flow) {
                int src_pin = FindLinkedOutput(graph_, pin.id);
                if (src_pin >= 0) {
                    const BpNode* src_node = FindPinOwner(graph_, src_pin);
                    if (src_node) CompileDataNode(*src_node);
                }
            }
        }

        // Generate code for this node
        if (node.name == "Branch") {
            CompileBranch(node);
            return;
        } else if (node.name == "For Loop") {
            CompileForLoop(node);
            return;
        } else if (node.name == "Print") {
            int msg_reg = GetInputReg(node, 1);
            Emit(OpCode::Print, static_cast<uint8_t>(msg_reg));
        } else if (node.name == "Set Position") {
            // ECS set via extern call
            int entity_reg = GetInputReg(node, 1);
            int pos_reg = GetInputReg(node, 2);
            Emit(OpCode::EcsSetVec3, static_cast<uint8_t>(entity_reg), 0, static_cast<uint8_t>(pos_reg));
        } else if (node.name == "Create Entity") {
            int name_reg = GetInputReg(node, 1);
            int result_reg = regs_.Alloc();
            // Store result for data output
            if (node.outputs.size() > 1) {
                pin_to_reg_[node.outputs[1].id] = result_reg;
            }
            int const_idx = AddConstant(BpValue::Entity(0));
            Emit(OpCode::LoadConst, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(const_idx));
            (void)name_reg;
        } else if (node.name == "Set Variable") {
            // Store to instance variable
            int val_reg = GetInputReg(node, 1);
            // Find which variable from node comment or extra data
            int var_idx = 0; // simplified
            Emit(OpCode::StoreVar, static_cast<uint8_t>(var_idx), static_cast<uint8_t>(val_reg));
        } else {
            // Generic flow node: emit based on code_template
            CompileGenericFlowNode(node);
        }

        // Follow flow output
        for (const auto& pin : node.outputs) {
            if (pin.type == BpPinType::Flow && pin.name != "False") {
                int next_pin = FindLinkedInput(graph_, pin.id);
                if (next_pin >= 0) {
                    const BpNode* next_node = FindPinOwner(graph_, next_pin);
                    if (next_node) CompileFlowNode(*next_node);
                }
                break; // only follow first flow output (True for Branch handled separately)
            }
        }
    }

    void CompileBranch(const BpNode& node) {
        int cond_reg = GetInputReg(node, 1); // Condition input

        // JumpIfFalse → else branch
        size_t jump_else_idx = code_.size();
        Emit(OpCode::JumpIfFalse, static_cast<uint8_t>(cond_reg), 0, 0, 0); // patch later

        // True branch
        if (node.outputs.size() >= 1) {
            int true_pin = FindLinkedInput(graph_, node.outputs[0].id);
            if (true_pin >= 0) {
                const BpNode* true_node = FindPinOwner(graph_, true_pin);
                if (true_node) CompileFlowNode(*true_node);
            }
        }

        // Jump over else
        size_t jump_end_idx = code_.size();
        Emit(OpCode::Jump, 0, 0, 0, 0); // patch later

        // Patch jump_else
        code_[jump_else_idx].extra = static_cast<int16_t>(code_.size() - jump_else_idx - 1);

        // False branch
        if (node.outputs.size() >= 2) {
            int false_pin = FindLinkedInput(graph_, node.outputs[1].id);
            if (false_pin >= 0) {
                const BpNode* false_node = FindPinOwner(graph_, false_pin);
                if (false_node) CompileFlowNode(*false_node);
            }
        }

        // Patch jump_end
        code_[jump_end_idx].extra = static_cast<int16_t>(code_.size() - jump_end_idx - 1);
    }

    void CompileForLoop(const BpNode& node) {
        int start_reg = GetInputReg(node, 1);
        int end_reg = GetInputReg(node, 2);
        int idx_reg = regs_.Alloc();

        // Store index output pin
        if (node.outputs.size() >= 2) {
            pin_to_reg_[node.outputs[1].id] = idx_reg;
        }

        // idx = start
        Emit(OpCode::Move, static_cast<uint8_t>(idx_reg), static_cast<uint8_t>(start_reg));

        // Loop condition check
        size_t loop_start = code_.size();
        int cmp_reg = regs_.Alloc();
        Emit(OpCode::CmpLe, static_cast<uint8_t>(cmp_reg),
             static_cast<uint8_t>(idx_reg), static_cast<uint8_t>(end_reg));
        size_t jump_exit_idx = code_.size();
        Emit(OpCode::JumpIfFalse, static_cast<uint8_t>(cmp_reg), 0, 0, 0);

        // Body
        if (node.outputs.size() >= 1) {
            int body_pin = FindLinkedInput(graph_, node.outputs[0].id);
            if (body_pin >= 0) {
                const BpNode* body_node = FindPinOwner(graph_, body_pin);
                if (body_node) CompileFlowNode(*body_node);
            }
        }

        // idx = idx + 1
        int one_const = AddConstant(BpValue::Float(1.0f));
        int one_reg = regs_.Alloc();
        Emit(OpCode::LoadConst, static_cast<uint8_t>(one_reg), static_cast<uint8_t>(one_const));
        Emit(OpCode::Add, static_cast<uint8_t>(idx_reg),
             static_cast<uint8_t>(idx_reg), static_cast<uint8_t>(one_reg));

        // Jump back to loop start
        int jump_back = static_cast<int>(loop_start) - static_cast<int>(code_.size()) - 1;
        Emit(OpCode::Jump, 0, 0, 0, static_cast<int16_t>(jump_back));

        // Patch exit jump
        code_[jump_exit_idx].extra = static_cast<int16_t>(code_.size() - jump_exit_idx - 1);

        // Done output
        if (node.outputs.size() >= 3) {
            int done_pin = FindLinkedInput(graph_, node.outputs[2].id);
            if (done_pin >= 0) {
                const BpNode* done_node = FindPinOwner(graph_, done_pin);
                if (done_node) CompileFlowNode(*done_node);
            }
        }
    }

    int CompileDataNode(const BpNode& node) {
        // Check if already compiled
        if (!node.outputs.empty()) {
            auto it = pin_to_reg_.find(node.outputs[0].id);
            if (it != pin_to_reg_.end()) return it->second;
        }

        int result_reg = regs_.Alloc();

        if (node.name == "Add") {
            int a_reg = GetInputReg(node, 0);
            int b_reg = GetInputReg(node, 1);
            Emit(OpCode::Add, static_cast<uint8_t>(result_reg),
                 static_cast<uint8_t>(a_reg), static_cast<uint8_t>(b_reg));
        } else if (node.name == "Subtract") {
            int a_reg = GetInputReg(node, 0);
            int b_reg = GetInputReg(node, 1);
            Emit(OpCode::Sub, static_cast<uint8_t>(result_reg),
                 static_cast<uint8_t>(a_reg), static_cast<uint8_t>(b_reg));
        } else if (node.name == "Multiply") {
            int a_reg = GetInputReg(node, 0);
            int b_reg = GetInputReg(node, 1);
            Emit(OpCode::Mul, static_cast<uint8_t>(result_reg),
                 static_cast<uint8_t>(a_reg), static_cast<uint8_t>(b_reg));
        } else if (node.name == "Divide") {
            int a_reg = GetInputReg(node, 0);
            int b_reg = GetInputReg(node, 1);
            Emit(OpCode::Div, static_cast<uint8_t>(result_reg),
                 static_cast<uint8_t>(a_reg), static_cast<uint8_t>(b_reg));
        } else if (node.name == "Sin") {
            int x_reg = GetInputReg(node, 0);
            Emit(OpCode::Sin, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(x_reg));
        } else if (node.name == "Cos") {
            int x_reg = GetInputReg(node, 0);
            Emit(OpCode::Cos, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(x_reg));
        } else if (node.name == "Sqrt") {
            int x_reg = GetInputReg(node, 0);
            Emit(OpCode::Sqrt, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(x_reg));
        } else if (node.name == "Abs") {
            int x_reg = GetInputReg(node, 0);
            Emit(OpCode::Abs, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(x_reg));
        } else if (node.name == "Negate") {
            int x_reg = GetInputReg(node, 0);
            Emit(OpCode::Neg, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(x_reg));
        } else if (node.name == "Get Position") {
            int entity_reg = GetInputReg(node, 0);
            Emit(OpCode::EcsGetVec3, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(entity_reg), 0);
        } else if (node.name == "Self Entity") {
            int const_idx = AddConstant(BpValue::Entity(0)); // placeholder: resolved at runtime
            Emit(OpCode::LoadConst, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(const_idx));
        } else if (node.name == "Float Constant" || node.name == "Constant Float") {
            float val = node.outputs.empty() ? 0.0f : node.outputs[0].default_float;
            int const_idx = AddConstant(BpValue::Float(val));
            Emit(OpCode::LoadConst, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(const_idx));
        } else if (node.name == "Int Constant" || node.name == "Constant Int") {
            int val = node.outputs.empty() ? 0 : node.outputs[0].default_int;
            int const_idx = AddConstant(BpValue::Int(val));
            Emit(OpCode::LoadConst, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(const_idx));
        } else if (node.name == "Get Variable") {
            int var_idx = GetVarIndex(node.comment);
            if (var_idx >= 0) {
                Emit(OpCode::LoadVar, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(var_idx));
            }
        } else if (node.name == "Bool Constant") {
            int const_idx = AddConstant(BpValue::Bool(node.outputs.empty() ? false : node.outputs[0].default_bool));
            Emit(OpCode::LoadConst, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(const_idx));
        } else {
            // Unknown pure node: load zero
            int const_idx = AddConstant(BpValue::Float(0.0f));
            Emit(OpCode::LoadConst, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(const_idx));
        }

        // Register output pin
        if (!node.outputs.empty()) {
            pin_to_reg_[node.outputs[0].id] = result_reg;
        }
        return result_reg;
    }

    int GetInputReg(const BpNode& node, int input_index) {
        if (input_index < 0 || input_index >= static_cast<int>(node.inputs.size())) {
            int r = regs_.Alloc();
            int ci = AddConstant(BpValue::Float(0.0f));
            Emit(OpCode::LoadConst, static_cast<uint8_t>(r), static_cast<uint8_t>(ci));
            return r;
        }

        const BpPin& pin = node.inputs[input_index];
        if (pin.type == BpPinType::Flow) {
            int r = regs_.Alloc();
            int ci = AddConstant(BpValue::Float(0.0f));
            Emit(OpCode::LoadConst, static_cast<uint8_t>(r), static_cast<uint8_t>(ci));
            return r;
        }

        int src_pin = FindLinkedOutput(graph_, pin.id);
        if (src_pin >= 0) {
            // Check if already computed
            auto it = pin_to_reg_.find(src_pin);
            if (it != pin_to_reg_.end()) return it->second;
            // Compile source node
            const BpNode* src_node = FindPinOwner(graph_, src_pin);
            if (src_node) return CompileDataNode(*src_node);
        }

        // Use default value
        int r = regs_.Alloc();
        int ci = AddConstant(BpValue::Float(pin.default_float));
        Emit(OpCode::LoadConst, static_cast<uint8_t>(r), static_cast<uint8_t>(ci));
        return r;
    }

    void CompileGenericFlowNode(const BpNode& node) {
        // For nodes with extern function mapping
        int fn_idx = BlueprintVM::Get().GetExternIndex(node.name);
        if (fn_idx >= 0) {
            int num_data_inputs = 0;
            int arg_start = regs_.Alloc();
            for (size_t i = 0; i < node.inputs.size(); ++i) {
                if (node.inputs[i].type != BpPinType::Flow) {
                    int r = GetInputReg(node, static_cast<int>(i));
                    if (num_data_inputs > 0) regs_.Alloc();
                    Emit(OpCode::Move, static_cast<uint8_t>(arg_start + num_data_inputs), static_cast<uint8_t>(r));
                    ++num_data_inputs;
                }
            }
            int result_reg = regs_.Alloc();
            Emit(OpCode::CallExtern, static_cast<uint8_t>(result_reg),
                 static_cast<uint8_t>(fn_idx), static_cast<uint8_t>(arg_start),
                 static_cast<int16_t>(num_data_inputs));
            if (!node.outputs.empty() && node.outputs.back().type != BpPinType::Flow) {
                pin_to_reg_[node.outputs.back().id] = result_reg;
            }
        }
    }
};

// ─── Lua compiler ──────────────────────────────────────────────────────────

class LuaCompiler {
public:
    LuaCompiler(const BlueprintAsset& asset) : asset_(asset) {}

    std::string Compile() {
        std::ostringstream out;
        out << "-- Generated by DSEngine Blueprint System\n";
        out << "-- Blueprint: " << asset_.name << "\n";
        out << "-- Do not edit manually\n\n";
        out << "local BP = {}\n\n";

        // Variable defaults
        out << "function BP:_init_vars()\n";
        for (const auto& var : asset_.variables) {
            out << "    self." << var.name << " = ";
            EmitDefaultValue(out, var);
            out << "\n";
        }
        out << "end\n\n";

        // Compile each function graph
        for (const auto& graph : asset_.graphs) {
            CompileGraph(out, graph);
        }

        out << "return BP\n";
        return out.str();
    }

private:
    const BlueprintAsset& asset_;
    int indent_ = 1;
    int var_counter_ = 0;

    std::string Indent() const { return std::string(indent_ * 4, ' '); }
    std::string FreshVar() { return "v" + std::to_string(var_counter_++); }

    void EmitDefaultValue(std::ostringstream& out, const BpVariable& var) {
        switch (var.type) {
            case BpVarType::Bool:   out << (var.default_bool ? "true" : "false"); break;
            case BpVarType::Int:    out << var.default_int; break;
            case BpVarType::Float:  out << var.default_float; break;
            case BpVarType::String: out << "\"" << var.default_string << "\""; break;
            case BpVarType::Vec2:   out << "vec2(" << var.default_vec[0] << ", " << var.default_vec[1] << ")"; break;
            case BpVarType::Vec3:   out << "vec3(" << var.default_vec[0] << ", " << var.default_vec[1] << ", " << var.default_vec[2] << ")"; break;
            case BpVarType::Vec4:   out << "vec4(" << var.default_vec[0] << ", " << var.default_vec[1] << ", " << var.default_vec[2] << ", " << var.default_vec[3] << ")"; break;
            case BpVarType::Entity: out << "nil"; break;
            case BpVarType::Array:  out << "{}"; break;
        }
    }

    void CompileGraph(std::ostringstream& out, const BpFunctionGraph& graph) {
        // Determine function signature
        if (graph.name == "EventGraph") {
            // Event graphs produce on_init / on_update methods
            for (const auto& node : graph.nodes) {
                if (node.category == "Event") {
                    if (node.name == "On Init") {
                        out << "function BP:on_init()\n";
                        indent_ = 1;
                        CompileFlowFrom(out, graph, node);
                        out << "end\n\n";
                    } else if (node.name == "On Update") {
                        out << "function BP:on_update(dt)\n";
                        indent_ = 1;
                        CompileFlowFrom(out, graph, node);
                        out << "end\n\n";
                    }
                }
            }
        } else {
            // User function
            out << "function BP:" << graph.name << "(";
            for (size_t i = 0; i < graph.input_params.size(); ++i) {
                if (i > 0) out << ", ";
                out << graph.input_params[i].name;
            }
            out << ")\n";
            indent_ = 1;
            // Compile from function entry node
            for (const auto& node : graph.nodes) {
                if (node.name == "Function Entry") {
                    CompileFlowFrom(out, graph, node);
                    break;
                }
            }
            out << "end\n\n";
        }
    }

    void CompileFlowFrom(std::ostringstream& out, const BpFunctionGraph& graph, const BpNode& node) {
        for (const auto& pin : node.outputs) {
            if (pin.type == BpPinType::Flow) {
                int next_pin = FindLinkedInput(graph, pin.id);
                if (next_pin >= 0) {
                    const BpNode* next = FindPinOwner(graph, next_pin);
                    if (next) CompileFlowNode(out, graph, *next);
                }
            }
        }
    }

    void CompileFlowNode(std::ostringstream& out, const BpFunctionGraph& graph, const BpNode& node) {
        if (node.name == "Branch") {
            std::string cond = InlineExpr(graph, node, 1);
            out << Indent() << "if " << cond << " then\n";
            ++indent_;
            // True
            if (node.outputs.size() >= 1) {
                int tp = FindLinkedInput(graph, node.outputs[0].id);
                if (tp >= 0) { const BpNode* tn = FindPinOwner(graph, tp); if (tn) CompileFlowNode(out, graph, *tn); }
            }
            --indent_;
            out << Indent() << "else\n";
            ++indent_;
            // False
            if (node.outputs.size() >= 2) {
                int fp = FindLinkedInput(graph, node.outputs[1].id);
                if (fp >= 0) { const BpNode* fn = FindPinOwner(graph, fp); if (fn) CompileFlowNode(out, graph, *fn); }
            }
            --indent_;
            out << Indent() << "end\n";
            return;
        } else if (node.name == "For Loop") {
            std::string start_v = InlineExpr(graph, node, 1);
            std::string end_v = InlineExpr(graph, node, 2);
            std::string idx_var = FreshVar();
            out << Indent() << "for " << idx_var << " = " << start_v << ", " << end_v << " do\n";
            ++indent_;
            if (node.outputs.size() >= 1) {
                int bp_ = FindLinkedInput(graph, node.outputs[0].id);
                if (bp_ >= 0) { const BpNode* bn = FindPinOwner(graph, bp_); if (bn) CompileFlowNode(out, graph, *bn); }
            }
            --indent_;
            out << Indent() << "end\n";
            // Done
            if (node.outputs.size() >= 3) {
                int dp = FindLinkedInput(graph, node.outputs[2].id);
                if (dp >= 0) { const BpNode* dn = FindPinOwner(graph, dp); if (dn) CompileFlowNode(out, graph, *dn); }
            }
            return;
        } else if (node.name == "Print") {
            out << Indent() << "print(" << InlineExpr(graph, node, 1) << ")\n";
        } else if (node.name == "Set Position") {
            out << Indent() << "ecs.set_position(" << InlineExpr(graph, node, 1) << ", " << InlineExpr(graph, node, 2) << ")\n";
        } else if (node.name == "Create Entity") {
            std::string var = FreshVar();
            out << Indent() << "local " << var << " = ecs.create_entity(" << InlineExpr(graph, node, 1) << ")\n";
        } else if (node.name == "Set Variable") {
            out << Indent() << "self." << node.comment << " = " << InlineExpr(graph, node, 1) << "\n";
        } else if (node.name == "Delay") {
            out << Indent() << "coroutine.yield(" << InlineExpr(graph, node, 1) << ")\n";
        } else if (node.name == "Play Sound") {
            out << Indent() << "audio.play(" << InlineExpr(graph, node, 1) << ")\n";
        } else {
            // Generic: use code_template
            if (!node.code_template.empty()) {
                std::string line = node.code_template;
                for (size_t i = 0; i < node.inputs.size(); ++i) {
                    std::string placeholder = "{input" + std::to_string(i) + "}";
                    size_t pos = line.find(placeholder);
                    if (pos != std::string::npos) {
                        line.replace(pos, placeholder.size(), InlineExpr(graph, node, static_cast<int>(i)));
                    }
                }
                out << Indent() << line << "\n";
            }
        }

        // Follow flow output
        for (const auto& pin : node.outputs) {
            if (pin.type == BpPinType::Flow && pin.name != "False") {
                int np = FindLinkedInput(graph, pin.id);
                if (np >= 0) { const BpNode* nn = FindPinOwner(graph, np); if (nn) CompileFlowNode(out, graph, *nn); }
                break;
            }
        }
    }

    std::string InlineExpr(const BpFunctionGraph& graph, const BpNode& node, int input_idx) {
        if (input_idx < 0 || input_idx >= static_cast<int>(node.inputs.size())) return "nil";
        const BpPin& pin = node.inputs[input_idx];
        if (pin.type == BpPinType::Flow) return "nil";

        int src_pin = FindLinkedOutput(graph, pin.id);
        if (src_pin >= 0) {
            const BpNode* src = FindPinOwner(graph, src_pin);
            if (src) return InlineDataNode(graph, *src);
        }

        // Default value
        switch (pin.type) {
            case BpPinType::Float: return std::to_string(pin.default_float);
            case BpPinType::Int:   return std::to_string(pin.default_int);
            case BpPinType::Bool:  return pin.default_bool ? "true" : "false";
            case BpPinType::String:return std::string("\"") + pin.default_string + "\"";
            default: return "nil";
        }
    }

    std::string InlineDataNode(const BpFunctionGraph& graph, const BpNode& node) {
        if (node.name == "Add") return "(" + InlineExpr(graph, node, 0) + " + " + InlineExpr(graph, node, 1) + ")";
        if (node.name == "Subtract") return "(" + InlineExpr(graph, node, 0) + " - " + InlineExpr(graph, node, 1) + ")";
        if (node.name == "Multiply") return "(" + InlineExpr(graph, node, 0) + " * " + InlineExpr(graph, node, 1) + ")";
        if (node.name == "Divide") return "(" + InlineExpr(graph, node, 0) + " / " + InlineExpr(graph, node, 1) + ")";
        if (node.name == "Sin") return "math.sin(" + InlineExpr(graph, node, 0) + ")";
        if (node.name == "Cos") return "math.cos(" + InlineExpr(graph, node, 0) + ")";
        if (node.name == "Sqrt") return "math.sqrt(" + InlineExpr(graph, node, 0) + ")";
        if (node.name == "Abs") return "math.abs(" + InlineExpr(graph, node, 0) + ")";
        if (node.name == "Negate") return "(-" + InlineExpr(graph, node, 0) + ")";
        if (node.name == "Get Position") return "ecs.get_position(" + InlineExpr(graph, node, 0) + ")";
        if (node.name == "Self Entity") return "self_entity";
        if (node.name == "Float Constant" || node.name == "Constant Float") {
            return node.outputs.empty() ? "0" : std::to_string(node.outputs[0].default_float);
        }
        if (node.name == "Int Constant" || node.name == "Constant Int") {
            return node.outputs.empty() ? "0" : std::to_string(node.outputs[0].default_int);
        }
        if (node.name == "Get Variable") return "self." + node.comment;
        if (node.name == "Bool Constant") {
            return (node.outputs.empty() ? false : node.outputs[0].default_bool) ? "true" : "false";
        }
        if (node.name == "Random Float") return "math.random()";
        if (node.name == "Delta Time") return "dt";
        return "nil";
    }
};

} // anonymous namespace

// ─── Public API ────────────────────────────────────────────────────────────

CompiledBlueprint CompileToByteCode(const BlueprintAsset& asset) {
    CompiledBlueprint result;
    result.version = asset.version;

    // Default variable values
    for (const auto& var : asset.variables) {
        switch (var.type) {
            case BpVarType::Bool:   result.default_variables.push_back(BpValue::Bool(var.default_bool)); break;
            case BpVarType::Int:    result.default_variables.push_back(BpValue::Int(var.default_int)); break;
            case BpVarType::Float:  result.default_variables.push_back(BpValue::Float(var.default_float)); break;
            case BpVarType::String: result.default_variables.push_back(BpValue::String(var.default_string)); break;
            case BpVarType::Vec3:   result.default_variables.push_back(BpValue::Vec3(var.default_vec[0], var.default_vec[1], var.default_vec[2])); break;
            default:                result.default_variables.push_back(BpValue()); break;
        }
    }

    // Compile each graph
    for (const auto& graph : asset.graphs) {
        result.functions.push_back(CompileFunctionGraph(graph, asset.variables));
    }

    return result;
}

std::string CompileToLua(const BlueprintAsset& asset) {
    LuaCompiler compiler(asset);
    return compiler.Compile();
}

CompiledFunction CompileFunctionGraph(const BpFunctionGraph& graph,
                                      const std::vector<BpVariable>& variables) {
    ByteCodeCompiler compiler(graph, variables);
    return compiler.Compile();
}

ValidationResult ValidateGraph(const BpFunctionGraph& graph) {
    ValidationResult result;

    // Check for unconnected flow outputs from event nodes
    for (const auto& node : graph.nodes) {
        if (node.category == "Event") {
            bool has_flow_connection = false;
            for (const auto& pin : node.outputs) {
                if (pin.type == BpPinType::Flow) {
                    for (const auto& link : graph.links) {
                        if (link.from_pin == pin.id) { has_flow_connection = true; break; }
                    }
                }
            }
            if (!has_flow_connection) {
                result.warnings.push_back("Event '" + node.name + "' has no connected flow output");
            }
        }
    }

    // Check for type mismatches in links
    for (const auto& link : graph.links) {
        const BpPin* from = FindPin(graph, link.from_pin);
        const BpPin* to = FindPin(graph, link.to_pin);
        if (from && to) {
            if (from->type != to->type && from->type != BpPinType::Any && to->type != BpPinType::Any
                && from->type != BpPinType::Wildcard && to->type != BpPinType::Wildcard) {
                result.errors.push_back(std::string("Type mismatch in link: ") + BpPinTypeName(from->type) +
                    " -> " + BpPinTypeName(to->type));
                result.valid = false;
            }
        }
    }

    return result;
}

}  // namespace dse::editor::bp
