#include "engine/runtime/engine_app.h"
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    std::string startup_script = "samples/lua/main.lua";
    try {
        std::filesystem::path exe_path = argc > 0 ? std::filesystem::absolute(std::filesystem::path(argv[0])) : std::filesystem::path();
        if (std::filesystem::exists(exe_path)) {
            auto exe_dir = exe_path.parent_path();
            auto candidate = exe_dir / "samples" / "lua" / "main.lua";
            if (std::filesystem::exists(candidate)) {
                startup_script = candidate.string();
            }
        }
    } catch (...) {
    }
    return dse::runtime::RunEngine({
        800,
        600,
        "DSEngine Lua Demo",
        BusinessMode::Lua,
        startup_script
    });
}
