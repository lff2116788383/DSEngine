#pragma once

/**
 * @file editor_blueprint.h
 * @brief Complete Blueprint system - asset, variable panel, node registry, function graphs
 *
 * Architecture:
 *   BlueprintAsset (.dbp) → BlueprintCompiler → ByteCode / Lua
 *   BlueprintComponent (ECS) holds a BlueprintInstance referencing the asset
 */

#include "editor_context.h"
#include "engine/reflect/reflect.h"
#include "imgui.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace dse::editor::bp {

// ─── Variable type system (reuses reflect::FieldType) ───────────────────────

enum class BpVarType {
    Bool,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Entity,
    Array,   // array of another BpVarType
};

const char* BpVarTypeName(BpVarType t);
BpVarType BpVarTypeFromName(const char* name);

struct BpVariable {
    std::string name;
    BpVarType type = BpVarType::Float;
    BpVarType array_element_type = BpVarType::Float; // when type == Array

    // Default values
    bool   default_bool = false;
    int    default_int = 0;
    float  default_float = 0.0f;
    char   default_string[128] = "";
    float  default_vec[4] = {0, 0, 0, 0};

    bool is_exposed = false; // exposed to Inspector on entity
};

// ─── Pin / Node / Link (enhanced from visual_script) ────────────────────────

enum class BpPinType { Flow, Bool, Int, Float, String, Vec2, Vec3, Vec4, Entity, Array, Any, Wildcard };
enum class BpPinKind { Input, Output };

const char* BpPinTypeName(BpPinType t);
ImU32 BpPinColor(BpPinType t);

struct BpPin {
    int id = 0;
    std::string name;
    BpPinType type = BpPinType::Any;
    BpPinKind kind = BpPinKind::Input;
    float default_float = 0.0f;
    float default_vec[4] = {0, 0, 0, 0};
    char  default_string[64] = "";
    bool  default_bool = false;
    int   default_int = 0;
};

struct BpNode {
    int id = 0;
    std::string name;
    std::string category;
    ImVec2 position{0, 0};
    ImVec2 size{160, 80};
    std::vector<BpPin> inputs;
    std::vector<BpPin> outputs;
    ImU32 header_color = IM_COL32(100, 100, 100, 255);
    std::string code_template; // Lua: {inputN}/{outputN} placeholders
    std::string comment;       // user annotation
};

struct BpLink {
    int id = 0;
    int from_pin = 0;
    int to_pin = 0;
};

// ─── Node Registry (data-driven) ───────────────────────────────────────────

struct NodeTemplate {
    std::string name;
    std::string category;       // "Event", "Math", "Flow", "ECS", "Input", "Physics", etc.
    std::string subcategory;    // "Arithmetic", "Trigonometry", etc.
    ImU32 header_color = IM_COL32(100, 100, 100, 255);
    std::vector<BpPin> inputs;
    std::vector<BpPin> outputs;
    std::string code_template;
    std::string description;
};

class NodeRegistry {
public:
    static NodeRegistry& Get();
    void RegisterDefaults();
    void Register(const NodeTemplate& tmpl);
    const std::vector<NodeTemplate>& All() const { return templates_; }
    const std::vector<std::string>& Categories() const { return categories_; }
    const NodeTemplate* Find(const std::string& name) const;
private:
    std::vector<NodeTemplate> templates_;
    std::vector<std::string> categories_;
    std::unordered_map<std::string, size_t> name_index_;
};

// ─── Function Graph ────────────────────────────────────────────────────────

struct BpFunctionGraph {
    std::string name;
    std::vector<BpNode> nodes;
    std::vector<BpLink> links;
    std::vector<BpPin> input_params;   // function parameters (shown as input node)
    std::vector<BpPin> output_params;  // return values (shown as output node)
    bool is_pure = false;              // pure = no side effects, no flow pins
    int next_id = 1;
};

// ─── Interface / Contract ──────────────────────────────────────────────────

struct BpInterfaceFunction {
    std::string name;
    std::vector<BpPin> inputs;
    std::vector<BpPin> outputs;
};

struct BpInterface {
    std::string name;
    std::vector<BpInterfaceFunction> functions;
};

// ─── Blueprint Asset ───────────────────────────────────────────────────────

struct BlueprintAsset {
    std::string name;
    std::string file_path;   // .dbp absolute path
    int version = 1;

    // Variables
    std::vector<BpVariable> variables;

    // Graphs: event graph (index 0) + user function graphs
    std::vector<BpFunctionGraph> graphs;

    // Interfaces this blueprint implements
    std::vector<std::string> implemented_interfaces;

    // Metadata
    std::string description;
    std::string author;
};

// ─── Blueprint Editor State ────────────────────────────────────────────────

struct BlueprintEditorState {
    BlueprintAsset asset;
    int active_graph_index = 0;       // which graph tab is active
    int selected_node = -1;
    int selected_link = -1;
    int selected_variable = -1;

    // Link creation
    bool creating_link = false;
    int link_start_pin = -1;

    // Context menu
    bool show_create_menu = false;
    ImVec2 create_menu_pos{0, 0};

    // Compilation
    std::string generated_lua;
    std::string compilation_errors;
    bool auto_compile = true;
    bool dirty = false;

    // Canvas state
    float zoom = 1.0f;
    ImVec2 scroll_offset{0, 0};
};

// ─── Public API ────────────────────────────────────────────────────────────

void InitBlueprintSystem();
void DrawBlueprintEditor(EditorContext& ctx);

BlueprintEditorState& GetBlueprintEditorState();

// Serialization
bool SaveBlueprintAsset(const BlueprintAsset& asset, const std::string& path);
bool LoadBlueprintAsset(BlueprintAsset& asset, const std::string& path);

// Helpers
inline BpPin MkPin(const char* name, BpPinType type) {
    BpPin p; p.name = name; p.type = type; return p;
}

// Test accessors
int BpNodeCount();
int BpLinkCount();
int BpVariableCount();
int BpFunctionGraphCount();
void BpResetState();

}  // namespace dse::editor::bp
