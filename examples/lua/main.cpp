#include "phase1/runtime/engine_app.h"

int main() {
    return phase1::runtime::RunEngine({
        800,
        600,
        "DSEngine Lua Demo",
        Phase1BusinessMode::Lua,
        "examples/lua/main.lua"
    });
}
