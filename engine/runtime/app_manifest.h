/**
 * @file app_manifest.h
 * @brief 运行时启动清单（窗口 + splash 配置）的读取。
 *
 * 打包后的游戏在 exe 旁放一份松散的 game.dsmanifest（JSON，不含任何机密），
 * standalone 宿主在创建窗口/弹出 splash 之前读取它，从而让“打包出的成品游戏”
 * 自带品牌化的窗口标题/尺寸与启动 splash（logo + 淡入淡出 + 颜色 + 时序）。
 *
 * 该清单也能直接从工程的 project.dseproj 读取（同名 "window" / "splash" 段），
 * 这样编辑器内调试运行与打包运行保持一致来源。
 */

#ifndef DSE_RUNTIME_APP_MANIFEST_H
#define DSE_RUNTIME_APP_MANIFEST_H

#include <string>

#include "engine/core/dse_export.h"
#include "engine/platform/splash_screen.h"

namespace dse::runtime {

/// 启动清单：窗口与 splash 配置。各字段 has_* 指示清单是否显式给出。
struct AppManifest {
    bool has_window_title = false;
    std::string window_title;
    bool has_window_size = false;
    int window_width = 0;
    int window_height = 0;

    /// 入口 Lua 脚本（相对包/exe 的逻辑路径，如 "scripts/main.lua"）。
    /// standalone 宿主在未显式传 --script 时用它启动，使打包成品双击即可运行。
    bool has_entry_script = false;
    std::string entry_script;

    /// 清单是否包含 "splash" 段；为 true 时 splash 字段有效。
    bool has_splash = false;
    dse::platform::SplashConfig splash{};
};

/**
 * @brief 从 JSON 文件读取启动清单（game.dsmanifest 或 project.dseproj）。
 *
 * splash.image 若为相对路径，按清单所在目录解析为绝对路径。
 *
 * @param path  清单文件路径。
 * @param out   解析结果（仅覆盖清单中显式出现的字段）。
 * @return 文件存在且为合法 JSON 对象时返回 true。
 */
DSE_EXPORT bool LoadAppManifest(const std::string& path, AppManifest& out);

/**
 * @brief 将启动清单写为 JSON 文件（game.dsmanifest）。
 *
 * 仅写出标记为 has_* 的段：窗口段在 has_window_title/has_window_size 任一为真时写出，
 * splash 段在 has_splash 为真时写出。splash.image_path 原样写入 "image" 字段，
 * 调用方需保证该路径相对清单目录可解析。
 *
 * @return 写入成功返回 true。
 */
DSE_EXPORT bool WriteAppManifest(const std::string& path, const AppManifest& manifest);

} // namespace dse::runtime

#endif // DSE_RUNTIME_APP_MANIFEST_H
