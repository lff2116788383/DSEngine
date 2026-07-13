/**
 * @file blueprint_compiler.cpp
 * @brief 运行时 .dbp 加载 + 图→字节码编译实现（引擎侧）
 */

#include "engine/scripting/blueprint/blueprint_compiler.h"
#include "engine/scripting/blueprint/blueprint_serialize.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cstdio>
#include <cstring>

#include <rapidjson/document.h>

#include "engine/base/debug.h"

namespace dse::bp {

BpVarType BpVarTypeFromName(const char* name) {
    if (!name) return BpVarType::Float;
    std::string s = name;
    if (s == "Bool")   return BpVarType::Bool;
    if (s == "Int")    return BpVarType::Int;
    if (s == "Float")  return BpVarType::Float;
    if (s == "String") return BpVarType::String;
    if (s == "Vec2")   return BpVarType::Vec2;
    if (s == "Vec3")   return BpVarType::Vec3;
    if (s == "Vec4")   return BpVarType::Vec4;
    if (s == "Entity") return BpVarType::Entity;
    if (s == "Array")  return BpVarType::Array;
    return BpVarType::Float;
}

namespace {

// ─── Register allocator ─────────────────────────────────────────────────────

class RegAlloc {
public:
    int Alloc() { return next_++; }
    int Count() const { return next_; }
private:
    int next_ = 0;
};

int FindLinkedOutput(const BpFunctionGraph& graph, int input_pin_id) {
    for (const auto& l : graph.links) if (l.to_pin == input_pin_id) return l.from_pin;
    return -1;
}
int FindLinkedInput(const BpFunctionGraph& graph, int output_pin_id) {
    for (const auto& l : graph.links) if (l.from_pin == output_pin_id) return l.to_pin;
    return -1;
}
const BpNode* FindPinOwner(const BpFunctionGraph& graph, int pin_id) {
    for (const auto& n : graph.nodes) {
        for (const auto& p : n.inputs) if (p.id == pin_id) return &n;
        for (const auto& p : n.outputs) if (p.id == pin_id) return &n;
    }
    return nullptr;
}

// ─── ByteCode compiler（与编辑器 ByteCodeCompiler 对齐）──────────────────────

class ByteCodeCompiler {
public:
    ByteCodeCompiler(const BpFunctionGraph& graph, const std::vector<BpVariable>& vars,
                     const BpNode* entry_node = nullptr, std::string function_name = {},
                     int num_params = -1)
        : graph_(graph),
          vars_(vars),
          entry_node_(entry_node),
          function_name_(std::move(function_name)),
          num_params_(num_params) {}

    CompiledFunction Compile() {
        CompiledFunction func;
        func.name = function_name_.empty() ? graph_.name : function_name_;
        func.num_params = num_params_ >= 0 ? num_params_ : static_cast<int>(graph_.input_params.size());
        for (int i = 0; i < func.num_params; ++i) regs_.Alloc();

        if (entry_node_) {
            int param_index = 0;
            for (const auto& output : entry_node_->outputs) {
                if (output.type != BpPinType::Flow && param_index < func.num_params) {
                    pin_to_reg_[output.id] = param_index++;
                }
            }
            CompileFlowFrom(*entry_node_);
        } else {
            const BpNode* function_entry = nullptr;
            for (const auto& node : graph_.nodes) {
                if (node.name == "Function Entry") {
                    function_entry = &node;
                    break;
                }
            }
            if (function_entry) {
                int param_index = 0;
                for (const auto& output : function_entry->outputs) {
                    if (output.type != BpPinType::Flow && param_index < func.num_params) {
                        pin_to_reg_[output.id] = param_index++;
                    }
                }
                CompileFlowFrom(*function_entry);
            } else {
                for (const auto& node : graph_.nodes) {
                    if (node.category == "Event" || node.category == "Network") CompileFlowFrom(node);
                }
            }
        }
        if (code_.empty()) {
            for (const auto& node : graph_.nodes) {
                if (!node.outputs.empty() && node.outputs[0].type != BpPinType::Flow) {
                    CompileDataNode(node);
                }
            }
        }
        current_node_id_ = -1;
        Emit(OpCode::Halt);
        func.code = std::move(code_);
        func.constants = std::move(constants_);
        func.source_nodes = std::move(source_nodes_);
        func.num_registers = regs_.Count();
        return func;
    }

private:
    const BpFunctionGraph& graph_;
    const std::vector<BpVariable>& vars_;
    const BpNode* entry_node_ = nullptr;
    std::string function_name_;
    int num_params_ = -1;
    RegAlloc regs_;
    std::vector<Instruction> code_;
    std::vector<BpValue> constants_;
    std::vector<int> source_nodes_;
    std::unordered_map<int, int> pin_to_reg_;
    int current_node_id_ = -1;

