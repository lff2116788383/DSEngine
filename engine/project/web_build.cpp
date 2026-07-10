/**
 * @file web_build.cpp
 * @brief ResolveWebPreset / PlanWebBuild / DetectEmscripten / RunWebBuild 实现 — 见 web_build.h。
 *
 * P1-3：外部命令改由统一的 dse::platform::RunProcess 执行（参数数组、流式输出、退出码、
 * cancel），不再用 std::system 拼接 shell 串、也不再 chdir 进程全局目录。
 */

#include "engine/project/web_build.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#include "engine/platform/process.h"
#include "engine/project/web_dist.h"

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

EmscriptenStatus DetectEmscripten() {
    EmscriptenStatus st;
    const char* emsdk = std::getenv("EMSDK");
    st.install_hint =
        "安装并激活 Emscripten：git clone https://github.com/emscripten-core/emsdk，"
        "然后 `emsdk install latest && emsdk activate latest`，"
        "并在当前会话 source 其 emsdk_env 脚本（设置 EMSDK 环境变量）后重试。";
    if (emsdk == nullptr || emsdk[0] == '\0') {
        st.available = false;
        st.detail = "未设置环境变量 EMSDK";
        return st;
    }
    // EMSDK 已设置即视为就绪；其目录/工具链文件是否有效交由 cmake 预设判定
    // （保持与既有 CLI 行为一致，便于单测在不安装 emsdk 的情况下覆盖后续逻辑）。
    st.emsdk = emsdk;
    st.available = true;
    st.detail = "EMSDK=" + st.emsdk;
    return st;
}

WebBuildResult RunWebBuild(const WebBuildOptions& opts) {
    WebBuildResult result;
    const WebBuildPlan plan = PlanWebBuild(opts);
    result.preset = plan.preset;
    result.configure_command = plan.configure_command;
    result.build_command = plan.build_command;

    auto log = [&](const std::string& line, bool err) {
        if (opts.on_line) opts.on_line(line, err);
    };

    // web 预设的工具链文件以 $env{EMSDK} 展开；缺失时 cmake 会判定预设被禁用，
    // 提前给出可操作的提示比让 cmake 报「preset disabled」更友好。
    EmscriptenStatus em = DetectEmscripten();
    if (!em.available) {
        result.error = em.detail + "；" + em.install_hint;
        log(result.error, true);
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

    // cmake --preset / --build --preset 需在含 CMakePresets.json 的目录运行。
    // 通过 RunProcess 的 working_dir 指定，避免修改进程全局 cwd（线程安全）。
    auto run_cmake = [&](const std::vector<std::string>& args, int& exit_out) -> bool {
        dse::platform::ProcessOptions po;
        po.executable = "cmake";
        po.args = args;
        po.working_dir = source;
        po.merge_stderr = false;
        auto sink = [&](std::string_view l, bool is_err) { log(std::string(l), is_err); };
        dse::platform::ProcessResult pr = dse::platform::RunProcess(
            po, sink, std::chrono::milliseconds::zero(), opts.cancel);
        if (pr.canceled) {
            result.canceled = true;
            result.error = "构建已取消";
            return false;
        }
        if (!pr.launched) {
            result.error = pr.error.empty() ? "无法启动 cmake（是否在 PATH 中？）" : pr.error;
            return false;
        }
        exit_out = pr.exit_code;
        return pr.exit_code == 0;
    };

    if (opts.run_configure) {
        log("$ " + plan.configure_command, false);
        if (!run_cmake({"--preset", plan.preset}, result.configure_exit)) {
            if (result.error.empty())
                result.error = "cmake 配置失败 (" + plan.configure_command + ")";
            return result;
        }
    }

    if (opts.run_build) {
        log("$ " + plan.build_command, false);
        if (!run_cmake({"--build", "--preset", plan.preset}, result.build_exit)) {
            if (result.error.empty())
                result.error = "cmake 编译失败 (" + plan.build_command + ")";
            return result;
        }
    }

    if (opts.collect_artifacts) {
        const std::string in_dir = opts.bin_dir.empty()
            ? (source / "bin").string() : opts.bin_dir;
        const std::string out_dir = opts.dist_dir.empty()
            ? (source / "dist" / "web").string() : opts.dist_dir;
        log("收集 Web 产物: " + in_dir + " -> " + out_dir, false);
        WebDistResult dist = CollectWebDistribution(in_dir, out_dir);
        if (!dist.ok) {
            result.error = "收集 Web 产物失败: " + dist.error;
            log(result.error, true);
            return result;
        }
        result.artifact_dir = out_dir;
        result.artifacts = dist.files;
        result.artifact_bytes = dist.total_bytes;
        for (const auto& f : dist.files) log("  + " + f, false);
    }

    result.ok = true;
    return result;
}

} // namespace dse::project
