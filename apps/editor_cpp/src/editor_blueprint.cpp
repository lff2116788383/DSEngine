/**
 * @file editor_blueprint.cpp
 * @brief Blueprint editor UI - variable panel, node registry, function graphs, canvas
 */

#include "editor_blueprint.h"
#include "editor_blueprint_vm.h"
#include "editor_blueprint_compiler.h"
#include "engine/scripting/blueprint/blueprint_serialize.h"
#include "editor_icons.h"
#include "editor_locale.h"
#include "editor_file_dialog.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <iostream>

#include "editor_panel_registry.h"

namespace dse::editor::bp {

// ─── BpVarType helpers ─────────────────────────────────────────────────────

const char* BpVarTypeName(BpVarType t) {
    switch (t) {
        case BpVarType::Bool:   return "Bool";
        case BpVarType::Int:    return "Int";
        case BpVarType::Float:  return "Float";
        case BpVarType::String: return "String";
        case BpVarType::Vec2:   return "Vec2";
        case BpVarType::Vec3:   return "Vec3";
        case BpVarType::Vec4:   return "Vec4";
        case BpVarType::Entity: return "Entity";
        case BpVarType::Array:  return "Array";
    }
    return "Unknown";
}

BpVarType BpVarTypeFromName(const char* name) {
    if (strcmp(name, "Bool") == 0) return BpVarType::Bool;
    if (strcmp(name, "Int") == 0) return BpVarType::Int;
    if (strcmp(name, "Float") == 0) return BpVarType::Float;
    if (strcmp(name, "String") == 0) return BpVarType::String;
    if (strcmp(name, "Vec2") == 0) return BpVarType::Vec2;
    if (strcmp(name, "Vec3") == 0) return BpVarType::Vec3;
    if (strcmp(name, "Vec4") == 0) return BpVarType::Vec4;
    if (strcmp(name, "Entity") == 0) return BpVarType::Entity;
    if (strcmp(name, "Array") == 0) return BpVarType::Array;
    return BpVarType::Float;
}

// ─── BpPinType helpers ─────────────────────────────────────────────────────

const char* BpPinTypeName(BpPinType t) {
    switch (t) {
        case BpPinType::Flow:     return "Flow";
        case BpPinType::Bool:     return "Bool";
        case BpPinType::Int:      return "Int";
        case BpPinType::Float:    return "Float";
        case BpPinType::String:   return "String";
        case BpPinType::Vec2:     return "Vec2";
        case BpPinType::Vec3:     return "Vec3";
        case BpPinType::Vec4:     return "Vec4";
        case BpPinType::Entity:   return "Entity";
        case BpPinType::Array:    return "Array";
        case BpPinType::Any:      return "Any";
        case BpPinType::Wildcard: return "Wildcard";
    }
    return "?";
}

BpPinType BpPinTypeFromName(const char* name) {
    if (!name) return BpPinType::Any;
    const std::string value = name;
    if (value == "Flow") return BpPinType::Flow;
    if (value == "Bool") return BpPinType::Bool;
    if (value == "Int") return BpPinType::Int;
    if (value == "Float") return BpPinType::Float;
    if (value == "String") return BpPinType::String;
    if (value == "Vec2") return BpPinType::Vec2;
    if (value == "Vec3") return BpPinType::Vec3;
    if (value == "Vec4") return BpPinType::Vec4;
    if (value == "Entity") return BpPinType::Entity;
    if (value == "Array") return BpPinType::Array;
    if (value == "Wildcard") return BpPinType::Wildcard;
    return BpPinType::Any;
}

ImU32 BpPinColor(BpPinType t) {
    switch (t) {
        case BpPinType::Flow:     return IM_COL32(220, 220, 220, 255);
        case BpPinType::Bool:     return IM_COL32(200, 50, 50, 255);
        case BpPinType::Int:      return IM_COL32(50, 200, 200, 255);
        case BpPinType::Float:    return IM_COL32(80, 200, 80, 255);
        case BpPinType::String:   return IM_COL32(200, 100, 200, 255);
        case BpPinType::Vec2:     return IM_COL32(200, 200, 50, 255);
        case BpPinType::Vec3:     return IM_COL32(200, 200, 50, 255);
        case BpPinType::Vec4:     return IM_COL32(200, 200, 50, 255);
        case BpPinType::Entity:   return IM_COL32(50, 150, 250, 255);
        case BpPinType::Array:    return IM_COL32(150, 100, 50, 255);
        case BpPinType::Any:      return IM_COL32(180, 180, 180, 255);
        case BpPinType::Wildcard: return IM_COL32(180, 180, 180, 255);
    }
    return IM_COL32(128, 128, 128, 255);
}

// ─── Node Registry ─────────────────────────────────────────────────────────

NodeRegistry& NodeRegistry::Get() {
    static NodeRegistry s_reg;
    return s_reg;
}

void NodeRegistry::Register(const NodeTemplate& tmpl) {
    name_index_[tmpl.name] = templates_.size();
    templates_.push_back(tmpl);
    // Track category
    if (std::find(categories_.begin(), categories_.end(), tmpl.category) == categories_.end()) {
        categories_.push_back(tmpl.category);
    }
}

const NodeTemplate* NodeRegistry::Find(const std::string& name) const {
    auto it = name_index_.find(name);
    if (it != name_index_.end()) return &templates_[it->second];
    return nullptr;
}

// MkPin helper is in editor_blueprint.h (inline)

void NodeRegistry::RegisterDefaults() {
    if (!templates_.empty()) return; // already registered

    // ── Event ───────────────────────────────────────────────────────────
    Register({"On Init", "Event", "", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", BpPinType::Flow)},
        "function on_init()\n{body}\nend", "Called once when entity is created"});

    Register({"On Update", "Event", "", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", BpPinType::Flow), MkPin("dt", BpPinType::Float)},
        "function on_update(dt)\n{body}\nend", "Called every frame"});

    Register({"On Begin Overlap", "Event", "", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", BpPinType::Flow), MkPin("Other", BpPinType::Entity)},
        "", "Called when collision begins"});

    Register({"On End Overlap", "Event", "", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", BpPinType::Flow), MkPin("Other", BpPinType::Entity)},
        "", "Called when collision ends"});

    Register({"On Destroy", "Event", "", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", BpPinType::Flow)},
        "", "Called before entity is destroyed"});

    Register({"Custom Event", "Event", "", IM_COL32(200, 80, 80, 255),
        {},
        {MkPin("Exec", BpPinType::Flow)},
        "", "User-defined event (callable from C++ or other blueprints)"});

    // ── Math ────────────────────────────────────────────────────────────
    Register({"Add", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = {input0} + {input1}", "A + B"});

    Register({"Subtract", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = {input0} - {input1}", "A - B"});

    Register({"Multiply", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = {input0} * {input1}", "A * B"});

    Register({"Divide", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = {input0} / {input1}", "A / B (division by zero returns 0)"});

    Register({"Modulo", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = {input0} % {input1}", "A % B"});

    Register({"Negate", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("X", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = -{input0}", "-X"});

    Register({"Abs", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("X", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = math.abs({input0})", "|X|"});

    Register({"Sin", "Math", "Trigonometry", IM_COL32(50, 150, 50, 255),
        {MkPin("Radians", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = math.sin({input0})", "Sine"});

    Register({"Cos", "Math", "Trigonometry", IM_COL32(50, 150, 50, 255),
        {MkPin("Radians", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = math.cos({input0})", "Cosine"});

    Register({"Atan2", "Math", "Trigonometry", IM_COL32(50, 150, 50, 255),
        {MkPin("Y", BpPinType::Float), MkPin("X", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = math.atan2({input0}, {input1})", "Arc tangent of Y/X"});

    Register({"Sqrt", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("X", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = math.sqrt({input0})", "Square root"});

    Register({"Power", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("Base", BpPinType::Float), MkPin("Exp", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = {input0} ^ {input1}", "Base ^ Exponent"});

    Register({"Min", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = math.min({input0}, {input1})", "Minimum of A and B"});

    Register({"Max", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = math.max({input0}, {input1})", "Maximum of A and B"});

    Register({"Clamp", "Math", "Arithmetic", IM_COL32(50, 150, 50, 255),
        {MkPin("Value", BpPinType::Float), MkPin("Min", BpPinType::Float), MkPin("Max", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = math.max({input1}, math.min({input0}, {input2}))", "Clamp value between min and max"});

    Register({"Lerp", "Math", "Interpolation", IM_COL32(50, 150, 50, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float), MkPin("Alpha", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = {input0} + ({input1} - {input0}) * {input2}", "Linear interpolation"});

    Register({"Random Float", "Math", "Random", IM_COL32(50, 150, 50, 255),
        {},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = math.random()", "Random value [0,1)"});

    Register({"Random Range", "Math", "Random", IM_COL32(50, 150, 50, 255),
        {MkPin("Min", BpPinType::Float), MkPin("Max", BpPinType::Float)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = {input0} + math.random() * ({input1} - {input0})", "Random value in [Min,Max)"});

    Register({"Delta Time", "Math", "", IM_COL32(50, 150, 50, 255),
        {},
        {MkPin("dt", BpPinType::Float)},
        "", "Current frame delta time"});

    // ── Vec3 ────────────────────────────────────────────────────────────
    Register({"Make Vec3", "Math", "Vector", IM_COL32(200, 200, 50, 255),
        {MkPin("X", BpPinType::Float), MkPin("Y", BpPinType::Float), MkPin("Z", BpPinType::Float)},
        {MkPin("Vec3", BpPinType::Vec3)},
        "{output0} = vec3({input0}, {input1}, {input2})", "Construct Vec3 from components"});

    Register({"Break Vec3", "Math", "Vector", IM_COL32(200, 200, 50, 255),
        {MkPin("Vec3", BpPinType::Vec3)},
        {MkPin("X", BpPinType::Float), MkPin("Y", BpPinType::Float), MkPin("Z", BpPinType::Float)},
        "", "Decompose Vec3 into X, Y, Z"});

    Register({"Vec3 Add", "Math", "Vector", IM_COL32(200, 200, 50, 255),
        {MkPin("A", BpPinType::Vec3), MkPin("B", BpPinType::Vec3)},
        {MkPin("Result", BpPinType::Vec3)},
        "{output0} = {input0} + {input1}", "Vector addition"});

    Register({"Vec3 Scale", "Math", "Vector", IM_COL32(200, 200, 50, 255),
        {MkPin("Vec3", BpPinType::Vec3), MkPin("Scale", BpPinType::Float)},
        {MkPin("Result", BpPinType::Vec3)},
        "{output0} = {input0} * {input1}", "Scale vector"});

    Register({"Vec3 Dot", "Math", "Vector", IM_COL32(200, 200, 50, 255),
        {MkPin("A", BpPinType::Vec3), MkPin("B", BpPinType::Vec3)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = vec3.dot({input0}, {input1})", "Dot product"});

    Register({"Vec3 Normalize", "Math", "Vector", IM_COL32(200, 200, 50, 255),
        {MkPin("Vec3", BpPinType::Vec3)},
        {MkPin("Result", BpPinType::Vec3)},
        "{output0} = vec3.normalize({input0})", "Normalize vector"});

    Register({"Vec3 Length", "Math", "Vector", IM_COL32(200, 200, 50, 255),
        {MkPin("Vec3", BpPinType::Vec3)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = vec3.length({input0})", "Vector length"});

    Register({"Vec3 Distance", "Math", "Vector", IM_COL32(200, 200, 50, 255),
        {MkPin("A", BpPinType::Vec3), MkPin("B", BpPinType::Vec3)},
        {MkPin("Result", BpPinType::Float)},
        "{output0} = vec3.distance({input0}, {input1})", "Distance between two points"});

    // ── Comparison / Logic ──────────────────────────────────────────────
    Register({"Equal", "Logic", "Comparison", IM_COL32(100, 150, 200, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = ({input0} == {input1})", "A == B"});

    Register({"Not Equal", "Logic", "Comparison", IM_COL32(100, 150, 200, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = ({input0} ~= {input1})", "A != B"});

    Register({"Less Than", "Logic", "Comparison", IM_COL32(100, 150, 200, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = ({input0} < {input1})", "A < B"});

    Register({"Greater Than", "Logic", "Comparison", IM_COL32(100, 150, 200, 255),
        {MkPin("A", BpPinType::Float), MkPin("B", BpPinType::Float)},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = ({input0} > {input1})", "A > B"});

    Register({"And", "Logic", "", IM_COL32(100, 150, 200, 255),
        {MkPin("A", BpPinType::Bool), MkPin("B", BpPinType::Bool)},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = ({input0} and {input1})", "Logical AND"});

    Register({"Or", "Logic", "", IM_COL32(100, 150, 200, 255),
        {MkPin("A", BpPinType::Bool), MkPin("B", BpPinType::Bool)},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = ({input0} or {input1})", "Logical OR"});

    Register({"Not", "Logic", "", IM_COL32(100, 150, 200, 255),
        {MkPin("A", BpPinType::Bool)},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = (not {input0})", "Logical NOT"});

    // ── Flow Control ────────────────────────────────────────────────────
    Register({"Branch", "Flow", "", IM_COL32(200, 200, 50, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Condition", BpPinType::Bool)},
        {MkPin("True", BpPinType::Flow), MkPin("False", BpPinType::Flow)},
        "if {input1} then\n{true_body}\nelse\n{false_body}\nend", "If/Else branch"});

    Register({"For Loop", "Flow", "", IM_COL32(200, 200, 50, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Start", BpPinType::Int), MkPin("End", BpPinType::Int)},
        {MkPin("Body", BpPinType::Flow), MkPin("Index", BpPinType::Int), MkPin("Done", BpPinType::Flow)},
        "", "Numeric for loop"});

    Register({"While Loop", "Flow", "", IM_COL32(200, 200, 50, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Condition", BpPinType::Bool)},
        {MkPin("Body", BpPinType::Flow), MkPin("Done", BpPinType::Flow)},
        "", "While loop"});

    Register({"Sequence", "Flow", "", IM_COL32(200, 200, 50, 255),
        {MkPin("Exec", BpPinType::Flow)},
        {MkPin("Then 0", BpPinType::Flow), MkPin("Then 1", BpPinType::Flow), MkPin("Then 2", BpPinType::Flow)},
        "", "Execute multiple flows in order"});

    Register({"Delay", "Flow", "Timer", IM_COL32(200, 200, 50, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Duration", BpPinType::Float)},
        {MkPin("Done", BpPinType::Flow)},
        "coroutine.yield({input1})", "Wait for duration (seconds)"});

    Register({"Set Timer", "Flow", "Timer", IM_COL32(200, 200, 50, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Interval", BpPinType::Float), MkPin("Loop", BpPinType::Bool)},
        {MkPin("Exec", BpPinType::Flow), MkPin("OnTimer", BpPinType::Flow)},
        "", "Set a repeating or one-shot timer"});

    // ── ECS ─────────────────────────────────────────────────────────────
    Register({"Get Position", "ECS", "Transform", IM_COL32(50, 100, 200, 255),
        {MkPin("Entity", BpPinType::Entity)},
        {MkPin("Position", BpPinType::Vec3)},
        "{output0} = ecs.get_position({input0})", "Get entity world position"});

    Register({"Set Position", "ECS", "Transform", IM_COL32(50, 100, 200, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("Position", BpPinType::Vec3)},
        {MkPin("Exec", BpPinType::Flow)},
        "ecs.set_position({input1}, {input2})", "Set entity world position"});

    Register({"Get Rotation", "ECS", "Transform", IM_COL32(50, 100, 200, 255),
        {MkPin("Entity", BpPinType::Entity)},
        {MkPin("Rotation", BpPinType::Vec3)},
        "{output0} = ecs.get_rotation({input0})", "Get entity euler rotation"});

    Register({"Set Rotation", "ECS", "Transform", IM_COL32(50, 100, 200, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("Rotation", BpPinType::Vec3)},
        {MkPin("Exec", BpPinType::Flow)},
        "ecs.set_rotation({input1}, {input2})", "Set entity euler rotation"});

    Register({"Get Scale", "ECS", "Transform", IM_COL32(50, 100, 200, 255),
        {MkPin("Entity", BpPinType::Entity)},
        {MkPin("Scale", BpPinType::Vec3)},
        "{output0} = ecs.get_scale({input0})", "Get entity scale"});

    Register({"Set Scale", "ECS", "Transform", IM_COL32(50, 100, 200, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("Scale", BpPinType::Vec3)},
        {MkPin("Exec", BpPinType::Flow)},
        "ecs.set_scale({input1}, {input2})", "Set entity scale"});

    Register({"Create Entity", "ECS", "", IM_COL32(50, 100, 200, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Name", BpPinType::String)},
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity)},
        "{output1} = ecs.create_entity({input1})", "Create a new entity"});

    Register({"Destroy Entity", "ECS", "", IM_COL32(50, 100, 200, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity)},
        {MkPin("Exec", BpPinType::Flow)},
        "ecs.destroy({input1})", "Destroy an entity"});

    Register({"Self Entity", "ECS", "", IM_COL32(50, 100, 200, 255),
        {},
        {MkPin("Entity", BpPinType::Entity)},
        "{output0} = self_entity", "Reference to the owning entity"});

    // ── Input ───────────────────────────────────────────────────────────
    Register({"Is Key Pressed", "Input", "Keyboard", IM_COL32(200, 150, 50, 255),
        {MkPin("Key", BpPinType::String)},
        {MkPin("Pressed", BpPinType::Bool)},
        "{output0} = input.is_key_pressed({input0})", "Check if key is currently held"});

    Register({"Is Key Just Pressed", "Input", "Keyboard", IM_COL32(200, 150, 50, 255),
        {MkPin("Key", BpPinType::String)},
        {MkPin("Pressed", BpPinType::Bool)},
        "{output0} = input.is_key_just_pressed({input0})", "Check if key was pressed this frame"});

    Register({"Get Mouse Position", "Input", "Mouse", IM_COL32(200, 150, 50, 255),
        {},
        {MkPin("Position", BpPinType::Vec2)},
        "{output0} = input.get_mouse_position()", "Get mouse cursor position"});

    Register({"Is Mouse Button", "Input", "Mouse", IM_COL32(200, 150, 50, 255),
        {MkPin("Button", BpPinType::Int)},
        {MkPin("Pressed", BpPinType::Bool)},
        "{output0} = input.is_mouse_button({input0})", "Check mouse button (0=Left,1=Right,2=Middle)"});

    Register({"Get Axis", "Input", "", IM_COL32(200, 150, 50, 255),
        {MkPin("Axis", BpPinType::String)},
        {MkPin("Value", BpPinType::Float)},
        "{output0} = input.get_axis({input0})", "Get input axis value (-1 to 1)"});

    // ── Physics ─────────────────────────────────────────────────────────
    Register({"Raycast", "Physics", "", IM_COL32(150, 50, 200, 255),
        {MkPin("Origin", BpPinType::Vec3), MkPin("Direction", BpPinType::Vec3), MkPin("MaxDist", BpPinType::Float)},
        {MkPin("Hit", BpPinType::Bool), MkPin("HitPoint", BpPinType::Vec3), MkPin("HitEntity", BpPinType::Entity)},
        "", "Cast a ray and return hit info"});

    Register({"Add Force", "Physics", "", IM_COL32(150, 50, 200, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("Force", BpPinType::Vec3)},
        {MkPin("Exec", BpPinType::Flow)},
        "physics.add_force({input1}, {input2})", "Apply force to rigid body"});

    Register({"Add Impulse", "Physics", "", IM_COL32(150, 50, 200, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("Impulse", BpPinType::Vec3)},
        {MkPin("Exec", BpPinType::Flow)},
        "physics.add_impulse({input1}, {input2})", "Apply instant impulse"});

    Register({"Set Velocity", "Physics", "", IM_COL32(150, 50, 200, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("Velocity", BpPinType::Vec3)},
        {MkPin("Exec", BpPinType::Flow)},
        "physics.set_velocity({input1}, {input2})", "Set linear velocity"});

    Register({"Get Velocity", "Physics", "", IM_COL32(150, 50, 200, 255),
        {MkPin("Entity", BpPinType::Entity)},
        {MkPin("Velocity", BpPinType::Vec3)},
        "{output0} = physics.get_velocity({input0})", "Get linear velocity"});

    // ── Audio ───────────────────────────────────────────────────────────
    Register({"Play Sound", "Audio", "", IM_COL32(255, 100, 100, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Sound", BpPinType::String)},
        {MkPin("Exec", BpPinType::Flow)},
        "audio.play({input1})", "Play a sound effect"});

    Register({"Stop Sound", "Audio", "", IM_COL32(255, 100, 100, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Sound", BpPinType::String)},
        {MkPin("Exec", BpPinType::Flow)},
        "audio.stop({input1})", "Stop a playing sound"});

    // ── Animation ───────────────────────────────────────────────────────
    Register({"Play Animation", "Animation", "", IM_COL32(255, 180, 80, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("Anim", BpPinType::String)},
        {MkPin("Exec", BpPinType::Flow)},
        "anim.play({input1}, {input2})", "Play animation clip"});

    Register({"Stop Animation", "Animation", "", IM_COL32(255, 180, 80, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity)},
        {MkPin("Exec", BpPinType::Flow)},
        "anim.stop({input1})", "Stop current animation"});

    // ── AI ──────────────────────────────────────────────────────────────
    Register({"Move To", "AI", "Navigation", IM_COL32(80, 200, 200, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("Target", BpPinType::Vec3)},
        {MkPin("Exec", BpPinType::Flow), MkPin("Reached", BpPinType::Flow)},
        "ai.move_to({input1}, {input2})", "Navigate entity to target position"});

    Register({"Find Path", "AI", "Navigation", IM_COL32(80, 200, 200, 255),
        {MkPin("Start", BpPinType::Vec3), MkPin("End", BpPinType::Vec3)},
        {MkPin("Valid", BpPinType::Bool)},
        "{output0} = ai.find_path({input0}, {input1})", "Check if path exists"});

    // ── Utility ─────────────────────────────────────────────────────────
    Register({"Print", "Utility", "", IM_COL32(180, 180, 180, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Message", BpPinType::Any)},
        {MkPin("Exec", BpPinType::Flow)},
        "print({input1})", "Print to console"});

    Register({"String Format", "Utility", "String", IM_COL32(180, 180, 180, 255),
        {MkPin("Format", BpPinType::String), MkPin("Arg0", BpPinType::Any)},
        {MkPin("Result", BpPinType::String)},
        "{output0} = string.format({input0}, {input1})", "Format string"});

    Register({"To String", "Utility", "String", IM_COL32(180, 180, 180, 255),
        {MkPin("Value", BpPinType::Any)},
        {MkPin("Result", BpPinType::String)},
        "{output0} = tostring({input0})", "Convert value to string"});

    Register({"Float Constant", "Variable", "Constants", IM_COL32(100, 180, 250, 255),
        {},
        {MkPin("Value", BpPinType::Float)},
        "", "Float literal"});

    Register({"Int Constant", "Variable", "Constants", IM_COL32(100, 180, 250, 255),
        {},
        {MkPin("Value", BpPinType::Int)},
        "", "Integer literal"});

    Register({"Bool Constant", "Variable", "Constants", IM_COL32(100, 180, 250, 255),
        {},
        {MkPin("Value", BpPinType::Bool)},
        "", "Boolean literal"});

    Register({"String Constant", "Variable", "Constants", IM_COL32(100, 180, 250, 255),
        {},
        {MkPin("Value", BpPinType::String)},
        "", "String literal"});

    Register({"Get Variable", "Variable", "", IM_COL32(100, 180, 250, 255),
        {},
        {MkPin("Value", BpPinType::Any)},
        "{output0} = self.{var_name}", "Read a blueprint variable"});

    Register({"Set Variable", "Variable", "", IM_COL32(100, 180, 250, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Value", BpPinType::Any)},
        {MkPin("Exec", BpPinType::Flow)},
        "self.{var_name} = {input1}", "Write a blueprint variable"});

    // ── Array ───────────────────────────────────────────────────────────
    Register({"Array Get", "Utility", "Array", IM_COL32(150, 100, 50, 255),
        {MkPin("Array", BpPinType::Array), MkPin("Index", BpPinType::Int)},
        {MkPin("Element", BpPinType::Any)},
        "{output0} = {input0}[{input1}]", "Get array element at index"});

    Register({"Array Set", "Utility", "Array", IM_COL32(150, 100, 50, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Array", BpPinType::Array), MkPin("Index", BpPinType::Int), MkPin("Value", BpPinType::Any)},
        {MkPin("Exec", BpPinType::Flow)},
        "{input1}[{input2}] = {input3}", "Set array element at index"});

    Register({"Array Push", "Utility", "Array", IM_COL32(150, 100, 50, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Array", BpPinType::Array), MkPin("Value", BpPinType::Any)},
        {MkPin("Exec", BpPinType::Flow)},
        "table.insert({input1}, {input2})", "Append element to array"});

    Register({"Array Length", "Utility", "Array", IM_COL32(150, 100, 50, 255),
        {MkPin("Array", BpPinType::Array)},
        {MkPin("Length", BpPinType::Int)},
        "{output0} = #{input0}", "Get array length"});

    // ── Network / Authority ─────────────────────────────────────────────
    Register({"Has Authority", "Network", "Authority", IM_COL32(50, 200, 150, 255),
        {},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = dse.net.is_server()", "True if this instance is the server (authority)"});

    Register({"Is Server", "Network", "Authority", IM_COL32(50, 200, 150, 255),
        {},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = dse.net.is_server()", "True if running as dedicated/listen server"});

    Register({"Is Client", "Network", "Authority", IM_COL32(50, 200, 150, 255),
        {},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = dse.net.is_client()", "True if running as network client"});

    Register({"Is Local Player", "Network", "Authority", IM_COL32(50, 200, 150, 255),
        {MkPin("Entity", BpPinType::Entity)},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = dse.net.is_local_player({input0})", "True if entity is owned by local player"});

    Register({"Get Net Role", "Network", "Authority", IM_COL32(50, 200, 150, 255),
        {MkPin("Entity", BpPinType::Entity)},
        {MkPin("Role", BpPinType::String)},
        "{output0} = dse.net.get_role({input0})", "Get replication role: 'authority'/'simulated'/'autonomous'"});

    // ── Network / Connection ────────────────────────────────────────────
    Register({"Net Init", "Network", "Connection", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow)},
        {MkPin("Exec", BpPinType::Flow), MkPin("Success", BpPinType::Bool)},
        "{output1} = dse.net.init()", "Initialize network subsystem"});

    Register({"Net Listen", "Network", "Connection", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Port", BpPinType::Int)},
        {MkPin("Exec", BpPinType::Flow), MkPin("Success", BpPinType::Bool)},
        "{output1} = dse.net.listen({input1})", "Start listening as server on port"});

    Register({"Net Connect", "Network", "Connection", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Host", BpPinType::String), MkPin("Port", BpPinType::Int)},
        {MkPin("Exec", BpPinType::Flow), MkPin("Connection", BpPinType::Int)},
        "{output1} = dse.net.connect({input1}, {input2})", "Connect to server (returns connection handle)"});

    Register({"Net Disconnect", "Network", "Connection", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Connection", BpPinType::Int)},
        {MkPin("Exec", BpPinType::Flow)},
        "dse.net.close({input1})", "Close a network connection"});

    Register({"Is Connected", "Network", "Connection", IM_COL32(50, 200, 150, 255),
        {},
        {MkPin("Result", BpPinType::Bool)},
        "{output0} = dse.repl.client_connected(__dse_repl_client)", "True if client is connected to server"});

    Register({"Get Client Count", "Network", "Connection", IM_COL32(50, 200, 150, 255),
        {},
        {MkPin("Count", BpPinType::Int)},
        "{output0} = dse.repl.server_client_count(__dse_repl_server)", "Number of connected clients (server only)"});

    Register({"Get Ping", "Network", "Connection", IM_COL32(50, 200, 150, 255),
        {MkPin("Connection", BpPinType::Int)},
        {MkPin("PingMs", BpPinType::Float)},
        "{output0} = (dse.net.get_quality({input0}) or {}).ping_ms or 0", "Get connection ping in milliseconds"});

    // ── Network / Replication ───────────────────────────────────────────
    Register({"Spawn Replicated", "Network", "Replication", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("Owner", BpPinType::Int)},
        {MkPin("Exec", BpPinType::Flow), MkPin("NetId", BpPinType::Int)},
        "{output1} = dse.repl.server_mark(__dse_repl_server, {input1}, {input2})",
        "Register entity for network replication (server only). Returns NetId."});

    Register({"Unreplicate", "Network", "Replication", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity)},
        {MkPin("Exec", BpPinType::Flow)},
        "dse.repl.server_unreplicate(__dse_repl_server, {input1})",
        "Remove entity from replication (server only)"});

    Register({"Set Net Owner", "Network", "Replication", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Entity", BpPinType::Entity), MkPin("OwnerConn", BpPinType::Int)},
        {MkPin("Exec", BpPinType::Flow)},
        "dse.repl.server_set_owner(__dse_repl_server, {input1}, {input2})",
        "Transfer ownership of replicated entity (server only)"});

    Register({"Server Replication Tick", "Network", "Replication", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow)},
        {MkPin("Exec", BpPinType::Flow)},
        "dse.repl.server_tick(__dse_repl_server)",
        "Send replication snapshot/delta to all clients (call once per server tick)"});

    Register({"Set AOI Policy", "Network", "Replication", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("Policy", BpPinType::String), MkPin("Radius", BpPinType::Float)},
        {MkPin("Exec", BpPinType::Flow)},
        "dse.repl.server_set_aoi(__dse_repl_server, {input1}, {input2})",
        "Set Area-of-Interest policy ('always' or 'distance') and radius"});

    Register({"Net Entity To Local", "Network", "Replication", IM_COL32(50, 200, 150, 255),
        {MkPin("NetId", BpPinType::Int)},
        {MkPin("Entity", BpPinType::Entity)},
        "{output0} = dse.repl.client_to_entity(__dse_repl_client, {input0})",
        "Convert NetId to local entity (client side)"});

    // ── Network / RPC ───────────────────────────────────────────────────
    Register({"Call Server RPC", "Network", "RPC", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("RpcName", BpPinType::String), MkPin("TargetNetId", BpPinType::Int), MkPin("Payload", BpPinType::String)},
        {MkPin("Exec", BpPinType::Flow), MkPin("Success", BpPinType::Bool)},
        "{output1} = dse.repl.rpc_client_send(__dse_repl_client, {input1}, {input2}, {input3})",
        "Send RPC from client to server"});

    Register({"Call Client RPC", "Network", "RPC", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("RpcName", BpPinType::String), MkPin("TargetNetId", BpPinType::Int), MkPin("Payload", BpPinType::String)},
        {MkPin("Exec", BpPinType::Flow)},
        "dse.repl.rpc_server_broadcast(__dse_repl_server, {input1}, {input2}, {input3})",
        "Send RPC from server to owning client"});

    Register({"Broadcast RPC", "Network", "RPC", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("RpcName", BpPinType::String), MkPin("TargetNetId", BpPinType::Int), MkPin("Payload", BpPinType::String)},
        {MkPin("Exec", BpPinType::Flow)},
        "dse.repl.rpc_server_broadcast(__dse_repl_server, {input1}, {input2}, {input3})",
        "Broadcast RPC from server to all clients (multicast)"});

    // ── Network / Movement ──────────────────────────────────────────────
    Register({"Send Move Input", "Network", "Movement", IM_COL32(50, 200, 150, 255),
        {MkPin("Exec", BpPinType::Flow), MkPin("NetId", BpPinType::Int), MkPin("DX", BpPinType::Float), MkPin("DY", BpPinType::Float), MkPin("DZ", BpPinType::Float)},
        {MkPin("Exec", BpPinType::Flow)},
        "dse.repl.client_send_move(__dse_repl_client, {input1}, {input2}, {input3}, {input4})",
        "Send movement input to server (client only)"});

    // ── Network / Events ────────────────────────────────────────────────
    Register({"On Net Connected", "Network", "Events", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", BpPinType::Flow), MkPin("Connection", BpPinType::Int)},
        "", "Fired when a network connection is established"});

    Register({"On Net Disconnected", "Network", "Events", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", BpPinType::Flow), MkPin("Connection", BpPinType::Int), MkPin("Reason", BpPinType::Int)},
        "", "Fired when a network connection is lost"});

    Register({"On Net Message", "Network", "Events", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", BpPinType::Flow), MkPin("Connection", BpPinType::Int), MkPin("Data", BpPinType::String)},
        "", "Fired when raw network message is received"});

    Register({"On RPC Received", "Network", "Events", IM_COL32(200, 50, 50, 255),
        {},
        {MkPin("Exec", BpPinType::Flow), MkPin("RpcName", BpPinType::String), MkPin("SenderNetId", BpPinType::Int), MkPin("Payload", BpPinType::String)},
        "", "Fired when an RPC call is received"});
}

// ─── Blueprint Editor State ────────────────────────────────────────────────

static BlueprintEditorState s_state;

BlueprintEditorState& GetBlueprintEditorState() { return s_state; }

int BpNodeCount() {
    if (s_state.asset.graphs.empty()) return 0;
    return static_cast<int>(s_state.asset.graphs[s_state.active_graph_index].nodes.size());
}
int BpLinkCount() {
    if (s_state.asset.graphs.empty()) return 0;
    return static_cast<int>(s_state.asset.graphs[s_state.active_graph_index].links.size());
}
int BpVariableCount() { return static_cast<int>(s_state.asset.variables.size()); }
int BpFunctionGraphCount() { return static_cast<int>(s_state.asset.graphs.size()); }
void BpResetState() { s_state = BlueprintEditorState{}; InitBlueprintSystem(); }

// ─── Init ──────────────────────────────────────────────────────────────────

void InitBlueprintSystem() {
    NodeRegistry::Get().RegisterDefaults();

    // Create default event graph if empty
    if (s_state.asset.graphs.empty()) {
        BpFunctionGraph event_graph;
        event_graph.name = "EventGraph";
        event_graph.next_id = 1;
        s_state.asset.graphs.push_back(std::move(event_graph));
    }
    if (s_state.asset.name.empty()) {
        s_state.asset.name = "NewBlueprint";
    }
}

// ─── Variable Panel ────────────────────────────────────────────────────────

namespace {

void DrawVariablePanel() {
    ImGui::BeginChild("##bp_vars", ImVec2(200, 0), true);
    ImGui::Text(MDI_ICON_VARIABLE "  Variables");
    ImGui::Separator();

    if (ImGui::Button(T("+ Add Variable"))) {
        BpVariable var;
        var.name = "NewVar_" + std::to_string(s_state.asset.variables.size());
        var.type = BpVarType::Float;
        s_state.asset.variables.push_back(var);
        s_state.dirty = true;
    }

    for (int i = 0; i < static_cast<int>(s_state.asset.variables.size()); ++i) {
        auto& var = s_state.asset.variables[i];
        ImGui::PushID(i);

        bool selected = (s_state.selected_variable == i);
        if (ImGui::Selectable(var.name.c_str(), selected)) {
            s_state.selected_variable = i;
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem(T("Delete"))) {
                s_state.asset.variables.erase(s_state.asset.variables.begin() + i);
                s_state.dirty = true;
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    // Variable detail editor
    if (s_state.selected_variable >= 0 && s_state.selected_variable < static_cast<int>(s_state.asset.variables.size())) {
        ImGui::Separator();
        auto& var = s_state.asset.variables[s_state.selected_variable];
        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "%s", var.name.c_str());
        if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
            var.name = name_buf;
            s_state.dirty = true;
        }

        const char* type_names[] = {"Bool", "Int", "Float", "String", "Vec2", "Vec3", "Vec4", "Entity", "Array"};
        int type_idx = static_cast<int>(var.type);
        if (ImGui::Combo("Type", &type_idx, type_names, 9)) {
            var.type = static_cast<BpVarType>(type_idx);
            s_state.dirty = true;
        }

        // Default value editor based on type
        switch (var.type) {
            case BpVarType::Bool:
                if (ImGui::Checkbox("Default", &var.default_bool)) s_state.dirty = true;
                break;
            case BpVarType::Int:
                if (ImGui::DragInt("Default", &var.default_int)) s_state.dirty = true;
                break;
            case BpVarType::Float:
                if (ImGui::DragFloat("Default", &var.default_float, 0.1f)) s_state.dirty = true;
                break;
            case BpVarType::String:
                if (ImGui::InputText("Default", var.default_string, sizeof(var.default_string))) s_state.dirty = true;
                break;
            case BpVarType::Vec2:
                if (ImGui::DragFloat2("Default", var.default_vec, 0.1f)) s_state.dirty = true;
                break;
            case BpVarType::Vec3:
                if (ImGui::DragFloat3("Default", var.default_vec, 0.1f)) s_state.dirty = true;
                break;
            case BpVarType::Vec4:
                if (ImGui::DragFloat4("Default", var.default_vec, 0.1f)) s_state.dirty = true;
                break;
            default: break;
        }

        if (ImGui::Checkbox("Exposed", &var.is_exposed)) s_state.dirty = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exposed variables appear in the Inspector on the entity");
    }

    ImGui::EndChild();
}

// ─── Function Graph Tabs ───────────────────────────────────────────────────

void DrawGraphTabs() {
    if (ImGui::BeginTabBar("##bp_graph_tabs")) {
        for (int i = 0; i < static_cast<int>(s_state.asset.graphs.size()); ++i) {
            ImGuiTabItemFlags flags = 0;
            if (ImGui::BeginTabItem(s_state.asset.graphs[i].name.c_str(), nullptr, flags)) {
                s_state.active_graph_index = i;
                ImGui::EndTabItem();
            }
        }

        // Add function button
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            BpFunctionGraph new_graph;
            new_graph.name = "Function_" + std::to_string(s_state.asset.graphs.size());
            new_graph.next_id = 1;
            s_state.asset.graphs.push_back(std::move(new_graph));
            s_state.dirty = true;
        }

        ImGui::EndTabBar();
    }
}

// ─── Node Canvas ───────────────────────────────────────────────────────────

int AllocNodeId(BpFunctionGraph& graph) { return graph.next_id++; }

BpNode CreateNodeFromTemplate(BpFunctionGraph& graph, const NodeTemplate& tmpl, ImVec2 pos) {
    BpNode node;
    node.id = AllocNodeId(graph);
    node.name = tmpl.name;
    node.category = tmpl.category;
    node.position = pos;
    node.header_color = tmpl.header_color;
    node.code_template = tmpl.code_template;
    for (auto pin : tmpl.inputs) { pin.id = AllocNodeId(graph); pin.kind = BpPinKind::Input; node.inputs.push_back(pin); }
    for (auto pin : tmpl.outputs) { pin.id = AllocNodeId(graph); pin.kind = BpPinKind::Output; node.outputs.push_back(pin); }
    return node;
}

void DrawNodeCanvas() {
    if (s_state.asset.graphs.empty()) return;
    auto& graph = s_state.asset.graphs[s_state.active_graph_index];

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 50 || canvas_size.y < 50) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // ── 辅助: pin 查找 / 屏幕坐标 (与绘制公式一致) ──────────────────────
    auto find_pin_owner = [&](int pin_id) -> BpNode* {
        for (auto& n : graph.nodes) {
            for (auto& p : n.inputs)  if (p.id == pin_id) return &n;
            for (auto& p : n.outputs) if (p.id == pin_id) return &n;
        }
        return nullptr;
    };
    auto find_pin = [&](int pin_id) -> BpPin* {
        for (auto& n : graph.nodes) {
            for (auto& p : n.inputs)  if (p.id == pin_id) return &p;
            for (auto& p : n.outputs) if (p.id == pin_id) return &p;
        }
        return nullptr;
    };
    auto pin_screen_pos = [&](const BpNode& n, int pin_id, bool is_output) -> ImVec2 {
        if (is_output) {
            for (size_t pi = 0; pi < n.outputs.size(); ++pi)
                if (n.outputs[pi].id == pin_id)
                    return ImVec2(canvas_pos.x + s_state.scroll_offset.x + n.position.x + n.size.x,
                                  canvas_pos.y + s_state.scroll_offset.y + n.position.y + 30 + pi * 20);
        } else {
            for (size_t pi = 0; pi < n.inputs.size(); ++pi)
                if (n.inputs[pi].id == pin_id)
                    return ImVec2(canvas_pos.x + s_state.scroll_offset.x + n.position.x,
                                  canvas_pos.y + s_state.scroll_offset.y + n.position.y + 30 + pi * 20);
        }
        return {0, 0};
    };
    auto pin_type_compatible = [](BpPinType a, BpPinType b) {
        if (a == BpPinType::Any || b == BpPinType::Any) return true;
        if (a == BpPinType::Wildcard || b == BpPinType::Wildcard) return true;
        return a == b;
    };
    bool canvas_hovered = ImGui::IsMouseHoveringRect(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y));

    // Background grid
    draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(30, 30, 30, 255));
    float grid_step = 32.0f * s_state.zoom;
    for (float x = fmodf(s_state.scroll_offset.x, grid_step); x < canvas_size.x; x += grid_step)
        draw_list->AddLine(ImVec2(canvas_pos.x + x, canvas_pos.y),
                           ImVec2(canvas_pos.x + x, canvas_pos.y + canvas_size.y), IM_COL32(50, 50, 50, 255));
    for (float y = fmodf(s_state.scroll_offset.y, grid_step); y < canvas_size.y; y += grid_step)
        draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + y),
                           ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + y), IM_COL32(50, 50, 50, 255));

    // Draw comments & groups (#4) behind nodes
    DrawBpComments(draw_list, canvas_pos);

    // Draw links
    for (const auto& link : graph.links) {
        ImVec2 p1{0,0}, p2{0,0};
        // Find pin positions (simplified: based on node position + pin index offset)
        for (const auto& n : graph.nodes) {
            for (size_t pi = 0; pi < n.outputs.size(); ++pi) {
                if (n.outputs[pi].id == link.from_pin) {
                    p1 = ImVec2(canvas_pos.x + s_state.scroll_offset.x + n.position.x + n.size.x,
                                canvas_pos.y + s_state.scroll_offset.y + n.position.y + 30 + pi * 20);
                }
            }
            for (size_t pi = 0; pi < n.inputs.size(); ++pi) {
                if (n.inputs[pi].id == link.to_pin) {
                    p2 = ImVec2(canvas_pos.x + s_state.scroll_offset.x + n.position.x,
                                canvas_pos.y + s_state.scroll_offset.y + n.position.y + 30 + pi * 20);
                }
            }
        }
        if (p1.x != 0 || p1.y != 0) {
            ImVec2 cp1(p1.x + 50, p1.y);
            ImVec2 cp2(p2.x - 50, p2.y);
            bool is_sel = (link.id == s_state.selected_link);
            draw_list->AddBezierCubic(p1, cp1, cp2, p2,
                is_sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(200, 200, 200, 200),
                is_sel ? 3.5f : 2.0f);
        }
    }

    // Draw link being created (from start pin to mouse)
    if (s_state.creating_link && s_state.link_start_pin >= 0) {
        BpNode* from_node = find_pin_owner(s_state.link_start_pin);
        BpPin* from_pin = find_pin(s_state.link_start_pin);
        if (from_node && from_pin) {
            bool is_out = (from_pin->kind == BpPinKind::Output);
            ImVec2 p1 = pin_screen_pos(*from_node, s_state.link_start_pin, is_out);
            ImVec2 p2 = ImGui::GetMousePos();
            ImU32 col = BpPinColor(from_pin->type);
            if (is_out)
                draw_list->AddBezierCubic(p1, ImVec2(p1.x + 50, p1.y), ImVec2(p2.x - 50, p2.y), p2, col, 2.0f);
            else
                draw_list->AddBezierCubic(p2, ImVec2(p2.x + 50, p2.y), ImVec2(p1.x - 50, p1.y), p1, col, 2.0f);
        }
    }

    // Draw nodes
    for (auto& node : graph.nodes) {
        ImVec2 node_pos(canvas_pos.x + s_state.scroll_offset.x + node.position.x,
                        canvas_pos.y + s_state.scroll_offset.y + node.position.y);
        ImVec2 node_end(node_pos.x + node.size.x, node_pos.y + node.size.y);

        // Node body
        draw_list->AddRectFilled(node_pos, node_end, IM_COL32(45, 45, 48, 230), 4.0f);
        // Header
        draw_list->AddRectFilled(node_pos, ImVec2(node_end.x, node_pos.y + 24), node.header_color, 4.0f, ImDrawFlags_RoundCornersTop);
        // Title
        draw_list->AddText(ImVec2(node_pos.x + 6, node_pos.y + 4), IM_COL32(255, 255, 255, 255), node.name.c_str());
        // Border (with debugger highlighting)
        bool is_selected = (node.id == s_state.selected_node) ||
            (std::find(s_state.selected_nodes.begin(), s_state.selected_nodes.end(), node.id) != s_state.selected_nodes.end());
        bool is_executing = (s_state.debug.active && node.id == s_state.debug.current_node_id);
        bool has_breakpoint = s_state.debug.HasBreakpoint(node.id);
        ImU32 border_color = is_executing ? IM_COL32(50, 255, 50, 255) :
                             is_selected  ? IM_COL32(255, 200, 50, 255) :
                             has_breakpoint ? IM_COL32(255, 60, 60, 255) :
                             IM_COL32(80, 80, 80, 255);
        float border_thick = (is_executing || has_breakpoint) ? 3.0f : 1.0f;
        draw_list->AddRect(node_pos, node_end, border_color, 4.0f, 0, border_thick);
        // Breakpoint indicator
        if (has_breakpoint) {
            draw_list->AddCircleFilled(ImVec2(node_pos.x - 8, node_pos.y + 12), 5, IM_COL32(255, 40, 40, 255));
        }

        // Input pins (with link creation hit test)
        for (size_t pi = 0; pi < node.inputs.size(); ++pi) {
            ImVec2 pin_pos(node_pos.x, node_pos.y + 30 + pi * 20);
            draw_list->AddCircleFilled(pin_pos, 5, BpPinColor(node.inputs[pi].type));
            draw_list->AddText(ImVec2(pin_pos.x + 8, pin_pos.y - 7), IM_COL32(200, 200, 200, 255), node.inputs[pi].name.c_str());

            ImVec2 hit_min(pin_pos.x - 8, pin_pos.y - 8);
            ImVec2 hit_max(pin_pos.x + 8, pin_pos.y + 8);
            if (canvas_hovered && ImGui::IsMouseHoveringRect(hit_min, hit_max)) {
                if (ImGui::IsMouseClicked(0)) {
                    s_state.creating_link = true;
                    s_state.link_start_pin = node.inputs[pi].id;
                }
                if (ImGui::IsMouseReleased(0) && s_state.creating_link && s_state.link_start_pin != node.inputs[pi].id) {
                    BpPin* start = find_pin(s_state.link_start_pin);
                    if (start && start->kind == BpPinKind::Output &&
                        pin_type_compatible(start->type, node.inputs[pi].type)) {
                        bool dup = false;
                        for (const auto& l : graph.links)
                            if (l.from_pin == s_state.link_start_pin && l.to_pin == node.inputs[pi].id) { dup = true; break; }
                        if (!dup) {
                            BpLink lnk;
                            lnk.id = AllocNodeId(graph);
                            lnk.from_pin = s_state.link_start_pin;
                            lnk.to_pin = node.inputs[pi].id;
                            graph.links.push_back(lnk);
                            s_state.dirty = true;
                            BpPushUndoState("Create Link");
                        }
                    }
                    s_state.creating_link = false;
                    s_state.link_start_pin = -1;
                }
            }
        }
        // Output pins (with link creation hit test)
        for (size_t pi = 0; pi < node.outputs.size(); ++pi) {
            ImVec2 pin_pos(node_pos.x + node.size.x, node_pos.y + 30 + pi * 20);
            draw_list->AddCircleFilled(pin_pos, 5, BpPinColor(node.outputs[pi].type));
            float text_w = ImGui::CalcTextSize(node.outputs[pi].name.c_str()).x;
            draw_list->AddText(ImVec2(pin_pos.x - text_w - 8, pin_pos.y - 7), IM_COL32(200, 200, 200, 255), node.outputs[pi].name.c_str());

            ImVec2 hit_min(pin_pos.x - 8, pin_pos.y - 8);
            ImVec2 hit_max(pin_pos.x + 8, pin_pos.y + 8);
            if (canvas_hovered && ImGui::IsMouseHoveringRect(hit_min, hit_max)) {
                if (ImGui::IsMouseClicked(0)) {
                    s_state.creating_link = true;
                    s_state.link_start_pin = node.outputs[pi].id;
                }
                if (ImGui::IsMouseReleased(0) && s_state.creating_link && s_state.link_start_pin != node.outputs[pi].id) {
                    BpPin* start = find_pin(s_state.link_start_pin);
                    if (start && start->kind == BpPinKind::Input &&
                        pin_type_compatible(node.outputs[pi].type, start->type)) {
                        bool dup = false;
                        for (const auto& l : graph.links)
                            if (l.from_pin == node.outputs[pi].id && l.to_pin == s_state.link_start_pin) { dup = true; break; }
                        if (!dup) {
                            BpLink lnk;
                            lnk.id = AllocNodeId(graph);
                            lnk.from_pin = node.outputs[pi].id;
                            lnk.to_pin = s_state.link_start_pin;
                            graph.links.push_back(lnk);
                            s_state.dirty = true;
                            BpPushUndoState("Create Link");
                        }
                    }
                    s_state.creating_link = false;
                    s_state.link_start_pin = -1;
                }
            }
        }

        // Node selection & drag start (skip when starting a link on a pin)
        if (canvas_hovered && ImGui::IsMouseHoveringRect(node_pos, node_end) &&
            ImGui::IsMouseClicked(0) && !s_state.creating_link) {
            bool ctrl = ImGui::GetIO().KeyCtrl;
            auto& sel = s_state.selected_nodes;
            if (ctrl) {
                // Ctrl+click 加选 / 减选
                auto it = std::find(sel.begin(), sel.end(), node.id);
                if (it != sel.end()) {
                    sel.erase(it);
                    if (s_state.selected_node == node.id)
                        s_state.selected_node = sel.empty() ? -1 : sel[0];
                } else {
                    sel.push_back(node.id);
                    s_state.selected_node = node.id;
                }
            } else {
                if (std::find(sel.begin(), sel.end(), node.id) == sel.end()) {
                    sel.clear();
                    sel.push_back(node.id);
                }
                s_state.selected_node = node.id;
            }
            s_state.dragging_node = node.id;
            s_state.drag_offset = ImVec2(ImGui::GetMousePos().x - node_pos.x,
                                         ImGui::GetMousePos().y - node_pos.y);
        }

        // Update node size based on pin count
        float min_h = 30 + std::max(node.inputs.size(), node.outputs.size()) * 20 + 10;
        if (node.size.y < min_h) node.size.y = static_cast<float>(min_h);
    }

    // Node dragging (拖单个节点；若该节点在多选中，整组一起移动)
    if (s_state.dragging_node >= 0) {
        auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(),
            [&](const BpNode& n) { return n.id == s_state.dragging_node; });
        if (it != graph.nodes.end()) {
            if (ImGui::IsMouseDragging(0) && !s_state.creating_link) {
                float nx = ImGui::GetMousePos().x - canvas_pos.x - s_state.scroll_offset.x - s_state.drag_offset.x;
                float ny = ImGui::GetMousePos().y - canvas_pos.y - s_state.scroll_offset.y - s_state.drag_offset.y;
                float dx = nx - it->position.x;
                float dy = ny - it->position.y;
                // 拖动源节点自身
                it->position.x = nx;
                it->position.y = ny;
                // 同组其余选中节点跟随
                for (auto& n : graph.nodes) {
                    if (n.id == s_state.dragging_node) continue;
                    if (std::find(s_state.selected_nodes.begin(), s_state.selected_nodes.end(), n.id) != s_state.selected_nodes.end()) {
                        n.position.x += dx;
                        n.position.y += dy;
                    }
                }
                s_state.dirty = true;
            }
            if (ImGui::IsMouseReleased(0)) {
                s_state.dragging_node = -1;
                BpPushUndoState("Move Node");
            }
        } else {
            s_state.dragging_node = -1;
        }
    }

    // Cancel link creation when released on empty canvas
    if (s_state.creating_link && ImGui::IsMouseReleased(0)) {
        s_state.creating_link = false;
        s_state.link_start_pin = -1;
    }

    // Link selection (click near midpoint of a bezier)
    if (canvas_hovered && ImGui::IsMouseClicked(0) && s_state.dragging_node < 0 && !s_state.creating_link) {
        ImVec2 mp = ImGui::GetMousePos();
        s_state.selected_link = -1;
        for (const auto& link : graph.links) {
            BpNode* fn = find_pin_owner(link.from_pin);
            BpNode* tn = find_pin_owner(link.to_pin);
            if (!fn || !tn) continue;
            ImVec2 p1 = pin_screen_pos(*fn, link.from_pin, true);
            ImVec2 p2 = pin_screen_pos(*tn, link.to_pin, false);
            ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            float dx = mp.x - mid.x, dy = mp.y - mid.y;
            if (dx * dx + dy * dy < 12.0f * 12.0f) {
                s_state.selected_link = link.id;
                s_state.selected_node = -1;
                break;
            }
        }
    }

    // Delete key: delete selected node or link
    if (canvas_hovered && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (s_state.selected_link >= 0) {
            graph.links.erase(std::remove_if(graph.links.begin(), graph.links.end(),
                [&](const BpLink& l) { return l.id == s_state.selected_link; }), graph.links.end());
            s_state.selected_link = -1;
            s_state.dirty = true;
            BpPushUndoState("Delete Link");
        } else if (s_state.selected_node >= 0) {
            int nid = s_state.selected_node;
            // Collect pins of the node being deleted
            std::vector<int> dead_pins;
            for (const auto& n : graph.nodes) {
                if (n.id != nid) continue;
                for (const auto& p : n.inputs)  dead_pins.push_back(p.id);
                for (const auto& p : n.outputs) dead_pins.push_back(p.id);
            }
            graph.links.erase(std::remove_if(graph.links.begin(), graph.links.end(),
                [&](const BpLink& l) {
                    for (int pid : dead_pins)
                        if (l.from_pin == pid || l.to_pin == pid) return true;
                    return false;
                }), graph.links.end());
            graph.nodes.erase(std::remove_if(graph.nodes.begin(), graph.nodes.end(),
                [&](const BpNode& n) { return n.id == nid; }), graph.nodes.end());
            s_state.selected_node = -1;
            s_state.dirty = true;
            BpPushUndoState("Delete Node");
        }
    }

    // Interaction: right-click to create node
    ImGui::SetCursorScreenPos(canvas_pos);
    ImGui::InvisibleButton("##bp_canvas", canvas_size,
                            ImGuiButtonFlags_MouseButtonLeft |
                            ImGuiButtonFlags_MouseButtonRight |
                            ImGuiButtonFlags_MouseButtonMiddle);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
        s_state.show_create_menu = true;
        s_state.create_menu_pos = ImVec2(ImGui::GetMousePos().x - canvas_pos.x - s_state.scroll_offset.x,
                                          ImGui::GetMousePos().y - canvas_pos.y - s_state.scroll_offset.y);
        ImGui::OpenPopup("bp_create_menu");
    }

    // Scroll with middle mouse
    if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(2)) {
        s_state.scroll_offset.x += ImGui::GetIO().MouseDelta.x;
        s_state.scroll_offset.y += ImGui::GetIO().MouseDelta.y;
    }

    // Zoom with scroll wheel (fixed point at mouse)
    if (ImGui::IsItemHovered() && std::abs(ImGui::GetIO().MouseWheel) > 0.01f) {
        float old_zoom = s_state.zoom;
        s_state.zoom += ImGui::GetIO().MouseWheel * 0.1f;
        s_state.zoom = std::max(0.3f, std::min(s_state.zoom, 3.0f));
        float factor = s_state.zoom / old_zoom;
        ImVec2 mp = ImGui::GetMousePos();
        s_state.scroll_offset.x = mp.x - canvas_pos.x - (mp.x - canvas_pos.x - s_state.scroll_offset.x) * factor;
        s_state.scroll_offset.y = mp.y - canvas_pos.y - (mp.y - canvas_pos.y - s_state.scroll_offset.y) * factor;
    }

    // Box selection: drag empty canvas with left mouse (skip when creating link)
    bool canvas_item_hovered = ImGui::IsItemHovered();
    if (canvas_item_hovered && ImGui::IsMouseClicked(0) && s_state.dragging_node < 0 &&
        !s_state.creating_link && s_state.selected_link < 0) {
        // Only start box-select when click hits empty canvas (not a node/pin).
        // Node/pin clicks set dragging_node/creating_link, so reaching here with
        // neither means empty space.
        ImVec2 mp = ImGui::GetMousePos();
        ImVec2 canvas_mp(mp.x - canvas_pos.x - s_state.scroll_offset.x,
                         mp.y - canvas_pos.y - s_state.scroll_offset.y);
        s_state.box_selecting = true;
        s_state.box_select_start = canvas_mp;
        if (!ImGui::GetIO().KeyCtrl) {
            s_state.selected_nodes.clear();
            s_state.selected_node = -1;
        }
    }
    if (s_state.box_selecting && ImGui::IsMouseReleased(0)) {
        s_state.box_selecting = false;
        ImVec2 mp = ImGui::GetMousePos();
        ImVec2 canvas_mp(mp.x - canvas_pos.x - s_state.scroll_offset.x,
                         mp.y - canvas_pos.y - s_state.scroll_offset.y);
        float minx = std::min(s_state.box_select_start.x, canvas_mp.x);
        float miny = std::min(s_state.box_select_start.y, canvas_mp.y);
        float maxx = std::max(s_state.box_select_start.x, canvas_mp.x);
        float maxy = std::max(s_state.box_select_start.y, canvas_mp.y);
        auto& sel = s_state.selected_nodes;
        for (const auto& n : graph.nodes) {
            if (n.position.x <= maxx && n.position.x + n.size.x >= minx &&
                n.position.y <= maxy && n.position.y + n.size.y >= miny) {
                if (std::find(sel.begin(), sel.end(), n.id) == sel.end()) sel.push_back(n.id);
            }
        }
        if (!s_state.selected_nodes.empty()) s_state.selected_node = s_state.selected_nodes[0];
    }

    // Draw box selection rectangle
    if (s_state.box_selecting) {
        ImVec2 mp = ImGui::GetMousePos();
        ImVec2 a(canvas_pos.x + s_state.scroll_offset.x + s_state.box_select_start.x,
                 canvas_pos.y + s_state.scroll_offset.y + s_state.box_select_start.y);
        draw_list->AddRectFilled(a, mp, IM_COL32(80, 120, 200, 60));
        draw_list->AddRect(a, mp, IM_COL32(120, 160, 240, 200), 0, 0, 1.5f);
    }

    // Keyboard shortcuts: copy / paste / duplicate selected nodes
    if (canvas_item_hovered || s_state.selected_node >= 0 || !s_state.selected_nodes.empty()) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !ImGui::GetIO().KeyShift) {
            // Copy
            s_state.clipboard_nodes.clear();
            s_state.clipboard_links.clear();
            s_state.clipboard_next_id = 1;
            std::vector<int> copy_ids;
            for (const auto& id : s_state.selected_nodes) copy_ids.push_back(id);
            if (copy_ids.empty() && s_state.selected_node >= 0) copy_ids.push_back(s_state.selected_node);
            if (!copy_ids.empty()) {
                std::unordered_map<int,int> pin_map;
                for (const auto& n : graph.nodes) {
                    if (std::find(copy_ids.begin(), copy_ids.end(), n.id) == copy_ids.end()) continue;
                    BpNode c = n;
                    // 记录旧 pin id → 新 id 映射
                    c.id = s_state.clipboard_next_id++;
                    c.inputs.clear(); c.outputs.clear();
                    for (const auto& p : n.inputs) {
                        BpPin np = p; np.id = s_state.clipboard_next_id++; np.kind = BpPinKind::Input;
                        pin_map[p.id] = np.id; c.inputs.push_back(np);
                    }
                    for (const auto& p : n.outputs) {
                        BpPin np = p; np.id = s_state.clipboard_next_id++; np.kind = BpPinKind::Output;
                        pin_map[p.id] = np.id; c.outputs.push_back(np);
                    }
                    s_state.clipboard_nodes.push_back(std::move(c));
                }
                for (const auto& l : graph.links) {
                    if (pin_map.count(l.from_pin) && pin_map.count(l.to_pin)) {
                        BpLink cl = l;
                        cl.from_pin = pin_map[l.from_pin];
                        cl.to_pin = pin_map[l.to_pin];
                        s_state.clipboard_links.push_back(cl);
                    }
                }
            }
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
            // Paste at mouse position
            if (!s_state.clipboard_nodes.empty()) {
                BpPushUndoState("Paste Nodes");
                std::unordered_map<int,int> pin_map;
                ImVec2 mouse = ImGui::GetMousePos();
                float base_x = mouse.x - canvas_pos.x - s_state.scroll_offset.x;
                float base_y = mouse.y - canvas_pos.y - s_state.scroll_offset.y;
                s_state.selected_nodes.clear();
                for (const auto& c : s_state.clipboard_nodes) {
                    BpNode n = c;
                    n.id = AllocNodeId(graph);
                    // 偏移使其靠近粘贴位置（以剪贴板第一个节点为基准）
                    n.position.x = base_x + (c.position.x - s_state.clipboard_nodes[0].position.x);
                    n.position.y = base_y + (c.position.y - s_state.clipboard_nodes[0].position.y);
                    n.inputs.clear(); n.outputs.clear();
                    for (const auto& p : c.inputs) {
                        BpPin np = p; np.id = AllocNodeId(graph); np.kind = BpPinKind::Input;
                        pin_map[p.id] = np.id; n.inputs.push_back(np);
                    }
                    for (const auto& p : c.outputs) {
                        BpPin np = p; np.id = AllocNodeId(graph); np.kind = BpPinKind::Output;
                        pin_map[p.id] = np.id; n.outputs.push_back(np);
                    }
                    graph.nodes.push_back(n);
                    s_state.selected_nodes.push_back(n.id);
                }
                for (const auto& cl : s_state.clipboard_links) {
                    if (pin_map.count(cl.from_pin) && pin_map.count(cl.to_pin)) {
                        BpLink nl = cl;
                        nl.id = AllocNodeId(graph);
                        nl.from_pin = pin_map[cl.from_pin];
                        nl.to_pin = pin_map[cl.to_pin];
                        graph.links.push_back(nl);
                    }
                }
                if (!s_state.selected_nodes.empty()) s_state.selected_node = s_state.selected_nodes[0];
                s_state.dirty = true;
            }
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
            // Duplicate selected (Ctrl+D) — 等价复制+偏移
            if (!s_state.selected_nodes.empty() || s_state.selected_node >= 0) {
                std::unordered_map<int,int> pin_map;
                std::vector<int> src_ids;
                for (const auto& id : s_state.selected_nodes) src_ids.push_back(id);
                if (src_ids.empty() && s_state.selected_node >= 0) src_ids.push_back(s_state.selected_node);
                BpPushUndoState("Duplicate Nodes");
                std::vector<int> new_ids;
                for (const auto& n : graph.nodes) {
                    if (std::find(src_ids.begin(), src_ids.end(), n.id) == src_ids.end()) continue;
                    BpNode c = n;
                    c.id = AllocNodeId(graph);
                    c.position.x += 30; c.position.y += 30;
                    c.inputs.clear(); c.outputs.clear();
                    for (const auto& p : n.inputs) {
                        BpPin np = p; np.id = AllocNodeId(graph); np.kind = BpPinKind::Input;
                        pin_map[p.id] = np.id; c.inputs.push_back(np);
                    }
                    for (const auto& p : n.outputs) {
                        BpPin np = p; np.id = AllocNodeId(graph); np.kind = BpPinKind::Output;
                        pin_map[p.id] = np.id; c.outputs.push_back(np);
                    }
                    graph.nodes.push_back(c);
                    new_ids.push_back(c.id);
                }
                for (const auto& l : graph.links) {
                    if (pin_map.count(l.from_pin) && pin_map.count(l.to_pin)) {
                        BpLink nl = l;
                        nl.id = AllocNodeId(graph);
                        nl.from_pin = pin_map[l.from_pin];
                        nl.to_pin = pin_map[l.to_pin];
                        graph.links.push_back(nl);
                    }
                }
                s_state.selected_nodes = new_ids;
                if (!new_ids.empty()) s_state.selected_node = new_ids[0];
                s_state.dirty = true;
            }
        }
    }

    // Multi-select with Ctrl+click handled in node loop below (IsMouseClicked on node).
    // Align selected nodes (right-click menu below)

    // ── Comment box interaction: select / drag / double-click edit / delete ──
    if (canvas_item_hovered) {
        // Select + drag (title bar drag; resize not implemented to keep it simple)
        bool comment_hit = false;
        for (int i = static_cast<int>(s_state.comments.size()) - 1; i >= 0; --i) {
            auto& comment = s_state.comments[i];
            ImVec2 cpos(canvas_pos.x + s_state.scroll_offset.x + comment.position.x,
                        canvas_pos.y + s_state.scroll_offset.y + comment.position.y);
            ImVec2 cend(cpos.x + comment.size.x, cpos.y + comment.size.y);
            ImRect title_rect(cpos, ImVec2(cend.x, cpos.y + 22));
            ImRect body_rect(cpos, cend);
            if (ImGui::IsMouseHoveringRect(cpos, cend)) {
                comment_hit = true;
                if (ImGui::IsMouseClicked(0)) {
                    s_state.selected_comment = i;
                    s_state.selected_node = -1;
                    s_state.selected_link = -1;
                    // 双击标题栏进入编辑
                    if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsMouseHoveringRect(title_rect.Min, title_rect.Max)) {
                        s_state.editing_comment = true;
                        snprintf(s_state.comment_edit_buf, sizeof(s_state.comment_edit_buf), "%s", comment.text.c_str());
                    }
                    if (ImGui::IsMouseHoveringRect(title_rect.Min, title_rect.Max)) {
                        s_state.dragging_comment = true;
                        s_state.dragging_comment_idx = i;
                        s_state.comment_drag_offset = ImVec2(ImGui::GetMousePos().x - cpos.x,
                                                             ImGui::GetMousePos().y - cpos.y);
                    }
                }
                // 右键删除
                if (ImGui::IsMouseClicked(1) && ImGui::IsMouseHoveringRect(title_rect.Min, title_rect.Max)) {
                    s_state.selected_comment = i;
                }
            }
        }
        // 拖动更新
        if (s_state.dragging_comment && s_state.dragging_comment_idx >= 0) {
            int idx = s_state.dragging_comment_idx;
            if (idx < static_cast<int>(s_state.comments.size())) {
                auto& comment = s_state.comments[idx];
                if (ImGui::IsMouseDragging(0)) {
                    comment.position.x = ImGui::GetMousePos().x - canvas_pos.x - s_state.scroll_offset.x - s_state.comment_drag_offset.x;
                    comment.position.y = ImGui::GetMousePos().y - canvas_pos.y - s_state.scroll_offset.y - s_state.comment_drag_offset.y;
                    s_state.dirty = true;
                }
                if (ImGui::IsMouseReleased(0)) {
                    s_state.dragging_comment = false;
                    s_state.dragging_comment_idx = -1;
                    BpPushUndoState("Move Comment");
                }
            } else {
                s_state.dragging_comment = false;
                s_state.dragging_comment_idx = -1;
            }
        }
        // 空白处点击取消注释选中
        if (ImGui::IsMouseClicked(0) && !comment_hit && s_state.dragging_node < 0 &&
            !s_state.creating_link && !s_state.box_selecting && s_state.selected_comment >= 0) {
            s_state.selected_comment = -1;
        }
    }

    // 双击编辑中的注释：在画布顶层渲染输入框（使用 BeginPopup 技巧不可靠，直接叠加窗口）
    if (s_state.editing_comment && s_state.selected_comment >= 0 &&
        s_state.selected_comment < static_cast<int>(s_state.comments.size())) {
        ImGui::SetNextWindowPos(ImVec2(canvas_pos.x + s_state.scroll_offset.x + s_state.comments[s_state.selected_comment].position.x + 4,
                                       canvas_pos.y + s_state.scroll_offset.y + s_state.comments[s_state.selected_comment].position.y + 24));
        ImGui::SetNextWindowSize(ImVec2(280, 0));
        ImGui::Begin("##bp_comment_edit", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##comment_text", s_state.comment_edit_buf, sizeof(s_state.comment_edit_buf),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            s_state.comments[s_state.selected_comment].text = s_state.comment_edit_buf;
            s_state.editing_comment = false;
            s_state.dirty = true;
            BpPushUndoState("Edit Comment");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) s_state.editing_comment = false;
        if (ImGui::IsMouseClicked(0)) {
            ImVec2 edit_win_pos = ImGui::GetWindowPos();
            ImVec2 edit_win_size = ImGui::GetWindowSize();
            if (!ImGui::IsMouseHoveringRect(edit_win_pos,
                    ImVec2(edit_win_pos.x + edit_win_size.x, edit_win_pos.y + edit_win_size.y))) {
                s_state.editing_comment = false;
            }
        }
        ImGui::End();
    }

    // Create node context menu (enhanced with search #3 and Add Comment #4)
    if (ImGui::BeginPopup("bp_create_menu")) {
        // Add Comment / Add Group options at top
        if (ImGui::MenuItem(MDI_ICON_COMMENT_TEXT_OUTLINE " Add Comment")) {
            BpAddComment(s_state.create_menu_pos);
        }
        if (s_state.selected_comment >= 0 &&
            s_state.selected_comment < static_cast<int>(s_state.comments.size())) {
            if (ImGui::MenuItem(MDI_ICON_DELETE " Delete Comment")) {
                BpPushUndoState("Delete Comment");
                s_state.comments.erase(s_state.comments.begin() + s_state.selected_comment);
                s_state.selected_comment = -1;
                s_state.dirty = true;
            }
        }
        if (s_state.selected_node >= 0) {
            if (ImGui::MenuItem(MDI_ICON_GROUP " Group Selected")) {
                BpAddNodeGroup("Group", s_state.selected_nodes.empty()
                    ? std::vector<int>{s_state.selected_node} : s_state.selected_nodes);
            }
            // 对齐 (仅多选时可用)
            if (s_state.selected_nodes.size() >= 2) {
                ImGui::Separator();
                if (ImGui::MenuItem("Align Left")) {
                    float minx = 1e9f;
                    for (const auto& n : graph.nodes)
                        if (std::find(s_state.selected_nodes.begin(), s_state.selected_nodes.end(), n.id) != s_state.selected_nodes.end())
                            minx = std::min(minx, n.position.x);
                    for (auto& n : graph.nodes)
                        if (std::find(s_state.selected_nodes.begin(), s_state.selected_nodes.end(), n.id) != s_state.selected_nodes.end())
                            n.position.x = minx;
                    s_state.dirty = true;
                    BpPushUndoState("Align Left");
                }
                if (ImGui::MenuItem("Align Top")) {
                    float miny = 1e9f;
                    for (const auto& n : graph.nodes)
                        if (std::find(s_state.selected_nodes.begin(), s_state.selected_nodes.end(), n.id) != s_state.selected_nodes.end())
                            miny = std::min(miny, n.position.y);
                    for (auto& n : graph.nodes)
                        if (std::find(s_state.selected_nodes.begin(), s_state.selected_nodes.end(), n.id) != s_state.selected_nodes.end())
                            n.position.y = miny;
                    s_state.dirty = true;
                    BpPushUndoState("Align Top");
                }
            }
        }
        // Toggle breakpoint on selected node
        if (s_state.selected_node >= 0) {
            bool has_bp = s_state.debug.HasBreakpoint(s_state.selected_node);
            if (ImGui::MenuItem(has_bp ? "Remove Breakpoint" : "Add Breakpoint")) {
                s_state.debug.ToggleBreakpoint(s_state.selected_node);
            }
        }
        ImGui::Separator();

        // Enhanced node search popup (#3)
        DrawNodeSearchPopup();
        ImGui::EndPopup();
    }
}

// ─── Lua Preview Panel ─────────────────────────────────────────────────────

void DrawLuaPreview() {
    ImGui::BeginChild("##bp_lua_preview", ImVec2(0, 150), true);
    ImGui::Text("Generated Lua:");
    ImGui::Separator();
    ImGui::TextWrapped("%s", s_state.generated_lua.c_str());
    if (!s_state.compilation_errors.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 80, 80, 255));
        ImGui::TextWrapped("%s", s_state.compilation_errors.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}

} // anonymous namespace

// ─── Main Draw ─────────────────────────────────────────────────────────────

void DrawBlueprintEditor(EditorContext& /*ctx*/) {
    // Keyboard shortcuts: Undo/Redo
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !ImGui::GetIO().KeyShift) {
        BpUndo();
    }
    if (ImGui::GetIO().KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) ||
        (ImGui::IsKeyPressed(ImGuiKey_Z) && ImGui::GetIO().KeyShift))) {
        BpRedo();
    }

    // Toolbar
    if (ImGui::Button(T("New"))) {
        // 新建空白蓝图 (丢弃当前编辑内容, 无确认以保持轻量; 顶部有 Save 按钮)
        s_state.asset = BlueprintAsset{};
        s_state.asset.name = "NewBlueprint";
        s_state.asset.graphs.push_back(BpFunctionGraph{});
        s_state.asset.graphs[0].name = "EventGraph";
        s_state.asset.graphs[0].next_id = 1;
        s_state.active_graph_index = 0;
        s_state.selected_node = -1;
        s_state.selected_link = -1;
        s_state.selected_variable = -1;
        s_state.generated_lua.clear();
        s_state.compilation_errors.clear();
        s_state.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(T("Open"))) {
        std::string path = dse::editor::OpenFileDialog("Open Blueprint",
            "Blueprint Files (*.dbp)\0*.dbp\0All Files (*.*)\0*.*\0", "dbp");
        if (!path.empty()) {
            BlueprintAsset loaded;
            if (LoadBlueprintAsset(loaded, path)) {
                s_state.asset = std::move(loaded);
                if (s_state.asset.graphs.empty()) {
                    s_state.asset.graphs.push_back(BpFunctionGraph{});
                    s_state.asset.graphs[0].name = "EventGraph";
                    s_state.asset.graphs[0].next_id = 1;
                }
                s_state.active_graph_index = 0;
                s_state.selected_node = -1;
                s_state.selected_link = -1;
                s_state.dirty = false;
                s_state.generated_lua = CompileToLua(s_state.asset);
                s_state.compilation_errors.clear();
                if (!s_state.asset.graphs.empty()) {
                    auto result = ValidateGraph(s_state.asset.graphs[s_state.active_graph_index]);
                    if (!result.valid) {
                        for (const auto& e : result.errors) s_state.compilation_errors += e + "\n";
                    }
                }
            } else {
                s_state.compilation_errors = "Failed to load blueprint: " + path;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(T("Save"))) {
        std::string path = s_state.asset.file_path;
        if (path.empty()) {
            path = dse::editor::SaveFileDialog("Save Blueprint",
                "Blueprint Files (*.dbp)\0*.dbp\0All Files (*.*)\0*.*\0", "dbp",
                s_state.asset.name.empty() ? "blueprint" : s_state.asset.name.c_str());
        }
        if (!path.empty()) {
            if (SaveBlueprintAsset(s_state.asset, path)) {
                s_state.asset.file_path = path;
                s_state.dirty = false;
            } else {
                s_state.compilation_errors = "Failed to save blueprint: " + path;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(T("Compile"))) {
        s_state.generated_lua = CompileToLua(s_state.asset);
        s_state.compilation_errors.clear();
        if (!s_state.asset.graphs.empty()) {
            auto result = ValidateGraph(s_state.asset.graphs[s_state.active_graph_index]);
            if (!result.valid) {
                for (const auto& e : result.errors) s_state.compilation_errors += e + "\n";
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(MDI_ICON_UNDO " Undo")) BpUndo();
    ImGui::SameLine();
    if (ImGui::Button(MDI_ICON_REDO " Redo")) BpRedo();
    ImGui::SameLine();
    ImGui::Checkbox("Auto Compile", &s_state.auto_compile);
    ImGui::SameLine();
    ImGui::Text("| %s", s_state.asset.name.c_str());
    ImGui::SameLine();
    int nc = BpNodeCount(), lc = BpLinkCount();
    ImGui::TextDisabled("(Nodes: %d  Links: %d  Vars: %d)", nc, lc, BpVariableCount());

    // Graph tabs
    DrawGraphTabs();

    // Main layout: templates left, variables, canvas center, debug bottom
    static bool show_templates = false;
    if (show_templates) {
        DrawBpTemplatePanel();
        ImGui::SameLine();
    }

    DrawVariablePanel();
    ImGui::SameLine();

    ImGui::BeginGroup();
    DrawNodeCanvas();
    DrawLuaPreview();
    DrawBlueprintDebugPanel();
    ImGui::EndGroup();

    // Template toggle (in menu bar area)
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    ImGui::Checkbox("Templates", &show_templates);

    // Auto compile
    if (s_state.auto_compile && s_state.dirty) {
        s_state.generated_lua = CompileToLua(s_state.asset);
        s_state.dirty = false;
    }
}

// ─── Serialization ─────────────────────────────────────────────────────────

namespace {

// editor::bp <-> dse::bp 转换：编辑器 DTO 是共享运行时契约的"超集"，仅多出画布
// 坐标/颜色/Lua 模板等授权可视信息。.dbp 序列化统一走引擎侧共享层
// (blueprint_serialize)，编辑器不再自带 JSON 解析/写出逻辑（单一数据源）。

dse::bp::BpPin ToSharedPin(const BpPin& p) {
    dse::bp::BpPin s;
    s.id = p.id;
    s.name = p.name;
    s.type = static_cast<dse::bp::BpPinType>(static_cast<int>(p.type));
    s.kind = static_cast<dse::bp::BpPinKind>(static_cast<int>(p.kind));
    s.default_float = p.default_float;
    for (int i = 0; i < 4; ++i) s.default_vec[i] = p.default_vec[i];
    s.default_string = p.default_string;
    s.default_bool = p.default_bool;
    s.default_int = p.default_int;
    return s;
}

BpPin FromSharedPin(const dse::bp::BpPin& s) {
    BpPin p;
    p.id = s.id;
    p.name = s.name;
    p.type = static_cast<BpPinType>(static_cast<int>(s.type));
    p.kind = static_cast<BpPinKind>(static_cast<int>(s.kind));
    p.default_float = s.default_float;
    for (int i = 0; i < 4; ++i) p.default_vec[i] = s.default_vec[i];
    snprintf(p.default_string, sizeof(p.default_string), "%s", s.default_string.c_str());
    p.default_bool = s.default_bool;
    p.default_int = s.default_int;
    return p;
}

dse::bp::BlueprintAsset ToShared(const BlueprintAsset& a) {
    dse::bp::BlueprintAsset s;
    s.name = a.name;
    s.file_path = a.file_path;
    s.version = a.version;
    s.description = a.description;
    s.author = a.author;
    s.implemented_interfaces = a.implemented_interfaces;
    for (const auto& v : a.variables) {
        dse::bp::BpVariable sv;
        sv.name = v.name;
        sv.type = static_cast<dse::bp::BpVarType>(static_cast<int>(v.type));
        sv.default_bool = v.default_bool;
        sv.default_int = v.default_int;
        sv.default_float = v.default_float;
        sv.default_string = v.default_string;
        for (int i = 0; i < 4; ++i) sv.default_vec[i] = v.default_vec[i];
        sv.is_exposed = v.is_exposed;
        s.variables.push_back(std::move(sv));
    }
    for (const auto& g : a.graphs) {
        dse::bp::BpFunctionGraph sg;
        sg.name = g.name;
        sg.is_pure = g.is_pure;
        sg.next_id = g.next_id;
        for (const auto& n : g.nodes) {
            dse::bp::BpNode sn;
            sn.id = n.id;
            sn.name = n.name;
            sn.category = n.category;
            sn.comment = n.comment;
            sn.pos_x = n.position.x;
            sn.pos_y = n.position.y;
            for (const auto& p : n.inputs) sn.inputs.push_back(ToSharedPin(p));
            for (const auto& p : n.outputs) sn.outputs.push_back(ToSharedPin(p));
            sg.nodes.push_back(std::move(sn));
        }
        for (const auto& l : g.links) {
            dse::bp::BpLink sl;
            sl.id = l.id;
            sl.from_pin = l.from_pin;
            sl.to_pin = l.to_pin;
            sg.links.push_back(sl);
        }
        for (const auto& p : g.input_params) sg.input_params.push_back(ToSharedPin(p));
        for (const auto& p : g.output_params) sg.output_params.push_back(ToSharedPin(p));
        s.graphs.push_back(std::move(sg));
    }
    return s;
}

BlueprintAsset FromShared(const dse::bp::BlueprintAsset& s) {
    BlueprintAsset a;
    a.name = s.name;
    a.file_path = s.file_path;
    a.version = s.version;
    a.description = s.description;
    a.author = s.author;
    a.implemented_interfaces = s.implemented_interfaces;
    for (const auto& v : s.variables) {
        BpVariable av;
        av.name = v.name;
        av.type = static_cast<BpVarType>(static_cast<int>(v.type));
        av.default_bool = v.default_bool;
        av.default_int = v.default_int;
        av.default_float = v.default_float;
        snprintf(av.default_string, sizeof(av.default_string), "%s", v.default_string.c_str());
        for (int i = 0; i < 4; ++i) av.default_vec[i] = v.default_vec[i];
        av.is_exposed = v.is_exposed;
        a.variables.push_back(std::move(av));
    }
    for (const auto& g : s.graphs) {
        BpFunctionGraph ag;
        ag.name = g.name;
        ag.is_pure = g.is_pure;
        ag.next_id = g.next_id;
        for (const auto& n : g.nodes) {
            BpNode an;
            an.id = n.id;
            an.name = n.name;
            an.category = n.category;
            an.comment = n.comment;
            an.position = ImVec2(n.pos_x, n.pos_y);
            // 从节点注册表恢复仅编辑器的渲染信息（颜色/Lua 模板），不入 .dbp。
            const NodeTemplate* tmpl = NodeRegistry::Get().Find(an.name);
            if (tmpl) {
                an.header_color = tmpl->header_color;
                an.code_template = tmpl->code_template;
            }
            for (const auto& p : n.inputs) an.inputs.push_back(FromSharedPin(p));
            for (const auto& p : n.outputs) an.outputs.push_back(FromSharedPin(p));
            ag.nodes.push_back(std::move(an));
        }
        for (const auto& l : g.links) {
            BpLink al;
            al.id = l.id;
            al.from_pin = l.from_pin;
            al.to_pin = l.to_pin;
            ag.links.push_back(al);
        }
        for (const auto& p : g.input_params) ag.input_params.push_back(FromSharedPin(p));
        for (const auto& p : g.output_params) ag.output_params.push_back(FromSharedPin(p));
        a.graphs.push_back(std::move(ag));
    }
    return a;
}

}  // namespace

bool SaveBlueprintAsset(const BlueprintAsset& asset, const std::string& path) {
    dse::bp::BlueprintDiagnostics diag;
    return dse::bp::SaveBlueprintAsset(ToShared(asset), path, diag);
}

bool LoadBlueprintAsset(BlueprintAsset& asset, const std::string& path) {
    dse::bp::BlueprintAsset shared;
    dse::bp::BlueprintDiagnostics diag;
    if (!dse::bp::LoadBlueprintAssetChecked(shared, path, diag)) return false;
    asset = FromShared(shared);
    asset.file_path = path;
    return true;
}

// P0-6 self-registration: data-driven; editor_app binds visibility by id.
DSE_EDITOR_PANEL([](dse::editor::PanelRegistry& reg) {
    dse::editor::PanelEntry e;
    e.id = "blueprint";
    e.display_name = "Blueprint";
    e.category = "Tool";
    e.menu_icon = MDI_ICON_SITEMAP;
    e.order = 260;
    e.draw = [](dse::editor::EditorContext& ctx) {
        auto* self = dse::editor::PanelRegistry::Get().Find("blueprint");
        bool* open = self ? self->visible : nullptr;
        ImGui::SetNextWindowSize(ImVec2(1100, 700), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Blueprint Editor", open)) {
            PanelRegistry::Get().DrawMaximizeRestoreButton();
            dse::editor::bp::DrawBlueprintEditor(ctx);
        }
        ImGui::End();
    };
    reg.Register(std::move(e));
});

}  // namespace dse::editor::bp
