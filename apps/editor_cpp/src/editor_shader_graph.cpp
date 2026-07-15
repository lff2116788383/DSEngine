#include "editor_locale.h"
#include "editor_shader_graph.h"
#include "editor_icons.h"
#include "editor_console_panel.h"
#include "editor_context.h"

#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"
#include "engine/base/debug.h"
#include "engine/ecs/components_3d.h"
#include "engine/render/shader_graph/shader_graph_asset.h"
#include "engine/render/shader_graph/shader_graph_codegen.h"
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

#include "editor_panel_registry.h"

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
    // 3D Preview sphere
    bool show_3d_preview = true;
    float preview_rotation_y = 0.0f;
    float preview_rotation_x = 0.3f;
    bool preview_auto_rotate = true;
    float preview_light_dir[3] = {0.5f, 0.7f, 0.5f};
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
        // ─── Extended Math nodes ─────────────────────────────────────────
        {"Subtract", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "A", PinType::Float, PinKind::Input, {}},
          {0, "B", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Divide", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "A", PinType::Float, PinKind::Input, {1}},
          {0, "B", PinType::Float, PinKind::Input, {1}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Power", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "Base", PinType::Float, PinKind::Input, {2}},
          {0, "Exp", PinType::Float, PinKind::Input, {2}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Abs", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Negate", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Saturate", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"One Minus", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Clamp", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}},
          {0, "Min", PinType::Float, PinKind::Input, {0}},
          {0, "Max", PinType::Float, PinKind::Input, {1}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Step", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "Edge", PinType::Float, PinKind::Input, {0.5f}},
          {0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Smoothstep", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "Edge0", PinType::Float, PinKind::Input, {0}},
          {0, "Edge1", PinType::Float, PinKind::Input, {1}},
          {0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Sin", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Cos", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Floor", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Fract", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Min", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "A", PinType::Float, PinKind::Input, {}},
          {0, "B", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Max", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "A", PinType::Float, PinKind::Input, {}},
          {0, "B", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Dot", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "A", PinType::Vec3, PinKind::Input, {}},
          {0, "B", PinType::Vec3, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Cross", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "A", PinType::Vec3, PinKind::Input, {}},
          {0, "B", PinType::Vec3, PinKind::Input, {}}},
         {{0, "Result", PinType::Vec3, PinKind::Output, {}}}},
        {"Normalize", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Vec3, PinKind::Input, {}}},
         {{0, "Result", PinType::Vec3, PinKind::Output, {}}}},
        {"Length", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Vec3, PinKind::Input, {}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Remap", "Math", IM_COL32(80, 180, 80, 255),
         {{0, "In", PinType::Float, PinKind::Input, {}},
          {0, "InMin", PinType::Float, PinKind::Input, {0}},
          {0, "InMax", PinType::Float, PinKind::Input, {1}},
          {0, "OutMin", PinType::Float, PinKind::Input, {0}},
          {0, "OutMax", PinType::Float, PinKind::Input, {1}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        // ─── Vector operations ───────────────────────────────────────────
        {"Split", "Vector", IM_COL32(160, 120, 200, 255),
         {{0, "In", PinType::Vec4, PinKind::Input, {}}},
         {{0, "R", PinType::Float, PinKind::Output, {}},
          {0, "G", PinType::Float, PinKind::Output, {}},
          {0, "B", PinType::Float, PinKind::Output, {}},
          {0, "A", PinType::Float, PinKind::Output, {}}}},
        {"Combine", "Vector", IM_COL32(160, 120, 200, 255),
         {{0, "R", PinType::Float, PinKind::Input, {}},
          {0, "G", PinType::Float, PinKind::Input, {}},
          {0, "B", PinType::Float, PinKind::Input, {}},
          {0, "A", PinType::Float, PinKind::Input, {1}}},
         {{0, "RGBA", PinType::Vec4, PinKind::Output, {}},
          {0, "RGB", PinType::Vec3, PinKind::Output, {}}}},
        {"Swizzle XY", "Vector", IM_COL32(160, 120, 200, 255),
         {{0, "In", PinType::Vec3, PinKind::Input, {}}},
         {{0, "XY", PinType::Vec2, PinKind::Output, {}}}},
        {"Make Vec3", "Vector", IM_COL32(160, 120, 200, 255),
         {{0, "X", PinType::Float, PinKind::Input, {}},
          {0, "Y", PinType::Float, PinKind::Input, {}},
          {0, "Z", PinType::Float, PinKind::Input, {}}},
         {{0, "Result", PinType::Vec3, PinKind::Output, {}}}},
        // ─── UV operations ───────────────────────────────────────────────
        {"Tiling Offset", "UV", IM_COL32(180, 160, 60, 255),
         {{0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Tiling", PinType::Vec2, PinKind::Input, {1, 1, 0, 0}},
          {0, "Offset", PinType::Vec2, PinKind::Input, {0, 0, 0, 0}}},
         {{0, "Out", PinType::Vec2, PinKind::Output, {}}}},
        {"Rotate UV", "UV", IM_COL32(180, 160, 60, 255),
         {{0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Center", PinType::Vec2, PinKind::Input, {0.5f, 0.5f, 0, 0}},
          {0, "Angle", PinType::Float, PinKind::Input, {}}},
         {{0, "Out", PinType::Vec2, PinKind::Output, {}}}},
        {"Polar UV", "UV", IM_COL32(180, 160, 60, 255),
         {{0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Center", PinType::Vec2, PinKind::Input, {0.5f, 0.5f, 0, 0}}},
         {{0, "Out", PinType::Vec2, PinKind::Output, {}}}},
        {"Parallax Mapping", "UV", IM_COL32(180, 160, 60, 255),
         {{0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Height", PinType::Float, PinKind::Input, {}},
          {0, "Scale", PinType::Float, PinKind::Input, {0.04f}}},
         {{0, "UV", PinType::Vec2, PinKind::Output, {}}}},
        // ─── Procedural / Noise ──────────────────────────────────────────
        {"Noise Perlin", "Procedural", IM_COL32(60, 160, 120, 255),
         {{0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Scale", PinType::Float, PinKind::Input, {10}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Noise Voronoi", "Procedural", IM_COL32(60, 160, 120, 255),
         {{0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Scale", PinType::Float, PinKind::Input, {5}}},
         {{0, "Distance", PinType::Float, PinKind::Output, {}},
          {0, "Cell ID", PinType::Float, PinKind::Output, {}}}},
        {"Gradient Noise", "Procedural", IM_COL32(60, 160, 120, 255),
         {{0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Scale", PinType::Float, PinKind::Input, {8}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        {"Checkerboard", "Procedural", IM_COL32(60, 160, 120, 255),
         {{0, "UV", PinType::Vec2, PinKind::Input, {}},
          {0, "Scale", PinType::Float, PinKind::Input, {4}}},
         {{0, "Result", PinType::Float, PinKind::Output, {}}}},
        // ─── Effect nodes ────────────────────────────────────────────────
        {"Dissolve", "Effect", IM_COL32(200, 80, 60, 255),
         {{0, "Noise", PinType::Float, PinKind::Input, {}},
          {0, "Threshold", PinType::Float, PinKind::Input, {0.5f}},
          {0, "Edge Width", PinType::Float, PinKind::Input, {0.05f}},
          {0, "Edge Color", PinType::Color, PinKind::Input, {1, 0.5f, 0, 1}}},
         {{0, "Alpha", PinType::Float, PinKind::Output, {}},
          {0, "Edge Mask", PinType::Float, PinKind::Output, {}}}},
        {"Rim Light", "Effect", IM_COL32(200, 80, 60, 255),
         {{0, "Power", PinType::Float, PinKind::Input, {3}},
          {0, "Color", PinType::Color, PinKind::Input, {1, 1, 1, 1}}},
         {{0, "Result", PinType::Vec3, PinKind::Output, {}}}},
        {"Triplanar", "Effect", IM_COL32(200, 80, 60, 255),
         {{0, "Texture", PinType::Texture2D, PinKind::Input, {}},
          {0, "Sharpness", PinType::Float, PinKind::Input, {1}}},
         {{0, "Color", PinType::Vec4, PinKind::Output, {}}}},
        // ─── Input/Geometry ──────────────────────────────────────────────
        {"World Position", "Input", IM_COL32(180, 180, 60, 255),
         {},
         {{0, "Position", PinType::Vec3, PinKind::Output, {}}}},
        {"World Normal", "Input", IM_COL32(180, 180, 60, 255),
         {},
         {{0, "Normal", PinType::Vec3, PinKind::Output, {}}}},
        {"View Direction", "Input", IM_COL32(180, 180, 60, 255),
         {},
         {{0, "Dir", PinType::Vec3, PinKind::Output, {}}}},
        {"Screen Position", "Input", IM_COL32(180, 180, 60, 255),
         {},
         {{0, "ScreenUV", PinType::Vec2, PinKind::Output, {}}}},
        {"Vertex Color", "Input", IM_COL32(180, 180, 60, 255),
         {},
         {{0, "Color", PinType::Vec4, PinKind::Output, {}}}},
        {"Camera Distance", "Input", IM_COL32(180, 180, 60, 255),
         {},
         {{0, "Distance", PinType::Float, PinKind::Output, {}}}},
    };
    return templates;
}

// ─── 与共享 .dshadergraph 资产契约(engine/render/shader_graph)的互转 ──────────
// 编辑器本地 PinType 与 shadergraph::PinType 的枚举顺序一致，逐项映射保证稳定。

shadergraph::PinType ToAssetPinType(PinType t) {
    switch (t) {
        case PinType::Float:     return shadergraph::PinType::Float;
        case PinType::Vec2:      return shadergraph::PinType::Vec2;
        case PinType::Vec3:      return shadergraph::PinType::Vec3;
        case PinType::Vec4:      return shadergraph::PinType::Vec4;
        case PinType::Color:     return shadergraph::PinType::Color;
        case PinType::Texture2D: return shadergraph::PinType::Texture2D;
        case PinType::Sampler:   return shadergraph::PinType::Sampler;
    }
    return shadergraph::PinType::Float;
}

PinType FromAssetPinType(shadergraph::PinType t) {
    switch (t) {
        case shadergraph::PinType::Float:     return PinType::Float;
        case shadergraph::PinType::Vec2:      return PinType::Vec2;
        case shadergraph::PinType::Vec3:      return PinType::Vec3;
        case shadergraph::PinType::Vec4:      return PinType::Vec4;
        case shadergraph::PinType::Color:     return PinType::Color;
        case shadergraph::PinType::Texture2D: return PinType::Texture2D;
        case shadergraph::PinType::Sampler:   return PinType::Sampler;
    }
    return PinType::Float;
}

shadergraph::PinDesc ToAssetPin(const Pin& p) {
    shadergraph::PinDesc d;
    d.id = p.id;
    d.name = p.name;
    d.type = ToAssetPinType(p.type);
    d.kind = p.kind == PinKind::Input ? shadergraph::PinKind::Input
                                      : shadergraph::PinKind::Output;
    for (int i = 0; i < 4; ++i) d.default_value[i] = p.default_value[i];
    return d;
}

Pin FromAssetPin(const shadergraph::PinDesc& d) {
    Pin p;
    p.id = d.id;
    p.name = d.name;
    p.type = FromAssetPinType(d.type);
    p.kind = d.kind == shadergraph::PinKind::Input ? PinKind::Input : PinKind::Output;
    for (int i = 0; i < 4; ++i) p.default_value[i] = d.default_value[i];
    return p;
}

shadergraph::ShaderGraphAsset ToAsset(const ShaderGraphState& s) {
    shadergraph::ShaderGraphAsset a;
    a.next_id = s.next_id;
    for (const auto& n : s.nodes) {
        shadergraph::NodeDesc nd;
        nd.id = n.id;
        nd.name = n.name;
        nd.category = n.category;
        nd.pos[0] = n.position.x;
        nd.pos[1] = n.position.y;
        nd.header_color = static_cast<uint32_t>(n.header_color);
        for (const auto& p : n.inputs) nd.inputs.push_back(ToAssetPin(p));
        for (const auto& p : n.outputs) nd.outputs.push_back(ToAssetPin(p));
        a.nodes.push_back(std::move(nd));
    }
    for (const auto& l : s.links) {
        a.links.push_back({l.id, l.from_pin, l.to_pin});
    }
    return a;
}

void FromAsset(const shadergraph::ShaderGraphAsset& a, ShaderGraphState& s) {
    s.nodes.clear();
    s.links.clear();
    s.next_id = a.next_id < 100 ? 100 : a.next_id;
    s.initialized = true;
    for (const auto& nd : a.nodes) {
        Node n;
        n.id = nd.id;
        n.name = nd.name;
        n.category = nd.category;
        n.position = ImVec2(nd.pos[0], nd.pos[1]);
        n.size = ImVec2(180, 0);
        n.header_color = static_cast<ImU32>(nd.header_color);
        for (const auto& p : nd.inputs) n.inputs.push_back(FromAssetPin(p));
        for (const auto& p : nd.outputs) n.outputs.push_back(FromAssetPin(p));
        s.nodes.push_back(std::move(n));
    }
    for (const auto& l : a.links) {
        Link lnk;
        lnk.id = l.id;
        lnk.from_pin = l.from_pin;
        lnk.to_pin = l.to_pin;
        s.links.push_back(lnk);
    }
}

// ─── Compile: 节点图 → GLSL fragment shader ──────────────────────────────────


// 配套顶点着色器：属性 location/语义与引擎网格顶点契约（forward_pbr.vert / GpuMeshVertex）一致，
// 透传片元所需 varying（含顶点色 v_color）。DrawShaded 已把顶点预变换到世界空间，故此处直接透传位置。
// 仅 OpenGL 后端（#version 430，PerFrame UBO 绑定点 0 与 MeshRenderer 逐 draw 绑定一致）。
std::string CompileGraphVertexGLSL() {
    // GLSL 顶点阶段与图无关（属性/varying 契约固定），直接取共享 codegen 的顶点输出。
    shadergraph::ShaderGraphAsset empty;
    return shadergraph::GenerateShader(empty, shadergraph::ShaderTarget::GLSL).vertex;
}

std::string CompileGraphToGLSL(const ShaderGraphState& s) {
    // 统一走引擎共享 codegen（与 Export HLSL/Vulkan/WebGL2/WebGPU 同一条路径），
    // 不再维护编辑器本地的 GLSL 发射器（节点覆盖为共享版的真子集）。
    // 仅 OpenGL 后端（#version 430）；图非法时返回空串，由调用方处理。
    auto res = shadergraph::GenerateShader(ToAsset(s), shadergraph::ShaderTarget::GLSL);
    return res.fragment;
}

} // namespace

int ShaderGraphNodeCount() { return static_cast<int>(GetState().nodes.size()); }
int ShaderGraphLinkCount() { return static_cast<int>(GetState().links.size()); }
void ShaderGraphResetGraph() {
    auto& s = GetState();
    s = ShaderGraphState{};
    InitDefaultGraph(s);
}

void DrawShaderGraphPanel(EditorContext& ctx) {
    ImGui::Begin("Shader Graph");

    auto& state = GetState();
    InitDefaultGraph(state);

    // Toolbar
    {
        ImGui::Text(MDI_ICON_PALETTE " Shader Graph");
        ImGui::SameLine(ImGui::GetWindowWidth() - 280);
        if (ImGui::Button(T("Compile"))) {
            std::string glsl = CompileGraphToGLSL(state);
            // 输出到文件
            std::ofstream out("shader_graph_output.frag");
            if (out.is_open()) { out << glsl; out.close(); }
            // 也输出到控制台日志
            EditorLog(LogLevel::Info, "[ShaderGraph] Compiled GLSL (" + std::to_string(glsl.size()) + " chars) -> shader_graph_output.frag");
        }
        // 通过引擎共享 codegen 从同一份节点图产出各后端源码。纯导出、不改材质。
        auto export_shader = [&](shadergraph::ShaderTarget target, const char* filename) {
            const char* lang = shadergraph::ShaderTargetName(target);
            auto res = shadergraph::GenerateShader(ToAsset(state), target);
            if (res.ok) {
                std::ofstream vout(filename);
                if (vout.is_open()) { vout << res.vertex << "\n" << res.fragment; vout.close(); }
                EditorLog(LogLevel::Info, std::string("[ShaderGraph] Exported ") + lang + " (vs " +
                          std::to_string(res.vertex.size()) + " + fs " +
                          std::to_string(res.fragment.size()) + " chars) -> " + filename);
                for (const auto& w : res.warnings)
                    EditorLog(LogLevel::Warning, "[ShaderGraph] " + w);
            } else {
                for (const auto& e : res.errors)
                    EditorLog(LogLevel::Error, std::string("[ShaderGraph] ") + lang + " export failed: " + e);
            }
        };
        ImGui::SameLine();
        if (ImGui::Button(T("Export HLSL")))
            export_shader(shadergraph::ShaderTarget::HLSL, "shader_graph_output.hlsl");
        ImGui::SameLine();
        if (ImGui::Button(T("Export Vulkan")))
            export_shader(shadergraph::ShaderTarget::GLSL_VULKAN, "shader_graph_output.vk.glsl");
        ImGui::SameLine();
        if (ImGui::Button(T("Export WebGL2")))
            export_shader(shadergraph::ShaderTarget::GLSL_ES, "shader_graph_output.webgl2.glsl");
        ImGui::SameLine();
        if (ImGui::Button(T("Export WebGPU")))
            export_shader(shadergraph::ShaderTarget::WGSL, "shader_graph_output.wgsl");
        ImGui::SameLine();
        // Export SPIR-V：经引擎 glslang 把 Vulkan GLSL 450 编译为真实 SPIR-V 二进制。
        // 仅当构建链接了 glslang 时可用；否则如实提示（不产伪二进制）。
        if (ImGui::Button(T("Export SPIR-V"))) {
            auto write_spv = [](const char* filename, const std::vector<uint32_t>& words) {
                std::ofstream out(filename, std::ios::binary);
                if (out.is_open())
                    out.write(reinterpret_cast<const char*>(words.data()),
                              static_cast<std::streamsize>(words.size() * sizeof(uint32_t)));
            };
            auto spv = shadergraph::GenerateSpirv(ToAsset(state));
            if (!spv.available) {
                EditorLog(LogLevel::Warning,
                          "[ShaderGraph] SPIR-V export unavailable: build without glslang (DSE_HAS_GLSLANG)");
            } else if (spv.ok) {
                write_spv("shader_graph_output.vert.spv", spv.vertex_spirv);
                write_spv("shader_graph_output.frag.spv", spv.fragment_spirv);
                EditorLog(LogLevel::Info,
                          "[ShaderGraph] Exported SPIR-V (vs " +
                          std::to_string(spv.vertex_spirv.size() * 4) + "B + fs " +
                          std::to_string(spv.fragment_spirv.size() * 4) +
                          "B) -> shader_graph_output.{vert,frag}.spv");
            } else {
                for (const auto& e : spv.errors)
                    EditorLog(LogLevel::Error, std::string("[ShaderGraph] SPIR-V export failed: ") + e);
            }
        }
        ImGui::SameLine();
        // Export DXBC：经引擎 d3dcompiler 把 HLSL SM5 编译为真实 DXBC 字节码（无需 GPU）。
        // 仅 Windows + D3D11 构建可用；否则如实提示（不产伪二进制）。
        if (ImGui::Button(T("Export DXBC"))) {
            auto write_bin = [](const char* filename, const std::vector<uint8_t>& bytes) {
                std::ofstream out(filename, std::ios::binary);
                if (out.is_open())
                    out.write(reinterpret_cast<const char*>(bytes.data()),
                              static_cast<std::streamsize>(bytes.size()));
            };
            auto dxbc = shadergraph::GenerateDxbc(ToAsset(state));
            if (!dxbc.available) {
                EditorLog(LogLevel::Warning,
                          "[ShaderGraph] DXBC export unavailable: build without D3D11/d3dcompiler");
            } else if (dxbc.ok) {
                write_bin("shader_graph_output.vs.dxbc", dxbc.vertex_dxbc);
                write_bin("shader_graph_output.ps.dxbc", dxbc.fragment_dxbc);
                EditorLog(LogLevel::Info,
                          "[ShaderGraph] Exported DXBC (vs " +
                          std::to_string(dxbc.vertex_dxbc.size()) + "B + ps " +
                          std::to_string(dxbc.fragment_dxbc.size()) +
                          "B) -> shader_graph_output.{vs,ps}.dxbc");
            } else {
                for (const auto& e : dxbc.errors)
                    EditorLog(LogLevel::Error, std::string("[ShaderGraph] DXBC export failed: ") + e);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Apply to Material"))) {
            // 生成配套顶点着色器 + 片元着色器（GLSL），两段一起送入 AssetManager 编译链接为可用程序。
            // 仅 OpenGL 后端能从 GLSL 源码编出有效句柄；其余后端返回 nullptr（材质保持原样，不假成功）。
            std::string vert_glsl = CompileGraphVertexGLSL();
            std::string frag_glsl = CompileGraphToGLSL(state);

            // 创建自定义着色器
            auto* asset_mgr = dse::core::ServiceLocator::Instance().Get<AssetManager>();
            if (asset_mgr) {
                std::string shader_name = GenerateUniqueShaderName();
                auto shader = asset_mgr->LoadShader(shader_name, vert_glsl, frag_glsl);

                if (shader && shader->GetHandle()) {
                    // AssetManager 只持有 weak_ptr；本 session 期间保活已应用的自定义着色器，
                    // 否则程序会被析构（GL 句柄删除）、运行时 GetShaderHandle 查不到。
                    static std::vector<decltype(shader)> s_applied_shaders;
                    s_applied_shaders.push_back(shader);
                    EditorLog(LogLevel::Info, "[ShaderGraph] Created custom shader '" + shader_name + "' (handle=" + std::to_string(shader->GetHandle().raw()) + ")");

                    // 应用到当前选中的实体（如果有材质组件）
                    if (ctx.selected_entity != entt::null && ctx.registry.valid(ctx.selected_entity)) {
                        if (ctx.registry.all_of<dse::MeshRendererComponent>(ctx.selected_entity)) {
                            auto& mesh = ctx.registry.get<dse::MeshRendererComponent>(ctx.selected_entity);
                            mesh.shader_variant = shader_name;
                            EditorLog(LogLevel::Info, "[ShaderGraph] Applied shader '" + shader_name + "' to selected entity");
                        }
                    }
                } else {
                    EditorLog(LogLevel::Error, "[ShaderGraph] Failed to create custom shader (GLSL custom shaders require the OpenGL backend)");
                }
            } else {
                EditorLog(LogLevel::Error, "[ShaderGraph] AssetManager not available");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Save"))) {
            std::string save_path;
#ifdef _WIN32
            char filename[MAX_PATH] = "shader_graph.dshadergraph";
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Shader Graph (*.dshadergraph)\0*.dshadergraph\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = "dshadergraph";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            if (GetSaveFileNameA(&ofn)) save_path = filename;
#else
            save_path = "shader_graph.dshadergraph";
#endif
            if (!save_path.empty()) {
                shadergraph::ShaderGraphDiagnostics diag;
                if (shadergraph::SaveShaderGraphToFile(ToAsset(state), save_path, diag)) {
                    EditorLog(LogLevel::Info, "[ShaderGraph] Saved to " + save_path +
                              " (schema v" + std::to_string(shadergraph::kShaderGraphSchemaVersion) + ")");
                } else {
                    std::string msg = diag.errors.empty() ? "unknown error" : diag.errors.front();
                    EditorLog(LogLevel::Error, "[ShaderGraph] Save failed: " + msg);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Load"))) {
            std::string load_path;
#ifdef _WIN32
            char filename[MAX_PATH] = "";
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Shader Graph (*.dshadergraph)\0*.dshadergraph\0Legacy Shader Graph (*.dsg)\0*.dsg\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn)) load_path = filename;
#else
            load_path = "shader_graph.dshadergraph";
#endif
            if (!load_path.empty()) {
                shadergraph::ShaderGraphAsset asset;
                shadergraph::ShaderGraphDiagnostics diag;
                if (shadergraph::LoadShaderGraphFromFile(load_path, asset, diag)) {
                    FromAsset(asset, state);
                    if (diag.migrated) {
                        EditorLog(LogLevel::Warning, "[ShaderGraph] Migrated legacy (unversioned) graph from " + load_path);
                    }
                    EditorLog(LogLevel::Info, "[ShaderGraph] Loaded from " + load_path + " (" + std::to_string(state.nodes.size()) + " nodes, " + std::to_string(state.links.size()) + " links)");
                } else {
                    std::string msg = diag.errors.empty() ? "unknown error" : diag.errors.front();
                    EditorLog(LogLevel::Error, "[ShaderGraph] Load failed: " + msg);
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
        if (ImGui::MenuItem(T("Delete Selected Node")) && state.selected_node >= 0) {
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
        if (ImGui::MenuItem(T("Delete Selected Link")) && state.selected_link >= 0) {
            state.links.erase(
                std::remove_if(state.links.begin(), state.links.end(),
                    [&](const Link& l) { return l.id == state.selected_link; }),
                state.links.end());
            state.selected_link = -1;
        }
        ImGui::Separator();
        ImGui::MenuItem(T("Show Properties"), nullptr, &state.show_properties);
        ImGui::MenuItem(T("Show GLSL Preview"), nullptr, &state.show_preview);

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
            if (ImGui::Button(T("Refresh"))) {
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

    // ─── 3D PBR Preview Sphere ──────────────────────────────────────────────
    if (state.show_3d_preview) {
        ImGui::SetNextWindowSize(ImVec2(220, 280), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Shader Preview", &state.show_3d_preview)) {
            if (state.preview_auto_rotate) {
                state.preview_rotation_y += ImGui::GetIO().DeltaTime * 0.5f;
            }
            ImGui::Checkbox("Auto Rotate", &state.preview_auto_rotate);
            ImGui::SliderFloat("Rot Y", &state.preview_rotation_y, -3.14159f, 3.14159f);
            ImGui::SliderFloat("Rot X", &state.preview_rotation_x, -1.5f, 1.5f);
            ImGui::SliderFloat3("Light", state.preview_light_dir, -1.0f, 1.0f);
            ImGui::Separator();

            // Software-rendered PBR sphere preview
            ImVec2 preview_pos = ImGui::GetCursorScreenPos();
            const float radius = 64.0f;
            ImVec2 center(preview_pos.x + radius + 8, preview_pos.y + radius + 4);
            ImDrawList* pdl = ImGui::GetWindowDrawList();

            // Gather material properties from PBR Output node defaults
            float base_r = 0.8f, base_g = 0.8f, base_b = 0.8f;
            float metallic_val = 0.0f, roughness_val = 0.5f;
            for (auto& nd : state.nodes) {
                if (nd.name == "PBR Output") {
                    if (nd.inputs.size() > 0) { base_r = nd.inputs[0].default_value[0]; base_g = nd.inputs[0].default_value[1]; base_b = nd.inputs[0].default_value[2]; }
                    if (nd.inputs.size() > 1) metallic_val = nd.inputs[1].default_value[0];
                    if (nd.inputs.size() > 2) roughness_val = nd.inputs[2].default_value[0];
                    break;
                }
            }

            // Light direction (normalized)
            float lx = state.preview_light_dir[0], ly = state.preview_light_dir[1], lz = state.preview_light_dir[2];
            float llen = std::sqrt(lx*lx + ly*ly + lz*lz);
            if (llen > 0.001f) { lx /= llen; ly /= llen; lz /= llen; }

            // Render sphere pixel by pixel (low-res for performance)
            const int res = 32;
            float pixel_size = radius * 2.0f / static_cast<float>(res);
            float cos_ry = std::cos(state.preview_rotation_y), sin_ry = std::sin(state.preview_rotation_y);
            float cos_rx = std::cos(state.preview_rotation_x), sin_rx = std::sin(state.preview_rotation_x);

            for (int py_i = 0; py_i < res; py_i++) {
                for (int px_i = 0; px_i < res; px_i++) {
                    float u = (static_cast<float>(px_i) + 0.5f) / static_cast<float>(res) * 2.0f - 1.0f;
                    float v = (static_cast<float>(py_i) + 0.5f) / static_cast<float>(res) * 2.0f - 1.0f;
                    float r2 = u * u + v * v;
                    if (r2 > 1.0f) continue;
                    float nz_local = std::sqrt(1.0f - r2);
                    float nx = u, ny = -v, nz = nz_local;
                    // Rotate normal
                    float nx2 = nx * cos_ry + nz * sin_ry;
                    float nz2 = -nx * sin_ry + nz * cos_ry;
                    nx = nx2; nz = nz2;
                    float ny2 = ny * cos_rx - nz * sin_rx;
                    float nz3 = ny * sin_rx + nz * cos_rx;
                    ny = ny2; nz = nz3;
                    // Lambertian diffuse
                    float ndl = std::max(0.0f, nx * lx + ny * ly + nz * lz);
                    // Specular (Blinn-Phong approx for preview)
                    float vx = 0, vy = 0, vz = 1; // view dir
                    float hx = lx + vx, hy = ly + vy, hz = lz + vz;
                    float hlen = std::sqrt(hx*hx + hy*hy + hz*hz);
                    if (hlen > 0.001f) { hx /= hlen; hy /= hlen; hz /= hlen; }
                    float ndh = std::max(0.0f, nx * hx + ny * hy + nz * hz);
                    float spec_power = 2.0f / (roughness_val * roughness_val + 0.001f);
                    float spec = std::pow(ndh, spec_power) * (1.0f - roughness_val);
                    // Mix based on metallic
                    float diff_r = base_r * ndl * (1.0f - metallic_val);
                    float diff_g = base_g * ndl * (1.0f - metallic_val);
                    float diff_b = base_b * ndl * (1.0f - metallic_val);
                    float spec_r = (metallic_val * base_r + (1.0f - metallic_val) * 0.04f) * spec;
                    float spec_g = (metallic_val * base_g + (1.0f - metallic_val) * 0.04f) * spec;
                    float spec_b = (metallic_val * base_b + (1.0f - metallic_val) * 0.04f) * spec;
                    // Ambient
                    float amb = 0.03f;
                    float cr = std::min(1.0f, diff_r + spec_r + amb * base_r);
                    float cg = std::min(1.0f, diff_g + spec_g + amb * base_g);
                    float cb = std::min(1.0f, diff_b + spec_b + amb * base_b);
                    // Gamma
                    cr = std::pow(cr, 1.0f / 2.2f);
                    cg = std::pow(cg, 1.0f / 2.2f);
                    cb = std::pow(cb, 1.0f / 2.2f);
                    ImU32 col = IM_COL32(static_cast<int>(cr * 255), static_cast<int>(cg * 255), static_cast<int>(cb * 255), 255);
                    ImVec2 pmin(center.x + u * radius - pixel_size * 0.5f, center.y + v * radius - pixel_size * 0.5f);
                    ImVec2 pmax(pmin.x + pixel_size, pmin.y + pixel_size);
                    pdl->AddRectFilled(pmin, pmax, col);
                }
            }
            ImGui::Dummy(ImVec2(radius * 2 + 16, radius * 2 + 8));
        }
        ImGui::End();
    }

    ImGui::End();
}

// P0-6 self-registration: data-driven; editor_app binds visibility by id.
DSE_EDITOR_PANEL([](dse::editor::PanelRegistry& reg) {
    dse::editor::PanelEntry e;
    e.id = "shader_graph";
    e.display_name = "Shader Graph";
    e.category = "Tool";
    e.menu_icon = MDI_ICON_PALETTE;
    e.order = 190;
    e.draw = [](dse::editor::EditorContext& ctx) { DrawShaderGraphPanel(ctx); };
    reg.Register(std::move(e));
});

} // namespace dse::editor
