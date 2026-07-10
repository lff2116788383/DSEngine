/**
 * @file blueprint_compiler.cpp
 * @brief 运行时 .dbp 加载 + 图→字节码编译实现（引擎侧）
 */

#include "engine/scripting/blueprint/blueprint_compiler.h"

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

static BpPinType BpPinTypeFromName(const char* name) {
    if (!name) return BpPinType::Any;
    std::string s = name;
    if (s == "Flow")     return BpPinType::Flow;
    if (s == "Bool")     return BpPinType::Bool;
    if (s == "Int")      return BpPinType::Int;
    if (s == "Float")    return BpPinType::Float;
    if (s == "String")   return BpPinType::String;
    if (s == "Vec2")     return BpPinType::Vec2;
    if (s == "Vec3")     return BpPinType::Vec3;
    if (s == "Vec4")     return BpPinType::Vec4;
    if (s == "Entity")   return BpPinType::Entity;
    if (s == "Array")    return BpPinType::Array;
    if (s == "Wildcard") return BpPinType::Wildcard;
    return BpPinType::Any;
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
    ByteCodeCompiler(const BpFunctionGraph& graph, const std::vector<BpVariable>& vars)
        : graph_(graph), vars_(vars) {}

    CompiledFunction Compile() {
        CompiledFunction func;
        func.name = graph_.name;
        func.num_params = static_cast<int>(graph_.input_params.size());
        for (int i = 0; i < func.num_params; ++i) regs_.Alloc();

        for (const auto& node : graph_.nodes) {
            if (node.category == "Event") CompileFlowFrom(node);
        }
        if (code_.empty()) {
            for (const auto& node : graph_.nodes) {
                if (!node.outputs.empty() && node.outputs[0].type != BpPinType::Flow) {
                    CompileDataNode(node);
                }
            }
        }
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
    std::unordered_map<int, int> pin_to_reg_;

    void Emit(OpCode op, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, int16_t extra = 0) {
        code_.push_back({op, a, b, c, extra});
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
        } else if (node.name == "Get Variable") {
            int var_idx = GetVarIndex(node.comment);
            if (var_idx >= 0) Emit(OpCode::LoadVar, (uint8_t)result_reg, (uint8_t)var_idx);
        } else if (node.name == "Bool Constant") {
            bool val = node.outputs.empty() ? false : node.outputs[0].default_bool;
            Emit(OpCode::LoadConst, (uint8_t)result_reg, (uint8_t)AddConstant(BpValue::Bool(val)));
        } else {
            Emit(OpCode::LoadConst, (uint8_t)result_reg, (uint8_t)AddConstant(BpValue::Float(0.0f)));
        }

        if (!node.outputs.empty()) pin_to_reg_[node.outputs[0].id] = result_reg;
        return result_reg;
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
        int r = regs_.Alloc();
        Emit(OpCode::LoadConst, (uint8_t)r, (uint8_t)AddConstant(BpValue::Float(pin.default_float)));
        return r;
    }

    void CompileGenericFlowNode(const BpNode& node) {
        int fn_idx = BlueprintVM::Get().GetExternIndex(node.name);
        if (fn_idx >= 0) {
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
            Emit(OpCode::CallExtern, (uint8_t)result_reg, (uint8_t)fn_idx,
                 (uint8_t)arg_start, (int16_t)num_data_inputs);
            if (!node.outputs.empty() && node.outputs.back().type != BpPinType::Flow)
                pin_to_reg_[node.outputs.back().id] = result_reg;
        }
    }
};

CompiledFunction CompileFunctionGraph(const BpFunctionGraph& graph, const std::vector<BpVariable>& vars) {
    ByteCodeCompiler compiler(graph, vars);
    return compiler.Compile();
}

}  // namespace

CompiledBlueprint CompileToByteCode(const BlueprintAsset& asset) {
    CompiledBlueprint result;
    result.version = asset.version;
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
    for (const auto& graph : asset.graphs)
        result.functions.push_back(CompileFunctionGraph(graph, asset.variables));
    return result;
}

