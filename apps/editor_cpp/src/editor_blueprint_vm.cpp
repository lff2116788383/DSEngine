/**
 * @file editor_blueprint_vm.cpp
 * @brief Blueprint VM implementation - register-based bytecode interpreter
 */

#include "editor_blueprint_vm.h"

#include <cmath>
#include <cstdio>
#include <algorithm>

namespace dse::editor::bp {

// ─── BlueprintVM ───────────────────────────────────────────────────────────

BlueprintVM& BlueprintVM::Get() {
    static BlueprintVM s_vm;
    return s_vm;
}

void BlueprintVM::RegisterExtern(const std::string& name, ExternFn fn) {
    auto it = extern_index_.find(name);
    if (it != extern_index_.end()) {
        extern_functions_[it->second].second = std::move(fn);
    } else {
        extern_index_[name] = static_cast<int>(extern_functions_.size());
        extern_functions_.emplace_back(name, std::move(fn));
    }
}

int BlueprintVM::GetExternIndex(const std::string& name) const {
    auto it = extern_index_.find(name);
    return (it != extern_index_.end()) ? it->second : -1;
}

namespace {
// Load call arguments / on_update delta_time into the entry registers.
void LoadEntryRegisters(const CompiledFunction& func, VmContext& ctx,
                        std::vector<BpValue>& regs, const std::vector<BpValue>& args) {
    for (size_t i = 0; i < args.size() && i < static_cast<size_t>(func.num_params); ++i) {
        regs[i] = args[i];
    }
    if (func.name == "on_update" && func.num_params >= 1) {
        regs[0] = BpValue::Float(ctx.delta_time);
    }
}
}  // namespace

BlueprintVM::StepResult BlueprintVM::RunOne(const CompiledFunction& func, VmContext& ctx,
                                            std::vector<BpValue>& regs, int& pc,
                                            BpValue& out_return) {
    const auto& code = func.code;
    const auto& constants = func.constants;
    const int code_size = static_cast<int>(code.size());

    // Bounds-checked register accessor (mirrors the engine VM): out-of-range
    // indices resolve to a scratch slot and flag last_error_ instead of reading
    // or writing past the register file.
    BpValue oob_scratch;
    auto R = [&](int idx) -> BpValue& {
        if (idx >= 0 && idx < static_cast<int>(regs.size())) return regs[idx];
        last_error_ = "Blueprint register index out of range";
        oob_scratch = BpValue();
        return oob_scratch;
    };

    const Instruction& instr = code[pc];
    ++instruction_count_;
    ++pc;

    switch (instr.op) {
            case OpCode::Nop: break;

            case OpCode::LoadConst:
                if (instr.b < constants.size()) R(instr.a) = constants[instr.b];
                break;

            case OpCode::LoadVar:
                if (ctx.instance && instr.b < ctx.instance->variables.size())
                    R(instr.a) = ctx.instance->variables[instr.b];
                break;

            case OpCode::StoreVar:
                if (ctx.instance && instr.a < ctx.instance->variables.size())
                    ctx.instance->variables[instr.a] = R(instr.b);
                break;

            case OpCode::Move:
                R(instr.a) = R(instr.b);
                break;

            // Arithmetic
            case OpCode::Add:
                R(instr.a) = BpValue::Float(R(instr.b).AsFloat() + R(instr.c).AsFloat());
                break;
            case OpCode::Sub:
                R(instr.a) = BpValue::Float(R(instr.b).AsFloat() - R(instr.c).AsFloat());
                break;
            case OpCode::Mul:
                R(instr.a) = BpValue::Float(R(instr.b).AsFloat() * R(instr.c).AsFloat());
                break;
            case OpCode::Div: {
                float divisor = R(instr.c).AsFloat();
                R(instr.a) = BpValue::Float(divisor != 0.0f ? R(instr.b).AsFloat() / divisor : 0.0f);
                break;
            }
            case OpCode::Neg:
                R(instr.a) = BpValue::Float(-R(instr.b).AsFloat());
                break;
            case OpCode::Mod: {
                float d = R(instr.c).AsFloat();
                R(instr.a) = BpValue::Float(d != 0.0f ? std::fmod(R(instr.b).AsFloat(), d) : 0.0f);
                break;
            }

            // Comparison
            case OpCode::CmpEq:
                R(instr.a) = BpValue::Bool(R(instr.b).AsFloat() == R(instr.c).AsFloat());
                break;
            case OpCode::CmpLt:
                R(instr.a) = BpValue::Bool(R(instr.b).AsFloat() < R(instr.c).AsFloat());
                break;
            case OpCode::CmpLe:
                R(instr.a) = BpValue::Bool(R(instr.b).AsFloat() <= R(instr.c).AsFloat());
                break;

            // Logic
            case OpCode::And:
                R(instr.a) = BpValue::Bool(R(instr.b).AsBool() && R(instr.c).AsBool());
                break;
            case OpCode::Or:
                R(instr.a) = BpValue::Bool(R(instr.b).AsBool() || R(instr.c).AsBool());
                break;
            case OpCode::Not:
                R(instr.a) = BpValue::Bool(!R(instr.b).AsBool());
                break;

            // Flow control
            case OpCode::Jump:
                pc += instr.extra;
                break;
            case OpCode::JumpIfFalse:
                if (!R(instr.a).AsBool()) pc += instr.extra;
                break;
            case OpCode::JumpIfTrue:
                if (R(instr.a).AsBool()) pc += instr.extra;
                break;

            // Math builtins
            case OpCode::Sin:
                R(instr.a) = BpValue::Float(std::sin(R(instr.b).AsFloat()));
                break;
            case OpCode::Cos:
                R(instr.a) = BpValue::Float(std::cos(R(instr.b).AsFloat()));
                break;
            case OpCode::Sqrt:
                R(instr.a) = BpValue::Float(std::sqrt(std::abs(R(instr.b).AsFloat())));
                break;
            case OpCode::Abs:
                R(instr.a) = BpValue::Float(std::abs(R(instr.b).AsFloat()));
                break;
            case OpCode::Pow:
                R(instr.a) = BpValue::Float(std::pow(R(instr.b).AsFloat(), R(instr.c).AsFloat()));
                break;
            case OpCode::Atan2:
                R(instr.a) = BpValue::Float(std::atan2(R(instr.b).AsFloat(), R(instr.c).AsFloat()));
                break;
            case OpCode::Min:
                R(instr.a) = BpValue::Float(std::min(R(instr.b).AsFloat(), R(instr.c).AsFloat()));
                break;
            case OpCode::Max:
                R(instr.a) = BpValue::Float(std::max(R(instr.b).AsFloat(), R(instr.c).AsFloat()));
                break;
            case OpCode::Clamp: {
                float val = R(instr.b).AsFloat();
                float lo = R(instr.c).AsFloat();
                float hi = R(instr.extra).AsFloat();
                R(instr.a) = BpValue::Float(std::clamp(val, lo, hi));
                break;
            }

            // Vec3 ops operate on single registers holding Type::Vec3 values.
            case OpCode::Vec3Add: {
                float x[3], y[3];
                R(instr.b).AsVec3(x); R(instr.c).AsVec3(y);
                R(instr.a) = BpValue::Vec3(x[0]+y[0], x[1]+y[1], x[2]+y[2]);
                break;
            }
            case OpCode::Vec3Sub: {
                float x[3], y[3];
                R(instr.b).AsVec3(x); R(instr.c).AsVec3(y);
                R(instr.a) = BpValue::Vec3(x[0]-y[0], x[1]-y[1], x[2]-y[2]);
                break;
            }
            case OpCode::Vec3Scale: {
                float x[3]; R(instr.b).AsVec3(x);
                float s = R(instr.c).AsFloat();
                R(instr.a) = BpValue::Vec3(x[0]*s, x[1]*s, x[2]*s);
                break;
            }
            case OpCode::Vec3Dot: {
                float x[3], y[3];
                R(instr.b).AsVec3(x); R(instr.c).AsVec3(y);
                R(instr.a) = BpValue::Float(x[0]*y[0] + x[1]*y[1] + x[2]*y[2]);
                break;
            }
            case OpCode::Vec3Normalize: {
                float x[3]; R(instr.b).AsVec3(x);
                float len = std::sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
                if (len > 1e-8f) { x[0]/=len; x[1]/=len; x[2]/=len; }
                R(instr.a) = BpValue::Vec3(x[0], x[1], x[2]);
                break;
            }
            case OpCode::MakeVec3:
                R(instr.a) = BpValue::Vec3(R(instr.b).AsFloat(), R(instr.c).AsFloat(), R(instr.extra).AsFloat());
                break;
            case OpCode::VecComponent: {
                float x[3]; R(instr.b).AsVec3(x);
                int k = (instr.c >= 0 && instr.c < 3) ? instr.c : 0;
                R(instr.a) = BpValue::Float(x[k]);
                break;
            }

            // External C++ call
            case OpCode::CallExtern: {
                if (instr.b >= constants.size() || constants[instr.b].type != BpValue::Type::String) {
                    last_error_ = "Blueprint external call has no function name";
                    out_return = BpValue();
                    return StepResult::Returned;
                }
                int fn_idx = GetExternIndex(constants[instr.b].str);
                int arg_start = instr.c;
                int num_args = instr.extra;
                if (fn_idx >= 0 && fn_idx < static_cast<int>(extern_functions_.size())) {
                    std::vector<BpValue> fn_args;
                    for (int i = 0; i < num_args; ++i)
                        fn_args.push_back(R(arg_start + i));
                    R(instr.a) = extern_functions_[fn_idx].second(fn_args);
                }
                break;
            }

            // String
            case OpCode::Concat:
                R(instr.a) = BpValue::String(R(instr.b).str + R(instr.c).str);
                break;

            // Print
            case OpCode::Print: {
                const auto& v = R(instr.a);
                switch (v.type) {
                    case BpValue::Type::Float: printf("[BP] %f\n", v.f); break;
                    case BpValue::Type::Int:   printf("[BP] %d\n", v.i); break;
                    case BpValue::Type::Bool:  printf("[BP] %s\n", v.b ? "true" : "false"); break;
                    case BpValue::Type::String:printf("[BP] %s\n", v.str.c_str()); break;
                    case BpValue::Type::Vec3:  printf("[BP] (%f, %f, %f)\n", v.v[0], v.v[1], v.v[2]); break;
                    default: printf("[BP] <value>\n"); break;
                }
                break;
            }

            // Array
            case OpCode::ArrayGet: {
                int idx = R(instr.c).AsInt();
                const auto& arr = R(instr.b).arr;
                if (idx >= 0 && idx < static_cast<int>(arr.size()))
                    R(instr.a) = arr[idx];
                break;
            }
            case OpCode::ArraySet: {
                int idx = R(instr.b).AsInt();
                auto& arr = R(instr.a).arr;
                if (idx >= 0 && idx < static_cast<int>(arr.size()))
                    arr[idx] = R(instr.c);
                break;
            }
            case OpCode::ArrayLen:
                R(instr.a) = BpValue::Int(static_cast<int>(R(instr.b).arr.size()));
                break;
            case OpCode::ArrayPush:
                R(instr.a).arr.push_back(R(instr.b));
                break;

            // Call user function
            case OpCode::Call: {
                int fn_idx = instr.a;
                int num_args = instr.b;
                int arg_start = instr.c;
                if (ctx.instance && ctx.instance->blueprint &&
                    fn_idx >= 0 && fn_idx < static_cast<int>(ctx.instance->blueprint->functions.size())) {
                    std::vector<BpValue> fn_args;
                    for (int i = 0; i < num_args; ++i)
                        fn_args.push_back(R(arg_start + i));
                    R(arg_start) = Execute(ctx.instance->blueprint->functions[fn_idx], ctx, fn_args);
                }
                break;
            }

            case OpCode::Return:
                out_return = instr.a < regs.size() ? regs[instr.a] : BpValue();
                return StepResult::Returned;

            case OpCode::Halt:
                out_return = regs.empty() ? BpValue() : regs[0];
                return StepResult::Returned;

            // ECS bridge is engine-side; the editor preview VM has no live world,
            // so reads yield neutral typed values and writes are no-ops. Kept
            // type-consistent so downstream Vec3/float nodes step correctly.
            case OpCode::EcsGetFloat:
                R(instr.a) = BpValue::Float(0.0f);
                break;
            case OpCode::EcsGetVec3:
                R(instr.a) = BpValue::Vec3(0.0f, 0.0f, 0.0f);
                break;
            case OpCode::EcsSetFloat:
            case OpCode::EcsSetVec3:
                break;

            default: break;
    }

    // Reject relative jumps landing outside the code range (pc == code_size is a
    // clean halt handled by the Execute/StepOnce loop guards).
    if (pc < 0 || pc > code_size) {
        last_error_ = "Blueprint jump target out of range";
        out_return = regs.empty() ? BpValue() : regs[0];
        return StepResult::Returned;
    }

    return StepResult::Continue;
}

BpValue BlueprintVM::Execute(const CompiledFunction& func, VmContext& ctx,
                              const std::vector<BpValue>& args) {
    instruction_count_ = 0;
    last_error_.clear();

    std::vector<BpValue> regs(func.num_registers);
    LoadEntryRegisters(func, ctx, regs, args);

    int pc = 0;
    const int code_size = static_cast<int>(func.code.size());
    constexpr int MAX_INSTRUCTIONS = 100000; // infinite loop protection

    while (pc < code_size && instruction_count_ < MAX_INSTRUCTIONS) {
        BpValue ret;
        if (RunOne(func, ctx, regs, pc, ret) == StepResult::Returned) return ret;
    }

    if (instruction_count_ >= MAX_INSTRUCTIONS) {
        last_error_ = "Blueprint execution exceeded instruction limit (possible infinite loop)";
    }

    return regs.empty() ? BpValue() : regs[0];
}

void BlueprintVM::BeginStep(StepState& state, const CompiledFunction& func, VmContext& ctx,
                            const std::vector<BpValue>& args) {
    instruction_count_ = 0;
    last_error_.clear();

    state.func = &func;
    state.pc = 0;
    state.finished = false;
    state.result = BpValue();
    state.regs.assign(func.num_registers, BpValue());
    LoadEntryRegisters(func, ctx, state.regs, args);
}

int BlueprintVM::CurrentNode(const StepState& state) const {
    if (!state.func) return -1;
    if (state.pc >= 0 && state.pc < static_cast<int>(state.func->source_nodes.size()))
        return state.func->source_nodes[state.pc];
    return -1;
}

int BlueprintVM::StepOnce(StepState& state, VmContext& ctx) {
    constexpr int MAX_INSTRUCTIONS = 100000;
    if (!state.func || state.finished) return -1;

    if (state.pc >= static_cast<int>(state.func->code.size()) ||
        instruction_count_ >= MAX_INSTRUCTIONS) {
        if (instruction_count_ >= MAX_INSTRUCTIONS)
            last_error_ = "Blueprint execution exceeded instruction limit (possible infinite loop)";
        state.finished = true;
        state.result = state.regs.empty() ? BpValue() : state.regs[0];
        return -1;
    }

    BpValue ret;
    if (RunOne(*state.func, ctx, state.regs, state.pc, ret) == StepResult::Returned) {
        state.finished = true;
        state.result = ret;
        return -1;
    }
    return CurrentNode(state);
}

void BlueprintVM::RunInit(BlueprintInstance& instance, uint32_t entity_id) {
    if (!instance.blueprint || instance.blueprint->functions.empty()) return;
    if (instance.initialized) return;

    // Initialize variables with defaults
    instance.variables = instance.blueprint->default_variables;
    instance.initialized = true;

    // Find and run on_init (function index 0 by convention)
    for (const auto& fn : instance.blueprint->functions) {
        if (fn.name == "on_init") {
            VmContext ctx;
            ctx.instance = &instance;
            ctx.entity_id = entity_id;
            Execute(fn, ctx);
            break;
        }
    }
}

void BlueprintVM::RunUpdate(BlueprintInstance& instance, uint32_t entity_id, float dt) {
    if (!instance.blueprint || !instance.initialized) return;

    for (const auto& fn : instance.blueprint->functions) {
        if (fn.name == "on_update") {
            VmContext ctx;
            ctx.instance = &instance;
            ctx.entity_id = entity_id;
            ctx.delta_time = dt;
            Execute(fn, ctx, {BpValue::Float(dt)});
            break;
        }
    }
}

}  // namespace dse::editor::bp
