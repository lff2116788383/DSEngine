#include "editor_shader_graph.h"
#include "editor_icons.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace dse::editor {

namespace {

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

} // namespace

void DrawShaderGraphPanel() {
    ImGui::Begin("Shader Graph");

    auto& state = GetState();
    InitDefaultGraph(state);

    // Toolbar
    {
        ImGui::Text(MDI_ICON_PALETTE " Shader Graph");
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        if (ImGui::Button("Compile")) {
            // TODO: Generate DSSL/GLSL from graph
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            // TODO: Serialize graph to JSON
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            // TODO: Load graph from JSON
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

        DrawBezierLink(dl, p1, p2, PinColor(from->type), 2.0f);
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
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Delete Selected") && state.selected_node >= 0) {
            // Remove links connected to this node
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

        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace dse::editor
