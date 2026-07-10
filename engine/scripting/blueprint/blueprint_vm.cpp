/**
 * @file blueprint_vm.cpp
 * @brief 运行时蓝图 VM 实现（引擎侧）
 */

#include "engine/scripting/blueprint/blueprint_vm.h"

#include <cmath>
#include <cstdio>
#include <algorithm>

namespace dse::bp {

float BpValue::AsFloat() const {
    switch (type) {
        case Type::Float: return f;
        case Type::Int:   return static_cast<float>(i);
        case Type::Bool:  return b ? 1.0f : 0.0f;
        default: return 0.0f;
    }
}

bool BpValue::AsBool() const {
    switch (type) {
        case Type::Bool:  return b;
        case Type::Int:   return i != 0;
        case Type::Float: return f != 0.0f;
        case Type::String: return !str.empty();
        default: return false;
    }
}

int BpValue::AsInt() const {
    switch (type) {
        case Type::Int:   return i;
        case Type::Float: return static_cast<int>(f);
        case Type::Bool:  return b ? 1 : 0;
        default: return 0;
    }
}

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

// 解析 ECS 操作作用的实体：寄存器持有的 entity_id 为 0（"Self Entity" 占位）时回退到当前实体
static uint32_t ResolveEntity(const BpValue& reg, uint32_t self) {
    if (reg.type == BpValue::Type::Entity && reg.entity_id != 0) return reg.entity_id;
    return self;
}

BpValue BlueprintVM::Execute(const CompiledFunction& func, VmContext& ctx,
                             const std::vector<BpValue>& args) {
    instruction_count_ = 0;
    last_error_.clear();

    std::vector<BpValue> regs(func.num_registers);

    for (size_t i = 0; i < args.size() && i < static_cast<size_t>(func.num_params); ++i) {
        regs[i] = args[i];
    }

    if (func.name == "on_update" && func.num_params >= 1) {
        regs[0] = BpValue::Float(ctx.delta_time);
    }

    const auto& code = func.code;
    const auto& constants = func.constants;
    int pc = 0;
    const int code_size = static_cast<int>(code.size());
    constexpr int MAX_INSTRUCTIONS = 100000;

    auto in_range = [&](int idx) { return idx >= 0 && idx < static_cast<int>(regs.size()); };

    while (pc < code_size && instruction_count_ < MAX_INSTRUCTIONS) {
        const Instruction& instr = code[pc];
        ++instruction_count_;
        ++pc;

        switch (instr.op) {
            case OpCode::Nop: break;

            case OpCode::LoadConst:
                if (instr.b < constants.size() && in_range(instr.a)) regs[instr.a] = constants[instr.b];
                break;

            case OpCode::LoadVar:
                if (ctx.instance && instr.b < ctx.instance->variables.size() && in_range(instr.a))
                    regs[instr.a] = ctx.instance->variables[instr.b];
                break;

            case OpCode::StoreVar:
                if (ctx.instance && instr.a < ctx.instance->variables.size() && in_range(instr.b))
                    ctx.instance->variables[instr.a] = regs[instr.b];
                break;

            case OpCode::Move:
                if (in_range(instr.a) && in_range(instr.b)) regs[instr.a] = regs[instr.b];
                break;

            case OpCode::Add:
                regs[instr.a] = BpValue::Float(regs[instr.b].AsFloat() + regs[instr.c].AsFloat());
                break;
            case OpCode::Sub:
                regs[instr.a] = BpValue::Float(regs[instr.b].AsFloat() - regs[instr.c].AsFloat());
                break;
            case OpCode::Mul:
                regs[instr.a] = BpValue::Float(regs[instr.b].AsFloat() * regs[instr.c].AsFloat());
                break;
            case OpCode::Div: {
                float divisor = regs[instr.c].AsFloat();
                regs[instr.a] = BpValue::Float(divisor != 0.0f ? regs[instr.b].AsFloat() / divisor : 0.0f);
                break;
            }
            case OpCode::Neg:
                regs[instr.a] = BpValue::Float(-regs[instr.b].AsFloat());
                break;
            case OpCode::Mod: {
                float d = regs[instr.c].AsFloat();
                regs[instr.a] = BpValue::Float(d != 0.0f ? std::fmod(regs[instr.b].AsFloat(), d) : 0.0f);
                break;
            }

            case OpCode::CmpEq:
                regs[instr.a] = BpValue::Bool(regs[instr.b].AsFloat() == regs[instr.c].AsFloat());
                break;
            case OpCode::CmpLt:
                regs[instr.a] = BpValue::Bool(regs[instr.b].AsFloat() < regs[instr.c].AsFloat());
                break;
            case OpCode::CmpLe:
                regs[instr.a] = BpValue::Bool(regs[instr.b].AsFloat() <= regs[instr.c].AsFloat());
                break;

            case OpCode::And:
                regs[instr.a] = BpValue::Bool(regs[instr.b].AsBool() && regs[instr.c].AsBool());
                break;
            case OpCode::Or:
                regs[instr.a] = BpValue::Bool(regs[instr.b].AsBool() || regs[instr.c].AsBool());
                break;
            case OpCode::Not:
                regs[instr.a] = BpValue::Bool(!regs[instr.b].AsBool());
                break;

            case OpCode::Jump:
                pc += instr.extra;
                break;
            case OpCode::JumpIfFalse:
                if (!regs[instr.a].AsBool()) pc += instr.extra;
                break;
            case OpCode::JumpIfTrue:
                if (regs[instr.a].AsBool()) pc += instr.extra;
                break;

            case OpCode::Sin:
                regs[instr.a] = BpValue::Float(std::sin(regs[instr.b].AsFloat()));
                break;
            case OpCode::Cos:
                regs[instr.a] = BpValue::Float(std::cos(regs[instr.b].AsFloat()));
                break;
            case OpCode::Sqrt:
                regs[instr.a] = BpValue::Float(std::sqrt(std::abs(regs[instr.b].AsFloat())));
                break;
            case OpCode::Abs:
                regs[instr.a] = BpValue::Float(std::abs(regs[instr.b].AsFloat()));
                break;
            case OpCode::Min:
                regs[instr.a] = BpValue::Float(std::min(regs[instr.b].AsFloat(), regs[instr.c].AsFloat()));
                break;
            case OpCode::Max:
                regs[instr.a] = BpValue::Float(std::max(regs[instr.b].AsFloat(), regs[instr.c].AsFloat()));
                break;
            case OpCode::Clamp: {
                float val = regs[instr.b].AsFloat();
                float lo = regs[instr.c].AsFloat();
                float hi = in_range(instr.extra) ? regs[instr.extra].AsFloat() : 1.0f;
                regs[instr.a] = BpValue::Float(std::clamp(val, lo, hi));
                break;
            }

            case OpCode::Vec3Add:
                for (int k = 0; k < 3; ++k)
                    regs[instr.a + k] = BpValue::Float(regs[instr.b + k].AsFloat() + regs[instr.c + k].AsFloat());
                break;
            case OpCode::Vec3Sub:
                for (int k = 0; k < 3; ++k)
                    regs[instr.a + k] = BpValue::Float(regs[instr.b + k].AsFloat() - regs[instr.c + k].AsFloat());
                break;
            case OpCode::Vec3Scale:
                for (int k = 0; k < 3; ++k)
                    regs[instr.a + k] = BpValue::Float(regs[instr.b + k].AsFloat() * regs[instr.c].AsFloat());
                break;
            case OpCode::Vec3Dot: {
                float dot = 0.0f;
                for (int k = 0; k < 3; ++k)
                    dot += regs[instr.b + k].AsFloat() * regs[instr.c + k].AsFloat();
                regs[instr.a] = BpValue::Float(dot);
                break;
            }
            case OpCode::Vec3Normalize: {
                float x = regs[instr.b].AsFloat(), y = regs[instr.b+1].AsFloat(), z = regs[instr.b+2].AsFloat();
                float len = std::sqrt(x*x + y*y + z*z);
                if (len > 1e-8f) { x/=len; y/=len; z/=len; }
                regs[instr.a] = BpValue::Float(x);
                regs[instr.a+1] = BpValue::Float(y);
                regs[instr.a+2] = BpValue::Float(z);
                break;
            }

            // ECS bridge — 通过 IBlueprintEcsBridge 真正读写组件
            case OpCode::EcsGetFloat: {
                if (ctx.ecs) {
                    uint32_t e = ResolveEntity(regs[instr.b], ctx.entity_id);
                    regs[instr.a] = BpValue::Float(ctx.ecs->GetFloat(e, instr.c));
                }
                break;
            }
            case OpCode::EcsSetFloat: {
                if (ctx.ecs) {
                    uint32_t e = ResolveEntity(regs[instr.a], ctx.entity_id);
                    ctx.ecs->SetFloat(e, instr.b, regs[instr.c].AsFloat());
                }
                break;
            }
            case OpCode::EcsGetVec3: {
                if (ctx.ecs) {
                    uint32_t e = ResolveEntity(regs[instr.b], ctx.entity_id);
                    float out[3] = {0, 0, 0};
                    ctx.ecs->GetVec3(e, instr.c, out);
                    regs[instr.a]   = BpValue::Float(out[0]);
                    regs[instr.a+1] = BpValue::Float(out[1]);
                    regs[instr.a+2] = BpValue::Float(out[2]);
                }
                break;
            }
            case OpCode::EcsSetVec3: {
                if (ctx.ecs) {
                    uint32_t e = ResolveEntity(regs[instr.a], ctx.entity_id);
                    float in[3] = { regs[instr.c].AsFloat(), regs[instr.c+1].AsFloat(), regs[instr.c+2].AsFloat() };
                    ctx.ecs->SetVec3(e, instr.b, in);
                }
                break;
            }

            case OpCode::CallExtern: {
                int fn_idx = instr.b;
                int arg_start = instr.c;
                int num_args = instr.extra;
                if (fn_idx >= 0 && fn_idx < static_cast<int>(extern_functions_.size())) {
                    std::vector<BpValue> fn_args;
                    for (int i = 0; i < num_args; ++i)
                        fn_args.push_back(regs[arg_start + i]);
                    regs[instr.a] = extern_functions_[fn_idx].second(fn_args);
                }
                break;
            }

            case OpCode::Concat:
                regs[instr.a] = BpValue::String(regs[instr.b].str + regs[instr.c].str);
                break;

            case OpCode::Print: {
                const auto& v = regs[instr.a];
                switch (v.type) {
                    case BpValue::Type::Float: printf("[BP] %f\n", v.f); break;
                    case BpValue::Type::Int:   printf("[BP] %d\n", v.i); break;
                    case BpValue::Type::Bool:  printf("[BP] %s\n", v.b ? "true" : "false"); break;
                    case BpValue::Type::String:printf("[BP] %s\n", v.str.c_str()); break;
                    default: printf("[BP] <value>\n"); break;
                }
                break;
            }

            case OpCode::ArrayGet: {
                int idx = regs[instr.c].AsInt();
                if (idx >= 0 && idx < static_cast<int>(regs[instr.b].arr.size()))
                    regs[instr.a] = regs[instr.b].arr[idx];
                break;
            }
            case OpCode::ArraySet: {
                int idx = regs[instr.b].AsInt();
                if (idx >= 0 && idx < static_cast<int>(regs[instr.a].arr.size()))
                    regs[instr.a].arr[idx] = regs[instr.c];
                break;
            }
            case OpCode::ArrayLen:
                regs[instr.a] = BpValue::Int(static_cast<int>(regs[instr.b].arr.size()));
                break;
            case OpCode::ArrayPush:
                regs[instr.a].arr.push_back(regs[instr.b]);
                break;

            case OpCode::Call: {
                int fn_idx = instr.a;
                int num_args = instr.b;
                int arg_start = instr.c;
                if (ctx.instance && ctx.instance->blueprint &&
                    fn_idx >= 0 && fn_idx < static_cast<int>(ctx.instance->blueprint->functions.size())) {
                    std::vector<BpValue> fn_args;
                    for (int i = 0; i < num_args; ++i)
                        fn_args.push_back(regs[arg_start + i]);
                    regs[arg_start] = Execute(ctx.instance->blueprint->functions[fn_idx], ctx, fn_args);
                }
                break;
            }

            case OpCode::Return:
                return in_range(instr.a) ? regs[instr.a] : BpValue();

            case OpCode::Halt:
                return regs.empty() ? BpValue() : regs[0];

            default: break;
        }
    }

    if (instruction_count_ >= MAX_INSTRUCTIONS) {
        last_error_ = "Blueprint execution exceeded instruction limit (possible infinite loop)";
    }

    return regs.empty() ? BpValue() : regs[0];
}

void BlueprintVM::RunInit(BlueprintInstance& instance, uint32_t entity_id, IBlueprintEcsBridge* ecs) {
    if (!instance.blueprint || instance.blueprint->functions.empty()) return;
    if (instance.initialized) return;

    instance.variables = instance.blueprint->default_variables;
    instance.initialized = true;

    for (const auto& fn : instance.blueprint->functions) {
        if (fn.name == "on_init") {
            VmContext ctx;
            ctx.instance = &instance;
            ctx.entity_id = entity_id;
            ctx.ecs = ecs;
            Execute(fn, ctx);
            break;
        }
    }
}

void BlueprintVM::RunUpdate(BlueprintInstance& instance, uint32_t entity_id, float dt, IBlueprintEcsBridge* ecs) {
    if (!instance.blueprint || !instance.initialized) return;

    for (const auto& fn : instance.blueprint->functions) {
        if (fn.name == "on_update") {
            VmContext ctx;
            ctx.instance = &instance;
            ctx.entity_id = entity_id;
            ctx.delta_time = dt;
            ctx.ecs = ecs;
            Execute(fn, ctx, {BpValue::Float(dt)});
            break;
        }
    }
}

}  // namespace dse::bp
