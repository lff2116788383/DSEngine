#include "engine/runtime/engine_app.h"
#include <filesystem>
#include <string>

// 强制使用独立显卡（NVIDIA GT 1030）而非集成显卡（Intel UHD 770）
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main(int argc, char** argv) {
    constexpr int window_width = 800;
    constexpr int window_height = 600;
    std::string startup_script = "samples/lua/main.lua";
    std::string script_arg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--script=", 0) == 0) {
            script_arg = arg.substr(9);
        }
    }
    if (!script_arg.empty()) {
        startup_script = script_arg;
    } else {
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
    }
    return dse::runtime::RunEngine({
        window_width,
        window_height,
        "DSEngine Lua Demo",
        BusinessMode::Lua,
        false,
        startup_script
    });
}
