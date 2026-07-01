#pragma once

/**
 * @file editor_blueprint_vm.h
 * @brief Blueprint Virtual Machine - lightweight register-based bytecode interpreter
 *
 * Design: ~20 opcodes, register-based (not stack), supports:
 *   - Arithmetic, comparison, logic
 *   - Flow control (jump, branch, call, return)
 *   - ECS operations (get/set component fields)
 *   - Variable load/store (blueprint instance state)
 *   - External C++ function calls via bridge
 */

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <variant>
#include <unordered_map>

namespace dse::editor::bp {

// ─── Opcodes ───────────────────────────────────────────────────────────────

enum class OpCode : uint8_t {
    Nop = 0,

    // Load/Store
    LoadConst,    // R[A] = Constants[B]
    LoadVar,      // R[A] = Instance.variables[B]
    StoreVar,     // Instance.variables[A] = R[B]
    Move,         // R[A] = R[B]

    // Arithmetic (float)
    Add,          // R[A] = R[B] + R[C]
    Sub,          // R[A] = R[B] - R[C]
    Mul,          // R[A] = R[B] * R[C]
    Div,          // R[A] = R[B] / R[C]
    Neg,          // R[A] = -R[B]
    Mod,          // R[A] = R[B] % R[C]

    // Comparison → bool in R[A]
    CmpEq,        // R[A] = (R[B] == R[C])
    CmpLt,        // R[A] = (R[B] < R[C])
    CmpLe,        // R[A] = (R[B] <= R[C])

    // Logic
    And,          // R[A] = R[B] && R[C]
    Or,           // R[A] = R[B] || R[C]
    Not,          // R[A] = !R[B]

    // Flow
    Jump,         // PC += A (signed offset)
    JumpIfFalse,  // if !R[A] then PC += B
    JumpIfTrue,   // if R[A] then PC += B

    // Function call
    Call,         // Call function[A] with B args starting at R[C]
    Return,       // Return from current function

    // Math builtins
    Sin,          // R[A] = sin(R[B])
    Cos,          // R[A] = cos(R[B])
    Sqrt,         // R[A] = sqrt(R[B])
    Abs,          // R[A] = abs(R[B])
    Min,          // R[A] = min(R[B], R[C])
    Max,          // R[A] = max(R[B], R[C])
    Clamp,        // R[A] = clamp(R[B], R[C], R[D])  (D encoded in extra)

    // Vec3 ops
    Vec3Add,      // R[A..A+2] = R[B..B+2] + R[C..C+2]
    Vec3Sub,
    Vec3Scale,    // R[A..A+2] = R[B..B+2] * R[C]
    Vec3Dot,      // R[A] = dot(R[B..B+2], R[C..C+2])
    Vec3Normalize,// R[A..A+2] = normalize(R[B..B+2])

    // ECS bridge
    EcsGetFloat,  // R[A] = entity[B].field[C]  (external call)
    EcsSetFloat,  // entity[A].field[B] = R[C]
    EcsGetVec3,   // R[A..A+2] = entity[B].vec3_field[C]
    EcsSetVec3,   // entity[A].vec3_field[B] = R[C..C+2]

    // External C++ call
    CallExtern,   // R[A] = extern_functions[B](R[C..C+N-1]), N in extra

    // String ops
    Concat,       // R[A] = R[B] .. R[C] (string concat)

    // Debug / Print
    Print,        // print(R[A])

    // Array ops
    ArrayGet,     // R[A] = R[B][R[C]]
    ArraySet,     // R[A][R[B]] = R[C]
    ArrayLen,     // R[A] = len(R[B])
    ArrayPush,    // push R[B] to R[A]

    Halt,         // Stop execution
};

// ─── Instruction encoding ──────────────────────────────────────────────────

struct Instruction {
    OpCode op = OpCode::Nop;
    uint8_t a = 0;   // destination register or first operand
    uint8_t b = 0;   // second operand
    uint8_t c = 0;   // third operand
    int16_t extra = 0; // extended data (jump offsets, function indices)
};

// ─── Runtime Value ─────────────────────────────────────────────────────────

struct BpValue {
    enum class Type : uint8_t { Nil, Bool, Int, Float, String, Vec3, Entity, Array };
    Type type = Type::Nil;

    union {
        bool   b;
        int    i;
        float  f;
        float  v[3];
        uint32_t entity_id;
    };
    std::string str;
    std::vector<BpValue> arr;

    BpValue() : type(Type::Nil), i(0) {}
    static BpValue Bool(bool val) { BpValue v; v.type = Type::Bool; v.b = val; return v; }
    static BpValue Int(int val) { BpValue v; v.type = Type::Int; v.i = val; return v; }
    static BpValue Float(float val) { BpValue v; v.type = Type::Float; v.f = val; return v; }
    static BpValue String(const std::string& s) { BpValue v; v.type = Type::String; v.str = s; v.i = 0; return v; }
    static BpValue Vec3(float x, float y, float z) { BpValue v; v.type = Type::Vec3; v.v[0]=x; v.v[1]=y; v.v[2]=z; return v; }
    static BpValue Entity(uint32_t id) { BpValue v; v.type = Type::Entity; v.entity_id = id; return v; }

    float AsFloat() const;
    bool AsBool() const;
    int AsInt() const;
};

// ─── Compiled Blueprint Function ───────────────────────────────────────────

struct CompiledFunction {
    std::string name;
    std::vector<Instruction> code;
    std::vector<BpValue> constants;
    int num_registers = 0;   // max registers needed
    int num_params = 0;
};

// ─── Compiled Blueprint ────────────────────────────────────────────────────

struct CompiledBlueprint {
    std::vector<CompiledFunction> functions; // index 0 = on_init, 1 = on_update, rest = user funcs
    std::vector<BpValue> default_variables;  // initial values for instance variables
    int version = 1;
};

// ─── Blueprint Instance (per-entity runtime state) ─────────────────────────

struct BlueprintInstance {
    const CompiledBlueprint* blueprint = nullptr;
    std::vector<BpValue> variables;  // mutable per-entity state
    bool initialized = false;
};

// ─── VM Execution ──────────────────────────────────────────────────────────

// External function bridge: C++ functions callable from blueprint
using ExternFn = std::function<BpValue(const std::vector<BpValue>& args)>;

struct VmContext {
    BlueprintInstance* instance = nullptr;
    uint32_t entity_id = 0;
    float delta_time = 0.0f;
};

class BlueprintVM {
public:
    static BlueprintVM& Get();

    /// Register an external C++ function callable from blueprints
    void RegisterExtern(const std::string& name, ExternFn fn);
    int GetExternIndex(const std::string& name) const;

    /// Execute a compiled function on a blueprint instance
    BpValue Execute(const CompiledFunction& func, VmContext& ctx,
                    const std::vector<BpValue>& args = {});

    /// Execute on_init for an instance
    void RunInit(BlueprintInstance& instance, uint32_t entity_id);

    /// Execute on_update for an instance
    void RunUpdate(BlueprintInstance& instance, uint32_t entity_id, float dt);

    // Debug
    int GetInstructionCount() const { return instruction_count_; }
    bool HasError() const { return !last_error_.empty(); }
    const std::string& GetLastError() const { return last_error_; }

private:
    std::vector<std::pair<std::string, ExternFn>> extern_functions_;
    std::unordered_map<std::string, int> extern_index_;
    int instruction_count_ = 0;
    std::string last_error_;
};

}  // namespace dse::editor::bp