    // RAII: attribute all instructions emitted within its scope to `node_id`,
    // restoring the previous attribution on exit (handles nested data nodes).
    struct NodeScope {
        ByteCodeCompiler* c;
        int prev;
        NodeScope(ByteCodeCompiler* comp, int node_id) : c(comp), prev(comp->current_node_id_) {
            c->current_node_id_ = node_id;
        }
        ~NodeScope() { c->current_node_id_ = prev; }
    };

    void Emit(OpCode op, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, int16_t extra = 0) {
        code_.push_back({op, a, b, c, extra});
        source_nodes_.push_back(current_node_id_);
    }
    int AddConstant(const BpValue& v) {
        constants_.push_back(v);
        return static_cast<int>(constants_.size() - 1);
    }
    int GetVarIndex(const std::string& name) const {
        for (size_t i = 0; i < vars_.size(); ++i) if (vars_[i].name == name) return static_cast<int>(i);
        return -1;
    }

    void CompileFlowFrom(const BpNode& event_node) {
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
        NodeScope ns(this, node.id);
        for (const auto& pin : node.inputs) {
            if (pin.type != BpPinType::Flow) {
                int src_pin = FindLinkedOutput(graph_, pin.id);
                if (src_pin >= 0) {
                    const BpNode* src_node = FindPinOwner(graph_, src_pin);
                    if (src_node) CompileDataNode(*src_node);
                }
            }
        }

        if (node.name == "Branch") { CompileBranch(node); return; }
        else if (node.name == "For Loop") { CompileForLoop(node); return; }
        else if (node.name == "Print") {
            int msg_reg = GetInputReg(node, 1);
            Emit(OpCode::Print, static_cast<uint8_t>(msg_reg));
        } else if (node.name == "Set Position") {
            int entity_reg = GetInputReg(node, 1);
            int pos_reg = GetInputReg(node, 2);
            Emit(OpCode::EcsSetVec3, static_cast<uint8_t>(entity_reg), 0, static_cast<uint8_t>(pos_reg));
        } else if (node.name == "Create Entity") {
            int result_reg = regs_.Alloc();
            if (node.outputs.size() > 1) pin_to_reg_[node.outputs[1].id] = result_reg;
            int const_idx = AddConstant(BpValue::Entity(0));
            Emit(OpCode::LoadConst, static_cast<uint8_t>(result_reg), static_cast<uint8_t>(const_idx));
        } else if (node.name == "Set Variable") {
            int val_reg = GetInputReg(node, 1);
            int var_idx = GetVarIndex(node.comment);
            if (var_idx < 0) var_idx = 0;
            Emit(OpCode::StoreVar, static_cast<uint8_t>(var_idx), static_cast<uint8_t>(val_reg));
        } else {
            CompileGenericFlowNode(node);
        }

        for (const auto& pin : node.outputs) {
            if (pin.type == BpPinType::Flow && pin.name != "False") {
                int next_pin = FindLinkedInput(graph_, pin.id);
                if (next_pin >= 0) {
                    const BpNode* next_node = FindPinOwner(graph_, next_pin);
                    if (next_node) CompileFlowNode(*next_node);
                }
                break;
            }
        }
    }

