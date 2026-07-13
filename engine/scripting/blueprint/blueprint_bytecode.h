#pragma once

#include <cstdint>

namespace dse::bp {

enum class OpCode : uint8_t {
    Nop = 0,
    LoadConst,
    LoadVar,
    StoreVar,
    Move,
    Add,
    Sub,
    Mul,
    Div,
    Neg,
    Mod,
    CmpEq,
    CmpLt,
    CmpLe,
    And,
    Or,
    Not,
    Jump,
    JumpIfFalse,
    JumpIfTrue,
    Call,
    Return,
    Sin,
    Cos,
    Sqrt,
    Abs,
    Pow,
    Atan2,
    Min,
    Max,
    Clamp,
    Vec3Add,
    Vec3Sub,
    Vec3Scale,
    Vec3Dot,
    Vec3Normalize,
    MakeVec3,      // a = vec3(b, c, extra) — b/c/extra hold scalar registers
    VecComponent,  // a = b.component[c]  (c: 0=x, 1=y, 2=z)
    EcsGetFloat,
    EcsSetFloat,
    EcsGetVec3,
    EcsSetVec3,
    CallExtern,
    Concat,
    Print,
    ArrayGet,
    ArraySet,
    ArrayLen,
    ArrayPush,
    Halt,
};

struct Instruction {
    OpCode op = OpCode::Nop;
    uint8_t a = 0;
    uint8_t b = 0;
    uint8_t c = 0;
    int16_t extra = 0;
};

}  // namespace dse::bp
