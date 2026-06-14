/**
 * @file web_build.cpp
 * @brief ResolveWebPreset / PlanWebBuild / RunWebBuild 实现 — 见 web_build.h。
 */

#include "engine/project/web_build.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace dse::project {

std::string ResolveWebPreset(bool debug, bool enable_3d,
                             const std::string& explicit_preset) {
    if (!explicit_preset.empty()) {
        return explicit_preset;
    }
    std::string preset = debug ? "web-debug" : "web-release";
    if (enable_3d) {
        preset += "-3d";
    }
    return preset;
}

WebBuildPlan PlanWebBuild(const WebBuildOptions& opts) {
    WebBuildPlan plan;
    plan.preset = ResolveWebPreset(opts.debug, opts.enable_3d, opts.preset);
    plan.configure_command = "cmake --preset " + plan.preset;
    plan.build_command = "cmake --build --preset " + plan.preset;
    return plan;
}

WebBuildResult RunWebBuild(const WebBuildOptions& opts) {
    WebBuildResult result;
    const WebBuildPlan plan = PlanWebBuild(opts);
    result.preset = plan.preset;
    result.configure_command = plan.configure_command;
    result.build_command = plan.build_command;

    // web 预设的工具链文件以 $env{EMSDK} 展开；缺失时 cmake 会判定预设被禁用，
    // 提前给出可操作的提示比让 cmake 报「preset disabled」更友好。
    const char* emsdk = std::getenv("EMSDK");
    if (emsdk == nullptr || emsdk[0] == '\0') {
        result.error =
            "未设置环境变量 EMSDK；请先安装并激活 Emscripten "
            "(emsdk install latest && emsdk activate latest)，"
            "或在当前会话 source 其 emsdk_env 脚本后重试";
        return result;
    }

    std::error_code ec;
    if (opts.source_dir.empty()) {
        result.error = "未指定仓库根目录 (source_dir)";
        return result;
    }
    const fs::path source(opts.source_dir);
    if (!fs::is_directory(source, ec)) {
        result.error = "仓库根目录不存在: " + opts.source_dir;
        return result;
    }
    if (!fs::exists(source / "CMakePresets.json", ec)) {
        result.error = "在 " + opts.source_dir + " 未找到 CMakePresets.json";
        return result;
    }

    // cmake --preset / --build --preset 需在含 CMakePresets.json 的目录运行；
    // 切到 source_dir 执行后再恢复，避免对命令做平台相关的路径转义。
    const fs::path prev_cwd = fs::current_path(ec);
    fs::current_path(source, ec);
    if (ec) {
        result.error = "无法进入仓库根目录: " + ec.message();
        return result;
    }

    auto restore = [&]() {
        std::error_code rec;
        fs::current_path(prev_cwd, rec);
    };

    if (opts.run_configure) {
        result.configure_exit = std::system(plan.configure_command.c_str());
        if (result.configure_exit != 0) {
            result.error = "cmake 配置失败 (" + plan.configure_command + ")";
            restore();
            return result;
        }
    }

    if (opts.run_build) {
        result.build_exit = std::system(plan.build_command.c_str());
        if (result.build_exit != 0) {
            result.error = "cmake 编译失败 (" + plan.build_command + ")";
            restore();
            return result;
        }
    }

    restore();
    result.ok = true;
    return result;
}

} // namespace dse::project