    void CompileBranch(const BpNode& node) {
        int cond_reg = GetInputReg(node, 1);
        size_t jump_else_idx = code_.size();
        Emit(OpCode::JumpIfFalse, static_cast<uint8_t>(cond_reg), 0, 0, 0);

        if (node.outputs.size() >= 1) {
            int true_pin = FindLinkedInput(graph_, node.outputs[0].id);
            if (true_pin >= 0) { const BpNode* n = FindPinOwner(graph_, true_pin); if (n) CompileFlowNode(*n); }
        }
        size_t jump_end_idx = code_.size();
        Emit(OpCode::Jump, 0, 0, 0, 0);
        code_[jump_else_idx].extra = static_cast<int16_t>(code_.size() - jump_else_idx - 1);

        if (node.outputs.size() >= 2) {
            int false_pin = FindLinkedInput(graph_, node.outputs[1].id);
            if (false_pin >= 0) { const BpNode* n = FindPinOwner(graph_, false_pin); if (n) CompileFlowNode(*n); }
        }
        code_[jump_end_idx].extra = static_cast<int16_t>(code_.size() - jump_end_idx - 1);
    }

    void CompileForLoop(const BpNode& node) {
        int start_reg = GetInputReg(node, 1);
        int end_reg = GetInputReg(node, 2);
        int idx_reg = regs_.Alloc();
        if (node.outputs.size() >= 2) pin_to_reg_[node.outputs[1].id] = idx_reg;

        Emit(OpCode::Move, static_cast<uint8_t>(idx_reg), static_cast<uint8_t>(start_reg));
        size_t loop_start = code_.size();
        int cmp_reg = regs_.Alloc();
        Emit(OpCode::CmpLe, static_cast<uint8_t>(cmp_reg), static_cast<uint8_t>(idx_reg), static_cast<uint8_t>(end_reg));
        size_t jump_exit_idx = code_.size();
        Emit(OpCode::JumpIfFalse, static_cast<uint8_t>(cmp_reg), 0, 0, 0);

        if (node.outputs.size() >= 1) {
            int body_pin = FindLinkedInput(graph_, node.outputs[0].id);
            if (body_pin >= 0) { const BpNode* n = FindPinOwner(graph_, body_pin); if (n) CompileFlowNode(*n); }
        }

        int one_const = AddConstant(BpValue::Float(1.0f));
        int one_reg = regs_.Alloc();
        Emit(OpCode::LoadConst, static_cast<uint8_t>(one_reg), static_cast<uint8_t>(one_const));
        Emit(OpCode::Add, static_cast<uint8_t>(idx_reg), static_cast<uint8_t>(idx_reg), static_cast<uint8_t>(one_reg));

        int jump_back = static_cast<int>(loop_start) - static_cast<int>(code_.size()) - 1;
        Emit(OpCode::Jump, 0, 0, 0, static_cast<int16_t>(jump_back));
        code_[jump_exit_idx].extra = static_cast<int16_t>(code_.size() - jump_exit_idx - 1);

        if (node.outputs.size() >= 3) {
            int done_pin = FindLinkedInput(graph_, node.outputs[2].id);
            if (done_pin >= 0) { const BpNode* n = FindPinOwner(graph_, done_pin); if (n) CompileFlowNode(*n); }
        }
    }

