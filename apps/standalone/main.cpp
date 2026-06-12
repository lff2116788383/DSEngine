#include "engine/runtime/engine_app.h"
#include "engine/runtime/app_manifest.h"
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
    std::string bundle_path;   // --bundle=game.bun（加密/明文资源包）
    std::string bundle_key;    // --key=...（AES-128-CTR 解密密钥）
    std::string lua_script  = "main.lua";
    std::string rhi_backend;   // --rhi=opengl|d3d11|vulkan
    std::string demo;          // --demo=3d_fracture
    int width  = 1280;
    int height = 720;
    std::string title = "DSEngine Game";
    // 命令行是否显式给出（用于让命令行覆盖 game.dsmanifest）。
    bool width_set = false;
    bool height_set = false;
    bool title_set = false;
};

StandaloneConfig ParseArgs(int argc, char** argv) {
    StandaloneConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--scene=", 0) == 0) {
            cfg.scene_path = arg.substr(8);
        } else if (arg.rfind("--pak=", 0) == 0) {
            cfg.pak_path = arg.substr(6);
        } else if (arg.rfind("--bundle=", 0) == 0) {
            cfg.bundle_path = arg.substr(9);
        } else if (arg.rfind("--key=", 0) == 0) {
            cfg.bundle_key = arg.substr(6);
        } else if (arg.rfind("--script=", 0) == 0) {
            cfg.lua_script = arg.substr(9);
        } else if (arg.rfind("--rhi=", 0) == 0) {
            cfg.rhi_backend = arg.substr(6);
        } else if (arg.rfind("--width=", 0) == 0) {
            cfg.width = std::atoi(arg.substr(8).c_str());
            cfg.width_set = true;
        } else if (arg.rfind("--height=", 0) == 0) {
            cfg.height = std::atoi(arg.substr(9).c_str());
            cfg.height_set = true;
        } else if (arg.rfind("--title=", 0) == 0) {
            cfg.title = arg.substr(8);
            cfg.title_set = true;
        } else if (arg.rfind("--demo=", 0) == 0) {
            cfg.demo = arg.substr(7);
        }
    }
    return cfg;
}

std::string FindFileNextToExe(const char* argv0, const std::string& extension) {
    try {
        auto exe_dir = std::filesystem::absolute(std::filesystem::path(argv0)).parent_path();
        for (const auto& entry : std::filesystem::directory_iterator(exe_dir)) {
            if (entry.path().extension() == extension) {
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
        cfg.pak_path = FindFileNextToExe(argv[0], ".dpak");
    }

    // Auto-detect encrypted/plain .bun bundle next to executable if not specified
    if (cfg.bundle_path.empty()) {
        auto detected = FindFileNextToExe(argv[0], ".bun");
        if (!detected.empty()) {
            cfg.bundle_path = detected;
        }
    }

    // Resolve Lua script path relative to exe.
    // 若磁盘存在则用绝对路径；否则保留逻辑路径（如 "scripts/main.lua"），
    // 由 Lua 运行时从已挂载的 .bun/.dpak VFS 读取（端到端加密场景）。
    std::string startup_script = cfg.lua_script;
    try {
        auto exe_dir = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
        auto candidate = exe_dir / cfg.lua_script;
        if (std::filesystem::exists(candidate)) {
            startup_script = candidate.string();
        }
    } catch (...) {}

    // Auto-detect data/ directory next to exe and set as data root (only if not already set)
    if (!std::getenv("DSE_DATA_ROOT")) {
        try {
            auto exe_dir = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
            auto data_dir = exe_dir / "data";
            if (std::filesystem::exists(data_dir) && std::filesystem::is_directory(data_dir)) {
                std::string data_root = data_dir.string();
#ifdef _WIN32
                _putenv_s("DSE_DATA_ROOT", data_root.c_str());
#else
                setenv("DSE_DATA_ROOT", data_root.c_str(), 1);
#endif
            }
        } catch (...) {}
    }

    // 读取 exe 旁的 game.dsmanifest（窗口 + 品牌化 splash），命令行参数优先级更高。
    dse::runtime::AppManifest manifest;
    bool has_manifest = false;
    try {
        auto exe_dir = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
        auto manifest_path = exe_dir / "game.dsmanifest";
        if (std::filesystem::exists(manifest_path)) {
            has_manifest = dse::runtime::LoadAppManifest(manifest_path.string(), manifest);
        }
    } catch (...) {}

    if (has_manifest) {
        if (manifest.has_window_title && !cfg.title_set) cfg.title = manifest.window_title;
        if (manifest.has_window_size) {
            if (!cfg.width_set && manifest.window_width > 0) cfg.width = manifest.window_width;
            if (!cfg.height_set && manifest.window_height > 0) cfg.height = manifest.window_height;
        }
    }

    dse::runtime::EngineRunConfig run_config;
    run_config.window_width = cfg.width;
    run_config.window_height = cfg.height;
    run_config.window_title = cfg.title;
    run_config.business_mode = BusinessMode::Lua;
    run_config.enable_editor = false;
    if (has_manifest && manifest.has_splash) {
        run_config.use_splash_config = true;
        run_config.splash = manifest.splash;
    }
    run_config.startup_lua_script_path = startup_script;
    run_config.asset_bundle_path = cfg.bundle_path;
    run_config.asset_bundle_key = cfg.bundle_key;
    run_config.asset_pak_path = cfg.pak_path;
    return dse::runtime::RunEngine(run_config);
}
