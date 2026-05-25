#include "editor_shader_graph.h"
#include "editor_icons.h"
#include "editor_console_panel.h"
#include "editor_context.h"

#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"
#include "engine/base/debug.h"
#include "engine/ecs/components_3d.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <queue>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

namespace dse::editor {

namespace {

// ─── 唯一着色器名称生成器 ─────────────────────────────────────────────────────

std::string GenerateUniqueShaderName() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    int random = dis(gen);
    
    return "ShaderGraph_" + std::to_string(timestamp) + "_" + std::to_string(random);
}

// ─── Node graph data model ───────────────────────────────────────────────────

enum class PinType { Float, Vec2, Vec3, Vec4, Color, Texture2D, Sampler };
enum class PinKind { Input, Output };

struct Pin {
    int id;
    std::string name;
    PinType type;
    PinKind kind;
    float default_value[4] = {0, 0, 0, 1};
};

struct Node {
    int id;
    std::string name;
    std::string category;
    ImVec2 position;
    ImVec2 size;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
    ImU32 header_color;
};

struct Link {
    int id;
    int from_pin;
    int to_pin;
};

struct ShaderGraphState {
    std::vector<Node> nodes;
    std::vector<Link> links;
    int next_id = 100;
    bool initialized = false;
    ImVec2 scroll_offset{0, 0};
    float zoom = 1.0f;
    int selected_node = -1;
    int dragging_node = -1;
    ImVec2 drag_offset;
    // Link creation state
    bool creating_link = false;
    int link_start_pin = -1;
    // Context menu
    bool show_create_menu = false;
    ImVec2 create_menu_pos;
    // Selected link for deletion
    int selected_link = -1;
    // GLSL preview
    bool show_preview = false;
    std::string preview_glsl;
    // Property editor
    bool show_properties = true;
    // Auto-compile for real-time preview
    bool auto_compile = true;
    bool graph_dirty = false;
    float compile_timer = 0.0f;
    static constexpr float kCompileDelay = 0.3f; // debounce 300ms
};

ShaderGraphState& GetState() {
    static ShaderGraphState state;
    return state;
}

int AllocId(ShaderGraphState& s) { return s.next_id++; }

ImU32 PinColor(PinType type) {
    switch (type) {
        case PinType::Float:     return IM_COL32(150, 200, 150, 255);
        case PinType::Vec2:      return IM_COL32(100, 200, 255, 255);
        case PinType::Vec3:      return IM_COL32(200, 150, 255, 255);
        case PinType::Vec4:      return IM_COL32(255, 180, 100, 255);
        case PinType::Color:     return IM_COL32(255, 100, 100, 255);
        case PinType::Texture2D: return IM_COL32(255, 255, 100, 255);
        case PinType::Sampler:   return IM_COL32(180, 180, 180, 255);
    }
    return IM_COL32(200, 200, 200, 255);
}

Node CreateNode(ShaderGraphState& s, const char* name, const char* category,
                ImVec2 pos, ImU32 header_col,
                std::vector<Pin> inputs, std::vector<Pin> outputs) {
    Node n;
    n.id = AllocId(s);
    n.name = name;
    n.category = category;
    n.position = pos;
    n.size = ImVec2(180, 0);
    n.header_color = header_col;
    for (auto& p : inputs) { p.id = AllocId(s); p.kind = PinKind::Input; n.inputs.push_back(p); }
    for (auto& p : outputs) { p.id = AllocId(s); p.kind = PinKind::Output; n.outputs.push_back(p); }
    return n;
}

void InitDefaultGraph(ShaderGraphState& s) {
    if (s.initialized) return;
    s.initialized = true;

    // PBR Output node
    s.nodes.push_back(CreateNode(s, "PBR Output", "Output",
        ImVec2(500, 100), IM_COL32(180, 60, 60, 255),
        {{0, "Base Color", PinType::Color, PinKind::Input, {0.8f, 0.8f, 0.8f, 1}},
         {0, "Metallic", PinType::Float, PinKind::Input, {0, 0, 0, 0}},
         {0, "Roughness", PinType::Float, PinKind::Input, {0.5f, 0, 0, 0}},
         {0, "Normal", PinType::Vec3, PinKind::Input, {0, 0, 1, 0}},
         {0, "Emission", PinType::Color, PinKind::Input, {0, 0, 0, 0}},
         {0, "AO", PinType::Float, PinKind::Input, {1, 0, 0, 0}},
         {0, "Alpha", PinType::Float, PinKind::Input, {1, 0, 0, 0}}},
        {}));

    // Texture Sample node
    s.nodes.push_back(CreateNode(s, "Texture Sample", "Texture",
        ImVec2(100, 80), IM_COL32(60, 120, 180, 255),
        {{0, "UV", PinType::Vec2, PinKind::Input, {0, 0, 0, 0}},
         {0, "Texture", PinType::Texture2D, PinKind::Input, {0, 0, 0, 0}}},
        {{0, "RGBA", PinType::Vec4, PinKind::Output, {}},
         {0, "R", PinType::Float, PinKind::Output, {}},
         {0, "G", PinType::Float, PinKind::Output, {}},
         {0, "B", PinType::Float, PinKind::Output, {}},
         {0, "A", PinType::Float, PinKind::Output, {}}}));

    // Color constant
    s.nodes.push_back(CreateNode(s, "Color", "Constant",
        ImVec2(100, 300), IM_COL32(180, 120, 60, 255),
        {},
        {{0, "Color", PinType::Color, PinKind::Output, {0.8f, 0.2f, 0.2f, 1}}}));

    // Float constant
    s.nodes.push_back(CreateNode(s, "Float", "Constant",
        ImVec2(100, 450), IM_COL32(100, 160, 100, 255),
        {},
        {{0, "Value", PinType::Float, PinKind::Output, {0.5f, 0, 0, 0}}}));

    // Add a link: Color -> PBR.BaseColor
    if (s.nodes.size() >= 3 && !s.nodes[2].outputs.empty() && !s.nodes[0].inputs.empty()) {
        Link lnk;
        lnk.id = AllocId(s);
        lnk.from_pin = s.nodes[2].outputs[0].id;
        lnk.to_pin = s.nodes[0].inputs[0].id;
        s.links.push_back(lnk);
    }
}

Pin* FindPin(ShaderGraphState& s, int pin_id) {
    for (auto& n : s.nodes) {
        for (auto& p : n.inputs) if (p.id == pin_id) return &p;
        for (auto& p : n.outputs) if (p.id == pin_id) return &p;
    }
    return nullptr;
}

Node* FindPinOwner(ShaderGraphState& s, int pin_id) {
    for (auto& n : s.nodes) {
        for (auto& p : n.inputs) if (p.id == pin_id) return &n;
        for (auto& p : n.outputs) if (p.id == pin_id) return &n;
    }
    return nullptr;
}

ImVec2 GetPinPos(const Node& node, const Pin& pin, bool is_output) {
    float y = node.position.y + 28.0f; // Header height
    if (is_output) {
        int idx = 0;
        for (auto& p : node.outputs) {
            if (p.id == pin.id) break;
            idx++;
        }
        return ImVec2(node.position.x + node.size.x,
                      y + static_cast<float>(idx) * 22.0f + 11.0f +
                      static_cast<float>(node.inputs.size()) * 22.0f);
    } else {
        int idx = 0;
        for (auto& p : node.inputs) {
            if (p.id == pin.id) break;
            idx++;
        }
        return ImVec2(node.position.x, y + static_cast<float>(idx) * 22.0f + 11.0f);
    }
}

void DrawBezierLink(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 color, float thick) {
    float dx = std::abs(p2.x - p1.x) * 0.5f;
    ImVec2 cp1(p1.x + dx, p1.y);
    ImVec2 cp2(p2.x - dx, p2.y);
    dl->AddBezierCubic(p1, cp1, cp2, p2, color, thick);
}

/// Template definitions for "Add Node" menu
struct NodeTemplate {
    const char* name;
    const char* category;
    ImU32 color;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
};

std::vector<NodeTemplate>& GetTemplates() {
    static std::vector<NodeTemplate> templates = {
        {"Float", "Constant", IM_COL32(100, 160, 100, 255),
         {}, {{0, "Value", PinType::Float, PinKind::Output, {0}}}},
        {"Vec2", "Constant", IM_COL32(100, 160, 200, 255),
         {}, {{0, "Value", PinType::Vec2, PinKind::Output, {0}}}},
        {"Vec3", "Constant", IM_COL32(160, 120, 200, 255),
         {}, {{0, "Value", PinType::Vec3, PinKind::Output, {0}}}},
        {"Color", "Constant", IM_COL32(180, 120, 60, 255),
         {}, {{0, "Color", PinType::Color, PinKind::Output, {0.5f, 0.5f, 0.5f, 1}}}},
        {"Texture Sample", "Texture", IM_COL32(60, 120, 180, 255),
         {{0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Texture", PinType::Texture2D, PinKind::Input, {}}},
         {{0, "RGBA", PinType::Vec4, PinKind::Output, {}},
          {0, "R", PinType::Float, PinKind::Output, {}},
          {0, "G", PinType::Float, PinKind::Output, {}},
          {0, "B", PinType::Float, PinKind::Output, {}}}},
        {"Add", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "A", PinType::Float, PinKind::Input, {}},
          {0, "B", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Multiply", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "A", PinType::Float, PinKind::Input, {}},
          {0, "B", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Lerp", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "A", PinType::Float, PinKind::Input, {}},
          {0, "B", PinType::Float, PinKind::Input, {}},
          {0, "T", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Normal Map", "Utility", IM_COL32(120, 80, 180, 255),
         {{0, "Texture", PinType::Texture2D, PinKind::Input, {}},
          {0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Strength", PinType::Float, PinKind::Input, {1}}},
         {{0, "Normal", PinType::Vec3, PinKind::Output, {}}}},
        {"Fresnel", "Utility", IM_COL32(120, 80, 180, 255),
         {{0, "Power", PinType::Float, PinKind::Input, {5}},
          {0, "Normal", PinType::Vec3, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"UV", "Input", IM_COL32(180, 180, 60, 255),
         {},
         {{0, "UV0", PinType::Vec2, PinKind::Output, {}}}},
        {"Time", "Input", IM_COL32(180, 180, 60, 255),
         {},
         {{0, "Time", PinType::Float, PinKind::Output, {}},
          {0, "Sin", PinType::Float, PinKind::Output, {}},
          {0, "Cos", PinType::Float, PinKind::Output, {}}}},
    };
    return templates;
}

// ─── JSON 序列化辅助 ─────────────────────────────────────────────────────────

std::string EscapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

std::string PinTypeToStr(PinType t) {
    switch (t) {
        case PinType::Float: return "Float";
        case PinType::Vec2: return "Vec2";
        case PinType::Vec3: return "Vec3";
        case PinType::Vec4: return "Vec4";
        case PinType::Color: return "Color";
        case PinType::Texture2D: return "Texture2D";
        case PinType::Sampler: return "Sampler";
    }
    return "Float";
}

PinType StrToPinType(const std::string& s) {
    if (s == "Vec2") return PinType::Vec2;
    if (s == "Vec3") return PinType::Vec3;
    if (s == "Vec4") return PinType::Vec4;
    if (s == "Color") return PinType::Color;
    if (s == "Texture2D") return PinType::Texture2D;
    if (s == "Sampler") return PinType::Sampler;
    return PinType::Float;
}

std::string SerializePinJson(const Pin& p) {
    std::ostringstream o;
    o << "{\"id\":" << p.id
      << ",\"name\":\"" << EscapeJson(p.name) << "\""
      << ",\"type\":\"" << PinTypeToStr(p.type) << "\""
      << ",\"kind\":" << (p.kind == PinKind::Input ? 0 : 1)
      << ",\"default\":[" << p.default_value[0] << "," << p.default_value[1]
      << "," << p.default_value[2] << "," << p.default_value[3] << "]}";
    return o.str();
}

std::string SerializeGraphJson(const ShaderGraphState& s) {
    std::ostringstream o;
    o << "{\n  \"next_id\":" << s.next_id << ",\n  \"nodes\":[\n";
    for (size_t ni = 0; ni < s.nodes.size(); ++ni) {
        auto& n = s.nodes[ni];
        o << "    {\"id\":" << n.id
          << ",\"name\":\"" << EscapeJson(n.name) << "\""
          << ",\"category\":\"" << EscapeJson(n.category) << "\""
          << ",\"pos\":[" << n.position.x << "," << n.position.y << "]"
          << ",\"color\":" << n.header_color
          << ",\"inputs\":[";
        for (size_t i = 0; i < n.inputs.size(); ++i) {
            if (i) o << ",";
            o << SerializePinJson(n.inputs[i]);
        }
        o << "],\"outputs\":[";
        for (size_t i = 0; i < n.outputs.size(); ++i) {
            if (i) o << ",";
            o << SerializePinJson(n.outputs[i]);
        }
        o << "]}";
        if (ni + 1 < s.nodes.size()) o << ",";
        o << "\n";
    }
    o << "  ],\n  \"links\":[\n";
    for (size_t li = 0; li < s.links.size(); ++li) {
        auto& l = s.links[li];
        o << "    {\"id\":" << l.id << ",\"from\":" << l.from_pin << ",\"to\":" << l.to_pin << "}";
        if (li + 1 < s.links.size()) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return o.str();
}

// 极简 JSON 值提取器 (足够处理我们生成的格式)
std::string JsonValue(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        size_t end = json.find('"', pos + 1);
        return json.substr(pos + 1, end - pos - 1);
    }
    if (json[pos] == '[') {
        int depth = 0;
        size_t start = pos;
        for (size_t i = pos; i < json.size(); ++i) {
            if (json[i] == '[') ++depth;
            else if (json[i] == ']') { --depth; if (depth == 0) return json.substr(start, i - start + 1); }
        }
    }
    size_t end = json.find_first_of(",}]\n", pos);
    return json.substr(pos, end - pos);
}

std::vector<std::string> JsonArray(const std::string& arr) {
    std::vector<std::string> result;
    if (arr.size() < 2) return result;
    // arr starts with '[' and ends with ']'
    int depth = 0;
    size_t start = 0;
    for (size_t i = 1; i < arr.size() - 1; ++i) {
        if (arr[i] == '{' || arr[i] == '[') { if (depth == 0) start = i; ++depth; }
        else if (arr[i] == '}' || arr[i] == ']') {
            --depth;
            if (depth == 0) result.push_back(arr.substr(start, i - start + 1));
        }
    }
    return result;
}

Pin ParsePinJson(const std::string& json) {
    Pin p;
    p.id = std::atoi(JsonValue(json, "id").c_str());
    p.name = JsonValue(json, "name");
    p.type = StrToPinType(JsonValue(json, "type"));
    p.kind = (std::atoi(JsonValue(json, "kind").c_str()) == 0) ? PinKind::Input : PinKind::Output;
    auto def_arr = JsonValue(json, "default");
    if (!def_arr.empty()) {
        sscanf(def_arr.c_str(), "[%f,%f,%f,%f]", &p.default_value[0], &p.default_value[1],
               &p.default_value[2], &p.default_value[3]);
    }
    return p;
}

bool DeserializeGraph(const std::string& json, ShaderGraphState& s) {
    s.nodes.clear();
    s.links.clear();
    s.next_id = std::atoi(JsonValue(json, "next_id").c_str());
    if (s.next_id < 100) s.next_id = 100;
    s.initialized = true;

    auto nodes_arr = JsonValue(json, "nodes");
    for (auto& nj : JsonArray(nodes_arr)) {
        Node n;
        n.id = std::atoi(JsonValue(nj, "id").c_str());
        n.name = JsonValue(nj, "name");
        n.category = JsonValue(nj, "category");
        auto pos_str = JsonValue(nj, "pos");
        sscanf(pos_str.c_str(), "[%f,%f]", &n.position.x, &n.position.y);
        n.size = ImVec2(180, 0);
        n.header_color = static_cast<ImU32>(std::stoul(JsonValue(nj, "color")));
        auto inputs_str = JsonValue(nj, "inputs");
        for (auto& pj : JsonArray(inputs_str)) n.inputs.push_back(ParsePinJson(pj));
        auto outputs_str = JsonValue(nj, "outputs");
        for (auto& pj : JsonArray(outputs_str)) n.outputs.push_back(ParsePinJson(pj));
        s.nodes.push_back(std::move(n));
    }

    auto links_arr = JsonValue(json, "links");
    for (auto& lj : JsonArray(links_arr)) {
        Link l;
        l.id = std::atoi(JsonValue(lj, "id").c_str());
        l.from_pin = std::atoi(JsonValue(lj, "from").c_str());
        l.to_pin = std::atoi(JsonValue(lj, "to").c_str());
        s.links.push_back(l);
    }
    return true;
}

// ─── Compile: 节点图 → GLSL fragment shader ──────────────────────────────────

std::string GlslTypeStr(PinType t) {
    switch (t) {
        case PinType::Float: return "float";
        case PinType::Vec2: return "vec2";
        case PinType::Vec3: return "vec3";
        case PinType::Vec4: return "vec4";
        case PinType::Color: return "vec4";
        case PinType::Texture2D: return "sampler2D";
        case PinType::Sampler: return "sampler2D";
    }
    return "float";
}

std::string CompileGraphToGLSL(const ShaderGraphState& s) {
    std::ostringstream o;
    o << "// Auto-generated by DSEngine Shader Graph\n";
    o << "#version 330 core\n\n";
    o << "in vec2 v_uv;\n";
    o << "in vec3 v_normal;\n";
    o << "in vec3 v_world_pos;\n";
    o << "in vec3 v_tangent;\n";
    o << "in vec3 v_bitangent;\n\n";
    o << "out vec4 FragColor;\n\n";
    o << "uniform float u_time;\n\n";

    // 收集所有 Texture2D 输入节点用的 uniform
    int tex_unit = 0;
    std::unordered_map<int, std::string> tex_uniforms; // node_id -> uniform name
    for (auto& n : s.nodes) {
        if (n.name == "Texture Sample") {
            std::string uname = "u_tex" + std::to_string(tex_unit++);
            tex_uniforms[n.id] = uname;
            o << "uniform sampler2D " << uname << ";\n";
        }
    }
    o << "\n";

    // 拓扑排序
    std::unordered_map<int, int> pin_to_node;
    for (size_t i = 0; i < s.nodes.size(); ++i) {
        for (auto& p : s.nodes[i].inputs) pin_to_node[p.id] = static_cast<int>(i);
        for (auto& p : s.nodes[i].outputs) pin_to_node[p.id] = static_cast<int>(i);
    }
    std::unordered_map<int, std::unordered_set<int>> deps; // node_idx -> set of dependency node_idx
    for (auto& l : s.links) {
        auto it_to = pin_to_node.find(l.to_pin);
        auto it_from = pin_to_node.find(l.from_pin);
        if (it_to != pin_to_node.end() && it_from != pin_to_node.end()) {
            deps[it_to->second].insert(it_from->second);
        }
    }
    // Kahn's algorithm
    std::unordered_map<int, int> in_degree;
    for (size_t i = 0; i < s.nodes.size(); ++i) in_degree[static_cast<int>(i)] = 0;
    for (auto& [node_idx, dep_set] : deps) {
        in_degree[node_idx] = static_cast<int>(dep_set.size());
    }
    std::queue<int> q;
    for (auto& [idx, deg] : in_degree) {
        if (deg == 0) q.push(idx);
    }
    std::vector<int> topo_order;
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        topo_order.push_back(cur);
        for (size_t i = 0; i < s.nodes.size(); ++i) {
            auto it = deps.find(static_cast<int>(i));
            if (it != deps.end() && it->second.count(cur)) {
                it->second.erase(cur);
                --in_degree[static_cast<int>(i)];
                if (in_degree[static_cast<int>(i)] == 0) q.push(static_cast<int>(i));
            }
        }
    }

    o << "void main() {\n";

    // 为每个输出 pin 生成变量名
    std::unordered_map<int, std::string> pin_vars; // pin_id -> variable name
    // 为链接建立 from_pin → to_pin 映射
    std::unordered_map<int, int> input_source; // to_pin -> from_pin
    for (auto& l : s.links) input_source[l.to_pin] = l.from_pin;

    for (int ni : topo_order) {
        auto& n = s.nodes[ni];
        std::string prefix = "n" + std::to_string(n.id) + "_";

        if (n.name == "Float") {
            std::string var = prefix + "val";
            o << "    float " << var << " = " << n.outputs[0].default_value[0] << ";\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "Vec2") {
            std::string var = prefix + "val";
            o << "    vec2 " << var << " = vec2(" << n.outputs[0].default_value[0] << ", "
              << n.outputs[0].default_value[1] << ");\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "Vec3") {
            std::string var = prefix + "val";
            o << "    vec3 " << var << " = vec3(" << n.outputs[0].default_value[0] << ", "
              << n.outputs[0].default_value[1] << ", " << n.outputs[0].default_value[2] << ");\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "Color") {
            std::string var = prefix + "col";
            o << "    vec4 " << var << " = vec4(" << n.outputs[0].default_value[0] << ", "
              << n.outputs[0].default_value[1] << ", " << n.outputs[0].default_value[2] << ", "
              << n.outputs[0].default_value[3] << ");\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "UV") {
            std::string var = prefix + "uv";
            o << "    vec2 " << var << " = v_uv;\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "Time") {
            o << "    float " << prefix << "t = u_time;\n";
            if (n.outputs.size() > 0) pin_vars[n.outputs[0].id] = prefix + "t";
            if (n.outputs.size() > 1) { o << "    float " << prefix << "sin = sin(" << prefix << "t);\n"; pin_vars[n.outputs[1].id] = prefix + "sin"; }
            if (n.outputs.size() > 2) { o << "    float " << prefix << "cos = cos(" << prefix << "t);\n"; pin_vars[n.outputs[2].id] = prefix + "cos"; }
        } else if (n.name == "Texture Sample") {
            auto it = tex_uniforms.find(n.id);
            std::string uname = (it != tex_uniforms.end()) ? it->second : "u_tex0";
            // UV input
            std::string uv_expr = "v_uv";
            if (!n.inputs.empty()) {
                auto src = input_source.find(n.inputs[0].id);
                if (src != input_source.end() && pin_vars.count(src->second)) uv_expr = pin_vars[src->second];
            }
            std::string var = prefix + "rgba";
            o << "    vec4 " << var << " = texture(" << uname << ", " << uv_expr << ");\n";
            if (n.outputs.size() > 0) pin_vars[n.outputs[0].id] = var;
            if (n.outputs.size() > 1) pin_vars[n.outputs[1].id] = var + ".r";
            if (n.outputs.size() > 2) pin_vars[n.outputs[2].id] = var + ".g";
            if (n.outputs.size() > 3) pin_vars[n.outputs[3].id] = var + ".b";
            if (n.outputs.size() > 4) pin_vars[n.outputs[4].id] = var + ".a";
        } else if (n.name == "Add" || n.name == "Multiply") {
            std::string a_expr = std::to_string(n.inputs[0].default_value[0]);
            std::string b_expr = std::to_string(n.inputs[1].default_value[0]);
            auto sa = input_source.find(n.inputs[0].id);
            if (sa != input_source.end() && pin_vars.count(sa->second)) a_expr = pin_vars[sa->second];
            auto sb = input_source.find(n.inputs[1].id);
            if (sb != input_source.end() && pin_vars.count(sb->second)) b_expr = pin_vars[sb->second];
            std::string op = (n.name == "Add") ? " + " : " * ";
            std::string var = prefix + "out";
            o << "    float " << var << " = " << a_expr << op << b_expr << ";\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "Lerp") {
            std::string a_expr = "0.0", b_expr = "1.0", t_expr = "0.5";
            if (n.inputs.size() >= 3) {
                auto sa = input_source.find(n.inputs[0].id);
                if (sa != input_source.end() && pin_vars.count(sa->second)) a_expr = pin_vars[sa->second];
                auto sb = input_source.find(n.inputs[1].id);
                if (sb != input_source.end() && pin_vars.count(sb->second)) b_expr = pin_vars[sb->second];
                auto st = input_source.find(n.inputs[2].id);
                if (st != input_source.end() && pin_vars.count(st->second)) t_expr = pin_vars[st->second];
            }
            std::string var = prefix + "out";
            o << "    float " << var << " = mix(" << a_expr << ", " << b_expr << ", " << t_expr << ");\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "Fresnel") {
            std::string power = "5.0";
            if (!n.inputs.empty()) {
                auto sp = input_source.find(n.inputs[0].id);
                if (sp != input_source.end() && pin_vars.count(sp->second)) power = pin_vars[sp->second];
                else power = std::to_string(n.inputs[0].default_value[0]);
            }
            std::string var = prefix + "out";
            o << "    float " << var << " = pow(1.0 - max(dot(normalize(v_normal), normalize(-v_world_pos)), 0.0), " << power << ");\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "Normal Map") {
            // Texture input
            std::string tex_expr = "u_tex0";
            if (n.inputs.size() > 0) {
                auto st = input_source.find(n.inputs[0].id);
                if (st != input_source.end() && pin_vars.count(st->second)) tex_expr = pin_vars[st->second];
            }
            // UV input
            std::string uv_expr = "v_uv";
            if (n.inputs.size() > 1) {
                auto su = input_source.find(n.inputs[1].id);
                if (su != input_source.end() && pin_vars.count(su->second)) uv_expr = pin_vars[su->second];
            }
            // Strength
            std::string strength = "1.0";
            if (n.inputs.size() > 2) {
                auto ss = input_source.find(n.inputs[2].id);
                if (ss != input_source.end() && pin_vars.count(ss->second)) strength = pin_vars[ss->second];
                else strength = std::to_string(n.inputs[2].default_value[0]);
            }
            std::string var = prefix + "out";
            o << "    vec3 " << prefix << "raw = texture(" << tex_expr << ", " << uv_expr << ").rgb * 2.0 - 1.0;\n";
            o << "    " << prefix << "raw.xy *= " << strength << ";\n";
            o << "    mat3 " << prefix << "TBN = mat3(normalize(v_tangent), normalize(v_bitangent), normalize(v_normal));\n";
            o << "    vec3 " << var << " = normalize(" << prefix << "TBN * " << prefix << "raw);\n";
            if (!n.outputs.empty()) pin_vars[n.outputs[0].id] = var;
        } else if (n.name == "PBR Output") {
            // 收集各输入
            auto get_input = [&](int idx, const char* fallback) -> std::string {
                if (idx >= static_cast<int>(n.inputs.size())) return fallback;
                auto src = input_source.find(n.inputs[idx].id);
                if (src != input_source.end() && pin_vars.count(src->second)) return pin_vars[src->second];
                return fallback;
            };
            std::string base_color = get_input(0, "vec4(0.8, 0.8, 0.8, 1.0)");
            std::string metallic   = get_input(1, "0.0");
            std::string roughness  = get_input(2, "0.5");
            std::string emission   = get_input(4, "vec4(0.0)");
            std::string alpha      = get_input(6, "1.0");
            o << "    // PBR Output\n";
            o << "    vec4 base = " << base_color << ";\n";
            o << "    FragColor = vec4(base.rgb, " << alpha << ");\n";
            o << "    // metallic=" << metallic << " roughness=" << roughness
              << " emission=" << emission << "\n";
        }
    }

    o << "}\n";
    return o.str();
}

} // namespace

void DrawShaderGraphPanel(EditorContext& ctx) {
    ImGui::Begin("Shader Graph");

    auto& state = GetState();
    InitDefaultGraph(state);

    // Toolbar
    {
        ImGui::Text(MDI_ICON_PALETTE " Shader Graph");
        ImGui::SameLine(ImGui::GetWindowWidth() - 280);
        if (ImGui::Button("Compile")) {
            std::string glsl = CompileGraphToGLSL(state);
            // 输出到文件
            std::ofstream out("shader_graph_output.frag");
            if (out.is_open()) { out << glsl; out.close(); }
            // 也输出到控制台日志
            EditorLog(LogLevel::Info, "[ShaderGraph] Compiled GLSL (" + std::to_string(glsl.size()) + " chars) -> shader_graph_output.frag");
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply to Material")) {
            std::string glsl = CompileGraphToGLSL(state);
            
            // 创建自定义着色器
            auto* asset_mgr = dse::core::ServiceLocator::Instance().Get<AssetManager>();
            if (asset_mgr) {
                std::string shader_name = GenerateUniqueShaderName();
                auto shader = asset_mgr->LoadShader(shader_name, "", glsl);  // 顶点着色器为空，使用默认
                
                if (shader) {
                    EditorLog(LogLevel::Info, "[ShaderGraph] Created custom shader '" + shader_name + "' (handle=" + std::to_string(shader->GetHandle()) + ")");
                    
                    // 应用到当前选中的实体（如果有材质组件）
                    if (ctx.selected_entity != entt::null && ctx.registry.valid(ctx.selected_entity)) {
                        if (ctx.registry.all_of<dse::MeshRendererComponent>(ctx.selected_entity)) {
                            auto& mesh = ctx.registry.get<dse::MeshRendererComponent>(ctx.selected_entity);
                            mesh.shader_variant = shader_name;
                            EditorLog(LogLevel::Info, "[ShaderGraph] Applied shader '" + shader_name + "' to selected entity");
                        }
                    }
                } else {
                    EditorLog(LogLevel::Error, "[ShaderGraph] Failed to create custom shader");
                }
            } else {
                EditorLog(LogLevel::Error, "[ShaderGraph] AssetManager not available");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            std::string save_path;
#ifdef _WIN32
            char filename[MAX_PATH] = "shader_graph.dsg";
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Shader Graph (*.dsg)\0*.dsg\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = "dsg";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            if (GetSaveFileNameA(&ofn)) save_path = filename;
#else
            save_path = "shader_graph.dsg";
#endif
            if (!save_path.empty()) {
                std::string json = SerializeGraphJson(state);
                std::ofstream out(save_path);
                if (out.is_open()) {
                    out << json;
                    out.close();
                    EditorLog(LogLevel::Info, "[ShaderGraph] Saved to " + save_path + " (" + std::to_string(json.size()) + " bytes)");
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            std::string load_path;
#ifdef _WIN32
            char filename[MAX_PATH] = "";
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Shader Graph (*.dsg)\0*.dsg\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn)) load_path = filename;
#else
            load_path = "shader_graph.dsg";
#endif
            if (!load_path.empty()) {
                std::ifstream in(load_path);
                if (in.is_open()) {
                    std::string json((std::istreambuf_iterator<char>(in)),
                                      std::istreambuf_iterator<char>());
                    in.close();
                    if (DeserializeGraph(json, state)) {
                        EditorLog(LogLevel::Info, "[ShaderGraph] Loaded from " + load_path + " (" + std::to_string(state.nodes.size()) + " nodes, " + std::to_string(state.links.size()) + " links)");
                    }
                }
            }
        }
    }

    ImGui::Separator();

    // Canvas
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
    if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;

    ImGui::InvisibleButton("canvas", canvas_size,
                            ImGuiButtonFlags_MouseButtonLeft |
                            ImGuiButtonFlags_MouseButtonRight |
                            ImGuiButtonFlags_MouseButtonMiddle);
    bool canvas_hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Canvas background
    dl->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                       IM_COL32(25, 25, 30, 255));

    // Grid
    float grid_size = 20.0f * state.zoom;
    for (float x = std::fmod(state.scroll_offset.x, grid_size); x < canvas_size.x; x += grid_size) {
        dl->AddLine(ImVec2(canvas_pos.x + x, canvas_pos.y),
                     ImVec2(canvas_pos.x + x, canvas_pos.y + canvas_size.y),
                     IM_COL32(40, 40, 45, 255));
    }
    for (float y = std::fmod(state.scroll_offset.y, grid_size); y < canvas_size.y; y += grid_size) {
        dl->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + y),
                     ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + y),
                     IM_COL32(40, 40, 45, 255));
    }

    // Offset for scrolling
    ImVec2 offset(canvas_pos.x + state.scroll_offset.x, canvas_pos.y + state.scroll_offset.y);

    // Draw links first (behind nodes)
    for (auto& link : state.links) {
        Pin* from = FindPin(state, link.from_pin);
        Pin* to = FindPin(state, link.to_pin);
        if (!from || !to) continue;
        Node* from_node = FindPinOwner(state, link.from_pin);
        Node* to_node = FindPinOwner(state, link.to_pin);
        if (!from_node || !to_node) continue;

        ImVec2 p1 = GetPinPos(*from_node, *from, true);
        ImVec2 p2 = GetPinPos(*to_node, *to, false);
        p1.x += offset.x; p1.y += offset.y;
        p2.x += offset.x; p2.y += offset.y;

        bool is_selected_link = (link.id == state.selected_link);
        ImU32 link_color = is_selected_link ? IM_COL32(255, 200, 50, 255) : PinColor(from->type);
        float link_thick = is_selected_link ? 3.5f : 2.0f;
        DrawBezierLink(dl, p1, p2, link_color, link_thick);
    }

    // Draw link being created
    if (state.creating_link && state.link_start_pin >= 0) {
        Pin* from = FindPin(state, state.link_start_pin);
        Node* from_node = FindPinOwner(state, state.link_start_pin);
        if (from && from_node) {
            bool is_out = (from->kind == PinKind::Output);
            ImVec2 p1 = GetPinPos(*from_node, *from, is_out);
            p1.x += offset.x; p1.y += offset.y;
            ImVec2 p2 = ImGui::GetMousePos();
            if (is_out) DrawBezierLink(dl, p1, p2, PinColor(from->type), 2.0f);
            else DrawBezierLink(dl, p2, p1, PinColor(from->type), 2.0f);
        }
    }

    // Draw nodes
    for (int ni = 0; ni < static_cast<int>(state.nodes.size()); ni++) {
        auto& node = state.nodes[ni];
        ImVec2 node_pos(node.position.x + offset.x, node.position.y + offset.y);

        float header_h = 24.0f;
        float pin_h = 22.0f;
        float body_h = std::max(static_cast<float>(node.inputs.size()),
                                static_cast<float>(node.outputs.size())) * pin_h + 4.0f;
        // If both inputs and outputs, stack them
        if (!node.inputs.empty() && !node.outputs.empty()) {
            body_h = (static_cast<float>(node.inputs.size()) +
                      static_cast<float>(node.outputs.size())) * pin_h + 4.0f;
        } else {
            body_h = std::max(static_cast<float>(node.inputs.size()),
                              static_cast<float>(node.outputs.size())) * pin_h + 4.0f;
        }
        node.size.y = header_h + body_h;

        ImVec2 node_max(node_pos.x + node.size.x, node_pos.y + node.size.y);
        bool is_selected = (state.selected_node == ni);

        // Node shadow
        dl->AddRectFilled(ImVec2(node_pos.x + 3, node_pos.y + 3),
                          ImVec2(node_max.x + 3, node_max.y + 3),
                          IM_COL32(0, 0, 0, 80), 6.0f);

        // Node body
        dl->AddRectFilled(node_pos, node_max, IM_COL32(35, 35, 40, 240), 6.0f);

        // Header
        ImVec2 header_max(node_max.x, node_pos.y + header_h);
        dl->AddRectFilled(node_pos, header_max, node.header_color, 6.0f, ImDrawFlags_RoundCornersTop);
        dl->AddText(ImVec2(node_pos.x + 8, node_pos.y + 4), IM_COL32(255, 255, 255, 255),
                    node.name.c_str());

        // Selection border
        if (is_selected) {
            dl->AddRect(node_pos, node_max, IM_COL32(255, 200, 80, 255), 6.0f, 0, 2.0f);
        }

        // Draw input pins
        float py = node_pos.y + header_h + 4.0f;
        for (auto& pin : node.inputs) {
            ImVec2 pin_pos(node_pos.x, py + pin_h * 0.5f);
            dl->AddCircleFilled(pin_pos, 5.0f, PinColor(pin.type));
            dl->AddCircle(pin_pos, 5.0f, IM_COL32(0, 0, 0, 200), 0, 1.0f);
            dl->AddText(ImVec2(node_pos.x + 10, py), IM_COL32(200, 200, 200, 255), pin.name.c_str());

            // Hit test for link creation
            ImVec2 hit_min(pin_pos.x - 8, pin_pos.y - 8);
            ImVec2 hit_max(pin_pos.x + 8, pin_pos.y + 8);
            if (canvas_hovered && ImGui::IsMouseHoveringRect(hit_min, hit_max)) {
                if (ImGui::IsMouseClicked(0)) {
                    state.creating_link = true;
                    state.link_start_pin = pin.id;
                }
                if (ImGui::IsMouseReleased(0) && state.creating_link && state.link_start_pin != pin.id) {
                    // Complete link
                    Link lnk;
                    lnk.id = AllocId(state);
                    Pin* start = FindPin(state, state.link_start_pin);
                    if (start && start->kind == PinKind::Output) {
                        lnk.from_pin = state.link_start_pin;
                        lnk.to_pin = pin.id;
                        state.links.push_back(lnk);
                        state.graph_dirty = true;
                    }
                    state.creating_link = false;
                    state.link_start_pin = -1;
                }
            }
            py += pin_h;
        }

        // Draw output pins
        for (auto& pin : node.outputs) {
            ImVec2 pin_pos(node_pos.x + node.size.x, py + pin_h * 0.5f);
            dl->AddCircleFilled(pin_pos, 5.0f, PinColor(pin.type));
            dl->AddCircle(pin_pos, 5.0f, IM_COL32(0, 0, 0, 200), 0, 1.0f);
            ImVec2 text_size = ImGui::CalcTextSize(pin.name.c_str());
            dl->AddText(ImVec2(node_pos.x + node.size.x - 10 - text_size.x, py),
                        IM_COL32(200, 200, 200, 255), pin.name.c_str());

            // Hit test for link creation
            ImVec2 hit_min(pin_pos.x - 8, pin_pos.y - 8);
            ImVec2 hit_max(pin_pos.x + 8, pin_pos.y + 8);
            if (canvas_hovered && ImGui::IsMouseHoveringRect(hit_min, hit_max)) {
                if (ImGui::IsMouseClicked(0)) {
                    state.creating_link = true;
                    state.link_start_pin = pin.id;
                }
                if (ImGui::IsMouseReleased(0) && state.creating_link && state.link_start_pin != pin.id) {
                    Link lnk;
                    lnk.id = AllocId(state);
                    Pin* start = FindPin(state, state.link_start_pin);
                    if (start && start->kind == PinKind::Input) {
                        lnk.from_pin = pin.id;
                        lnk.to_pin = state.link_start_pin;
                        state.links.push_back(lnk);
                        state.graph_dirty = true;
                    }
                    state.creating_link = false;
                    state.link_start_pin = -1;
                }
            }
            py += pin_h;
        }

        // Node selection + dragging
        if (canvas_hovered && ImGui::IsMouseHoveringRect(node_pos, node_max)) {
            if (ImGui::IsMouseClicked(0)) {
                state.selected_node = ni;
                state.dragging_node = ni;
                state.drag_offset = ImVec2(ImGui::GetMousePos().x - node_pos.x,
                                            ImGui::GetMousePos().y - node_pos.y);
            }
        }
    }

    // Node dragging
    if (state.dragging_node >= 0 && state.dragging_node < static_cast<int>(state.nodes.size())) {
        if (ImGui::IsMouseDragging(0) && !state.creating_link) {
            auto& n = state.nodes[state.dragging_node];
            n.position.x = ImGui::GetMousePos().x - offset.x - state.drag_offset.x;
            n.position.y = ImGui::GetMousePos().y - offset.y - state.drag_offset.y;
        }
        if (ImGui::IsMouseReleased(0)) {
            state.dragging_node = -1;
        }
    }

    // Cancel link creation
    if (state.creating_link && ImGui::IsMouseReleased(0)) {
        state.creating_link = false;
        state.link_start_pin = -1;
    }

    // Canvas panning (middle mouse)
    if (canvas_hovered && ImGui::IsMouseDragging(2)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        state.scroll_offset.x += delta.x;
        state.scroll_offset.y += delta.y;
    }

    // Zoom with scroll wheel
    if (canvas_hovered && std::abs(ImGui::GetIO().MouseWheel) > 0.01f) {
        float old_zoom = state.zoom;
        state.zoom += ImGui::GetIO().MouseWheel * 0.1f;
        state.zoom = std::max(0.3f, std::min(state.zoom, 3.0f));
        // Adjust scroll to zoom toward mouse
        float factor = state.zoom / old_zoom;
        ImVec2 mp = ImGui::GetMousePos();
        state.scroll_offset.x = mp.x - canvas_pos.x - (mp.x - canvas_pos.x - state.scroll_offset.x) * factor;
        state.scroll_offset.y = mp.y - canvas_pos.y - (mp.y - canvas_pos.y - state.scroll_offset.y) * factor;
    }

    // Delete key: delete selected node or link
    if (canvas_hovered && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (state.selected_link >= 0) {
            state.links.erase(
                std::remove_if(state.links.begin(), state.links.end(),
                    [&](const Link& l) { return l.id == state.selected_link; }),
                state.links.end());
            state.selected_link = -1;
            state.graph_dirty = true;
        } else if (state.selected_node >= 0 && state.selected_node < static_cast<int>(state.nodes.size())) {
            auto& n = state.nodes[state.selected_node];
            state.links.erase(std::remove_if(state.links.begin(), state.links.end(),
                [&](const Link& l) {
                    for (auto& p : n.inputs) if (p.id == l.to_pin || p.id == l.from_pin) return true;
                    for (auto& p : n.outputs) if (p.id == l.to_pin || p.id == l.from_pin) return true;
                    return false;
                }), state.links.end());
            state.nodes.erase(state.nodes.begin() + state.selected_node);
            state.selected_node = -1;
            state.graph_dirty = true;
        }
    }

    // Link selection (click near bezier)
    if (canvas_hovered && ImGui::IsMouseClicked(0) && state.dragging_node < 0 && !state.creating_link) {
        ImVec2 mp = ImGui::GetMousePos();
        state.selected_link = -1;
        for (auto& link : state.links) {
            Pin* from = FindPin(state, link.from_pin);
            Pin* to = FindPin(state, link.to_pin);
            if (!from || !to) continue;
            Node* fn = FindPinOwner(state, link.from_pin);
            Node* tn = FindPinOwner(state, link.to_pin);
            if (!fn || !tn) continue;
            ImVec2 p1 = GetPinPos(*fn, *from, true);
            ImVec2 p2 = GetPinPos(*tn, *to, false);
            p1.x += offset.x; p1.y += offset.y;
            p2.x += offset.x; p2.y += offset.y;
            // Simple proximity test at midpoint
            ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            float dist = std::sqrt((mp.x - mid.x) * (mp.x - mid.x) + (mp.y - mid.y) * (mp.y - mid.y));
            if (dist < 12.0f) {
                state.selected_link = link.id;
                state.selected_node = -1;
                break;
            }
        }
    }

    // Right-click context menu: add node
    if (canvas_hovered && ImGui::IsMouseClicked(1)) {
        state.show_create_menu = true;
        state.create_menu_pos = ImGui::GetMousePos();
        ImGui::OpenPopup("AddNodeMenu");
    }

    if (ImGui::BeginPopup("AddNodeMenu")) {
        ImGui::Text("Add Node");
        ImGui::Separator();

        std::string last_cat;
        for (auto& tmpl : GetTemplates()) {
            if (tmpl.category != last_cat) {
                if (!last_cat.empty()) ImGui::Separator();
                ImGui::TextDisabled("%s", tmpl.category);
                last_cat = tmpl.category;
            }
            if (ImGui::MenuItem(tmpl.name)) {
                ImVec2 world_pos(state.create_menu_pos.x - offset.x,
                                 state.create_menu_pos.y - offset.y);
                state.nodes.push_back(CreateNode(state, tmpl.name, tmpl.category,
                                                  world_pos, tmpl.color,
                                                  tmpl.inputs, tmpl.outputs));
                state.graph_dirty = true;
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Delete Selected Node") && state.selected_node >= 0) {
            auto& n = state.nodes[state.selected_node];
            state.links.erase(std::remove_if(state.links.begin(), state.links.end(),
                [&](const Link& l) {
                    for (auto& p : n.inputs) if (p.id == l.to_pin || p.id == l.from_pin) return true;
                    for (auto& p : n.outputs) if (p.id == l.to_pin || p.id == l.from_pin) return true;
                    return false;
                }), state.links.end());
            state.nodes.erase(state.nodes.begin() + state.selected_node);
            state.selected_node = -1;
        }
        if (ImGui::MenuItem("Delete Selected Link") && state.selected_link >= 0) {
            state.links.erase(
                std::remove_if(state.links.begin(), state.links.end(),
                    [&](const Link& l) { return l.id == state.selected_link; }),
                state.links.end());
            state.selected_link = -1;
        }
        ImGui::Separator();
        ImGui::MenuItem("Show Properties", nullptr, &state.show_properties);
        ImGui::MenuItem("Show GLSL Preview", nullptr, &state.show_preview);

        ImGui::EndPopup();
    }

    // ─── Properties sidebar ─────────────────────────────────────────────────
    if (state.show_properties && state.selected_node >= 0 &&
        state.selected_node < static_cast<int>(state.nodes.size())) {
        ImGui::SetNextWindowSize(ImVec2(250, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Node Properties", &state.show_properties)) {
            auto& n = state.nodes[state.selected_node];
            ImGui::Text("Node: %s", n.name.c_str());
            ImGui::TextDisabled("Category: %s", n.category.c_str());
            ImGui::Separator();

            ImGui::Text("Position: %.0f, %.0f", n.position.x, n.position.y);

            // Editable default values for inputs
            if (!n.inputs.empty()) {
                ImGui::Text("Inputs:");
                for (auto& pin : n.inputs) {
                    ImGui::PushID(pin.id);
                    bool changed = false;
                    switch (pin.type) {
                        case PinType::Float:
                            ImGui::SetNextItemWidth(100);
                            changed = ImGui::DragFloat(pin.name.c_str(), &pin.default_value[0], 0.01f);
                            break;
                        case PinType::Vec2:
                            ImGui::SetNextItemWidth(160);
                            changed = ImGui::DragFloat2(pin.name.c_str(), pin.default_value, 0.01f);
                            break;
                        case PinType::Vec3:
                            ImGui::SetNextItemWidth(200);
                            changed = ImGui::DragFloat3(pin.name.c_str(), pin.default_value, 0.01f);
                            break;
                        case PinType::Vec4:
                        case PinType::Color:
                            changed = ImGui::ColorEdit4(pin.name.c_str(), pin.default_value);
                            break;
                        default:
                            ImGui::Text("%s", pin.name.c_str());
                            break;
                    }
                    if (changed) state.graph_dirty = true;
                    ImGui::PopID();
                }
            }

            // Editable default values for outputs (constants)
            if (!n.outputs.empty() && n.inputs.empty()) {
                ImGui::Text("Output Values:");
                for (auto& pin : n.outputs) {
                    ImGui::PushID(pin.id);
                    bool changed = false;
                    switch (pin.type) {
                        case PinType::Float:
                            ImGui::SetNextItemWidth(100);
                            changed = ImGui::DragFloat(pin.name.c_str(), &pin.default_value[0], 0.01f);
                            break;
                        case PinType::Color:
                            changed = ImGui::ColorEdit4(pin.name.c_str(), pin.default_value);
                            break;
                        case PinType::Vec2:
                            ImGui::SetNextItemWidth(160);
                            changed = ImGui::DragFloat2(pin.name.c_str(), pin.default_value, 0.01f);
                            break;
                        case PinType::Vec3:
                            ImGui::SetNextItemWidth(200);
                            changed = ImGui::DragFloat3(pin.name.c_str(), pin.default_value, 0.01f);
                            break;
                        default:
                            ImGui::Text("%s", pin.name.c_str());
                            break;
                    }
                    if (changed) state.graph_dirty = true;
                    ImGui::PopID();
                }
            }
        }
        ImGui::End();
    }

    // ─── GLSL Preview panel ─────────────────────────────────────────────────
    // Auto-compile with debounce
    if (state.show_preview && state.auto_compile && state.graph_dirty) {
        state.compile_timer += ImGui::GetIO().DeltaTime;
        if (state.compile_timer >= state.kCompileDelay) {
            state.preview_glsl = CompileGraphToGLSL(state);
            state.graph_dirty = false;
            state.compile_timer = 0.0f;
        }
    }

    if (state.show_preview) {
        ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("GLSL Preview", &state.show_preview)) {
            if (ImGui::Button("Refresh")) {
                state.preview_glsl = CompileGraphToGLSL(state);
                state.graph_dirty = false;
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto", &state.auto_compile);
            ImGui::SameLine();
            ImGui::TextDisabled("%d chars", static_cast<int>(state.preview_glsl.size()));
            if (state.graph_dirty && state.auto_compile) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "(pending...)");
            }
            ImGui::Separator();
            ImGui::BeginChild("glsl_code", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            if (!state.preview_glsl.empty()) {
                ImGui::TextUnformatted(state.preview_glsl.c_str(), state.preview_glsl.c_str() + state.preview_glsl.size());
            } else {
                ImGui::TextDisabled("Click 'Refresh' or enable 'Auto' to compile the graph.");
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    // Zoom indicator
    {
        char zoom_txt[32];
        snprintf(zoom_txt, sizeof(zoom_txt), "Zoom: %.0f%%", state.zoom * 100.0f);
        dl->AddText(ImVec2(canvas_pos.x + 8, canvas_pos.y + canvas_size.y - 18),
                    IM_COL32(150, 150, 150, 200), zoom_txt);
    }

    ImGui::End();
}

} // namespace dse::editor
