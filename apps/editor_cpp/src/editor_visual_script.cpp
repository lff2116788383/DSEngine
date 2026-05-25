/**
 * @file editor_visual_script.cpp
 * @brief 可视化脚本（蓝图）编辑器 — 节点图编辑 → 生成 Lua 代码
 *
 * 复用 Shader Graph 的节点编辑模式：
 *   - 画布上放置节点，连线表示数据/事件流
 *   - 节点类型：事件(OnUpdate/OnInit)、变量获取/设置、数学运算、ECS操作、流程控制
 *   - 编译结果是可执行的 Lua 脚本
 */

#include "editor_visual_script.h"
#include "editor_context.h"
#include "editor_icons.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dse::editor {

namespace {

// ─── Data model ─────────────────────────────────────────────────────────

enum class VsPinType { Flow, Float, Vec3, String, Bool, Entity, Any };
enum class VsPinKind { Input, Output };

const char* PinTypeLabel(VsPinType t) {
    switch (t) {
        case VsPinType::Flow:   return "Flow";
        case VsPinType::Float:  return "Float";
        case VsPinType::Vec3:   return "Vec3";
        case VsPinType::String: return "String";
        case VsPinType::Bool:   return "Bool";
        case VsPinType::Entity: return "Entity";
        case VsPinType::Any:    return "Any";
        default: return "?";
    }
}

ImU32 PinColor(VsPinType t) {
    switch (t) {
        case VsPinType::Flow:   return IM_COL32(220, 220, 220, 255);
        case VsPinType::Float:  return IM_COL32(80, 200, 80, 255);
        case VsPinType::Vec3:   return IM_COL32(200, 200, 50, 255);
        case VsPinType::String: return IM_COL32(200, 100, 200, 255);
        case VsPinType::Bool:   return IM_COL32(200, 50, 50, 255);
        case VsPinType::Entity: return IM_COL32(50, 150, 250, 255);
        case VsPinType::Any:    return IM_COL32(180, 180, 180, 255);
        default: return IM_COL32(128, 128, 128, 255);
    }
}

struct VsPin {
    int id;
    std::string name;
    VsPinType type;
    VsPinKind kind;
    float default_float = 0.0f;
    float default_vec3[3] = {0, 0, 0};
    char default_string[64] = "";
    bool default_bool = false;
};

struct VsNode {
    int id;
    std::string name;
    std::string category;  // Event, Math, ECS, Flow, Variable
    ImVec2 position;
    ImVec2 size;
    std::vector<VsPin> inputs;
    std::vector<VsPin> outputs;
    ImU32 header_color;
    std::string code_template;  // Lua code template with {input0}, {output0} placeholders
};

struct VsLink {
    int id;
    int from_pin;
    int to_pin;
};

struct VsState {
    std::vector<VsNode> nodes;
    std::vector<VsLink> links;
    int next_id = 1;
    float zoom = 1.0f;
    ImVec2 scroll_offset{0, 0};
    int selected_node = -1;
    int selected_link = -1;
    // Link creation
    bool creating_link = false;
    int link_start_pin = -1;
    // Context menu
    bool show_create_menu = false;
    ImVec2 create_menu_pos;
    // Lua output
    std::string generated_lua;
    bool auto_compile = true;
    bool graph_dirty = false;
};

static VsState s_state;

int AllocId() { return s_state.next_id++; }

// ─── Pin lookup ────────────────────────────────────────────────────────

VsPin* FindPin(int pin_id) {
    for (auto& n : s_state.nodes) {
        for (auto& p : n.inputs) if (p.id == pin_id) return &p;
        for (auto& p : n.outputs) if (p.id == pin_id) return &p;
    }
    return nullptr;
}

VsNode* FindPinOwner(int pin_id) {
    for (auto& n : s_state.nodes) {
        for (auto& p : n.inputs) if (p.id == pin_id) return &n;
        for (auto& p : n.outputs) if (p.id == pin_id) return &n;
    }
    return nullptr;
}

int FindLinkedOutputPin(int input_pin_id) {
    for (auto& l : s_state.links) {
        if (l.to_pin == input_pin_id) return l.from_pin;
    }
    return -1;
}

// ─── Node factory ──────────────────────────────────────────────────────

VsNode MakeNode(const char* name, const char* cat, ImU32 col,
                 std::vector<VsPin> ins, std::vector<VsPin> outs,
                 const char* code_tmpl = "") {
    VsNode n;
    n.id = AllocId();
    n.name = name;
    n.category = cat;
    n.header_color = col;
    n.position = s_state.create_menu_pos;
    n.size = ImVec2(160, 80);
    n.code_template = code_tmpl;
    for (auto& p : ins) { p.id = AllocId(); p.kind = VsPinKind::Input; }
    for (auto& p : outs) { p.id = AllocId(); p.kind = VsPinKind::Output; }
    n.inputs = std::move(ins);
    n.outputs = std::move(outs);
    return n;
}

VsPin MkPin(const char* name, VsPinType type) {
    VsPin p;
    p.id = 0;
    p.name = name;
    p.type = type;
    p.kind = VsPinKind::Input;
    return p;
}

void AddEventOnUpdate() {
    s_state.nodes.push_back(MakeNode("On Update", "Event", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", VsPinType::Flow), MkPin("dt", VsPinType::Float)},
        "function on_update(dt)\n{body}\nend"
    ));
}

void AddEventOnInit() {
    s_state.nodes.push_back(MakeNode("On Init", "Event", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", VsPinType::Flow)},
        "function on_init()\n{body}\nend"
    ));
}

void AddMathAdd() {
    s_state.nodes.push_back(MakeNode("Add", "Math", IM_COL32(50, 150, 50, 255),
        {MkPin("A", VsPinType::Float), MkPin("B", VsPinType::Float)},
        {MkPin("Result", VsPinType::Float)},
        "{output0} = {input0} + {input1}"
    ));
}

void AddMathMultiply() {
    s_state.nodes.push_back(MakeNode("Multiply", "Math", IM_COL32(50, 150, 50, 255),
        {MkPin("A", VsPinType::Float), MkPin("B", VsPinType::Float)},
        {MkPin("Result", VsPinType::Float)},
        "{output0} = {input0} * {input1}"
    ));
}

void AddMathSin() {
    s_state.nodes.push_back(MakeNode("Sin", "Math", IM_COL32(50, 150, 50, 255),
        {MkPin("X", VsPinType::Float)},
        {MkPin("Result", VsPinType::Float)},
        "{output0} = math.sin({input0})"
    ));
}

void AddFlowBranch() {
    s_state.nodes.push_back(MakeNode("Branch", "Flow", IM_COL32(200, 200, 50, 255),
        {MkPin("Exec", VsPinType::Flow), MkPin("Condition", VsPinType::Bool)},
        {MkPin("True", VsPinType::Flow), MkPin("False", VsPinType::Flow)},
        "if {input1} then\n{true_body}\nelse\n{false_body}\nend"
    ));
}

void AddFlowForLoop() {
    s_state.nodes.push_back(MakeNode("For Loop", "Flow", IM_COL32(200, 200, 50, 255),
        {MkPin("Exec", VsPinType::Flow), MkPin("Start", VsPinType::Float), MkPin("End", VsPinType::Float)},
        {MkPin("Body", VsPinType::Flow), MkPin("Index", VsPinType::Float), MkPin("Done", VsPinType::Flow)},
        "for {output1} = {input1}, {input2} do\n{body}\nend"
    ));
}

void AddEcsGetPosition() {
    s_state.nodes.push_back(MakeNode("Get Position", "ECS", IM_COL32(50, 100, 200, 255),
        {MkPin("Entity", VsPinType::Entity)},
        {MkPin("Position", VsPinType::Vec3)},
        "{output0} = ecs.get_position({input0})"
    ));
}

void AddEcsSetPosition() {
    s_state.nodes.push_back(MakeNode("Set Position", "ECS", IM_COL32(50, 100, 200, 255),
        {MkPin("Exec", VsPinType::Flow), MkPin("Entity", VsPinType::Entity), MkPin("Position", VsPinType::Vec3)},
        {MkPin("Exec", VsPinType::Flow)},
        "ecs.set_position({input1}, {input2})"
    ));
}

void AddEcsCreateEntity() {
    s_state.nodes.push_back(MakeNode("Create Entity", "ECS", IM_COL32(50, 100, 200, 255),
        {MkPin("Exec", VsPinType::Flow), MkPin("Name", VsPinType::String)},
        {MkPin("Exec", VsPinType::Flow), MkPin("Entity", VsPinType::Entity)},
        "{output1} = ecs.create_entity({input1})"
    ));
}

void AddPrint() {
    s_state.nodes.push_back(MakeNode("Print", "Utility", IM_COL32(180, 180, 180, 255),
        {MkPin("Exec", VsPinType::Flow), MkPin("Message", VsPinType::Any)},
        {MkPin("Exec", VsPinType::Flow)},
        "print({input1})"
    ));
}

void AddVarGetSelf() {
    s_state.nodes.push_back(MakeNode("Self Entity", "Variable", IM_COL32(100, 180, 250, 255),
        {},
        {MkPin("Entity", VsPinType::Entity)},
        "{output0} = self_entity"
    ));
}

void AddConstFloat() {
    s_state.nodes.push_back(MakeNode("Float Constant", "Variable", IM_COL32(100, 180, 250, 255),
        {},
        {MkPin("Value", VsPinType::Float)}
    ));
}

// ─── Compile to Lua ────────────────────────────────────────────────────

std::string CompileVisualScriptToLua() {
    std::ostringstream lua;
    lua << "-- Generated by DSEngine Visual Script Editor\n";
    lua << "-- Do not edit manually\n\n";

    // Topological sort (data flow nodes)
    std::unordered_map<int, int> in_degree;
    std::unordered_map<int, std::vector<int>> adj;  // node_id → downstream node_ids
    for (auto& n : s_state.nodes) in_degree[n.id] = 0;
    for (auto& l : s_state.links) {
        VsNode* from_node = FindPinOwner(l.from_pin);
        VsNode* to_node = FindPinOwner(l.to_pin);
        if (from_node && to_node && from_node->id != to_node->id) {
            adj[from_node->id].push_back(to_node->id);
            in_degree[to_node->id]++;
        }
    }

    std::queue<int> q;
    for (auto& [nid, deg] : in_degree) {
        if (deg == 0) q.push(nid);
    }
    std::vector<int> sorted_ids;
    while (!q.empty()) {
        int nid = q.front(); q.pop();
        sorted_ids.push_back(nid);
        for (int next : adj[nid]) {
            in_degree[next]--;
            if (in_degree[next] == 0) q.push(next);
        }
    }

    // Generate variable names
    std::unordered_map<int, std::string> pin_var;  // pin_id → variable name
    int var_counter = 0;
    for (int nid : sorted_ids) {
        VsNode* n = nullptr;
        for (auto& nd : s_state.nodes) {
            if (nd.id == nid) { n = &nd; break; }
        }
        if (!n) continue;
        for (auto& p : n->outputs) {
            if (p.type != VsPinType::Flow) {
                pin_var[p.id] = "v" + std::to_string(var_counter++);
            }
        }
    }

    // Resolve input values (linked pin var or default literal)
    auto ResolveInput = [&](const VsPin& p) -> std::string {
        int linked = FindLinkedOutputPin(p.id);
        if (linked >= 0 && pin_var.count(linked)) return pin_var[linked];
        switch (p.type) {
            case VsPinType::Float:  return std::to_string(p.default_float);
            case VsPinType::Vec3: {
                char buf[64];
                snprintf(buf, sizeof(buf), "vec3(%.4f, %.4f, %.4f)",
                         p.default_vec3[0], p.default_vec3[1], p.default_vec3[2]);
                return buf;
            }
            case VsPinType::String: return std::string("\"") + p.default_string + "\"";
            case VsPinType::Bool:   return p.default_bool ? "true" : "false";
            case VsPinType::Entity: return "nil";
            default: return "nil";
        }
    };

    // Emit code per node in topological order
    for (int nid : sorted_ids) {
        VsNode* n = nullptr;
        for (auto& nd : s_state.nodes) { if (nd.id == nid) { n = &nd; break; } }
        if (!n || n->code_template.empty()) {
            // Float constant: just assign the default
            if (n && n->name == "Float Constant" && !n->outputs.empty()) {
                lua << "local " << pin_var[n->outputs[0].id] << " = "
                    << n->outputs[0].default_float << "\n";
            }
            continue;
        }

        // Simple template substitution
        std::string code = n->code_template;
        for (int i = 0; i < static_cast<int>(n->inputs.size()); ++i) {
            std::string placeholder = "{input" + std::to_string(i) + "}";
            std::string value = ResolveInput(n->inputs[i]);
            size_t pos;
            while ((pos = code.find(placeholder)) != std::string::npos) {
                code.replace(pos, placeholder.size(), value);
            }
        }
        for (int i = 0; i < static_cast<int>(n->outputs.size()); ++i) {
            std::string placeholder = "{output" + std::to_string(i) + "}";
            std::string value = pin_var.count(n->outputs[i].id) ? pin_var[n->outputs[i].id] : "_";
            size_t pos;
            while ((pos = code.find(placeholder)) != std::string::npos) {
                code.replace(pos, placeholder.size(), value);
            }
        }

        // For event nodes with {body}, collect connected flow statements
        // (simplified: just add placeholder comment)
        {
            size_t bp = code.find("{body}");
            if (bp != std::string::npos) code.replace(bp, 6, "  -- TODO: connected flow nodes");
            bp = code.find("{true_body}");
            if (bp != std::string::npos) code.replace(bp, 11, "  -- true branch");
            bp = code.find("{false_body}");
            if (bp != std::string::npos) code.replace(bp, 12, "  -- false branch");
        }

        // Prepend local for data nodes
        bool has_output_data = false;
        for (auto& p : n->outputs) {
            if (p.type != VsPinType::Flow) { has_output_data = true; break; }
        }
        if (has_output_data && n->category != "Event") {
            lua << "local " << code << "\n";
        } else {
            lua << code << "\n";
        }
    }

    return lua.str();
}

// ─── Pin position calculation ──────────────────────────────────────────

ImVec2 GetPinPos(const VsNode& node, const VsPin& pin, int index) {
    float y = node.position.y + 28.0f + index * 22.0f;
    float x = (pin.kind == VsPinKind::Input) ? node.position.x : node.position.x + node.size.x;
    return ImVec2(x, y);
}

// ─── Drawing ───────────────────────────────────────────────────────────

void DrawNode(ImDrawList* dl, VsNode& node, const ImVec2& offset, bool selected) {
    ImVec2 p0(node.position.x + offset.x, node.position.y + offset.y);
    int max_pins = std::max(static_cast<int>(node.inputs.size()), static_cast<int>(node.outputs.size()));
    node.size.y = 28.0f + std::max(max_pins, 1) * 22.0f + 8.0f;
    ImVec2 p1(p0.x + node.size.x, p0.y + node.size.y);

    // Shadow
    dl->AddRectFilled(ImVec2(p0.x + 4, p0.y + 4), ImVec2(p1.x + 4, p1.y + 4),
                       IM_COL32(0, 0, 0, 60), 4.0f);
    // Body
    dl->AddRectFilled(p0, p1, IM_COL32(45, 45, 48, 230), 4.0f);
    // Header
    dl->AddRectFilled(p0, ImVec2(p1.x, p0.y + 24), node.header_color, 4.0f, ImDrawFlags_RoundCornersTop);
    dl->AddText(ImVec2(p0.x + 6, p0.y + 4), IM_COL32(255, 255, 255, 255), node.name.c_str());
    // Border
    dl->AddRect(p0, p1, selected ? IM_COL32(255, 200, 50, 255) : IM_COL32(80, 80, 80, 200), 4.0f,
                ImDrawFlags_RoundCornersAll, selected ? 2.0f : 1.0f);

    // Pins
    auto draw_pin = [&](const VsPin& pin, int idx) {
        ImVec2 pp = GetPinPos(node, pin, idx);
        pp.x += offset.x;
        pp.y += offset.y;
        ImU32 col = PinColor(pin.type);
        if (pin.type == VsPinType::Flow) {
            // Triangle for flow pins
            float sz = 5.0f;
            if (pin.kind == VsPinKind::Output) {
                dl->AddTriangleFilled(ImVec2(pp.x, pp.y - sz), ImVec2(pp.x, pp.y + sz),
                                       ImVec2(pp.x + sz * 1.5f, pp.y), col);
            } else {
                dl->AddTriangleFilled(ImVec2(pp.x, pp.y - sz), ImVec2(pp.x, pp.y + sz),
                                       ImVec2(pp.x - sz * 1.5f, pp.y), col);
            }
        } else {
            dl->AddCircleFilled(pp, 5.0f, col);
        }

        // Label
        if (pin.kind == VsPinKind::Input) {
            dl->AddText(ImVec2(pp.x + 8, pp.y - 8), IM_COL32(200, 200, 200, 255), pin.name.c_str());
        } else {
            ImVec2 ts = ImGui::CalcTextSize(pin.name.c_str());
            dl->AddText(ImVec2(pp.x - ts.x - 8, pp.y - 8), IM_COL32(200, 200, 200, 255), pin.name.c_str());
        }
    };

    for (int i = 0; i < static_cast<int>(node.inputs.size()); ++i) draw_pin(node.inputs[i], i);
    for (int i = 0; i < static_cast<int>(node.outputs.size()); ++i) draw_pin(node.outputs[i], i);
}

void DrawLinks(ImDrawList* dl, const ImVec2& offset) {
    for (auto& link : s_state.links) {
        VsPin* from_pin = FindPin(link.from_pin);
        VsPin* to_pin = FindPin(link.to_pin);
        VsNode* from_node = FindPinOwner(link.from_pin);
        VsNode* to_node = FindPinOwner(link.to_pin);
        if (!from_pin || !to_pin || !from_node || !to_node) continue;

        int from_idx = 0, to_idx = 0;
        for (int i = 0; i < static_cast<int>(from_node->outputs.size()); ++i) {
            if (from_node->outputs[i].id == link.from_pin) { from_idx = i; break; }
        }
        for (int i = 0; i < static_cast<int>(to_node->inputs.size()); ++i) {
            if (to_node->inputs[i].id == link.to_pin) { to_idx = i; break; }
        }

        ImVec2 p1 = GetPinPos(*from_node, *from_pin, from_idx);
        ImVec2 p2 = GetPinPos(*to_node, *to_pin, to_idx);
        p1.x += offset.x; p1.y += offset.y;
        p2.x += offset.x; p2.y += offset.y;

        ImU32 col = PinColor(from_pin->type);
        bool is_selected = (link.id == s_state.selected_link);
        float thick = is_selected ? 3.0f : 2.0f;

        float dx = std::abs(p2.x - p1.x) * 0.5f;
        if (dx < 30) dx = 30;
        dl->AddBezierCubic(p1, ImVec2(p1.x + dx, p1.y), ImVec2(p2.x - dx, p2.y), p2,
                            col, thick);
    }
}

// ─── Context menu ──────────────────────────────────────────────────────

void DrawCreateNodeMenu() {
    if (!s_state.show_create_menu) return;

    ImGui::SetNextWindowPos(ImGui::GetMousePosOnOpeningCurrentPopup());
    if (ImGui::BeginPopup("vs_create_menu")) {
        if (ImGui::BeginMenu("Event")) {
            if (ImGui::MenuItem("On Init")) { AddEventOnInit(); s_state.graph_dirty = true; }
            if (ImGui::MenuItem("On Update")) { AddEventOnUpdate(); s_state.graph_dirty = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Math")) {
            if (ImGui::MenuItem("Add")) { AddMathAdd(); s_state.graph_dirty = true; }
            if (ImGui::MenuItem("Multiply")) { AddMathMultiply(); s_state.graph_dirty = true; }
            if (ImGui::MenuItem("Sin")) { AddMathSin(); s_state.graph_dirty = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Flow Control")) {
            if (ImGui::MenuItem("Branch (If/Else)")) { AddFlowBranch(); s_state.graph_dirty = true; }
            if (ImGui::MenuItem("For Loop")) { AddFlowForLoop(); s_state.graph_dirty = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("ECS")) {
            if (ImGui::MenuItem("Get Position")) { AddEcsGetPosition(); s_state.graph_dirty = true; }
            if (ImGui::MenuItem("Set Position")) { AddEcsSetPosition(); s_state.graph_dirty = true; }
            if (ImGui::MenuItem("Create Entity")) { AddEcsCreateEntity(); s_state.graph_dirty = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Utility")) {
            if (ImGui::MenuItem("Print")) { AddPrint(); s_state.graph_dirty = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Variable")) {
            if (ImGui::MenuItem("Self Entity")) { AddVarGetSelf(); s_state.graph_dirty = true; }
            if (ImGui::MenuItem("Float Constant")) { AddConstFloat(); s_state.graph_dirty = true; }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    } else {
        s_state.show_create_menu = false;
    }
}

} // anonymous namespace

// ─── Public interface ──────────────────────────────────────────────────

const std::string& GetVisualScriptLuaOutput() {
    return s_state.generated_lua;
}

void DrawVisualScriptEditor(EditorContext& /*ctx*/) {
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);

    // Toolbar
    if (ImGui::Button("Compile to Lua")) {
        s_state.generated_lua = CompileVisualScriptToLua();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Compile", &s_state.auto_compile);
    ImGui::SameLine();
    ImGui::Text("Nodes: %d  Links: %d", static_cast<int>(s_state.nodes.size()),
                static_cast<int>(s_state.links.size()));

    // Split: canvas left, preview right
    float preview_width = 300.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float canvas_width = avail.x - preview_width - 8;
    if (canvas_width < 200) canvas_width = avail.x;

    // ── Canvas ────────────────────────────────────────────────────────
    ImGui::BeginChild("vs_canvas", ImVec2(canvas_width, avail.y), true);
    {
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImGui::InvisibleButton("vs_bg", canvas_size);
        bool bg_hovered = ImGui::IsItemHovered();

        ImVec2 offset = s_state.scroll_offset;

        // Background grid
        dl->AddRectFilled(canvas_pos,
                           ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                           IM_COL32(25, 25, 28, 255));
        float grid_step = 32.0f;
        for (float x = std::fmod(offset.x, grid_step); x < canvas_size.x; x += grid_step) {
            dl->AddLine(ImVec2(canvas_pos.x + x, canvas_pos.y),
                        ImVec2(canvas_pos.x + x, canvas_pos.y + canvas_size.y),
                        IM_COL32(40, 40, 43, 255));
        }
        for (float y = std::fmod(offset.y, grid_step); y < canvas_size.y; y += grid_step) {
            dl->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + y),
                        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + y),
                        IM_COL32(40, 40, 43, 255));
        }

        ImVec2 draw_offset(canvas_pos.x + offset.x, canvas_pos.y + offset.y);

        // Draw links
        DrawLinks(dl, draw_offset);

        // Draw link being created
        if (s_state.creating_link) {
            VsPin* start_pin = FindPin(s_state.link_start_pin);
            VsNode* start_node = FindPinOwner(s_state.link_start_pin);
            if (start_pin && start_node) {
                int idx = 0;
                auto& list = (start_pin->kind == VsPinKind::Output) ? start_node->outputs : start_node->inputs;
                for (int i = 0; i < static_cast<int>(list.size()); ++i) {
                    if (list[i].id == s_state.link_start_pin) { idx = i; break; }
                }
                ImVec2 p1 = GetPinPos(*start_node, *start_pin, idx);
                p1.x += draw_offset.x; p1.y += draw_offset.y;
                ImVec2 p2 = ImGui::GetMousePos();
                float dx = std::abs(p2.x - p1.x) * 0.5f;
                if (dx < 30) dx = 30;
                dl->AddBezierCubic(p1, ImVec2(p1.x + dx, p1.y), ImVec2(p2.x - dx, p2.y), p2,
                                    IM_COL32(200, 200, 200, 150), 2.0f);
            }
        }

        // Draw nodes
        for (auto& node : s_state.nodes) {
            bool sel = (node.id == s_state.selected_node);
            DrawNode(dl, node, draw_offset, sel);
        }

        // ── Interaction ──────────────────────────────────────────────
        ImVec2 mouse = ImGui::GetMousePos();

        // Pan (middle mouse)
        if (bg_hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            s_state.scroll_offset.x += ImGui::GetIO().MouseDelta.x;
            s_state.scroll_offset.y += ImGui::GetIO().MouseDelta.y;
        }

        // Right-click context menu
        if (bg_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            s_state.show_create_menu = true;
            s_state.create_menu_pos = ImVec2(mouse.x - draw_offset.x, mouse.y - draw_offset.y);
            ImGui::OpenPopup("vs_create_menu");
        }
        DrawCreateNodeMenu();

        // Left-click: select node / start link
        if (bg_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            s_state.selected_node = -1;
            s_state.selected_link = -1;

            // Check pin click
            for (auto& node : s_state.nodes) {
                auto check_pins = [&](std::vector<VsPin>& pins) {
                    for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
                        ImVec2 pp = GetPinPos(node, pins[i], i);
                        pp.x += draw_offset.x; pp.y += draw_offset.y;
                        float dx2 = mouse.x - pp.x, dy2 = mouse.y - pp.y;
                        if (dx2 * dx2 + dy2 * dy2 < 64.0f) {
                            if (!s_state.creating_link) {
                                s_state.creating_link = true;
                                s_state.link_start_pin = pins[i].id;
                            } else {
                                // Complete link
                                VsPin* start = FindPin(s_state.link_start_pin);
                                if (start && start->kind != pins[i].kind) {
                                    VsLink link;
                                    link.id = AllocId();
                                    if (start->kind == VsPinKind::Output) {
                                        link.from_pin = s_state.link_start_pin;
                                        link.to_pin = pins[i].id;
                                    } else {
                                        link.from_pin = pins[i].id;
                                        link.to_pin = s_state.link_start_pin;
                                    }
                                    s_state.links.push_back(link);
                                    s_state.graph_dirty = true;
                                }
                                s_state.creating_link = false;
                            }
                            return true;
                        }
                    }
                    return false;
                };
                if (check_pins(node.inputs) || check_pins(node.outputs)) break;
            }

            // Check node click (for drag / selection)
            if (!s_state.creating_link) {
                for (auto& node : s_state.nodes) {
                    ImVec2 np0(node.position.x + draw_offset.x, node.position.y + draw_offset.y);
                    ImVec2 np1(np0.x + node.size.x, np0.y + node.size.y);
                    if (mouse.x >= np0.x && mouse.x <= np1.x && mouse.y >= np0.y && mouse.y <= np1.y) {
                        s_state.selected_node = node.id;
                        break;
                    }
                }
            }
        }

        // Cancel link creation on right-click
        if (s_state.creating_link && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            s_state.creating_link = false;
        }

        // Drag selected node
        if (s_state.selected_node >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            for (auto& node : s_state.nodes) {
                if (node.id == s_state.selected_node) {
                    node.position.x += ImGui::GetIO().MouseDelta.x;
                    node.position.y += ImGui::GetIO().MouseDelta.y;
                    break;
                }
            }
        }

        // Delete key
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (s_state.selected_node >= 0) {
                int nid = s_state.selected_node;
                // Remove links connected to this node
                s_state.links.erase(std::remove_if(s_state.links.begin(), s_state.links.end(),
                    [nid](const VsLink& l) {
                        VsNode* fn = FindPinOwner(l.from_pin);
                        VsNode* tn = FindPinOwner(l.to_pin);
                        return (fn && fn->id == nid) || (tn && tn->id == nid);
                    }), s_state.links.end());
                s_state.nodes.erase(std::remove_if(s_state.nodes.begin(), s_state.nodes.end(),
                    [nid](const VsNode& n) { return n.id == nid; }), s_state.nodes.end());
                s_state.selected_node = -1;
                s_state.graph_dirty = true;
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Lua Preview ───────────────────────────────────────────────────
    ImGui::BeginChild("vs_lua_preview", ImVec2(0, avail.y), true);
    {
        ImGui::Text("Generated Lua:");
        ImGui::Separator();

        if (s_state.auto_compile && s_state.graph_dirty) {
            s_state.generated_lua = CompileVisualScriptToLua();
            s_state.graph_dirty = false;
        }

        ImGui::BeginChild("vs_lua_scroll", ImVec2(0, 0), false,
                           ImGuiWindowFlags_HorizontalScrollbar);
        if (!s_state.generated_lua.empty()) {
            ImGui::TextUnformatted(s_state.generated_lua.c_str(),
                                   s_state.generated_lua.c_str() + s_state.generated_lua.size());
        } else {
            ImGui::TextDisabled("Click 'Compile to Lua' or add nodes and connect them.");
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
}

} // namespace dse::editor
