/**
 * @file web_build.h
 * @brief Web(Emscripten) 构建：驱动既有 CMake 预设完成「配置 + 编译」，
 *        产出 emscripten 宿主产物（bin/index.{html,js,wasm[,data]}）。
 *        dse CLI `build --target web` 与单元测试共用同一份逻辑。
 *
 * 设计与 web_dist 保持一致：纯逻辑（预设名推导、命令规划）可被单测覆盖，
 * 真正的外部命令执行（cmake 配置/编译）收敛在 RunWebBuild 一处薄封装里。
 * 不引入平台宏到引擎核心；仅依赖 std::filesystem 与现成 CMake 预设。
 */

#ifndef DSE_PROJECT_WEB_BUILD_H
#define DSE_PROJECT_WEB_BUILD_H

#include <string>

#include "engine/core/dse_export.h"

namespace dse::project {

/// `build --target web` 的输入选项。
struct WebBuildOptions {
    std::string source_dir;     ///< 仓库根（含 CMakePresets.json）；为空由调用方先行解析
    std::string preset;         ///< 显式预设名；非空则优先，忽略 debug/enable_3d
    bool debug = false;         ///< true=web-debug*；false=web-release*（-Os）
    bool enable_3d = false;     ///< true=*-3d 预设（DSE_WEB_ENABLE_3D=ON）
    bool run_configure = true;  ///< 执行 `cmake --preset <p>`
    bool run_build = true;      ///< 执行 `cmake --build --preset <p>`
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

struct WebBuildResult {
    bool ok = false;
    std::string error;
    std::string preset;             ///< 实际使用的预设名
    std::string configure_command;
    std::string build_command;
    int configure_exit = 0;         ///< cmake 配置返回码（未执行=0）
    int build_exit = 0;             ///< cmake 编译返回码（未执行=0）
};

/**
 * @brief 校验 EMSDK 与 source_dir 后，切到 source_dir 依次执行配置/编译。
 *
 * 失败时 error 含原因；configure_exit/build_exit 记录各步返回码。
 * 需要环境变量 `EMSDK` 已设置（web 预设的工具链文件依赖它）。
 */
DSE_EXPORT WebBuildResult RunWebBuild(const WebBuildOptions& opts);

} // namespace dse::project

#endif // DSE_PROJECT_WEB_BUILD_H
