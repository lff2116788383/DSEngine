/**
 * @file editor_blueprint_compiler.cpp
 * @brief Blueprint compiler - dual target: ByteCode (VM) + Lua source (export)
 */

#include "editor_blueprint_compiler.h"

#include "engine/scripting/blueprint/blueprint_compiler.h"

#include <sstream>
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <unordered_set>

namespace dse::editor::bp {

namespace {

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

// ─── Runtime compiler adapter ───────────────────────────────────────────────

::dse::bp::BpVarType ToRuntimeType(BpVarType type) {
    switch (type) {
        case BpVarType::Bool: return ::dse::bp::BpVarType::Bool;
        case BpVarType::Int: return ::dse::bp::BpVarType::Int;
        case BpVarType::Float: return ::dse::bp::BpVarType::Float;
        case BpVarType::String: return ::dse::bp::BpVarType::String;
        case BpVarType::Vec2: return ::dse::bp::BpVarType::Vec2;
        case BpVarType::Vec3: return ::dse::bp::BpVarType::Vec3;
        case BpVarType::Vec4: return ::dse::bp::BpVarType::Vec4;
        case BpVarType::Entity: return ::dse::bp::BpVarType::Entity;
        case BpVarType::Array: return ::dse::bp::BpVarType::Array;
    }
    return ::dse::bp::BpVarType::Float;
}

::dse::bp::BpPinType ToRuntimeType(BpPinType type) {
    switch (type) {
        case BpPinType::Flow: return ::dse::bp::BpPinType::Flow;
        case BpPinType::Bool: return ::dse::bp::BpPinType::Bool;
        case BpPinType::Int: return ::dse::bp::BpPinType::Int;
        case BpPinType::Float: return ::dse::bp::BpPinType::Float;
        case BpPinType::String: return ::dse::bp::BpPinType::String;
        case BpPinType::Vec2: return ::dse::bp::BpPinType::Vec2;
        case BpPinType::Vec3: return ::dse::bp::BpPinType::Vec3;
        case BpPinType::Vec4: return ::dse::bp::BpPinType::Vec4;
        case BpPinType::Entity: return ::dse::bp::BpPinType::Entity;
        case BpPinType::Array: return ::dse::bp::BpPinType::Array;
        case BpPinType::Any: return ::dse::bp::BpPinType::Any;
        case BpPinType::Wildcard: return ::dse::bp::BpPinType::Wildcard;
    }
    return ::dse::bp::BpPinType::Any;
}

::dse::bp::BpPin ToRuntimePin(const BpPin& pin) {
    ::dse::bp::BpPin runtime_pin;
    runtime_pin.id = pin.id;
    runtime_pin.name = pin.name;
    runtime_pin.type = ToRuntimeType(pin.type);
    runtime_pin.kind = pin.kind == BpPinKind::Input
        ? ::dse::bp::BpPinKind::Input
        : ::dse::bp::BpPinKind::Output;
    runtime_pin.default_float = pin.default_float;
    runtime_pin.default_int = pin.default_int;
    runtime_pin.default_bool = pin.default_bool;
    runtime_pin.default_string = pin.default_string;
    std::copy(std::begin(pin.default_vec), std::end(pin.default_vec),
              std::begin(runtime_pin.default_vec));
    return runtime_pin;
}

::dse::bp::BpFunctionGraph ToRuntimeGraph(const BpFunctionGraph& graph) {
    ::dse::bp::BpFunctionGraph runtime_graph;
    runtime_graph.name = graph.name;
    runtime_graph.is_pure = graph.is_pure;
    runtime_graph.next_id = graph.next_id;
    for (const auto& pin : graph.input_params) runtime_graph.input_params.push_back(ToRuntimePin(pin));
    for (const auto& pin : graph.output_params) runtime_graph.output_params.push_back(ToRuntimePin(pin));
    for (const auto& node : graph.nodes) {
        ::dse::bp::BpNode runtime_node;
        runtime_node.id = node.id;
        runtime_node.name = node.name;
        runtime_node.category = node.category;
        runtime_node.comment = node.comment;
        for (const auto& pin : node.inputs) runtime_node.inputs.push_back(ToRuntimePin(pin));
        for (const auto& pin : node.outputs) runtime_node.outputs.push_back(ToRuntimePin(pin));
        runtime_graph.nodes.push_back(std::move(runtime_node));
    }
    for (const auto& link : graph.links) {
        runtime_graph.links.push_back({link.id, link.from_pin, link.to_pin});
    }
    return runtime_graph;
}

::dse::bp::BlueprintAsset ToRuntimeAsset(const BlueprintAsset& asset) {
    ::dse::bp::BlueprintAsset runtime_asset;
    runtime_asset.name = asset.name;
    runtime_asset.file_path = asset.file_path;
    runtime_asset.version = asset.version;
    runtime_asset.description = asset.description;
    for (const auto& variable : asset.variables) {
        ::dse::bp::BpVariable runtime_variable;
        runtime_variable.name = variable.name;
        runtime_variable.type = ToRuntimeType(variable.type);
        runtime_variable.array_element_type = ToRuntimeType(variable.array_element_type);
        runtime_variable.default_bool = variable.default_bool;
        runtime_variable.default_int = variable.default_int;
        runtime_variable.default_float = variable.default_float;
        runtime_variable.default_string = variable.default_string;
        std::copy(std::begin(variable.default_vec), std::end(variable.default_vec),
                  std::begin(runtime_variable.default_vec));
        runtime_variable.is_exposed = variable.is_exposed;
        runtime_asset.variables.push_back(std::move(runtime_variable));
    }
    for (const auto& graph : asset.graphs) runtime_asset.graphs.push_back(ToRuntimeGraph(graph));
    return runtime_asset;
}

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
    std::unordered_set<int> active_flow_nodes_;  ///< 当前流程递归链上的节点 id，用于检测环、避免无限递归

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
                if (node.category == "Event" || node.category == "Network") {
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
                    } else if (node.name == "On Net Connected") {
                        out << "function BP:on_net_connected(connection)\n";
                        indent_ = 1;
                        CompileFlowFrom(out, graph, node);
                        out << "end\n\n";
                    } else if (node.name == "On Net Disconnected") {
                        out << "function BP:on_net_disconnected(connection, reason)\n";
                        indent_ = 1;
                        CompileFlowFrom(out, graph, node);
                        out << "end\n\n";
                    } else if (node.name == "On Net Message") {
                        out << "function BP:on_net_message(connection, data)\n";
                        indent_ = 1;
                        CompileFlowFrom(out, graph, node);
                        out << "end\n\n";
                    } else if (node.name == "On RPC Received") {
                        out << "function BP:on_rpc_received(rpc_name, sender_net_id, payload)\n";
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
        // 环检测：若该节点已在当前流程递归链上，说明蓝图存在循环引用，
        // 截断展开并输出提示，避免无限递归导致栈溢出崩溃。
        if (!active_flow_nodes_.insert(node.id).second) {
            out << Indent() << "-- [Blueprint] cycle detected at node '" << node.name
                << "' (id " << node.id << "), flow truncated\n";
            return;
        }
        struct FlowGuard {
            std::unordered_set<int>& set;
            int id;
            ~FlowGuard() { set.erase(id); }
        } flow_guard{active_flow_nodes_, node.id};

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
                // Replace {inputN} placeholders
                for (size_t i = 0; i < node.inputs.size(); ++i) {
                    std::string placeholder = "{input" + std::to_string(i) + "}";
                    size_t pos = line.find(placeholder);
                    while (pos != std::string::npos) {
                        line.replace(pos, placeholder.size(), InlineExpr(graph, node, static_cast<int>(i)));
                        pos = line.find(placeholder, pos);
                    }
                }
                // Replace {outputN} with local variable names
                bool has_output_var = false;
                std::string out_var;
                for (size_t i = 0; i < node.outputs.size(); ++i) {
                    if (node.outputs[i].type != BpPinType::Flow) {
                        std::string placeholder = "{output" + std::to_string(i) + "}";
                        size_t pos = line.find(placeholder);
                        if (pos != std::string::npos) {
                            out_var = FreshVar();
                            has_output_var = true;
                            while (pos != std::string::npos) {
                                line.replace(pos, placeholder.size(), out_var);
                                pos = line.find(placeholder, pos);
                            }
                        }
                    }
                }
                if (has_output_var) {
                    out << Indent() << "local " << line << "\n";
                } else {
                    out << Indent() << line << "\n";
                }
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

        // Network pure data nodes
        if (node.name == "Has Authority") return "dse.net.is_server()";
        if (node.name == "Is Server") return "dse.net.is_server()";
        if (node.name == "Is Client") return "dse.net.is_client()";
        if (node.name == "Is Local Player") return "dse.net.is_local_player(" + InlineExpr(graph, node, 0) + ")";
        if (node.name == "Get Net Role") return "dse.net.get_role(" + InlineExpr(graph, node, 0) + ")";
        if (node.name == "Is Connected") return "dse.repl.client_connected(__dse_repl_client)";
        if (node.name == "Get Client Count") return "dse.repl.server_client_count(__dse_repl_server)";
        if (node.name == "Get Ping") return "((dse.net.get_quality(" + InlineExpr(graph, node, 0) + ") or {}).ping_ms or 0)";
        if (node.name == "Net Entity To Local") return "dse.repl.client_to_entity(__dse_repl_client, " + InlineExpr(graph, node, 0) + ")";

        // Generic: use code_template for data nodes
        if (!node.code_template.empty()) {
            std::string tmpl = node.code_template;
            // Check if template has {output0} = expr pattern
            std::string out_prefix = "{output0} = ";
            if (tmpl.rfind(out_prefix, 0) == 0) {
                std::string expr = tmpl.substr(out_prefix.size());
                for (size_t i = 0; i < node.inputs.size(); ++i) {
                    std::string ph = "{input" + std::to_string(i) + "}";
                    size_t pos = expr.find(ph);
                    while (pos != std::string::npos) {
                        expr.replace(pos, ph.size(), InlineExpr(graph, node, static_cast<int>(i)));
                        pos = expr.find(ph, pos);
                    }
                }
                return expr;
            }
        }

        return "nil";
    }
};

} // anonymous namespace

// ─── Public API ────────────────────────────────────────────────────────────

CompiledBlueprint CompileToByteCode(const BlueprintAsset& asset) {
    return ::dse::bp::CompileToByteCode(ToRuntimeAsset(asset));
}

std::string CompileToLua(const BlueprintAsset& asset) {
    LuaCompiler compiler(asset);
    return compiler.Compile();
}

CompiledFunction CompileFunctionGraph(const BpFunctionGraph& graph,
                                      const std::vector<BpVariable>& variables) {
    BlueprintAsset asset;
    asset.variables = variables;
    return ::dse::bp::CompileFunctionGraph(
        ToRuntimeGraph(graph), ToRuntimeAsset(asset).variables);
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
