#pragma once

/**
 * @file blueprint_vm.h
 * @brief 运行时蓝图虚拟机 — 寄存器式字节码解释器（引擎侧，无编辑器/ImGui 依赖）
 *
 * 与编辑器侧 dse::editor::bp::BlueprintVM 使用同一套指令集与 .dbp 资源格式，
 * 但独立实现，供引擎运行时（BlueprintSystem）在无编辑器时驱动蓝图组件。
 * ECS 读写通过 IBlueprintEcsBridge 抽象，避免 VM 直接依赖具体组件类型。
 */

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include "engine/scripting/blueprint/blueprint_bytecode.h"

namespace dse::bp {

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
    /// Fill out[0..2] with vec components. Vec3 values pass through; any other
    /// type yields {0,0,0} (never reads uninitialised union members).
    void AsVec3(float out[3]) const;
};

// ─── Compiled forms ─────────────────────────────────────────────────────────

struct CompiledFunction {
    std::string name;
    std::vector<Instruction> code;
    std::vector<BpValue> constants;
    int num_registers = 0;
    int num_params = 0;
    // Parallel to `code`: source graph node id each instruction was emitted for
    // (-1 = compiler-synthesized). Consumed by the editor step debugger to map
    // program counter → node for highlighting and node-level breakpoints.
    std::vector<int> source_nodes;
};

struct CompiledBlueprint {
    std::vector<CompiledFunction> functions; // on_init / on_update / user funcs
    std::vector<BpValue> default_variables;
    int version = 1;            // source .dbp schema version
    int bytecode_version = 1;   // emitter/opcode format version (see kBytecodeVersion)
};

struct BlueprintInstance {
    const CompiledBlueprint* blueprint = nullptr;
    std::vector<BpValue> variables;
    bool initialized = false;
};

// ─── ECS 桥接（由引擎运行时提供，字段索引 0 = TransformComponent.position）───

class IBlueprintEcsBridge {
public:
    virtual ~IBlueprintEcsBridge() = default;
    virtual void  GetVec3(uint32_t entity, int field, float out[3]) = 0;
    virtual void  SetVec3(uint32_t entity, int field, const float in[3]) = 0;
    virtual float GetFloat(uint32_t entity, int field) = 0;
    virtual void  SetFloat(uint32_t entity, int field, float value) = 0;
};

using ExternFn = std::function<BpValue(const std::vector<BpValue>& args)>;

struct VmContext {
    BlueprintInstance* instance = nullptr;
    uint32_t entity_id = 0;
    float delta_time = 0.0f;
    IBlueprintEcsBridge* ecs = nullptr;
};

class BlueprintVM {
public:
    static BlueprintVM& Get();

    void RegisterExtern(const std::string& name, ExternFn fn);
    int GetExternIndex(const std::string& name) const;

    BpValue Execute(const CompiledFunction& func, VmContext& ctx,
                    const std::vector<BpValue>& args = {});

    void RunInit(BlueprintInstance& instance, uint32_t entity_id, IBlueprintEcsBridge* ecs);
    void RunUpdate(BlueprintInstance& instance, uint32_t entity_id, float dt, IBlueprintEcsBridge* ecs);

    int GetInstructionCount() const { return instruction_count_; }
    bool HasError() const { return !last_error_.empty(); }
    const std::string& GetLastError() const { return last_error_; }

private:
    std::vector<std::pair<std::string, ExternFn>> extern_functions_;
    std::unordered_map<std::string, int> extern_index_;
    int instruction_count_ = 0;
    std::string last_error_;
};

}  // namespace dse::bp
