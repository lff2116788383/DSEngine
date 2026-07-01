/**
 * @file editor_blueprint.cpp
 * @brief Blueprint editor UI - variable panel, node registry, function graphs, canvas
 */

#include "editor_blueprint.h"
#include "editor_blueprint_vm.h"
#include "editor_blueprint_compiler.h"
#include "editor_icons.h"
#include "editor_locale.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

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

    if (ImGui::Button("+ Add Variable")) {
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
            if (ImGui::MenuItem("Delete")) {
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
            draw_list->AddBezierCubic(p1, cp1, cp2, p2, IM_COL32(200, 200, 200, 200), 2.0f);
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
        bool is_selected = (node.id == s_state.selected_node);
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

        // Input pins
        for (size_t pi = 0; pi < node.inputs.size(); ++pi) {
            ImVec2 pin_pos(node_pos.x, node_pos.y + 30 + pi * 20);
            draw_list->AddCircleFilled(pin_pos, 5, BpPinColor(node.inputs[pi].type));
            draw_list->AddText(ImVec2(pin_pos.x + 8, pin_pos.y - 7), IM_COL32(200, 200, 200, 255), node.inputs[pi].name.c_str());
        }
        // Output pins
        for (size_t pi = 0; pi < node.outputs.size(); ++pi) {
            ImVec2 pin_pos(node_pos.x + node.size.x, node_pos.y + 30 + pi * 20);
            draw_list->AddCircleFilled(pin_pos, 5, BpPinColor(node.outputs[pi].type));
            float text_w = ImGui::CalcTextSize(node.outputs[pi].name.c_str()).x;
            draw_list->AddText(ImVec2(pin_pos.x - text_w - 8, pin_pos.y - 7), IM_COL32(200, 200, 200, 255), node.outputs[pi].name.c_str());
        }

        // Update node size based on pin count
        float min_h = 30 + std::max(node.inputs.size(), node.outputs.size()) * 20 + 10;
        if (node.size.y < min_h) node.size.y = static_cast<float>(min_h);
    }

    // Interaction: right-click to create node
    ImGui::SetCursorScreenPos(canvas_pos);
    ImGui::InvisibleButton("##bp_canvas", canvas_size);
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

    // Create node context menu (enhanced with search #3 and Add Comment #4)
    if (ImGui::BeginPopup("bp_create_menu")) {
        // Add Comment / Add Group options at top
        if (ImGui::MenuItem(MDI_ICON_COMMENT_TEXT_OUTLINE " Add Comment")) {
            BpAddComment(s_state.create_menu_pos);
        }
        if (s_state.selected_node >= 0) {
            if (ImGui::MenuItem(MDI_ICON_GROUP " Group Selected")) {
                BpAddNodeGroup("Group", {s_state.selected_node});
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
    if (ImGui::Button("Compile")) {
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

bool SaveBlueprintAsset(const BlueprintAsset& asset, const std::string& path) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("name", rapidjson::Value(asset.name.c_str(), alloc), alloc);
    doc.AddMember("version", asset.version, alloc);
    doc.AddMember("description", rapidjson::Value(asset.description.c_str(), alloc), alloc);

    // Variables
    rapidjson::Value vars_arr(rapidjson::kArrayType);
    for (const auto& var : asset.variables) {
        rapidjson::Value v(rapidjson::kObjectType);
        v.AddMember("name", rapidjson::Value(var.name.c_str(), alloc), alloc);
        v.AddMember("type", rapidjson::Value(BpVarTypeName(var.type), alloc), alloc);
        v.AddMember("default_bool", var.default_bool, alloc);
        v.AddMember("default_int", var.default_int, alloc);
        v.AddMember("default_float", var.default_float, alloc);
        v.AddMember("default_string", rapidjson::Value(var.default_string, alloc), alloc);
        rapidjson::Value vec_arr(rapidjson::kArrayType);
        for (int i = 0; i < 4; ++i) vec_arr.PushBack(var.default_vec[i], alloc);
        v.AddMember("default_vec", vec_arr, alloc);
        v.AddMember("is_exposed", var.is_exposed, alloc);
        vars_arr.PushBack(v, alloc);
    }
    doc.AddMember("variables", vars_arr, alloc);

    // Graphs
    rapidjson::Value graphs_arr(rapidjson::kArrayType);
    for (const auto& graph : asset.graphs) {
        rapidjson::Value g(rapidjson::kObjectType);
        g.AddMember("name", rapidjson::Value(graph.name.c_str(), alloc), alloc);
        g.AddMember("next_id", graph.next_id, alloc);
        g.AddMember("is_pure", graph.is_pure, alloc);

        // Nodes
        rapidjson::Value nodes_arr(rapidjson::kArrayType);
        for (const auto& node : graph.nodes) {
            rapidjson::Value n(rapidjson::kObjectType);
            n.AddMember("id", node.id, alloc);
            n.AddMember("name", rapidjson::Value(node.name.c_str(), alloc), alloc);
            n.AddMember("category", rapidjson::Value(node.category.c_str(), alloc), alloc);
            n.AddMember("pos_x", node.position.x, alloc);
            n.AddMember("pos_y", node.position.y, alloc);
            n.AddMember("comment", rapidjson::Value(node.comment.c_str(), alloc), alloc);

            // Pins
            rapidjson::Value ins(rapidjson::kArrayType);
            for (const auto& p : node.inputs) {
                rapidjson::Value pin(rapidjson::kObjectType);
                pin.AddMember("id", p.id, alloc);
                pin.AddMember("name", rapidjson::Value(p.name.c_str(), alloc), alloc);
                pin.AddMember("type", rapidjson::Value(BpPinTypeName(p.type), alloc), alloc);
                pin.AddMember("default_float", p.default_float, alloc);
                pin.AddMember("default_int", p.default_int, alloc);
                pin.AddMember("default_bool", p.default_bool, alloc);
                ins.PushBack(pin, alloc);
            }
            n.AddMember("inputs", ins, alloc);

            rapidjson::Value outs(rapidjson::kArrayType);
            for (const auto& p : node.outputs) {
                rapidjson::Value pin(rapidjson::kObjectType);
                pin.AddMember("id", p.id, alloc);
                pin.AddMember("name", rapidjson::Value(p.name.c_str(), alloc), alloc);
                pin.AddMember("type", rapidjson::Value(BpPinTypeName(p.type), alloc), alloc);
                outs.PushBack(pin, alloc);
            }
            n.AddMember("outputs", outs, alloc);
            nodes_arr.PushBack(n, alloc);
        }
        g.AddMember("nodes", nodes_arr, alloc);

        // Links
        rapidjson::Value links_arr(rapidjson::kArrayType);
        for (const auto& link : graph.links) {
            rapidjson::Value l(rapidjson::kObjectType);
            l.AddMember("id", link.id, alloc);
            l.AddMember("from_pin", link.from_pin, alloc);
            l.AddMember("to_pin", link.to_pin, alloc);
            links_arr.PushBack(l, alloc);
        }
        g.AddMember("links", links_arr, alloc);

        graphs_arr.PushBack(g, alloc);
    }
    doc.AddMember("graphs", graphs_arr, alloc);

    // Interfaces
    rapidjson::Value ifaces(rapidjson::kArrayType);
    for (const auto& iface : asset.implemented_interfaces) {
        ifaces.PushBack(rapidjson::Value(iface.c_str(), alloc), alloc);
    }
    doc.AddMember("interfaces", ifaces, alloc);

    // Write
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << buffer.GetString();
    return true;
}

bool LoadBlueprintAsset(BlueprintAsset& asset, const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError() || !doc.IsObject()) return false;

    asset.file_path = path;
    if (doc.HasMember("name")) asset.name = doc["name"].GetString();
    if (doc.HasMember("version")) asset.version = doc["version"].GetInt();
    if (doc.HasMember("description")) asset.description = doc["description"].GetString();

    // Variables
    if (doc.HasMember("variables") && doc["variables"].IsArray()) {
        for (auto& v : doc["variables"].GetArray()) {
            BpVariable var;
            if (v.HasMember("name")) var.name = v["name"].GetString();
            if (v.HasMember("type")) var.type = BpVarTypeFromName(v["type"].GetString());
            if (v.HasMember("default_bool")) var.default_bool = v["default_bool"].GetBool();
            if (v.HasMember("default_int")) var.default_int = v["default_int"].GetInt();
            if (v.HasMember("default_float")) var.default_float = v["default_float"].GetFloat();
            if (v.HasMember("default_string")) snprintf(var.default_string, sizeof(var.default_string), "%s", v["default_string"].GetString());
            if (v.HasMember("default_vec") && v["default_vec"].IsArray()) {
                auto arr = v["default_vec"].GetArray();
                for (int i = 0; i < 4 && i < static_cast<int>(arr.Size()); ++i)
                    var.default_vec[i] = arr[i].GetFloat();
            }
            if (v.HasMember("is_exposed")) var.is_exposed = v["is_exposed"].GetBool();
            asset.variables.push_back(var);
        }
    }

    // Graphs
    if (doc.HasMember("graphs") && doc["graphs"].IsArray()) {
        for (auto& g : doc["graphs"].GetArray()) {
            BpFunctionGraph graph;
            if (g.HasMember("name")) graph.name = g["name"].GetString();
            if (g.HasMember("next_id")) graph.next_id = g["next_id"].GetInt();
            if (g.HasMember("is_pure")) graph.is_pure = g["is_pure"].GetBool();

            if (g.HasMember("nodes") && g["nodes"].IsArray()) {
                for (auto& n : g["nodes"].GetArray()) {
                    BpNode node;
                    if (n.HasMember("id")) node.id = n["id"].GetInt();
                    if (n.HasMember("name")) node.name = n["name"].GetString();
                    if (n.HasMember("category")) node.category = n["category"].GetString();
                    if (n.HasMember("pos_x")) node.position.x = n["pos_x"].GetFloat();
                    if (n.HasMember("pos_y")) node.position.y = n["pos_y"].GetFloat();
                    if (n.HasMember("comment")) node.comment = n["comment"].GetString();

                    // Restore header color from registry
                    const NodeTemplate* tmpl = NodeRegistry::Get().Find(node.name);
                    if (tmpl) {
                        node.header_color = tmpl->header_color;
                        node.code_template = tmpl->code_template;
                    }

                    if (n.HasMember("inputs") && n["inputs"].IsArray()) {
                        for (auto& p : n["inputs"].GetArray()) {
                            BpPin pin;
                            if (p.HasMember("id")) pin.id = p["id"].GetInt();
                            if (p.HasMember("name")) pin.name = p["name"].GetString();
                            if (p.HasMember("default_float")) pin.default_float = p["default_float"].GetFloat();
                            if (p.HasMember("default_int")) pin.default_int = p["default_int"].GetInt();
                            if (p.HasMember("default_bool")) pin.default_bool = p["default_bool"].GetBool();
                            pin.kind = BpPinKind::Input;
                            node.inputs.push_back(pin);
                        }
                    }
                    if (n.HasMember("outputs") && n["outputs"].IsArray()) {
                        for (auto& p : n["outputs"].GetArray()) {
                            BpPin pin;
                            if (p.HasMember("id")) pin.id = p["id"].GetInt();
                            if (p.HasMember("name")) pin.name = p["name"].GetString();
                            pin.kind = BpPinKind::Output;
                            node.outputs.push_back(pin);
                        }
                    }
                    graph.nodes.push_back(std::move(node));
                }
            }

            if (g.HasMember("links") && g["links"].IsArray()) {
                for (auto& l : g["links"].GetArray()) {
                    BpLink link;
                    if (l.HasMember("id")) link.id = l["id"].GetInt();
                    if (l.HasMember("from_pin")) link.from_pin = l["from_pin"].GetInt();
                    if (l.HasMember("to_pin")) link.to_pin = l["to_pin"].GetInt();
                    graph.links.push_back(link);
                }
            }

            asset.graphs.push_back(std::move(graph));
        }
    }

    // Interfaces
    if (doc.HasMember("interfaces") && doc["interfaces"].IsArray()) {
        for (auto& i : doc["interfaces"].GetArray()) {
            asset.implemented_interfaces.push_back(i.GetString());
        }
    }

    return true;
}

}  // namespace dse::editor::bp