bool LoadBlueprintAsset(BlueprintAsset& asset, const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError() || !doc.IsObject()) {
        DEBUG_LOG_ERROR("[Blueprint] .dbp 解析失败: %s", path.c_str());
        return false;
    }

    asset.file_path = path;
    if (doc.HasMember("name") && doc["name"].IsString()) asset.name = doc["name"].GetString();
    if (doc.HasMember("version") && doc["version"].IsInt()) asset.version = doc["version"].GetInt();
    if (doc.HasMember("description") && doc["description"].IsString()) asset.description = doc["description"].GetString();

    if (doc.HasMember("variables") && doc["variables"].IsArray()) {
        for (auto& v : doc["variables"].GetArray()) {
            if (!v.IsObject()) continue;
            BpVariable var;
            if (v.HasMember("name") && v["name"].IsString()) var.name = v["name"].GetString();
            if (v.HasMember("type") && v["type"].IsString()) var.type = BpVarTypeFromName(v["type"].GetString());
            if (v.HasMember("default_bool") && v["default_bool"].IsBool()) var.default_bool = v["default_bool"].GetBool();
            if (v.HasMember("default_int") && v["default_int"].IsInt()) var.default_int = v["default_int"].GetInt();
            if (v.HasMember("default_float") && v["default_float"].IsNumber()) var.default_float = v["default_float"].GetFloat();
            if (v.HasMember("default_string") && v["default_string"].IsString()) var.default_string = v["default_string"].GetString();
            if (v.HasMember("default_vec") && v["default_vec"].IsArray()) {
                auto arr = v["default_vec"].GetArray();
                for (int i = 0; i < 4 && i < static_cast<int>(arr.Size()); ++i)
                    if (arr[i].IsNumber()) var.default_vec[i] = arr[i].GetFloat();
            }
            if (v.HasMember("is_exposed") && v["is_exposed"].IsBool()) var.is_exposed = v["is_exposed"].GetBool();
            asset.variables.push_back(std::move(var));
        }
    }

    if (doc.HasMember("graphs") && doc["graphs"].IsArray()) {
        for (auto& g : doc["graphs"].GetArray()) {
            if (!g.IsObject()) continue;
            BpFunctionGraph graph;
            if (g.HasMember("name") && g["name"].IsString()) graph.name = g["name"].GetString();
            if (g.HasMember("next_id") && g["next_id"].IsInt()) graph.next_id = g["next_id"].GetInt();
            if (g.HasMember("is_pure") && g["is_pure"].IsBool()) graph.is_pure = g["is_pure"].GetBool();

            if (g.HasMember("nodes") && g["nodes"].IsArray()) {
                for (auto& n : g["nodes"].GetArray()) {
                    if (!n.IsObject()) continue;
                    BpNode node;
                    if (n.HasMember("id") && n["id"].IsInt()) node.id = n["id"].GetInt();
                    if (n.HasMember("name") && n["name"].IsString()) node.name = n["name"].GetString();
                    if (n.HasMember("category") && n["category"].IsString()) node.category = n["category"].GetString();
                    if (n.HasMember("comment") && n["comment"].IsString()) node.comment = n["comment"].GetString();

                    if (n.HasMember("inputs") && n["inputs"].IsArray()) {
                        for (auto& p : n["inputs"].GetArray()) {
                            if (!p.IsObject()) continue;
                            BpPin pin; pin.kind = BpPinKind::Input;
                            if (p.HasMember("id") && p["id"].IsInt()) pin.id = p["id"].GetInt();
                            if (p.HasMember("name") && p["name"].IsString()) pin.name = p["name"].GetString();
                            if (p.HasMember("type") && p["type"].IsString()) pin.type = BpPinTypeFromName(p["type"].GetString());
                            if (p.HasMember("default_float") && p["default_float"].IsNumber()) pin.default_float = p["default_float"].GetFloat();
                            if (p.HasMember("default_int") && p["default_int"].IsInt()) pin.default_int = p["default_int"].GetInt();
                            if (p.HasMember("default_bool") && p["default_bool"].IsBool()) pin.default_bool = p["default_bool"].GetBool();
                            node.inputs.push_back(std::move(pin));
                        }
                    }
                    if (n.HasMember("outputs") && n["outputs"].IsArray()) {
                        for (auto& p : n["outputs"].GetArray()) {
                            if (!p.IsObject()) continue;
                            BpPin pin; pin.kind = BpPinKind::Output;
                            if (p.HasMember("id") && p["id"].IsInt()) pin.id = p["id"].GetInt();
                            if (p.HasMember("name") && p["name"].IsString()) pin.name = p["name"].GetString();
                            if (p.HasMember("type") && p["type"].IsString()) pin.type = BpPinTypeFromName(p["type"].GetString());
                            node.outputs.push_back(std::move(pin));
                        }
                    }
                    graph.nodes.push_back(std::move(node));
                }
            }

            if (g.HasMember("links") && g["links"].IsArray()) {
                for (auto& l : g["links"].GetArray()) {
                    if (!l.IsObject()) continue;
                    BpLink link;
                    if (l.HasMember("id") && l["id"].IsInt()) link.id = l["id"].GetInt();
                    if (l.HasMember("from_pin") && l["from_pin"].IsInt()) link.from_pin = l["from_pin"].GetInt();
                    if (l.HasMember("to_pin") && l["to_pin"].IsInt()) link.to_pin = l["to_pin"].GetInt();
                    graph.links.push_back(link);
                }
            }
            asset.graphs.push_back(std::move(graph));
        }
    }
    return true;
}

}  // namespace dse::bp
