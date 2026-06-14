/**
 * @file game_main.cpp
 * @brief DSEngine SDK 完整消费者 Demo —— C++ 宿主。
 *
 * 与同目录的 main.cpp（最小链接验证）不同，本程序演示一个真实游戏宿主：
 * 仅用已安装的 SDK 公共头，经 dse::runtime::RunEngine(BusinessMode::Lua)
 * 启动引擎，并加载 demo_scene.lua 搭建一个带 Jolt 物理的 3D 场景。
 *
 * 用法：
 *   consumer_game [--script=demo_scene.lua] [--frames=N]
 *   --frames=N 设置 DSE_MAX_FRAMES，N 帧后自动退出（便于 CI / 无人值守冒烟）。
 *
 * 运行前置：可执行文件目录需有引擎运行时（DLL）、demo_scene.lua 以及 data/
 * （着色器等运行时资源）。CMake POST_BUILD 已自动拷贝 DLL 与脚本；data/ 可由
 * 引擎 bin/data 拷贝或通过 DSE_DATA_ROOT 指定。
 */

#include "engine/runtime/engine_app.h"
#include "engine/runtime/app_manifest.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

void SetEnv(const char* key, const char* value) {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}

} // namespace

int main(int argc, char** argv) {
    std::string script = "demo_scene.lua";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--script=", 0) == 0) {
            script = arg.substr(9);
        } else if (arg.rfind("--frames=", 0) == 0) {
            // 转交引擎主循环：到达 DSE_MAX_FRAMES 后 RunOneFrame 返回 false。
            SetEnv("DSE_MAX_FRAMES", arg.substr(9).c_str());
        }
    }

    // 解析 exe 旁的资源：脚本绝对路径 + 自动设置 DSE_DATA_ROOT（着色器等）。
    fs::path exe_dir;
    try {
        exe_dir = fs::absolute(fs::path(argv[0])).parent_path();
    } catch (...) {}

    std::string startup_script = script;
    try {
        auto candidate = exe_dir / script;
        if (fs::exists(candidate)) startup_script = candidate.string();
    } catch (...) {}

    if (!std::getenv("DSE_DATA_ROOT")) {
        try {
            auto data_dir = exe_dir / "data";
            if (fs::exists(data_dir) && fs::is_directory(data_dir)) {
                SetEnv("DSE_DATA_ROOT", data_dir.string().c_str());
            }
        } catch (...) {}
    }

    dse::runtime::EngineRunConfig config;
    config.window_width = 1280;
    config.window_height = 720;
    config.window_title = "DSEngine SDK Consumer Demo";
    config.business_mode = BusinessMode::Lua;
    config.enable_editor = false;
    config.startup_lua_script_path = startup_script;

    return dse::runtime::RunEngine(config);
}
