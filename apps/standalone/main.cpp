#include "engine/runtime/engine_app.h"
#include "engine/assets/pak_reader.h"
#include "engine/base/debug.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <iostream>

namespace {

struct StandaloneConfig {
    std::string scene_path  = "main.dscene";
    std::string pak_path;
    std::string lua_script  = "main.lua";
    std::string rhi_backend;   // --rhi=opengl|d3d11|vulkan
    std::string demo;          // --demo=3d_fracture
    int width  = 1280;
    int height = 720;
    std::string title = "DSEngine Game";
};

StandaloneConfig ParseArgs(int argc, char** argv) {
    StandaloneConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--scene=", 0) == 0) {
            cfg.scene_path = arg.substr(8);
        } else if (arg.rfind("--pak=", 0) == 0) {
            cfg.pak_path = arg.substr(6);
        } else if (arg.rfind("--script=", 0) == 0) {
            cfg.lua_script = arg.substr(9);
        } else if (arg.rfind("--rhi=", 0) == 0) {
            cfg.rhi_backend = arg.substr(6);
        } else if (arg.rfind("--width=", 0) == 0) {
            cfg.width = std::atoi(arg.substr(8).c_str());
        } else if (arg.rfind("--height=", 0) == 0) {
            cfg.height = std::atoi(arg.substr(9).c_str());
        } else if (arg.rfind("--title=", 0) == 0) {
            cfg.title = arg.substr(8);
        } else if (arg.rfind("--demo=", 0) == 0) {
            cfg.demo = arg.substr(7);
        }
    }
    return cfg;
}

std::string FindPakNextToExe(const char* argv0) {
    try {
        auto exe_dir = std::filesystem::absolute(std::filesystem::path(argv0)).parent_path();
        for (const auto& entry : std::filesystem::directory_iterator(exe_dir)) {
            if (entry.path().extension() == ".dpak") {
                return entry.path().string();
            }
        }
    } catch (...) {}
    return "";
}

} // namespace

int main(int argc, char** argv) {
    auto cfg = ParseArgs(argc, argv);

    // --demo= 设置环境变量 DSE_DEMO，供 Lua main.lua 读取
    if (!cfg.demo.empty()) {
#ifdef _WIN32
        _putenv_s("DSE_DEMO", cfg.demo.c_str());
#else
        setenv("DSE_DEMO", cfg.demo.c_str(), 1);
#endif
    }

    // --rhi= 命令行参数覆盖环境变量 DSE_RHI_BACKEND
    if (!cfg.rhi_backend.empty()) {
#ifdef _WIN32
        _putenv_s("DSE_RHI_BACKEND", cfg.rhi_backend.c_str());
#else
        setenv("DSE_RHI_BACKEND", cfg.rhi_backend.c_str(), 1);
#endif
    }

    // Auto-detect .dpak next to executable if not specified
    if (cfg.pak_path.empty()) {
        cfg.pak_path = FindPakNextToExe(argv[0]);
    }

    // Resolve Lua script path relative to exe
    std::string startup_script = cfg.lua_script;
    try {
        auto exe_dir = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
        auto candidate = exe_dir / cfg.lua_script;
        if (std::filesystem::exists(candidate)) {
            startup_script = candidate.string();
        }
    } catch (...) {}

    return dse::runtime::RunEngine({
        cfg.width,
        cfg.height,
        cfg.title,
        BusinessMode::Lua,
        false,
        startup_script
    });
}
