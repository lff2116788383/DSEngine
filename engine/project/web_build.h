/**
 * @file web_build.h
 * @brief Web(Emscripten) 构建：驱动既有 CMake 预设完成「配置 + 编译」，
 *        产出 emscripten 宿主产物（bin/index.{html,js,wasm[,data]}）。
 *        dse CLI `build --target web` 与 editor「Build Game → Web」共用同一份逻辑
 *        （P1-3：不再各自实现，editor 不再只压缩预构建目录）。
 *
 * 外部命令通过统一的 dse::platform::RunProcess 执行（参数数组、流式输出、退出码、
 * cancel/timeout、无控制台窗口），不再使用 std::system 拼接 shell 串。预设名推导、
 * 命令规划等纯逻辑可被单测覆盖。
 */

#ifndef DSE_PROJECT_WEB_BUILD_H
#define DSE_PROJECT_WEB_BUILD_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "engine/core/dse_export.h"

namespace dse::project {

/// 流式日志行回调：line 不含换行；is_stderr 区分 stderr。
using WebBuildLogFn = std::function<void(const std::string& line, bool is_stderr)>;

/// `build --target web` 的输入选项。
struct WebBuildOptions {
    std::string source_dir;     ///< 仓库根（含 CMakePresets.json）；为空由调用方先行解析
    std::string preset;         ///< 显式预设名；非空则优先，忽略 debug/enable_3d
    bool debug = false;         ///< true=web-debug*；false=web-release*（-Os）
    bool enable_3d = false;     ///< true=*-3d 预设（DSE_WEB_ENABLE_3D=ON）
    bool run_configure = true;  ///< 执行 `cmake --preset <p>`
    bool run_build = true;      ///< 执行 `cmake --build --preset <p>`

    // --- P1-3: 流式/取消/产物收集（默认关闭，保持向后兼容）---
    WebBuildLogFn on_line;                  ///< 非空则逐行回显 cmake 输出
    const std::atomic<bool>* cancel = nullptr; ///< 非空且变 true 时终止构建进程树
    bool collect_artifacts = false;         ///< 构建成功后收集 index.* 到 dist_dir
    std::string bin_dir;                    ///< emscripten 产物目录；空=<source>/bin
    std::string dist_dir;                   ///< 收集目标目录；空=<source>/dist/web
};

/// 配置/编译命令规划（纯字符串，便于日志与单测断言）。
struct WebBuildPlan {
    std::string preset;            ///< 解析后的预设名
    std::string configure_command; ///< 形如 `cmake --preset web-release`
    std::string build_command;     ///< 形如 `cmake --build --preset web-release`
};

/**
 * @brief 由开关推导预设名。explicit_preset 非空时直接返回它。
 *
 * 映射：debug×enable_3d → web-debug / web-release / web-debug-3d / web-release-3d。
 */
DSE_EXPORT std::string ResolveWebPreset(bool debug, bool enable_3d,
                                        const std::string& explicit_preset);

/// 由选项生成配置/编译命令串（不执行）。
DSE_EXPORT WebBuildPlan PlanWebBuild(const WebBuildOptions& opts);

/// Emscripten 工具链检测结果。
struct EmscriptenStatus {
    bool available = false;     ///< EMSDK 已设置且指向存在的目录
    std::string emsdk;          ///< EMSDK 环境变量值（若有）
    std::string detail;         ///< 诊断信息
    std::string install_hint;   ///< 缺失时的可操作安装/激活指引
};

/// 检测 EMSDK 环境变量与其目录是否就绪（不执行 emcc，纯环境检查，便于单测）。
DSE_EXPORT EmscriptenStatus DetectEmscripten();

struct WebBuildResult {
    bool ok = false;
    std::string error;
    std::string preset;             ///< 实际使用的预设名
    std::string configure_command;
    std::string build_command;
    int configure_exit = 0;         ///< cmake 配置返回码（未执行=0）
    int build_exit = 0;             ///< cmake 编译返回码（未执行=0）
    bool canceled = false;          ///< 因 cancel 标志被终止

    // --- P1-3: 产物收集结果（collect_artifacts=true 时填充）---
    std::string artifact_dir;               ///< 收集目标目录
    std::vector<std::string> artifacts;     ///< 已收集文件名
    std::uint64_t artifact_bytes = 0;       ///< 收集文件总字节数
};

/**
 * @brief 校验 EMSDK 与 source_dir 后，在 source_dir 依次执行配置/编译。
 *
 * 命令经 dse::platform::RunProcess 执行（cmake + 参数数组），输出可经 on_line 流式
 * 回显，cancel 可中止。build 成功且 collect_artifacts=true 时收集 index.* 到 dist_dir。
 * 失败时 error 含原因；configure_exit/build_exit 记录各步返回码。
 * 需要环境变量 `EMSDK` 已设置（web 预设的工具链文件依赖它）。
 */
DSE_EXPORT WebBuildResult RunWebBuild(const WebBuildOptions& opts);

} // namespace dse::project

#endif // DSE_PROJECT_WEB_BUILD_H