    int CompileDataNode(const BpNode& node) {
        if (!node.outputs.empty()) {
            auto it = pin_to_reg_.find(node.outputs[0].id);
            if (it != pin_to_reg_.end()) return it->second;
        }
        NodeScope ns(this, node.id);
        if (node.name == "Delta Time" && function_name_ == "on_update" && num_params_ > 0) {
            if (!node.outputs.empty()) pin_to_reg_[node.outputs[0].id] = 0;
            return 0;
        }

        // Break Vec3 produces up to three scalar outputs; map each output pin to
        // its own register via a VecComponent extract from the single Vec3 input.
        if (node.name == "Break Vec3") {
            int vec_reg = GetInputReg(node, 0);
            int first = -1;
            for (size_t i = 0; i < node.outputs.size() && i < 3; ++i) {
                int comp_reg = regs_.Alloc();
                Emit(OpCode::VecComponent, (uint8_t)comp_reg, (uint8_t)vec_reg, (uint8_t)i);
                pin_to_reg_[node.outputs[i].id] = comp_reg;
                if (i == 0) first = comp_reg;
            }
            return first >= 0 ? first : regs_.Alloc();
        }

        int result_reg = regs_.Alloc();

        if (node.name == "Add") {
            Emit(OpCode::Add, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Subtract") {
            Emit(OpCode::Sub, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Multiply") {
            Emit(OpCode::Mul, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Divide") {
            Emit(OpCode::Div, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Sin") {
            Emit(OpCode::Sin, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0));
        } else if (node.name == "Cos") {
            Emit(OpCode::Cos, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0));
        } else if (node.name == "Sqrt") {
            Emit(OpCode::Sqrt, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0));
        } else if (node.name == "Abs") {
            Emit(OpCode::Abs, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0));
        } else if (node.name == "Negate") {
            Emit(OpCode::Neg, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0));
        } else if (node.name == "Modulo") {
            Emit(OpCode::Mod, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Power") {
            Emit(OpCode::Pow, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Atan2") {
            Emit(OpCode::Atan2, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Min") {
            Emit(OpCode::Min, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Max") {
            Emit(OpCode::Max, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Clamp") {
            int val = GetInputReg(node, 0), lo = GetInputReg(node, 1), hi = GetInputReg(node, 2);
            Emit(OpCode::Clamp, (uint8_t)result_reg, (uint8_t)val, (uint8_t)lo, (int16_t)hi);
        } else if (node.name == "Lerp") {
            // result = A + (B - A) * Alpha, composed from scalar opcodes.
            int a = GetInputReg(node, 0), b = GetInputReg(node, 1), t = GetInputReg(node, 2);
            int diff = regs_.Alloc();
            Emit(OpCode::Sub, (uint8_t)diff, (uint8_t)b, (uint8_t)a);
            int scaled = regs_.Alloc();
            Emit(OpCode::Mul, (uint8_t)scaled, (uint8_t)diff, (uint8_t)t);
            Emit(OpCode::Add, (uint8_t)result_reg, (uint8_t)a, (uint8_t)scaled);
        } else if (node.name == "Equal") {
            Emit(OpCode::CmpEq, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Not Equal") {
            Emit(OpCode::CmpEq, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
            Emit(OpCode::Not, (uint8_t)result_reg, (uint8_t)result_reg);
        } else if (node.name == "Less Than") {
            Emit(OpCode::CmpLt, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Greater Than") {
            // A > B  ==  B < A
            int a = GetInputReg(node, 0), b = GetInputReg(node, 1);
            Emit(OpCode::CmpLt, (uint8_t)result_reg, (uint8_t)b, (uint8_t)a);
        } else if (node.name == "And") {
            Emit(OpCode::And, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Or") {
            Emit(OpCode::Or, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Not") {
            Emit(OpCode::Not, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0));
        } else if (node.name == "Make Vec3") {
            int x = GetInputReg(node, 0), y = GetInputReg(node, 1), z = GetInputReg(node, 2);
            Emit(OpCode::MakeVec3, (uint8_t)result_reg, (uint8_t)x, (uint8_t)y, (int16_t)z);
        } else if (node.name == "Vec3 Add") {
            Emit(OpCode::Vec3Add, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Vec3 Scale") {
            Emit(OpCode::Vec3Scale, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Vec3 Dot") {
            Emit(OpCode::Vec3Dot, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Vec3 Normalize") {
            Emit(OpCode::Vec3Normalize, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0));
        } else if (node.name == "Vec3 Length") {
            int v = GetInputReg(node, 0);
            int dot = regs_.Alloc();
            Emit(OpCode::Vec3Dot, (uint8_t)dot, (uint8_t)v, (uint8_t)v);
            Emit(OpCode::Sqrt, (uint8_t)result_reg, (uint8_t)dot);
        } else if (node.name == "Vec3 Distance") {
            int a = GetInputReg(node, 0), b = GetInputReg(node, 1);
            int diff = regs_.Alloc();
            Emit(OpCode::Vec3Sub, (uint8_t)diff, (uint8_t)a, (uint8_t)b);
            int dot = regs_.Alloc();
            Emit(OpCode::Vec3Dot, (uint8_t)dot, (uint8_t)diff, (uint8_t)diff);
            Emit(OpCode::Sqrt, (uint8_t)result_reg, (uint8_t)dot);
        } else if (node.name == "Array Get") {
            Emit(OpCode::ArrayGet, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), (uint8_t)GetInputReg(node, 1));
        } else if (node.name == "Array Length") {
            Emit(OpCode::ArrayLen, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0));
        } else if (node.name == "Get Position") {
            Emit(OpCode::EcsGetVec3, (uint8_t)result_reg, (uint8_t)GetInputReg(node, 0), 0);
        } else if (node.name == "Self Entity") {
            int const_idx = AddConstant(BpValue::Entity(0));
            Emit(OpCode::LoadConst, (uint8_t)result_reg, (uint8_t)const_idx);
        } else if (node.name == "Float Constant" || node.name == "Constant Float") {
            float val = node.outputs.empty() ? 0.0f : node.outputs[0].default_float;
            Emit(OpCode::LoadConst, (uint8_t)result_reg, (uint8_t)AddConstant(BpValue::Float(val)));
        } else if (node.name == "Int Constant" || node.name == "Constant Int") {
            int val = node.outputs.empty() ? 0 : node.outputs[0].default_int;
            Emit(OpCode::LoadConst, (uint8_t)result_reg, (uint8_t)AddConstant(BpValue::Int(val)));
        } else if (node.name == "Bool Constant") {
            bool val = node.outputs.empty() ? false : node.outputs[0].default_bool;
            Emit(OpCode::LoadConst, (uint8_t)result_reg, (uint8_t)AddConstant(BpValue::Bool(val)));
        } else if (node.name == "String Constant") {
            std::string val = node.outputs.empty() ? std::string() : node.outputs[0].default_string;
            Emit(OpCode::LoadConst, (uint8_t)result_reg, (uint8_t)AddConstant(BpValue::String(val)));
        } else if (node.name == "Get Variable") {
            int var_idx = GetVarIndex(node.comment);
            if (var_idx >= 0) Emit(OpCode::LoadVar, (uint8_t)result_reg, (uint8_t)var_idx);
        } else {
            // Unrecognised data node: route to a named external function instead of
            // silently loading 0.0f. Hosts register the implementation by node name;
            // an unresolved call leaves the result Nil rather than a deceptive value.
            EmitExternDataCall(node, result_reg);
        }

        if (!node.outputs.empty()) pin_to_reg_[node.outputs[0].id] = result_reg;
        return result_reg;
    }

    // Emit a CallExtern for a pure-data node whose result register is already
    // allocated. Arguments are materialised into a contiguous register block.
    void EmitExternDataCall(const BpNode& node, int result_reg) {
        std::vector<int> in_regs;
        for (size_t i = 0; i < node.inputs.size(); ++i)
            if (node.inputs[i].type != BpPinType::Flow)
                in_regs.push_back(GetInputReg(node, static_cast<int>(i)));
        int arg_start = regs_.Alloc();
        for (size_t i = 0; i < in_regs.size(); ++i) {
            if (i > 0) regs_.Alloc();
            Emit(OpCode::Move, (uint8_t)(arg_start + i), (uint8_t)in_regs[i]);
        }
        int name_const = AddConstant(BpValue::String(node.name));
        Emit(OpCode::CallExtern, (uint8_t)result_reg, (uint8_t)name_const,
             (uint8_t)arg_start, (int16_t)in_regs.size());
    }

    int GetInputReg(const BpNode& node, int input_index) {
        if (input_index < 0 || input_index >= static_cast<int>(node.inputs.size())) {
            int r = regs_.Alloc();
            Emit(OpCode::LoadConst, (uint8_t)r, (uint8_t)AddConstant(BpValue::Float(0.0f)));
            return r;
        }
        const BpPin& pin = node.inputs[input_index];
        if (pin.type == BpPinType::Flow) {
            int r = regs_.Alloc();
            Emit(OpCode::LoadConst, (uint8_t)r, (uint8_t)AddConstant(BpValue::Float(0.0f)));
            return r;
        }
        int src_pin = FindLinkedOutput(graph_, pin.id);
        if (src_pin >= 0) {
            auto it = pin_to_reg_.find(src_pin);
            if (it != pin_to_reg_.end()) return it->second;
            const BpNode* src_node = FindPinOwner(graph_, src_pin);
            if (src_node) return CompileDataNode(*src_node);
        }
        BpValue default_value;
        switch (pin.type) {
            case BpPinType::Bool:   default_value = BpValue::Bool(pin.default_bool); break;
            case BpPinType::Int:    default_value = BpValue::Int(pin.default_int); break;
            case BpPinType::String: default_value = BpValue::String(pin.default_string); break;
            case BpPinType::Vec3:
                default_value = BpValue::Vec3(pin.default_vec[0], pin.default_vec[1], pin.default_vec[2]);
                break;
            default: default_value = BpValue::Float(pin.default_float); break;
        }
        int r = regs_.Alloc();
        Emit(OpCode::LoadConst, (uint8_t)r, (uint8_t)AddConstant(default_value));
        return r;
    }

    void CompileGenericFlowNode(const BpNode& node) {
        int num_data_inputs = 0;
        int arg_start = regs_.Alloc();
        for (size_t i = 0; i < node.inputs.size(); ++i) {
            if (node.inputs[i].type != BpPinType::Flow) {
                int r = GetInputReg(node, static_cast<int>(i));
                if (num_data_inputs > 0) regs_.Alloc();
                Emit(OpCode::Move, (uint8_t)(arg_start + num_data_inputs), (uint8_t)r);
                ++num_data_inputs;
            }
        }
        int result_reg = regs_.Alloc();
        int name_const = AddConstant(BpValue::String(node.name));
        Emit(OpCode::CallExtern, (uint8_t)result_reg, (uint8_t)name_const,
             (uint8_t)arg_start, (int16_t)num_data_inputs);
        if (!node.outputs.empty() && node.outputs.back().type != BpPinType::Flow)
            pin_to_reg_[node.outputs.back().id] = result_reg;
    }
};

CompiledFunction CompileFunctionGraphImpl(const BpFunctionGraph& graph,
                                          const std::vector<BpVariable>& vars) {
    ByteCodeCompiler compiler(graph, vars);
    return compiler.Compile();
}

}  // namespace

CompiledFunction CompileFunctionGraph(const BpFunctionGraph& graph,
                                      const std::vector<BpVariable>& variables) {
    return CompileFunctionGraphImpl(graph, variables);
}

CompiledBlueprint CompileToByteCode(const BlueprintAsset& asset) {
    CompiledBlueprint result;
    result.version = asset.version;
    result.bytecode_version = kBytecodeVersion;
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
    for (const auto& graph : asset.graphs) {
        if (graph.name != "EventGraph") {
            result.functions.push_back(CompileFunctionGraphImpl(graph, asset.variables));
            continue;
        }

        for (const auto& node : graph.nodes) {
            std::string function_name;
            int num_params = 0;
            if (node.name == "On Init") {
                function_name = "on_init";
            } else if (node.name == "On Update") {
                function_name = "on_update";
                num_params = 1;
            } else if (node.name == "On Net Connected") {
                function_name = "on_net_connected";
                num_params = 1;
            } else if (node.name == "On Net Disconnected") {
                function_name = "on_net_disconnected";
                num_params = 2;
            } else if (node.name == "On Net Message") {
                function_name = "on_net_message";
                num_params = 2;
            } else if (node.name == "On RPC Received") {
                function_name = "on_rpc_received";
                num_params = 3;
            } else {
                continue;
            }

            ByteCodeCompiler compiler(graph, asset.variables, &node, function_name, num_params);
            result.functions.push_back(compiler.Compile());
        }
    }
    return result;
}

bool LoadBlueprintAsset(BlueprintAsset& asset, const std::string& path) {
    BlueprintDiagnostics diag;
    return LoadBlueprintAssetChecked(asset, path, diag);
}

bool ValidateCompiledFunction(const CompiledFunction& fn, std::string& error) {
    const int nregs  = fn.num_registers;
    const int nconst = static_cast<int>(fn.constants.size());
    const int ncode  = static_cast<int>(fn.code.size());
    auto reg_ok = [&](int r) { return r >= 0 && r < nregs; };

    for (int pc = 0; pc < ncode; ++pc) {
        const Instruction& in = fn.code[pc];
        auto fail = [&](const char* what) {
            error = fn.name + ": instruction " + std::to_string(pc) + ": " + what;
            return false;
        };
        auto need = [&](bool cond, const char* what) { return cond ? true : fail(what); };

        switch (in.op) {
            case OpCode::Nop:
            case OpCode::Halt:
                break;

            case OpCode::LoadConst:
                if (!need(reg_ok(in.a), "LoadConst dst register out of range")) return false;
                if (!need(in.b < nconst, "LoadConst constant index out of range")) return false;
                break;
            case OpCode::LoadVar:
                if (!need(reg_ok(in.a), "LoadVar dst register out of range")) return false;
                break;
            case OpCode::StoreVar:
                if (!need(reg_ok(in.b), "StoreVar src register out of range")) return false;
                break;

            // unary: a = f(b)
            case OpCode::Move: case OpCode::Neg: case OpCode::Not:
            case OpCode::Sin: case OpCode::Cos: case OpCode::Sqrt: case OpCode::Abs:
            case OpCode::Vec3Normalize: case OpCode::ArrayLen:
            case OpCode::VecComponent:
                if (!need(reg_ok(in.a) && reg_ok(in.b), "unary register operand out of range")) return false;
                break;

            // binary: a = f(b, c)
            case OpCode::Add: case OpCode::Sub: case OpCode::Mul: case OpCode::Div:
            case OpCode::Mod: case OpCode::Pow: case OpCode::Atan2:
            case OpCode::Min: case OpCode::Max:
            case OpCode::CmpEq: case OpCode::CmpLt: case OpCode::CmpLe:
            case OpCode::And: case OpCode::Or:
            case OpCode::Vec3Add: case OpCode::Vec3Sub: case OpCode::Vec3Scale:
            case OpCode::Vec3Dot: case OpCode::Concat:
            case OpCode::ArrayGet: case OpCode::ArraySet: case OpCode::ArrayPush:
                if (!need(reg_ok(in.a) && reg_ok(in.b) && reg_ok(in.c),
                          "binary register operand out of range")) return false;
                break;

            // ternary with extra register: a = f(b, c, extra)
            case OpCode::Clamp: case OpCode::MakeVec3:
                if (!need(reg_ok(in.a) && reg_ok(in.b) && reg_ok(in.c) && reg_ok(in.extra),
                          "register operand out of range")) return false;
                break;

            case OpCode::EcsGetFloat: case OpCode::EcsGetVec3:
                if (!need(reg_ok(in.a) && reg_ok(in.b), "ECS get register out of range")) return false;
                break;
            case OpCode::EcsSetFloat: case OpCode::EcsSetVec3:
                if (!need(reg_ok(in.a) && reg_ok(in.c), "ECS set register out of range")) return false;
                break;

            case OpCode::Print: case OpCode::Return:
                if (!need(reg_ok(in.a), "register operand out of range")) return false;
                break;

            case OpCode::Jump:
            case OpCode::JumpIfFalse:
            case OpCode::JumpIfTrue: {
                if (in.op != OpCode::Jump && !need(reg_ok(in.a), "jump condition register out of range"))
                    return false;
                int target = pc + 1 + in.extra;  // VM advances pc before applying extra
                if (!need(target >= 0 && target <= ncode, "jump target out of range")) return false;
                break;
            }

            case OpCode::CallExtern: {
                if (!need(reg_ok(in.a), "CallExtern dst register out of range")) return false;
                if (!need(in.b < nconst && fn.constants[in.b].type == BpValue::Type::String,
                          "CallExtern name constant invalid")) return false;
                if (!need(in.extra >= 0 && reg_ok(in.c) &&
                          (in.extra == 0 || reg_ok(in.c + in.extra - 1)),
                          "CallExtern argument register range out of range")) return false;
                break;
            }
            case OpCode::Call: {
                if (!need(in.b >= 0 && reg_ok(in.c) && (in.b == 0 || reg_ok(in.c + in.b - 1)),
                          "Call argument register range out of range")) return false;
                break;
            }

            default:
                break;
        }
    }
    return true;
}

bool ValidateCompiledBlueprint(const CompiledBlueprint& bp, std::string& error) {
    if (bp.bytecode_version > kBytecodeVersion) {
        error = "compiled blueprint bytecode version " + std::to_string(bp.bytecode_version) +
                " is newer than supported version " + std::to_string(kBytecodeVersion);
        return false;
    }
    for (const auto& fn : bp.functions) {
        if (!ValidateCompiledFunction(fn, error)) return false;
    }
    return true;
}

}  // namespace dse::bp
